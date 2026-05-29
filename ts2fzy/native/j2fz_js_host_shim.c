#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

typedef int32_t (*j2fz_callback_i32_i32_t)(int32_t);

typedef struct pending_request pending_request_t;
typedef struct callback_registration callback_registration_t;
typedef struct utf8_cache utf8_cache_t;

struct pending_request {
  uint64_t id;
  int complete;
  int ok;
  char *payload;
  pthread_cond_t cond;
  pending_request_t *next;
};

struct callback_registration {
  uint64_t handle;
  j2fz_callback_i32_i32_t callback;
  void *ctx;
  callback_registration_t *next;
};

struct utf8_cache {
  uint64_t value_handle;
  uint8_t *bytes;
  size_t len;
  utf8_cache_t *next;
};

static int g_socket_fd = -1;
static int g_reader_started = 0;
static int g_reader_running = 0;
static uint64_t g_next_request_id = 1;
static uint64_t g_next_registration_handle = 1;
static pthread_t g_reader_thread;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static pending_request_t *g_pending = NULL;
static callback_registration_t *g_registrations = NULL;
static utf8_cache_t *g_utf8_cache = NULL;

static int ensure_connected(void);
static int send_request(const char *op, const char *args, char **payload_out);
static void *reader_main(void *arg);
static int write_full(const char *data, size_t len);
static int write_line(const char *line);
static int parse_u64_value(const char *raw, uint64_t *out);
static int parse_i32_value(const char *raw, int32_t *out);
static int parse_f64_value(const char *raw, double *out);
static char *hex_encode(const uint8_t *bytes, size_t len);
static uint8_t *hex_decode(const char *hex, size_t *out_len);
static callback_registration_t *find_registration(uint64_t handle);
static void free_utf8_cache(uint64_t value_handle);

static char *dup_cstr(const char *value) {
  size_t len = strlen(value);
  char *out = (char *)malloc(len + 1);
  if (out == NULL) {
    return NULL;
  }
  memcpy(out, value, len + 1);
  return out;
}

static void append_pending(pending_request_t *pending) {
  pending->next = g_pending;
  g_pending = pending;
}

static pending_request_t *find_pending(uint64_t id) {
  pending_request_t *cursor = g_pending;
  while (cursor != NULL) {
    if (cursor->id == id) {
      return cursor;
    }
    cursor = cursor->next;
  }
  return NULL;
}

static void remove_pending(uint64_t id) {
  pending_request_t **cursor = &g_pending;
  while (*cursor != NULL) {
    if ((*cursor)->id == id) {
      pending_request_t *found = *cursor;
      *cursor = found->next;
      found->next = NULL;
      return;
    }
    cursor = &(*cursor)->next;
  }
}

static callback_registration_t *find_registration(uint64_t handle) {
  callback_registration_t *cursor = g_registrations;
  while (cursor != NULL) {
    if (cursor->handle == handle) {
      return cursor;
    }
    cursor = cursor->next;
  }
  return NULL;
}

static utf8_cache_t *find_utf8_cache(uint64_t value_handle) {
  utf8_cache_t *cursor = g_utf8_cache;
  while (cursor != NULL) {
    if (cursor->value_handle == value_handle) {
      return cursor;
    }
    cursor = cursor->next;
  }
  return NULL;
}

static void free_utf8_cache(uint64_t value_handle) {
  utf8_cache_t **cursor = &g_utf8_cache;
  while (*cursor != NULL) {
    if ((*cursor)->value_handle == value_handle) {
      utf8_cache_t *found = *cursor;
      *cursor = found->next;
      free(found->bytes);
      free(found);
      return;
    }
    cursor = &(*cursor)->next;
  }
}

static int store_utf8_cache(uint64_t value_handle, const uint8_t *bytes, size_t len, uint64_t *ptr_out, size_t *len_out) {
  utf8_cache_t *cache = find_utf8_cache(value_handle);
  if (cache == NULL) {
    cache = (utf8_cache_t *)calloc(1, sizeof(utf8_cache_t));
    if (cache == NULL) {
      return -1;
    }
    cache->value_handle = value_handle;
    cache->next = g_utf8_cache;
    g_utf8_cache = cache;
  } else {
    free(cache->bytes);
    cache->bytes = NULL;
    cache->len = 0;
  }
  cache->bytes = (uint8_t *)malloc(len + 1);
  if (cache->bytes == NULL) {
    return -1;
  }
  memcpy(cache->bytes, bytes, len);
  cache->bytes[len] = 0;
  cache->len = len;
  *ptr_out = (uint64_t)(uintptr_t)cache->bytes;
  *len_out = len;
  return 0;
}

static int hex_value(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
  if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
  return -1;
}

static char *hex_encode(const uint8_t *bytes, size_t len) {
  static const char digits[] = "0123456789abcdef";
  char *out = (char *)malloc(len * 2 + 1);
  if (out == NULL) {
    return NULL;
  }
  for (size_t index = 0; index < len; index += 1) {
    out[index * 2] = digits[(bytes[index] >> 4) & 0x0f];
    out[index * 2 + 1] = digits[bytes[index] & 0x0f];
  }
  out[len * 2] = 0;
  return out;
}

static uint8_t *hex_decode(const char *hex, size_t *out_len) {
  size_t len = strlen(hex);
  if ((len % 2) != 0) {
    return NULL;
  }
  uint8_t *out = (uint8_t *)malloc(len / 2 + 1);
  if (out == NULL) {
    return NULL;
  }
  for (size_t index = 0; index < len; index += 2) {
    int hi = hex_value(hex[index]);
    int lo = hex_value(hex[index + 1]);
    if (hi < 0 || lo < 0) {
      free(out);
      return NULL;
    }
    out[index / 2] = (uint8_t)((hi << 4) | lo);
  }
  out[len / 2] = 0;
  *out_len = len / 2;
  return out;
}

static int parse_u64_value(const char *raw, uint64_t *out) {
  if (raw == NULL || *raw == 0) {
    return -1;
  }
  errno = 0;
  unsigned long long value = strtoull(raw, NULL, 10);
  if (errno != 0) {
    return -1;
  }
  *out = (uint64_t)value;
  return 0;
}

static int parse_i32_value(const char *raw, int32_t *out) {
  if (raw == NULL || *raw == 0) {
    return -1;
  }
  errno = 0;
  long value = strtol(raw, NULL, 10);
  if (errno != 0 || value < INT32_MIN || value > INT32_MAX) {
    return -1;
  }
  *out = (int32_t)value;
  return 0;
}

static int parse_f64_value(const char *raw, double *out) {
  if (raw == NULL || *raw == 0) {
    return -1;
  }
  errno = 0;
  double value = strtod(raw, NULL);
  if (errno != 0) {
    return -1;
  }
  *out = value;
  return 0;
}

static int write_full(const char *data, size_t len) {
  size_t offset = 0;
  while (offset < len) {
    ssize_t written = write(g_socket_fd, data + offset, len - offset);
    if (written < 0) {
      if (errno == EINTR) {
        continue;
      }
      return -1;
    }
    offset += (size_t)written;
  }
  return 0;
}

static int write_line(const char *line) {
  size_t len = strlen(line);
  if (write_full(line, len) != 0) {
    return -1;
  }
  return write_full("\n", 1);
}

static int ensure_connected(void) {
  if (g_socket_fd >= 0) {
    return 0;
  }
  const char *socket_path = getenv("J2FZ_JS_HOST_SOCKET");
  if (socket_path == NULL || *socket_path == 0) {
    return -1;
  }
  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    return -1;
  }
  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  if (strlen(socket_path) >= sizeof(addr.sun_path)) {
    close(fd);
    return -1;
  }
  strcpy(addr.sun_path, socket_path);
  if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
    close(fd);
    return -1;
  }
  g_socket_fd = fd;
  g_reader_running = 1;
  if (!g_reader_started) {
    if (pthread_create(&g_reader_thread, NULL, reader_main, NULL) != 0) {
      close(g_socket_fd);
      g_socket_fd = -1;
      g_reader_running = 0;
      return -1;
    }
    g_reader_started = 1;
  }
  return 0;
}

static int send_request(const char *op, const char *args, char **payload_out) {
  *payload_out = NULL;
  pthread_mutex_lock(&g_lock);
  if (ensure_connected() != 0) {
    pthread_mutex_unlock(&g_lock);
    return -1;
  }
  pending_request_t *pending = (pending_request_t *)calloc(1, sizeof(pending_request_t));
  if (pending == NULL) {
    pthread_mutex_unlock(&g_lock);
    return -1;
  }
  pending->id = g_next_request_id++;
  pthread_cond_init(&pending->cond, NULL);
  append_pending(pending);
  char request[16384];
  if (args != NULL && *args != 0) {
    snprintf(request, sizeof(request), "REQ\t%llu\t%s\t%s",
      (unsigned long long)pending->id, op, args);
  } else {
    snprintf(request, sizeof(request), "REQ\t%llu\t%s",
      (unsigned long long)pending->id, op);
  }
  if (write_line(request) != 0) {
    remove_pending(pending->id);
    pthread_cond_destroy(&pending->cond);
    free(pending);
    pthread_mutex_unlock(&g_lock);
    return -1;
  }
  while (!pending->complete) {
    pthread_cond_wait(&pending->cond, &g_lock);
  }
  remove_pending(pending->id);
  int ok = pending->ok;
  if (pending->payload != NULL) {
    *payload_out = pending->payload;
    pending->payload = NULL;
  }
  pthread_cond_destroy(&pending->cond);
  free(pending);
  pthread_mutex_unlock(&g_lock);
  return ok ? 0 : -1;
}

static int handle_response_line(char *line) {
  char *save = NULL;
  char *kind = strtok_r(line, "\t", &save);
  if (kind == NULL) {
    return 0;
  }
  if (strcmp(kind, "RES") == 0) {
    char *id_raw = strtok_r(NULL, "\t", &save);
    char *status = strtok_r(NULL, "\t", &save);
    char *payload = save;
    uint64_t id = 0;
    if (parse_u64_value(id_raw, &id) != 0 || status == NULL) {
      return 0;
    }
    pthread_mutex_lock(&g_lock);
    pending_request_t *pending = find_pending(id);
    if (pending != NULL) {
      pending->ok = strcmp(status, "OK") == 0;
      pending->payload = payload != NULL ? dup_cstr(payload) : dup_cstr("");
      pending->complete = 1;
      pthread_cond_signal(&pending->cond);
    }
    pthread_mutex_unlock(&g_lock);
    return 0;
  }
  if (strcmp(kind, "CBREQ") == 0) {
    char *cb_id_raw = strtok_r(NULL, "\t", &save);
    char *handle_raw = strtok_r(NULL, "\t", &save);
    char *value_raw = strtok_r(NULL, "\t", &save);
    uint64_t cb_id = 0;
    uint64_t handle = 0;
    int32_t value = 0;
    if (parse_u64_value(cb_id_raw, &cb_id) != 0
        || parse_u64_value(handle_raw, &handle) != 0
        || parse_i32_value(value_raw, &value) != 0) {
      return 0;
    }
    callback_registration_t *registration = NULL;
    pthread_mutex_lock(&g_lock);
    registration = find_registration(handle);
    pthread_mutex_unlock(&g_lock);
    char response[512];
    if (registration == NULL || registration->callback == NULL) {
      char *message = hex_encode((const uint8_t *)"missing callback", strlen("missing callback"));
      if (message == NULL) {
        return 0;
      }
      snprintf(response, sizeof(response), "CBRES\t%llu\tERR\t%s",
        (unsigned long long)cb_id, message);
      free(message);
      return write_line(response);
    }
    int32_t result = registration->callback(value);
    snprintf(response, sizeof(response), "CBRES\t%llu\tOK\t%d",
      (unsigned long long)cb_id, (int)result);
    return write_line(response);
  }
  return 0;
}

static void *reader_main(void *arg) {
  (void)arg;
  char buffer[4096];
  char line[16384];
  size_t line_len = 0;
  while (g_reader_running) {
    ssize_t read_count = read(g_socket_fd, buffer, sizeof(buffer));
    if (read_count <= 0) {
      break;
    }
    for (ssize_t index = 0; index < read_count; index += 1) {
      char ch = buffer[index];
      if (ch == '\n') {
        line[line_len] = 0;
        handle_response_line(line);
        line_len = 0;
      } else if (line_len + 1 < sizeof(line)) {
        line[line_len++] = ch;
      }
    }
  }
  return NULL;
}

static int request_handle_op(const char *op, const char *args, uint64_t *out) {
  char *payload = NULL;
  if (send_request(op, args, &payload) != 0) {
    free(payload);
    return -1;
  }
  int rc = parse_u64_value(payload, out);
  free(payload);
  return rc;
}

int32_t j2fz_js_module_open(const uint8_t *spec_borrowed, size_t len, uint64_t *module_out, void *module_ctx) {
  (void)module_ctx;
  if (spec_borrowed == NULL || module_out == NULL) return -1;
  char *spec_hex = hex_encode(spec_borrowed, len);
  if (spec_hex == NULL) return -1;
  uint64_t handle = 0;
  int rc = request_handle_op("OPEN_MODULE", spec_hex, &handle);
  free(spec_hex);
  if (rc != 0) return -1;
  *module_out = handle;
  return 0;
}

int32_t j2fz_js_module_close(uint64_t module_handle) {
  char args[64];
  snprintf(args, sizeof(args), "%llu", (unsigned long long)module_handle);
  char *payload = NULL;
  int rc = send_request("CLOSE_MODULE", args, &payload);
  free(payload);
  return rc == 0 ? 0 : -1;
}

int32_t j2fz_js_export_get(uint64_t module_handle, const uint8_t *export_borrowed, size_t len, uint64_t *export_out, void *export_ctx) {
  (void)export_ctx;
  if (export_borrowed == NULL || export_out == NULL) return -1;
  char *export_hex = hex_encode(export_borrowed, len);
  if (export_hex == NULL) return -1;
  char args[16384];
  snprintf(args, sizeof(args), "%llu\t%s", (unsigned long long)module_handle, export_hex);
  uint64_t handle = 0;
  int rc = request_handle_op("GET_EXPORT", args, &handle);
  free(export_hex);
  if (rc != 0) return -1;
  *export_out = handle;
  return 0;
}

int32_t j2fz_js_export_release(uint64_t export_handle) {
  char args[64];
  snprintf(args, sizeof(args), "%llu", (unsigned long long)export_handle);
  char *payload = NULL;
  int rc = send_request("RELEASE_EXPORT", args, &payload);
  free(payload);
  return rc == 0 ? 0 : -1;
}

int32_t j2fz_js_value_release(uint64_t value_handle) {
  free_utf8_cache(value_handle);
  char args[64];
  snprintf(args, sizeof(args), "%llu", (unsigned long long)value_handle);
  char *payload = NULL;
  int rc = send_request("RELEASE_VALUE", args, &payload);
  free(payload);
  return rc == 0 ? 0 : -1;
}

static int32_t create_simple_value(const char *op, const char *args, uint64_t *value_out, size_t value_len) {
  if (value_out == NULL || value_len == 0) return -1;
  uint64_t handle = 0;
  if (request_handle_op(op, args, &handle) != 0) return -1;
  *value_out = handle;
  return 0;
}

int32_t j2fz_js_value_null(uint64_t *value_out, size_t value_len) {
  return create_simple_value("CREATE_NULL", "", value_out, value_len);
}

int32_t j2fz_js_value_bool(int32_t value, uint64_t *value_out, size_t value_len) {
  char args[32];
  snprintf(args, sizeof(args), "%d", value != 0 ? 1 : 0);
  return create_simple_value("CREATE_BOOL", args, value_out, value_len);
}

int32_t j2fz_js_value_i32(int32_t value, uint64_t *value_out, size_t value_len) {
  char args[32];
  snprintf(args, sizeof(args), "%d", (int)value);
  return create_simple_value("CREATE_I32", args, value_out, value_len);
}

int32_t j2fz_js_value_u64(uint64_t value, uint64_t *value_out, size_t value_len) {
  char args[64];
  snprintf(args, sizeof(args), "%llu", (unsigned long long)value);
  return create_simple_value("CREATE_U64", args, value_out, value_len);
}

int32_t j2fz_js_value_f64(double value, uint64_t *value_out, size_t value_len) {
  char args[128];
  snprintf(args, sizeof(args), "%.17g", value);
  return create_simple_value("CREATE_F64", args, value_out, value_len);
}

int32_t j2fz_js_value_str(const uint8_t *value_borrowed, size_t len, uint64_t *value_out, size_t value_len) {
  if (value_borrowed == NULL) return -1;
  char *hex = hex_encode(value_borrowed, len);
  if (hex == NULL) return -1;
  int32_t rc = create_simple_value("CREATE_STR", hex, value_out, value_len);
  free(hex);
  return rc;
}

int32_t j2fz_js_value_bytes(const uint8_t *ptr_borrowed, size_t len, uint64_t *value_out, size_t value_len) {
  if (ptr_borrowed == NULL) return -1;
  char *hex = hex_encode(ptr_borrowed, len);
  if (hex == NULL) return -1;
  int32_t rc = create_simple_value("CREATE_BYTES", hex, value_out, value_len);
  free(hex);
  return rc;
}

static int32_t request_scalar_i32(const char *op, uint64_t handle, int32_t *value_out, size_t value_len) {
  if (value_out == NULL || value_len == 0) return -1;
  char args[64];
  snprintf(args, sizeof(args), "%llu", (unsigned long long)handle);
  char *payload = NULL;
  if (send_request(op, args, &payload) != 0) {
    free(payload);
    return -1;
  }
  int rc = parse_i32_value(payload, value_out);
  free(payload);
  return rc;
}

static int32_t request_scalar_u64(const char *op, uint64_t handle, uint64_t *value_out, size_t value_len) {
  if (value_out == NULL || value_len == 0) return -1;
  char args[64];
  snprintf(args, sizeof(args), "%llu", (unsigned long long)handle);
  char *payload = NULL;
  if (send_request(op, args, &payload) != 0) {
    free(payload);
    return -1;
  }
  int rc = parse_u64_value(payload, value_out);
  free(payload);
  return rc;
}

int32_t j2fz_js_value_as_bool(uint64_t value_handle, int32_t *value_out, size_t value_len) {
  return request_scalar_i32("AS_BOOL", value_handle, value_out, value_len);
}

int32_t j2fz_js_value_as_i32(uint64_t value_handle, int32_t *value_out, size_t value_len) {
  return request_scalar_i32("AS_I32", value_handle, value_out, value_len);
}

int32_t j2fz_js_value_as_u64(uint64_t value_handle, uint64_t *value_out, size_t value_len) {
  return request_scalar_u64("AS_U64", value_handle, value_out, value_len);
}

int32_t j2fz_js_value_as_f64(uint64_t value_handle, double *value_out, size_t value_len) {
  if (value_out == NULL || value_len == 0) return -1;
  char args[64];
  snprintf(args, sizeof(args), "%llu", (unsigned long long)value_handle);
  char *payload = NULL;
  if (send_request("AS_F64", args, &payload) != 0) {
    free(payload);
    return -1;
  }
  int rc = parse_f64_value(payload, value_out);
  free(payload);
  return rc;
}

int32_t j2fz_js_value_as_utf8(uint64_t value_handle, uint64_t *utf8_ptr_out, size_t utf8_ptr_len, size_t *utf8_len_out, size_t utf8_len_len) {
  if (utf8_ptr_out == NULL || utf8_len_out == NULL || utf8_ptr_len == 0 || utf8_len_len == 0) return -1;
  char args[64];
  snprintf(args, sizeof(args), "%llu", (unsigned long long)value_handle);
  char *payload = NULL;
  if (send_request("AS_UTF8", args, &payload) != 0) {
    free(payload);
    return -1;
  }
  size_t decoded_len = 0;
  uint8_t *decoded = hex_decode(payload, &decoded_len);
  free(payload);
  if (decoded == NULL) return -1;
  int rc = store_utf8_cache(value_handle, decoded, decoded_len, utf8_ptr_out, utf8_len_out);
  free(decoded);
  return rc == 0 ? 0 : -1;
}

int32_t j2fz_js_value_copy_utf8(uint64_t value_handle, uint8_t *ptr_out, size_t len) {
  if (ptr_out == NULL) return -1;
  char args[64];
  snprintf(args, sizeof(args), "%llu", (unsigned long long)value_handle);
  char *payload = NULL;
  if (send_request("COPY_UTF8", args, &payload) != 0) {
    free(payload);
    return -1;
  }
  size_t decoded_len = 0;
  uint8_t *decoded = hex_decode(payload, &decoded_len);
  free(payload);
  if (decoded == NULL) return -1;
  if (len < decoded_len) {
    free(decoded);
    return -1;
  }
  memcpy(ptr_out, decoded, decoded_len);
  free(decoded);
  return 0;
}

int32_t j2fz_js_call(uint64_t function_handle, const uint64_t *argv_borrowed, size_t argv_len, uint64_t *result_out, size_t result_len) {
  if (result_out == NULL || result_len == 0) return -1;
  char args[16384];
  size_t offset = (size_t)snprintf(args, sizeof(args), "%llu\t",
    (unsigned long long)function_handle);
  for (size_t index = 0; index < argv_len; index += 1) {
    if (index > 0) {
      offset += (size_t)snprintf(args + offset, sizeof(args) - offset, ",");
    }
    offset += (size_t)snprintf(args + offset, sizeof(args) - offset, "%llu",
      (unsigned long long)argv_borrowed[index]);
  }
  uint64_t handle = 0;
  if (request_handle_op("CALL", args, &handle) != 0) return -1;
  *result_out = handle;
  return 0;
}

int32_t j2fz_js_await(uint64_t value_handle, uint64_t *result_out, size_t result_len) {
  if (result_out == NULL || result_len == 0) return -1;
  char args[64];
  snprintf(args, sizeof(args), "%llu", (unsigned long long)value_handle);
  uint64_t handle = 0;
  if (request_handle_op("AWAIT", args, &handle) != 0) return -1;
  *result_out = handle;
  return 0;
}

int32_t j2fz_js_host_register_i32_i32(const uint8_t *name_borrowed, size_t len, j2fz_callback_i32_i32_t cb, void *cb_ctx, uint64_t *registration_out, void *registration_ctx) {
  (void)registration_ctx;
  if (name_borrowed == NULL || cb == NULL || registration_out == NULL) return -1;
  callback_registration_t *registration = (callback_registration_t *)calloc(1, sizeof(callback_registration_t));
  if (registration == NULL) return -1;
  registration->handle = g_next_registration_handle++;
  registration->callback = cb;
  registration->ctx = cb_ctx;
  char *name_hex = hex_encode(name_borrowed, len);
  if (name_hex == NULL) {
    free(registration);
    return -1;
  }
  char args[16384];
  snprintf(args, sizeof(args), "%llu\t%s",
    (unsigned long long)registration->handle, name_hex);
  free(name_hex);
  char *payload = NULL;
  if (send_request("REGISTER_NATIVE_I32_I32", args, &payload) != 0) {
    free(payload);
    free(registration);
    return -1;
  }
  free(payload);
  pthread_mutex_lock(&g_lock);
  registration->next = g_registrations;
  g_registrations = registration;
  pthread_mutex_unlock(&g_lock);
  *registration_out = registration->handle;
  return 0;
}

int32_t j2fz_js_host_registration_release(uint64_t registration_handle) {
  char args[64];
  snprintf(args, sizeof(args), "%llu", (unsigned long long)registration_handle);
  char *payload = NULL;
  int rc = send_request("UNREGISTER_NATIVE_I32_I32", args, &payload);
  free(payload);
  pthread_mutex_lock(&g_lock);
  callback_registration_t **cursor = &g_registrations;
  while (*cursor != NULL) {
    if ((*cursor)->handle == registration_handle) {
      callback_registration_t *found = *cursor;
      *cursor = found->next;
      free(found);
      break;
    }
    cursor = &(*cursor)->next;
  }
  pthread_mutex_unlock(&g_lock);
  return rc == 0 ? 0 : -1;
}

int32_t j2fz_test_add_three_i32(int32_t value) {
  return value + 3;
}

void *j2fz_test_add_three_i32_ptr(void) {
  return (void *)(uintptr_t)&j2fz_test_add_three_i32;
}
