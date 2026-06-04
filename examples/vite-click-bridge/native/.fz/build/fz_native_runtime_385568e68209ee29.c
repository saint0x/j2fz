#include <arpa/inet.h>
#include <ctype.h>
#ifdef __APPLE__
#include <crt_externs.h>
#endif
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <spawn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/poll.h>
#if defined(__linux__) || defined(__APPLE__)
#include <sys/random.h>
#endif
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

extern char** environ;

typedef int32_t (*fz_task_entry_fn)(void);
typedef int32_t (*fz_callback_i32_v0)(int32_t);
typedef uint64_t fz_async_handle_t;
extern int32_t fz_task_entry_0(void) __asm__("_fzy2ts_api_js_host_touch");
extern int32_t fz_task_entry_1(void) __asm__("_fzy2ts_api_touch");
extern int32_t fz_task_entry_2(void) __asm__("_fzy2ts_bridge_easy_touch");
extern int32_t fz_task_entry_3(void) __asm__("_fzy2ts_model_types_touch");
extern int32_t fz_task_entry_4(void) __asm__("_fzy2ts_model_touch");
extern int32_t fz_task_entry_5(void) __asm__("_fzy2ts_touch");
static fz_task_entry_fn fz_task_entries[] = {
  fz_task_entry_0,
  fz_task_entry_1,
  fz_task_entry_2,
  fz_task_entry_3,
  fz_task_entry_4,
  fz_task_entry_5,
};
static const int fz_task_entry_count = 6;

static const char* fz_string_literals[] = {
  "",
  "J2FZ_EXAMPLE_MODULE",
  "doubleLater",
  "j2fz:host",
  "native_add_ten",
};
static const int fz_string_literal_count = 5;


#define FZ_MAX_DYNAMIC_STRINGS 16384
#define FZ_MAX_CONN_STATES 2048
#define FZ_MAX_HTTP_READ 262144
#define FZ_MAX_PROC_STATES 1024
#define FZ_MAX_HTTP_HEADERS 128
#define FZ_MAX_HTTP_STREAMS 256
#define FZ_MAX_WEBSOCKETS 256
#define FZ_MAX_SPAWN_THREADS 4096
#define FZ_MAX_CONN_META 128
#define FZ_MAX_ROUTE_PARAMS 64
#define FZ_MAX_LISTS 2048
#define FZ_MAX_LIST_ITEMS 4096
#define FZ_MAX_MAPS 2048
#define FZ_MAX_MAP_ENTRIES 4096
#define FZ_MAX_AGGREGATES 4096
#define FZ_MAX_AGGREGATE_ITEMS 64
#define FZ_MAX_INTERVALS 512
#define FZ_MAX_JSON_VALUES 16384
#define FZ_MAX_STORAGE_KV 1024
#define FZ_MAX_NET_POLL_WATCHES 256
#define FZ_STRING_INDEX_CAPACITY 65536

static char* fz_dynamic_strings[FZ_MAX_DYNAMIC_STRINGS];
static int fz_dynamic_string_count = 0;
static int32_t fz_string_index_ids[FZ_STRING_INDEX_CAPACITY];
static uint32_t fz_string_index_hashes[FZ_STRING_INDEX_CAPACITY];
static pthread_rwlock_t fz_string_lock = PTHREAD_RWLOCK_INITIALIZER;
static pthread_once_t fz_string_index_once = PTHREAD_ONCE_INIT;

static int fz_listener_fd = -1;
static pthread_mutex_t fz_listener_lock = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
  int in_use;
  int fd;
  short events;
} fz_net_poll_watch;

static fz_net_poll_watch fz_net_poll_watches[FZ_MAX_NET_POLL_WATCHES];
static pthread_mutex_t fz_net_poll_lock = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
  int in_use;
  int fd;
  int32_t method_id;
  int32_t path_id;
  int32_t body_id;
  int32_t request_id;
  int32_t remote_addr_id;
  int keep_alive;
  int request_headers_ready;
  int request_body_mode;
  int request_body_eof;
  int request_body_fully_buffered;
  int request_body_active;
  int64_t request_body_remaining;
  int64_t request_chunk_remaining;
  char* request_body_buf;
  size_t request_body_buf_len;
  size_t request_body_buf_pos;
  int header_count;
  int32_t header_key_ids[FZ_MAX_CONN_META];
  int32_t header_value_ids[FZ_MAX_CONN_META];
  int response_header_count;
  int32_t response_header_key_ids[FZ_MAX_CONN_META];
  int32_t response_header_value_ids[FZ_MAX_CONN_META];
  int query_count;
  int32_t query_key_ids[FZ_MAX_CONN_META];
  int32_t query_value_ids[FZ_MAX_CONN_META];
  int param_count;
  int32_t param_key_ids[FZ_MAX_ROUTE_PARAMS];
  int32_t param_value_ids[FZ_MAX_ROUTE_PARAMS];
} fz_conn_state;

static fz_conn_state fz_conn_states[FZ_MAX_CONN_STATES];
static pthread_mutex_t fz_conn_lock = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
  char* data;
  size_t len;
  size_t cap;
} fz_bytes_buf;

typedef struct {
  int in_use;
  pid_t pid;
  int stdout_fd;
  int stderr_fd;
  int done;
  int exit_notified;
  int exit_code;
  size_t stdout_read_pos;
  size_t stderr_read_pos;
  int32_t stdout_id;
  int32_t stderr_id;
  fz_bytes_buf stdout_buf;
  fz_bytes_buf stderr_buf;
} fz_proc_state;

typedef struct {
  int in_use;
  int count;
  char* items[FZ_MAX_LIST_ITEMS];
} fz_list_state;

typedef struct {
  int in_use;
  int count;
  int32_t items[FZ_MAX_LIST_ITEMS];
} fz_array_state;

typedef struct {
  int in_use;
  int32_t element_kind;
  int32_t count;
  uint32_t items[FZ_MAX_LIST_ITEMS];
} fz_numeric_vec_state;

typedef struct {
  int in_use;
  int count;
  char* keys[FZ_MAX_MAP_ENTRIES];
  char* values[FZ_MAX_MAP_ENTRIES];
} fz_map_state;

typedef struct {
  int in_use;
  int32_t period_ms;
  int64_t next_ms;
} fz_interval_state;

typedef struct {
  int in_use;
  int32_t value_id;
} fz_json_value_state;

typedef struct {
  int in_use;
  int32_t path_id;
  int32_t map_handle;
} fz_storage_kv_state;

typedef struct {
  int in_use;
  int32_t tag;
  int32_t count;
  uint64_t items[FZ_MAX_AGGREGATE_ITEMS];
} fz_aggregate_state;

static fz_proc_state fz_proc_states[FZ_MAX_PROC_STATES];
static pthread_mutex_t fz_proc_lock = PTHREAD_MUTEX_INITIALIZER;
static int32_t fz_proc_default_timeout_ms = 30000;
static int32_t fz_proc_last_error_id = 0;
static int32_t fz_last_exit_class = 0;
static fz_list_state fz_lists[FZ_MAX_LISTS];
static fz_array_state fz_arrays[FZ_MAX_LISTS];
static fz_numeric_vec_state fz_numeric_vecs[FZ_MAX_LISTS];
static fz_map_state fz_maps[FZ_MAX_MAPS];
static fz_aggregate_state fz_aggregates[FZ_MAX_AGGREGATES];
static fz_interval_state fz_intervals[FZ_MAX_INTERVALS];
static fz_json_value_state fz_json_values[FZ_MAX_JSON_VALUES];
static fz_storage_kv_state fz_storage_kv[FZ_MAX_STORAGE_KV];
static pthread_mutex_t fz_collections_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t fz_list_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t fz_array_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t fz_map_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t fz_aggregate_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t fz_storage_kv_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t fz_time_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t fz_json_lock = PTHREAD_MUTEX_INITIALIZER;
static int32_t fz_conn_request_counter = 0;
static int32_t fz_last_error_code = 0;
static int32_t fz_last_error_class = 0;
static int32_t fz_last_error_message_id = 0;
static int fz_log_json = 0;
static int fz_log_enabled = 1;
static int32_t fz_log_min_level = 0;
static int32_t fz_log_sink = 0;

typedef struct {
  int32_t key_id;
  int32_t value_id;
} fz_http_header_pair;

typedef struct {
  int in_use;
  pid_t pid;
  int stdout_fd;
  int stderr_fd;
  int done;
  int eof;
  int closed;
  int exit_code;
  int32_t status_code;
  int32_t error_id;
  size_t stdout_read_pos;
  fz_bytes_buf stdout_buf;
  fz_bytes_buf stderr_buf;
} fz_http_stream_state;

typedef struct {
  int in_use;
  int fd;
  int closed;
  int32_t last_kind_id;
  int32_t last_error_id;
  int32_t close_code;
} fz_websocket_state;

static fz_http_header_pair fz_http_headers[FZ_MAX_HTTP_HEADERS];
static int fz_http_header_count = 0;
static pthread_mutex_t fz_http_lock = PTHREAD_MUTEX_INITIALIZER;
static int32_t fz_http_last_status = 0;
static int32_t fz_http_last_body_id = 0;
static int32_t fz_http_last_error_id = 0;
static fz_http_stream_state fz_http_stream_states[FZ_MAX_HTTP_STREAMS];
static fz_websocket_state fz_websocket_states[FZ_MAX_WEBSOCKETS];
static int fz_fs_fd = -1;
static char fz_fs_base_path[512] = {0};
static char fz_fs_tmp_path[544] = {0};
static pthread_mutex_t fz_fs_lock = PTHREAD_MUTEX_INITIALIZER;
typedef struct {
  int in_use;
  int32_t handle;
  int32_t task_ref;
  int32_t context_id;
  int32_t group_id;
  pthread_t thread;
  int started;
  int finished;
  int detached;
  int joined;
  int cancelled;
  int32_t result;
} fz_spawn_state;

typedef struct {
  int in_use;
  int32_t id;
  int32_t active_count;
} fz_task_group_state;

static fz_spawn_state fz_spawn_states[FZ_MAX_SPAWN_THREADS];
static fz_task_group_state fz_task_groups[256];
static int32_t fz_next_spawn_handle = 1;
static int32_t fz_next_task_group_id = 1;
static int32_t fz_spawn_active_count = 0;
static int32_t fz_spawn_max_active = 1024;
static pthread_mutex_t fz_spawn_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_once_t fz_spawn_atexit_once = PTHREAD_ONCE_INIT;
static __thread int32_t fz_tls_task_context = 0;
static __thread int32_t fz_tls_task_handle = 0;
static __thread int64_t fz_tls_async_deadline_ms = 0;
static __thread int32_t fz_tls_async_cancelled = 0;
static fz_callback_i32_v0 fz_host_callbacks[64];
static int fz_host_initialized = 0;
static pthread_mutex_t fz_host_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_once_t fz_env_bootstrap_once = PTHREAD_ONCE_INIT;

typedef struct {
  int32_t handle;
} fz_spawn_ctx;

static int fz_mark_cloexec(int fd);
static void fz_proc_set_last_error(const char* msg);
static void fz_bytes_buf_init(fz_bytes_buf* buf);
static void fz_bytes_buf_free(fz_bytes_buf* buf);
static int fz_bytes_buf_append(fz_bytes_buf* buf, const char* data, size_t len);
static int fz_wait_for_fd_event(int fd, short events, int timeout_ms);
static void fz_conn_state_reset_request_body(fz_conn_state* state);
static void fz_conn_state_reset_response_headers(fz_conn_state* state);
static int fz_parse_chunked_flag(const char* headers, int header_len);
static int fz_conn_recv_into_body_buffer(fz_conn_state* state, size_t want, int timeout_ms);
static int fz_conn_read_body_chunk(fz_conn_state* state, char** out_ptr, size_t* out_len, int32_t max_bytes);
static int fz_conn_discard_body(fz_conn_state* state);
static int fz_send_http_response_state(
    fz_conn_state* state,
    int status_code,
    const char* content_type,
    const char* body,
    int close_after);
static fz_websocket_state* fz_websocket_state_get(int32_t handle);
static int32_t fz_websocket_state_alloc(int fd);
static int fz_websocket_write_frame(int fd, uint8_t opcode, const char* payload, size_t payload_len);
static int fz_websocket_read_frame(
    fz_websocket_state* ws,
    int32_t max_bytes,
    int32_t* out_kind_id,
    int32_t* out_close_code,
    int32_t* out_error_id);
static void fz_dotenv_load(void);
static void fz_env_bootstrap(void);
static const char* fz_env_get_bootstrapped(const char* key);
static int fz_has_env_value(const char* key);
static void fz_log_bind_target(int listener_fd);
static void fz_crypto_memzero(void* ptr, size_t len);
static int fz_crypto_fill_random(void* out, size_t len);
static char* fz_crypto_hex_encode(const uint8_t* data, size_t len);
static char* fz_crypto_base64_encode_alloc(const uint8_t* data, size_t len);
static char* fz_crypto_base64_url_encode_alloc(const uint8_t* data, size_t len);
static int fz_crypto_base64_decode_alloc(const char* input, uint8_t** out, size_t* out_len);
static int fz_crypto_base64_url_decode_alloc(const char* input, uint8_t** out, size_t* out_len);
static int fz_json_parse_string(const char** cursor, char** out);
static int fz_parse_json_string_array(const char* raw, char*** out_items, int* out_count);
static int fz_parse_json_env_object(const char* raw, char*** out_items, int* out_count);
static void fz_free_string_list(char** items, int count);
static int fz_json_parse_value_slice(const char* raw, const char** out_start, const char** out_end);
static fz_spawn_state* fz_spawn_state_by_handle_locked(int32_t handle);
static const char* fz_lookup_string_unlocked(int32_t id);
static uint32_t fz_string_hash_bytes(const char* data, size_t len);
static int32_t fz_find_string_slice_unlocked(const char* value, size_t len, uint32_t hash);
static int32_t fz_find_string_cstr_unlocked(const char* value, uint32_t hash);
static void fz_string_index_insert_unlocked(int32_t id, const char* value, uint32_t hash);
static void fz_string_index_bootstrap(void);
int32_t fz_native_net_request_id(int32_t conn_fd);
int32_t fz_native_net_write(int32_t conn_fd, int32_t status_code, int32_t body_id);


static const char* fz_lookup_string_unlocked(int32_t id) {
  if (id <= 0) {
    return "";
  }
  if (id <= fz_string_literal_count) {
    const char* literal = fz_string_literals[id - 1];
    return literal == NULL ? "" : literal;
  }
  int dynamic_index = id - fz_string_literal_count - 1;
  if (dynamic_index < 0 || dynamic_index >= fz_dynamic_string_count) {
    return "";
  }
  const char* value = fz_dynamic_strings[dynamic_index];
  return value == NULL ? "" : value;
}

static uint32_t fz_string_hash_bytes(const char* data, size_t len) {
  uint32_t hash = 2166136261u;
  if (data == NULL) {
    return hash;
  }
  for (size_t i = 0; i < len; i++) {
    hash ^= (uint8_t)data[i];
    hash *= 16777619u;
  }
  return hash;
}

static int32_t fz_find_string_slice_unlocked(const char* value, size_t len, uint32_t hash) {
  if (value == NULL) {
    return 0;
  }
  size_t slot = (size_t)hash & (FZ_STRING_INDEX_CAPACITY - 1);
  for (size_t probe = 0; probe < FZ_STRING_INDEX_CAPACITY; probe++) {
    int32_t id = fz_string_index_ids[slot];
    if (id == 0) {
      return 0;
    }
    if (fz_string_index_hashes[slot] == hash) {
      const char* existing = fz_lookup_string_unlocked(id);
      if (existing != NULL && strncmp(existing, value, len) == 0 && existing[len] == '\0') {
        return id;
      }
    }
    slot = (slot + 1) & (FZ_STRING_INDEX_CAPACITY - 1);
  }
  return 0;
}

static int32_t fz_find_string_cstr_unlocked(const char* value, uint32_t hash) {
  if (value == NULL) {
    return 0;
  }
  return fz_find_string_slice_unlocked(value, strlen(value), hash);
}

static void fz_string_index_insert_unlocked(int32_t id, const char* value, uint32_t hash) {
  if (id <= 0 || value == NULL) {
    return;
  }
  size_t slot = (size_t)hash & (FZ_STRING_INDEX_CAPACITY - 1);
  for (size_t probe = 0; probe < FZ_STRING_INDEX_CAPACITY; probe++) {
    if (fz_string_index_ids[slot] == 0) {
      fz_string_index_ids[slot] = id;
      fz_string_index_hashes[slot] = hash;
      return;
    }
    slot = (slot + 1) & (FZ_STRING_INDEX_CAPACITY - 1);
  }
}

static void fz_string_index_bootstrap(void) {
  pthread_rwlock_wrlock(&fz_string_lock);
  for (int i = 0; i < fz_string_literal_count; i++) {
    const char* literal = fz_string_literals[i];
    if (literal == NULL) {
      literal = "";
    }
    fz_string_index_insert_unlocked(i + 1, literal, fz_string_hash_bytes(literal, strlen(literal)));
  }
  pthread_rwlock_unlock(&fz_string_lock);
}

static const char* fz_lookup_string(int32_t id) {
  const char* value = "";
  (void)pthread_once(&fz_string_index_once, fz_string_index_bootstrap);
  pthread_rwlock_rdlock(&fz_string_lock);
  value = fz_lookup_string_unlocked(id);
  pthread_rwlock_unlock(&fz_string_lock);
  return value;
}

const uint8_t* fz_native_str_ptr(int32_t value_id) {
  return (const uint8_t*)fz_lookup_string(value_id);
}

static int32_t fz_intern_owned(char* owned) {
  if (owned == NULL) {
    return 0;
  }
  (void)pthread_once(&fz_string_index_once, fz_string_index_bootstrap);
  uint32_t hash = fz_string_hash_bytes(owned, strlen(owned));
  pthread_rwlock_wrlock(&fz_string_lock);
  int32_t existing_id = fz_find_string_cstr_unlocked(owned, hash);
  if (existing_id != 0) {
    pthread_rwlock_unlock(&fz_string_lock);
    free(owned);
    return existing_id;
  }
  if (fz_dynamic_string_count >= FZ_MAX_DYNAMIC_STRINGS) {
    pthread_rwlock_unlock(&fz_string_lock);
    free(owned);
    return 0;
  }
  int index = fz_dynamic_string_count;
  fz_dynamic_strings[index] = owned;
  fz_dynamic_string_count++;
  int32_t id = fz_string_literal_count + index + 1;
  fz_string_index_insert_unlocked(id, owned, hash);
  pthread_rwlock_unlock(&fz_string_lock);
  return id;
}

static int32_t fz_intern_slice(const char* data, size_t len) {
  if (data == NULL) {
    return 0;
  }
  (void)pthread_once(&fz_string_index_once, fz_string_index_bootstrap);
  uint32_t hash = fz_string_hash_bytes(data, len);
  pthread_rwlock_rdlock(&fz_string_lock);
  int32_t existing_id = fz_find_string_slice_unlocked(data, len, hash);
  pthread_rwlock_unlock(&fz_string_lock);
  if (existing_id != 0) {
    return existing_id;
  }
  char* owned = (char*)malloc(len + 1);
  if (owned == NULL) {
    return 0;
  }
  if (len > 0) {
    memcpy(owned, data, len);
  }
  owned[len] = '\0';
  return fz_intern_owned(owned);
}

static void fz_set_last_error(int32_t code, int32_t class_id, const char* message) {
  if (message == NULL) {
    message = "";
  }
  fz_last_error_code = code;
  fz_last_error_class = class_id;
  fz_last_error_message_id = fz_intern_slice(message, strlen(message));
  const char* debug_errors = getenv("FZ_NATIVE_DEBUG_ERRORS");
  if (debug_errors != NULL && debug_errors[0] != '\0' && !(debug_errors[0] == '0' && debug_errors[1] == '\0')) {
    fprintf(stderr, "[fz-native-error] code=%d class=%d message=%s\n", code, class_id, message);
  }
}

static void fz_crypto_memzero(void* ptr, size_t len) {
  if (ptr == NULL || len == 0) {
    return;
  }
  volatile uint8_t* bytes = (volatile uint8_t*)ptr;
  while (len > 0) {
    *bytes++ = 0;
    len--;
  }
}

static int32_t fz_list_alloc(void) {
  for (int i = 0; i < FZ_MAX_LISTS; i++) {
    if (!fz_lists[i].in_use) {
      memset(&fz_lists[i], 0, sizeof(fz_lists[i]));
      fz_lists[i].in_use = 1;
      return i + 1;
    }
  }
  return -1;
}

static int32_t fz_aggregate_alloc(void) {
  for (int i = 0; i < FZ_MAX_AGGREGATES; i++) {
    if (!fz_aggregates[i].in_use) {
      memset(&fz_aggregates[i], 0, sizeof(fz_aggregates[i]));
      fz_aggregates[i].in_use = 1;
      return i + 1;
    }
  }
  return -1;
}

static fz_aggregate_state* fz_aggregate_get(uint64_t handle) {
  if (handle == 0 || handle > (uint64_t)FZ_MAX_AGGREGATES) {
    return NULL;
  }
  fz_aggregate_state* aggregate = &fz_aggregates[(size_t)handle - 1];
  return aggregate->in_use ? aggregate : NULL;
}

static fz_list_state* fz_list_get(int32_t handle) {
  if (handle <= 0 || handle > FZ_MAX_LISTS) {
    return NULL;
  }
  fz_list_state* list = &fz_lists[handle - 1];
  return list->in_use ? list : NULL;
}

static int fz_list_push_cstr(fz_list_state* list, const char* value) {
  if (list == NULL || list->count >= FZ_MAX_LIST_ITEMS) {
    return -1;
  }
  if (value == NULL) {
    value = "";
  }
  char* dup = strdup(value);
  if (dup == NULL) {
    return -1;
  }
  list->items[list->count++] = dup;
  return 0;
}

static int32_t fz_array_alloc(void) {
  for (int i = 0; i < FZ_MAX_LISTS; i++) {
    if (!fz_arrays[i].in_use) {
      memset(&fz_arrays[i], 0, sizeof(fz_arrays[i]));
      fz_arrays[i].in_use = 1;
      return i + 1;
    }
  }
  return -1;
}

static fz_array_state* fz_array_get(int32_t handle) {
  if (handle <= 0 || handle > FZ_MAX_LISTS) {
    return NULL;
  }
  fz_array_state* array = &fz_arrays[handle - 1];
  return array->in_use ? array : NULL;
}

static int fz_array_push_i32(fz_array_state* array, int32_t value) {
  if (array == NULL || array->count >= FZ_MAX_LIST_ITEMS) {
    return -1;
  }
  array->items[array->count++] = value;
  return 0;
}

static int32_t fz_numeric_vec_alloc(void) {
  for (int i = 0; i < FZ_MAX_LISTS; i++) {
    if (!fz_numeric_vecs[i].in_use) {
      memset(&fz_numeric_vecs[i], 0, sizeof(fz_numeric_vecs[i]));
      fz_numeric_vecs[i].in_use = 1;
      return i + 1;
    }
  }
  return -1;
}

static fz_numeric_vec_state* fz_numeric_vec_get(uintptr_t handle) {
  if (handle == 0 || handle > FZ_MAX_LISTS) {
    return NULL;
  }
  fz_numeric_vec_state* vec = &fz_numeric_vecs[(size_t)handle - 1];
  return vec->in_use ? vec : NULL;
}

static int fz_numeric_vec_push_bits32(fz_numeric_vec_state* vec, uint32_t bits) {
  if (vec == NULL || vec->count >= FZ_MAX_LIST_ITEMS) {
    return -1;
  }
  vec->items[vec->count++] = bits;
  return 0;
}

int32_t fz_native_vec_len(uintptr_t handle) {
  pthread_mutex_lock(&fz_collections_lock);
  fz_numeric_vec_state* vec = fz_numeric_vec_get(handle);
  int32_t len = vec == NULL ? 0 : vec->count;
  pthread_mutex_unlock(&fz_collections_lock);
  return len;
}

int32_t fz_native_vec_get_i32(uintptr_t handle, int32_t index) {
  pthread_mutex_lock(&fz_collections_lock);
  fz_numeric_vec_state* vec = fz_numeric_vec_get(handle);
  if (vec == NULL || vec->element_kind != 2 || index < 0 || index >= vec->count) {
    pthread_mutex_unlock(&fz_collections_lock);
    return 0;
  }
  int32_t value = 0;
  uint32_t bits = vec->items[index];
  memcpy(&value, &bits, sizeof(value));
  pthread_mutex_unlock(&fz_collections_lock);
  return value;
}

int32_t fz_native_vec_get_u32(uintptr_t handle, int32_t index) {
  pthread_mutex_lock(&fz_collections_lock);
  fz_numeric_vec_state* vec = fz_numeric_vec_get(handle);
  if (vec == NULL || vec->element_kind != 3 || index < 0 || index >= vec->count) {
    pthread_mutex_unlock(&fz_collections_lock);
    return 0;
  }
  int32_t value = (int32_t)vec->items[index];
  pthread_mutex_unlock(&fz_collections_lock);
  return value;
}

float fz_native_vec_get_f32(uintptr_t handle, int32_t index) {
  pthread_mutex_lock(&fz_collections_lock);
  fz_numeric_vec_state* vec = fz_numeric_vec_get(handle);
  if (vec == NULL || vec->element_kind != 1 || index < 0 || index >= vec->count) {
    pthread_mutex_unlock(&fz_collections_lock);
    return 0.0f;
  }
  float value = 0.0f;
  uint32_t bits = vec->items[index];
  memcpy(&value, &bits, sizeof(value));
  pthread_mutex_unlock(&fz_collections_lock);
  return value;
}

uint64_t fz_native_agg_new(int32_t tag, int32_t count) {
  if (count < 0 || count > FZ_MAX_AGGREGATE_ITEMS) {
    return 0;
  }
  pthread_mutex_lock(&fz_aggregate_lock);
  int32_t handle = fz_aggregate_alloc();
  if (handle > 0) {
    fz_aggregate_state* aggregate = &fz_aggregates[handle - 1];
    aggregate->tag = tag;
    aggregate->count = count;
  }
  pthread_mutex_unlock(&fz_aggregate_lock);
  return handle > 0 ? (uint64_t)handle : 0;
}

int32_t fz_native_agg_set_i64(uint64_t handle, int32_t index, uint64_t value) {
  pthread_mutex_lock(&fz_aggregate_lock);
  fz_aggregate_state* aggregate = fz_aggregate_get(handle);
  if (aggregate == NULL || index < 0 || index >= aggregate->count) {
    pthread_mutex_unlock(&fz_aggregate_lock);
    return -1;
  }
  aggregate->items[index] = value;
  pthread_mutex_unlock(&fz_aggregate_lock);
  return 0;
}

uint64_t fz_native_agg_get_i64(uint64_t handle, int32_t index) {
  pthread_mutex_lock(&fz_aggregate_lock);
  fz_aggregate_state* aggregate = fz_aggregate_get(handle);
  if (aggregate == NULL || index < 0 || index >= aggregate->count) {
    pthread_mutex_unlock(&fz_aggregate_lock);
    return 0;
  }
  uint64_t value = aggregate->items[index];
  pthread_mutex_unlock(&fz_aggregate_lock);
  return value;
}

int32_t fz_native_agg_tag(uint64_t handle) {
  pthread_mutex_lock(&fz_aggregate_lock);
  fz_aggregate_state* aggregate = fz_aggregate_get(handle);
  int32_t tag = aggregate == NULL ? 0 : aggregate->tag;
  pthread_mutex_unlock(&fz_aggregate_lock);
  return tag;
}

static int32_t fz_map_alloc(void) {
  for (int i = 0; i < FZ_MAX_MAPS; i++) {
    if (!fz_maps[i].in_use) {
      memset(&fz_maps[i], 0, sizeof(fz_maps[i]));
      fz_maps[i].in_use = 1;
      return i + 1;
    }
  }
  return -1;
}

static fz_map_state* fz_map_get(int32_t handle) {
  if (handle <= 0 || handle > FZ_MAX_MAPS) {
    return NULL;
  }
  fz_map_state* map = &fz_maps[handle - 1];
  return map->in_use ? map : NULL;
}

static int fz_map_find_index(fz_map_state* map, const char* key) {
  if (map == NULL || key == NULL) {
    return -1;
  }
  for (int i = 0; i < map->count; i++) {
    if (map->keys[i] != NULL && strcmp(map->keys[i], key) == 0) {
      return i;
    }
  }
  return -1;
}

static int32_t fz_storage_kv_alloc(void) {
  for (int i = 0; i < FZ_MAX_STORAGE_KV; i++) {
    if (!fz_storage_kv[i].in_use) {
      memset(&fz_storage_kv[i], 0, sizeof(fz_storage_kv[i]));
      fz_storage_kv[i].in_use = 1;
      return i + 1;
    }
  }
  return -1;
}

static fz_storage_kv_state* fz_storage_kv_get(int32_t handle) {
  if (handle <= 0 || handle > FZ_MAX_STORAGE_KV) {
    return NULL;
  }
  fz_storage_kv_state* kv = &fz_storage_kv[handle - 1];
  return kv->in_use ? kv : NULL;
}

static char* fz_trim_ascii(char* text) {
  if (text == NULL) {
    return NULL;
  }
  while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n') {
    text++;
  }
  size_t len = strlen(text);
  while (len > 0 && (text[len - 1] == ' ' || text[len - 1] == '\t' || text[len - 1] == '\r' || text[len - 1] == '\n')) {
    text[--len] = '\0';
  }
  return text;
}

static void fz_unquote_env_value(char* value) {
  if (value == NULL) {
    return;
  }
  size_t len = strlen(value);
  if (len < 2) {
    return;
  }
  char quote = value[0];
  if ((quote != '\'' && quote != '\"') || value[len - 1] != quote) {
    return;
  }
  value[len - 1] = '\0';
  memmove(value, value + 1, len - 1);
  if (quote == '\"') {
    char* src = value;
    char* dst = value;
    while (*src != '\0') {
      if (*src == '\\' && src[1] != '\0') {
        src++;
        switch (*src) {
          case 'n': *dst++ = '\n'; break;
          case 'r': *dst++ = '\r'; break;
          case 't': *dst++ = '\t'; break;
          case '\\': *dst++ = '\\'; break;
          case '\"': *dst++ = '\"'; break;
          default: *dst++ = *src; break;
        }
        src++;
        continue;
      }
      *dst++ = *src++;
    }
    *dst = '\0';
  }
}

static void fz_dotenv_load(void) {
  const char* path = getenv("FZ_DOTENV_PATH");
  if (path == NULL || path[0] == '\0') {
    path = ".env";
  }
  FILE* file = fopen(path, "r");
  if (file == NULL) {
    return;
  }
  char line[4096];
  while (fgets(line, sizeof(line), file) != NULL) {
    char* entry = fz_trim_ascii(line);
    if (entry == NULL || entry[0] == '\0' || entry[0] == '#') {
      continue;
    }
    if (strncmp(entry, "export ", 7) == 0) {
      entry = fz_trim_ascii(entry + 7);
      if (entry == NULL || entry[0] == '\0') {
        continue;
      }
    }
    char* eq = strchr(entry, '=');
    if (eq == NULL) {
      continue;
    }
    *eq = '\0';
    char* key = fz_trim_ascii(entry);
    char* value = fz_trim_ascii(eq + 1);
    if (key == NULL || key[0] == '\0' || value == NULL) {
      continue;
    }
    fz_unquote_env_value(value);
    if (getenv(key) == NULL) {
      (void)setenv(key, value, 0);
    }
  }
  fclose(file);
}

static void fz_env_bootstrap(void) {
  fz_dotenv_load();
}

static const char* fz_env_get_bootstrapped(const char* key) {
  if (key == NULL || key[0] == '\0') {
    return NULL;
  }
  (void)pthread_once(&fz_env_bootstrap_once, fz_env_bootstrap);
  return getenv(key);
}

static int fz_has_env_value(const char* key) {
  const char* value = fz_env_get_bootstrapped(key);
  return value != NULL && value[0] != '\0';
}

static int fz_parse_port_from_env(const char* key, int fallback) {
  const char* raw = fz_env_get_bootstrapped(key);
  if (raw == NULL || raw[0] == '\0') {
    return fallback;
  }
  char* end = NULL;
  long parsed = strtol(raw, &end, 10);
  if (end == raw || parsed <= 0 || parsed > 65535) {
    return fallback;
  }
  return (int)parsed;
}

static int fz_default_port(void) {
  int port = 8787;
  port = fz_parse_port_from_env("PORT", port);
  port = fz_parse_port_from_env("AGENT_PORT", port);
  port = fz_parse_port_from_env("FZ_PORT", port);
  return port;
}

static const char* fz_default_host_name(void) {
  const char* host = fz_env_get_bootstrapped("FZ_HOST");
  if (host == NULL || host[0] == '\0') {
    host = fz_env_get_bootstrapped("AGENT_HOST");
  }
  if (host == NULL || host[0] == '\0') {
    host = "127.0.0.1";
  }
  return host;
}

static uint32_t fz_default_addr(void) {
  const char* host = fz_default_host_name();
  struct in_addr addr;
  if (inet_pton(AF_INET, host, &addr) == 1) {
    return addr.s_addr;
  }
  if (strcmp(host, "localhost") == 0) {
    return htonl(INADDR_LOOPBACK);
  }
  return htonl(INADDR_LOOPBACK);
}

static void fz_log_bind_target(int listener_fd) {
  struct sockaddr_in addr;
  socklen_t addr_len = sizeof(addr);
  memset(&addr, 0, sizeof(addr));
  if (getsockname(listener_fd, (struct sockaddr*)&addr, &addr_len) != 0) {
    return;
  }
  char host[64];
  const char* rendered = inet_ntop(AF_INET, &addr.sin_addr, host, sizeof(host));
  if (rendered == NULL) {
    rendered = "127.0.0.1";
  }
  int port = (int)ntohs(addr.sin_port);
  const char* host_source = fz_has_env_value("FZ_HOST")
      ? "FZ_HOST"
      : (fz_has_env_value("AGENT_HOST") ? "AGENT_HOST" : "default");
  const char* port_source = fz_has_env_value("FZ_PORT")
      ? "FZ_PORT"
      : (fz_has_env_value("AGENT_PORT")
            ? "AGENT_PORT"
            : (fz_has_env_value("PORT") ? "PORT" : "default"));
  fprintf(
      stderr,
      "[fz-runtime] listen active addr=%s port=%d (host_source=%s port_source=%s)\n",
      rendered,
      port,
      host_source,
      port_source);
  fflush(stderr);
}

static int64_t fz_now_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (int64_t)ts.tv_sec * 1000 + (ts.tv_nsec / 1000000);
}

static int fz_async_current_task_cancelled(void) {
  if (fz_tls_async_cancelled) {
    return 1;
  }
  if (fz_tls_task_handle <= 0) {
    return 0;
  }
  int cancelled = 0;
  pthread_mutex_lock(&fz_spawn_lock);
  fz_spawn_state* state = fz_spawn_state_by_handle_locked(fz_tls_task_handle);
  if (state != NULL && state->cancelled) {
    cancelled = 1;
  }
  pthread_mutex_unlock(&fz_spawn_lock);
  if (cancelled) {
    fz_tls_async_cancelled = 1;
  }
  return cancelled;
}

static int fz_async_deadline_expired(void) {
  return fz_tls_async_deadline_ms > 0 && fz_now_ms() >= fz_tls_async_deadline_ms;
}

static int fz_async_effective_timeout_ms(int timeout_ms) {
  if (fz_async_current_task_cancelled()) {
    return 0;
  }
  if (fz_tls_async_deadline_ms <= 0) {
    return timeout_ms;
  }
  int64_t remaining = fz_tls_async_deadline_ms - fz_now_ms();
  if (remaining <= 0) {
    return 0;
  }
  if (timeout_ms < 0 || remaining < (int64_t)timeout_ms) {
    return remaining > INT32_MAX ? INT32_MAX : (int)remaining;
  }
  return timeout_ms;
}

typedef struct {
  uint32_t state[8];
  uint64_t bitlen;
  uint8_t data[64];
  size_t datalen;
} fz_sha256_ctx;

static uint32_t fz_sha256_rotr(uint32_t value, uint32_t bits) {
  return (value >> bits) | (value << (32 - bits));
}

static uint32_t fz_sha256_ch(uint32_t x, uint32_t y, uint32_t z) {
  return (x & y) ^ (~x & z);
}

static uint32_t fz_sha256_maj(uint32_t x, uint32_t y, uint32_t z) {
  return (x & y) ^ (x & z) ^ (y & z);
}

static uint32_t fz_sha256_ep0(uint32_t x) {
  return fz_sha256_rotr(x, 2) ^ fz_sha256_rotr(x, 13) ^ fz_sha256_rotr(x, 22);
}

static uint32_t fz_sha256_ep1(uint32_t x) {
  return fz_sha256_rotr(x, 6) ^ fz_sha256_rotr(x, 11) ^ fz_sha256_rotr(x, 25);
}

static uint32_t fz_sha256_sig0(uint32_t x) {
  return fz_sha256_rotr(x, 7) ^ fz_sha256_rotr(x, 18) ^ (x >> 3);
}

static uint32_t fz_sha256_sig1(uint32_t x) {
  return fz_sha256_rotr(x, 17) ^ fz_sha256_rotr(x, 19) ^ (x >> 10);
}

static const uint32_t fz_sha256_k[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u,
    0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu,
    0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu,
    0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau, 0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu,
    0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
    0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u, 0x19a4c116u,
    0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u,
    0xc67178f2u};

static void fz_sha256_transform(fz_sha256_ctx* ctx, const uint8_t data[64]) {
  uint32_t m[64];
  for (int i = 0; i < 16; i++) {
    m[i] = ((uint32_t)data[i * 4] << 24) | ((uint32_t)data[(i * 4) + 1] << 16)
        | ((uint32_t)data[(i * 4) + 2] << 8) | (uint32_t)data[(i * 4) + 3];
  }
  for (int i = 16; i < 64; i++) {
    m[i] = fz_sha256_sig1(m[i - 2]) + m[i - 7] + fz_sha256_sig0(m[i - 15]) + m[i - 16];
  }
  uint32_t a = ctx->state[0];
  uint32_t b = ctx->state[1];
  uint32_t c = ctx->state[2];
  uint32_t d = ctx->state[3];
  uint32_t e = ctx->state[4];
  uint32_t f = ctx->state[5];
  uint32_t g = ctx->state[6];
  uint32_t h = ctx->state[7];
  for (int i = 0; i < 64; i++) {
    uint32_t t1 = h + fz_sha256_ep1(e) + fz_sha256_ch(e, f, g) + fz_sha256_k[i] + m[i];
    uint32_t t2 = fz_sha256_ep0(a) + fz_sha256_maj(a, b, c);
    h = g;
    g = f;
    f = e;
    e = d + t1;
    d = c;
    c = b;
    b = a;
    a = t1 + t2;
  }
  ctx->state[0] += a;
  ctx->state[1] += b;
  ctx->state[2] += c;
  ctx->state[3] += d;
  ctx->state[4] += e;
  ctx->state[5] += f;
  ctx->state[6] += g;
  ctx->state[7] += h;
}

static void fz_sha256_init(fz_sha256_ctx* ctx) {
  memset(ctx, 0, sizeof(*ctx));
  ctx->state[0] = 0x6a09e667u;
  ctx->state[1] = 0xbb67ae85u;
  ctx->state[2] = 0x3c6ef372u;
  ctx->state[3] = 0xa54ff53au;
  ctx->state[4] = 0x510e527fu;
  ctx->state[5] = 0x9b05688cu;
  ctx->state[6] = 0x1f83d9abu;
  ctx->state[7] = 0x5be0cd19u;
}

static void fz_sha256_update(fz_sha256_ctx* ctx, const uint8_t* data, size_t len) {
  if (ctx == NULL || (data == NULL && len != 0)) {
    return;
  }
  for (size_t i = 0; i < len; i++) {
    ctx->data[ctx->datalen++] = data[i];
    if (ctx->datalen == 64) {
      fz_sha256_transform(ctx, ctx->data);
      ctx->bitlen += 512;
      ctx->datalen = 0;
    }
  }
}

static void fz_sha256_final(fz_sha256_ctx* ctx, uint8_t hash[32]) {
  size_t i = ctx->datalen;
  if (i < 56) {
    ctx->data[i++] = 0x80;
    while (i < 56) {
      ctx->data[i++] = 0x00;
    }
  } else {
    ctx->data[i++] = 0x80;
    while (i < 64) {
      ctx->data[i++] = 0x00;
    }
    fz_sha256_transform(ctx, ctx->data);
    memset(ctx->data, 0, 56);
  }
  ctx->bitlen += (uint64_t)ctx->datalen * 8u;
  ctx->data[63] = (uint8_t)(ctx->bitlen);
  ctx->data[62] = (uint8_t)(ctx->bitlen >> 8);
  ctx->data[61] = (uint8_t)(ctx->bitlen >> 16);
  ctx->data[60] = (uint8_t)(ctx->bitlen >> 24);
  ctx->data[59] = (uint8_t)(ctx->bitlen >> 32);
  ctx->data[58] = (uint8_t)(ctx->bitlen >> 40);
  ctx->data[57] = (uint8_t)(ctx->bitlen >> 48);
  ctx->data[56] = (uint8_t)(ctx->bitlen >> 56);
  fz_sha256_transform(ctx, ctx->data);
  for (i = 0; i < 4; i++) {
    hash[i] = (uint8_t)((ctx->state[0] >> (24 - i * 8)) & 0xff);
    hash[i + 4] = (uint8_t)((ctx->state[1] >> (24 - i * 8)) & 0xff);
    hash[i + 8] = (uint8_t)((ctx->state[2] >> (24 - i * 8)) & 0xff);
    hash[i + 12] = (uint8_t)((ctx->state[3] >> (24 - i * 8)) & 0xff);
    hash[i + 16] = (uint8_t)((ctx->state[4] >> (24 - i * 8)) & 0xff);
    hash[i + 20] = (uint8_t)((ctx->state[5] >> (24 - i * 8)) & 0xff);
    hash[i + 24] = (uint8_t)((ctx->state[6] >> (24 - i * 8)) & 0xff);
    hash[i + 28] = (uint8_t)((ctx->state[7] >> (24 - i * 8)) & 0xff);
  }
}

static void fz_sha256_hash(const uint8_t* data, size_t len, uint8_t out[32]) {
  fz_sha256_ctx ctx;
  fz_sha256_init(&ctx);
  fz_sha256_update(&ctx, data, len);
  fz_sha256_final(&ctx, out);
}

static void fz_hmac_sha256_hash(
    const uint8_t* key,
    size_t key_len,
    const uint8_t* data,
    size_t data_len,
    uint8_t out[32]) {
  uint8_t key_block[64];
  memset(key_block, 0, sizeof(key_block));
  if (key_len > 64) {
    fz_sha256_hash(key, key_len, key_block);
  } else if (key_len > 0 && key != NULL) {
    memcpy(key_block, key, key_len);
  }
  uint8_t ipad[64];
  uint8_t opad[64];
  for (size_t i = 0; i < 64; i++) {
    ipad[i] = (uint8_t)(key_block[i] ^ 0x36u);
    opad[i] = (uint8_t)(key_block[i] ^ 0x5cu);
  }
  uint8_t inner[32];
  fz_sha256_ctx ctx;
  fz_sha256_init(&ctx);
  fz_sha256_update(&ctx, ipad, sizeof(ipad));
  fz_sha256_update(&ctx, data, data_len);
  fz_sha256_final(&ctx, inner);
  fz_sha256_init(&ctx);
  fz_sha256_update(&ctx, opad, sizeof(opad));
  fz_sha256_update(&ctx, inner, sizeof(inner));
  fz_sha256_final(&ctx, out);
  fz_crypto_memzero(key_block, sizeof(key_block));
  fz_crypto_memzero(ipad, sizeof(ipad));
  fz_crypto_memzero(opad, sizeof(opad));
  fz_crypto_memzero(inner, sizeof(inner));
}

static int fz_crypto_fill_random(void* out, size_t len) {
  if (len == 0) {
    return 0;
  }
  if (out == NULL) {
    errno = EINVAL;
    return -1;
  }
#if defined(__APPLE__)
  arc4random_buf(out, len);
  return 0;
#elif defined(__linux__)
  uint8_t* cursor = (uint8_t*)out;
  size_t remaining = len;
  while (remaining > 0) {
    size_t chunk = remaining > 256 ? 256 : remaining;
    if (getentropy(cursor, chunk) == 0) {
      cursor += chunk;
      remaining -= chunk;
      continue;
    }
    if (errno == EINTR) {
      continue;
    }
    if (errno != ENOSYS) {
      return -1;
    }
    break;
  }
  if (remaining == 0) {
    return 0;
  }
#endif
  int fd = open("/dev/urandom", O_RDONLY);
  if (fd < 0) {
    return -1;
  }
#if !defined(__linux__)
  uint8_t* cursor = (uint8_t*)out;
  size_t remaining = len;
#endif
  while (remaining > 0) {
    ssize_t got = read(fd, cursor, remaining);
    if (got < 0) {
      if (errno == EINTR) {
        continue;
      }
      close(fd);
      return -1;
    }
    if (got == 0) {
      close(fd);
      return -1;
    }
    cursor += (size_t)got;
    remaining -= (size_t)got;
  }
  close(fd);
  return 0;
}

static char* fz_crypto_hex_encode(const uint8_t* data, size_t len) {
  static const char* hex = "0123456789abcdef";
  char* out = (char*)malloc((len * 2) + 1);
  if (out == NULL) {
    return NULL;
  }
  for (size_t i = 0; i < len; i++) {
    out[i * 2] = hex[(data[i] >> 4) & 0x0f];
    out[(i * 2) + 1] = hex[data[i] & 0x0f];
  }
  out[len * 2] = '\0';
  return out;
}

static char* fz_crypto_base64_encode_alloc(const uint8_t* data, size_t len) {
  static const char alphabet[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  size_t out_len = ((len + 2) / 3) * 4;
  char* out = (char*)malloc(out_len + 1);
  if (out == NULL) {
    return NULL;
  }
  size_t in_index = 0;
  size_t out_index = 0;
  while (in_index < len) {
    size_t remaining = len - in_index;
    uint32_t octet_a = data[in_index++];
    uint32_t octet_b = remaining > 1 ? data[in_index++] : 0;
    uint32_t octet_c = remaining > 2 ? data[in_index++] : 0;
    uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;
    out[out_index++] = alphabet[(triple >> 18) & 0x3f];
    out[out_index++] = alphabet[(triple >> 12) & 0x3f];
    out[out_index++] = remaining > 1 ? alphabet[(triple >> 6) & 0x3f] : '=';
    out[out_index++] = remaining > 2 ? alphabet[triple & 0x3f] : '=';
  }
  out[out_len] = '\0';
  return out;
}

static char* fz_crypto_base64_url_encode_alloc(const uint8_t* data, size_t len) {
  char* out = fz_crypto_base64_encode_alloc(data, len);
  if (out == NULL) {
    return NULL;
  }
  size_t write_index = 0;
  for (size_t read_index = 0; out[read_index] != '\0'; read_index++) {
    char ch = out[read_index];
    if (ch == '=') {
      continue;
    }
    if (ch == '+') {
      ch = '-';
    } else if (ch == '/') {
      ch = '_';
    }
    out[write_index++] = ch;
  }
  out[write_index] = '\0';
  return out;
}

static int fz_crypto_base64_value(int ch) {
  if (ch >= 'A' && ch <= 'Z') return ch - 'A';
  if (ch >= 'a' && ch <= 'z') return ch - 'a' + 26;
  if (ch >= '0' && ch <= '9') return ch - '0' + 52;
  if (ch == '+') return 62;
  if (ch == '/') return 63;
  return -1;
}

static int fz_crypto_base64_decode_alloc(const char* input, uint8_t** out, size_t* out_len) {
  if (out == NULL || out_len == NULL) {
    errno = EINVAL;
    return -1;
  }
  *out = NULL;
  *out_len = 0;
  if (input == NULL) {
    return 0;
  }
  size_t len = strlen(input);
  if (len == 0) {
    *out = (uint8_t*)calloc(1, 1);
    return *out == NULL ? -1 : 0;
  }
  if ((len % 4) != 0) {
    errno = EINVAL;
    return -1;
  }
  size_t padding = 0;
  if (len >= 1 && input[len - 1] == '=') padding++;
  if (len >= 2 && input[len - 2] == '=') padding++;
  size_t decoded_len = (len / 4) * 3 - padding;
  uint8_t* buf = (uint8_t*)malloc(decoded_len == 0 ? 1 : decoded_len);
  if (buf == NULL) {
    return -1;
  }
  size_t out_index = 0;
  for (size_t i = 0; i < len; i += 4) {
    int is_last_block = (i + 4) == len ? 1 : 0;
    int vals[4];
    for (int j = 0; j < 4; j++) {
      int ch = (unsigned char)input[i + (size_t)j];
      if (ch == '=') {
        if (!is_last_block || j < 2) {
          free(buf);
          errno = EINVAL;
          return -1;
        }
        if (j == 2 && input[i + 3] != '=') {
          free(buf);
          errno = EINVAL;
          return -1;
        }
        vals[j] = 0;
        continue;
      }
      vals[j] = fz_crypto_base64_value(ch);
      if (vals[j] < 0) {
        free(buf);
        errno = EINVAL;
        return -1;
      }
    }
    if (input[i + 2] == '=' && input[i + 3] != '=') {
      free(buf);
      errno = EINVAL;
      return -1;
    }
    uint32_t triple =
        ((uint32_t)vals[0] << 18) | ((uint32_t)vals[1] << 12) | ((uint32_t)vals[2] << 6)
        | (uint32_t)vals[3];
    if (out_index < decoded_len) buf[out_index++] = (uint8_t)((triple >> 16) & 0xff);
    if (out_index < decoded_len) buf[out_index++] = (uint8_t)((triple >> 8) & 0xff);
    if (out_index < decoded_len) buf[out_index++] = (uint8_t)(triple & 0xff);
  }
  *out = buf;
  *out_len = decoded_len;
  return 0;
}

static int fz_crypto_base64_url_decode_alloc(const char* input, uint8_t** out, size_t* out_len) {
  if (out == NULL || out_len == NULL) {
    errno = EINVAL;
    return -1;
  }
  *out = NULL;
  *out_len = 0;
  if (input == NULL) {
    return 0;
  }
  size_t len = strlen(input);
  if (len == 0) {
    return fz_crypto_base64_decode_alloc("", out, out_len);
  }
  int saw_padding = 0;
  for (size_t i = 0; i < len; i++) {
    char ch = input[i];
    if (ch == '=') {
      saw_padding = 1;
      continue;
    }
    if (saw_padding) {
      errno = EINVAL;
      return -1;
    }
    if (!((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9')
          || ch == '-' || ch == '_')) {
      errno = EINVAL;
      return -1;
    }
  }

  size_t rem = len % 4;
  size_t padded_len = len;
  if (!saw_padding) {
    if (rem == 1) {
      errno = EINVAL;
      return -1;
    }
    if (rem != 0) {
      padded_len += 4 - rem;
    }
  } else if (rem != 0) {
    errno = EINVAL;
    return -1;
  }

  char* standard = (char*)malloc(padded_len + 1);
  if (standard == NULL) {
    errno = ENOMEM;
    return -1;
  }
  for (size_t i = 0; i < len; i++) {
    char ch = input[i];
    if (ch == '-') {
      standard[i] = '+';
    } else if (ch == '_') {
      standard[i] = '/';
    } else {
      standard[i] = ch;
    }
  }
  for (size_t i = len; i < padded_len; i++) {
    standard[i] = '=';
  }
  standard[padded_len] = '\0';
  int rc = fz_crypto_base64_decode_alloc(standard, out, out_len);
  fz_crypto_memzero(standard, padded_len);
  free(standard);
  return rc;
}

static int32_t fz_exit_class_from_status(int timed_out, int status, int spawn_error) {
  if (spawn_error) {
    return 3;
  }
  if (timed_out) {
    return 2;
  }
  if (WIFEXITED(status)) {
    return WEXITSTATUS(status) == 0 ? 0 : 1;
  }
  if (WIFSIGNALED(status)) {
    return 1;
  }
  return 1;
}

static const char* fz_fs_path(void) {
  if (fz_fs_base_path[0] != '\0') {
    return fz_fs_base_path;
  }
  const char* from_env = getenv("FZ_FS_PATH");
  if (from_env == NULL || from_env[0] == '\0') {
    from_env = "/tmp/fozzy_native_store.dat";
  }
  snprintf(fz_fs_base_path, sizeof(fz_fs_base_path), "%s", from_env);
  snprintf(fz_fs_tmp_path, sizeof(fz_fs_tmp_path), "%s.tmp", from_env);
  return fz_fs_base_path;
}

static int fz_fs_ensure_open(void) {
  if (fz_fs_fd >= 0) {
    return fz_fs_fd;
  }
  const char* path = fz_fs_path();
  int fd = open(path, O_CREAT | O_RDWR, 0644);
  if (fd < 0) {
    return -1;
  }
  (void)fz_mark_cloexec(fd);
  fz_fs_fd = fd;
  return fd;
}

static void fz_http_headers_clear(void) {
  pthread_mutex_lock(&fz_http_lock);
  fz_http_header_count = 0;
  pthread_mutex_unlock(&fz_http_lock);
}

static void fz_conn_state_reset_request_body(fz_conn_state* state) {
  if (state == NULL) {
    return;
  }
  if (state->request_body_buf != NULL) {
    free(state->request_body_buf);
  }
  state->request_body_buf = NULL;
  state->request_body_buf_len = 0;
  state->request_body_buf_pos = 0;
  state->request_headers_ready = 0;
  state->request_body_mode = 0;
  state->request_body_eof = 1;
  state->request_body_fully_buffered = 0;
  state->request_body_active = 0;
  state->request_body_remaining = 0;
  state->request_chunk_remaining = 0;
  state->body_id = 0;
}

static void fz_conn_state_reset_response_headers(fz_conn_state* state) {
  if (state == NULL) {
    return;
  }
  state->response_header_count = 0;
}

static char* fz_json_escape_owned(const char* input) {
  if (input == NULL) {
    input = "";
  }
  size_t in_len = strlen(input);
  size_t cap = (in_len * 6) + 1;
  char* out = (char*)malloc(cap);
  if (out == NULL) {
    return NULL;
  }
  size_t j = 0;
  for (size_t i = 0; i < in_len; i++) {
    unsigned char ch = (unsigned char)input[i];
    switch (ch) {
      case '\"': out[j++] = '\\'; out[j++] = '\"'; break;
      case '\\': out[j++] = '\\'; out[j++] = '\\'; break;
      case '\b': out[j++] = '\\'; out[j++] = 'b'; break;
      case '\f': out[j++] = '\\'; out[j++] = 'f'; break;
      case '\n': out[j++] = '\\'; out[j++] = 'n'; break;
      case '\r': out[j++] = '\\'; out[j++] = 'r'; break;
      case '\t': out[j++] = '\\'; out[j++] = 't'; break;
      default:
        if (ch < 0x20) {
          static const char* hex = "0123456789abcdef";
          out[j++] = '\\';
          out[j++] = 'u';
          out[j++] = '0';
          out[j++] = '0';
          out[j++] = hex[(ch >> 4) & 0xF];
          out[j++] = hex[ch & 0xF];
        } else {
          out[j++] = (char)ch;
        }
        break;
    }
  }
  out[j] = '\0';
  return out;
}

static int32_t fz_json_value_alloc(int32_t value_id) {
  if (value_id <= 0) {
    return -1;
  }
  pthread_mutex_lock(&fz_json_lock);
  for (int i = 0; i < FZ_MAX_JSON_VALUES; i++) {
    if (!fz_json_values[i].in_use) {
      fz_json_values[i].in_use = 1;
      fz_json_values[i].value_id = value_id;
      pthread_mutex_unlock(&fz_json_lock);
      return i + 1;
    }
  }
  pthread_mutex_unlock(&fz_json_lock);
  return -1;
}

static int32_t fz_json_value_get_id(int32_t handle) {
  if (handle <= 0 || handle > FZ_MAX_JSON_VALUES) {
    return 0;
  }
  pthread_mutex_lock(&fz_json_lock);
  fz_json_value_state* slot = &fz_json_values[handle - 1];
  int32_t value_id = slot->in_use ? slot->value_id : 0;
  pthread_mutex_unlock(&fz_json_lock);
  return value_id;
}

static int32_t fz_json_value_alloc_from_slice(const char* start, const char* end) {
  if (start == NULL || end == NULL || end < start) {
    return -1;
  }
  int32_t value_id = fz_intern_slice(start, (size_t)(end - start));
  if (value_id <= 0) {
    return -1;
  }
  return fz_json_value_alloc(value_id);
}

static const char* fz_json_ws(const char* p) {
  while (p != NULL && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) {
    p++;
  }
  return p;
}

static int fz_json_match_lit(const char** cursor, const char* lit) {
  const char* p = fz_json_ws(*cursor);
  size_t n = strlen(lit);
  if (strncmp(p, lit, n) != 0) {
    return -1;
  }
  *cursor = p + n;
  return 0;
}

static int fz_json_skip_string_token(const char** cursor) {
  const char* p = fz_json_ws(*cursor);
  if (p == NULL || *p != '\"') {
    return -1;
  }
  p++;
  while (*p != '\0') {
    if (*p == '\"') {
      *cursor = p + 1;
      return 0;
    }
    if (*p == '\\') {
      p++;
      if (*p == '\0') {
        return -1;
      }
      if (*p == 'u') {
        p++;
        for (int i = 0; i < 4; i++) {
          if (!isxdigit((unsigned char)p[i])) {
            return -1;
          }
        }
        p += 4;
        continue;
      }
      p++;
      continue;
    }
    p++;
  }
  return -1;
}

static int fz_json_skip_number_token(const char** cursor) {
  const char* p = fz_json_ws(*cursor);
  if (p == NULL) {
    return -1;
  }
  if (*p == '-') {
    p++;
  }
  if (*p == '0') {
    p++;
  } else if (isdigit((unsigned char)*p)) {
    while (isdigit((unsigned char)*p)) p++;
  } else {
    return -1;
  }
  if (*p == '.') {
    p++;
    if (!isdigit((unsigned char)*p)) {
      return -1;
    }
    while (isdigit((unsigned char)*p)) p++;
  }
  if (*p == 'e' || *p == 'E') {
    p++;
    if (*p == '+' || *p == '-') {
      p++;
    }
    if (!isdigit((unsigned char)*p)) {
      return -1;
    }
    while (isdigit((unsigned char)*p)) p++;
  }
  *cursor = p;
  return 0;
}

static int fz_json_skip_value_token(const char** cursor, int depth);

static int fz_json_skip_array_token(const char** cursor, int depth) {
  if (depth > 256) {
    return -1;
  }
  const char* p = fz_json_ws(*cursor);
  if (p == NULL || *p != '[') {
    return -1;
  }
  p = fz_json_ws(p + 1);
  if (*p == ']') {
    *cursor = p + 1;
    return 0;
  }
  for (;;) {
    if (fz_json_skip_value_token(&p, depth + 1) != 0) {
      return -1;
    }
    p = fz_json_ws(p);
    if (*p == ',') {
      p = fz_json_ws(p + 1);
      continue;
    }
    if (*p == ']') {
      *cursor = p + 1;
      return 0;
    }
    return -1;
  }
}

static int fz_json_skip_object_token(const char** cursor, int depth) {
  if (depth > 256) {
    return -1;
  }
  const char* p = fz_json_ws(*cursor);
  if (p == NULL || *p != '{') {
    return -1;
  }
  p = fz_json_ws(p + 1);
  if (*p == '}') {
    *cursor = p + 1;
    return 0;
  }
  for (;;) {
    if (fz_json_skip_string_token(&p) != 0) {
      return -1;
    }
    p = fz_json_ws(p);
    if (*p != ':') {
      return -1;
    }
    p = fz_json_ws(p + 1);
    if (fz_json_skip_value_token(&p, depth + 1) != 0) {
      return -1;
    }
    p = fz_json_ws(p);
    if (*p == ',') {
      p = fz_json_ws(p + 1);
      continue;
    }
    if (*p == '}') {
      *cursor = p + 1;
      return 0;
    }
    return -1;
  }
}

static int fz_json_skip_value_token(const char** cursor, int depth) {
  const char* p = fz_json_ws(*cursor);
  if (p == NULL || *p == '\0') {
    return -1;
  }
  int rc = -1;
  switch (*p) {
    case '\"': rc = fz_json_skip_string_token(&p); break;
    case '{': rc = fz_json_skip_object_token(&p, depth); break;
    case '[': rc = fz_json_skip_array_token(&p, depth); break;
    case 't': rc = fz_json_match_lit(&p, "true"); break;
    case 'f': rc = fz_json_match_lit(&p, "false"); break;
    case 'n': rc = fz_json_match_lit(&p, "null"); break;
    default: rc = fz_json_skip_number_token(&p); break;
  }
  if (rc == 0) {
    *cursor = p;
  }
  return rc;
}

static int fz_json_parse_value_slice(const char* raw, const char** out_start, const char** out_end) {
  if (raw == NULL || out_start == NULL || out_end == NULL) {
    return -1;
  }
  const char* start = fz_json_ws(raw);
  const char* p = start;
  if (fz_json_skip_value_token(&p, 0) != 0) {
    return -1;
  }
  p = fz_json_ws(p);
  if (*p != '\0') {
    return -1;
  }
  *out_start = start;
  *out_end = p;
  return 0;
}

static int fz_json_object_lookup(const char* raw, const char* key, const char** out_start, const char** out_end) {
  if (raw == NULL || key == NULL || out_start == NULL || out_end == NULL) {
    return -1;
  }
  const char* p = fz_json_ws(raw);
  if (*p != '{') {
    return -1;
  }
  p = fz_json_ws(p + 1);
  if (*p == '}') {
    return 0;
  }
  for (;;) {
    char* candidate = NULL;
    if (fz_json_parse_string(&p, &candidate) != 0) {
      return -1;
    }
    p = fz_json_ws(p);
    if (*p != ':') {
      free(candidate);
      return -1;
    }
    p = fz_json_ws(p + 1);
    const char* value_start = p;
    if (fz_json_skip_value_token(&p, 0) != 0) {
      free(candidate);
      return -1;
    }
    const char* value_end = p;
    int matches = strcmp(candidate == NULL ? "" : candidate, key) == 0;
    free(candidate);
    if (matches) {
      *out_start = value_start;
      *out_end = value_end;
      return 1;
    }
    p = fz_json_ws(p);
    if (*p == ',') {
      p = fz_json_ws(p + 1);
      continue;
    }
    if (*p == '}') {
      return 0;
    }
    return -1;
  }
}

static int fz_json_array_lookup(const char* raw, int index, const char** out_start, const char** out_end) {
  if (raw == NULL || index < 0 || out_start == NULL || out_end == NULL) {
    return -1;
  }
  const char* p = fz_json_ws(raw);
  if (*p != '[') {
    return -1;
  }
  p = fz_json_ws(p + 1);
  if (*p == ']') {
    return 0;
  }
  int at = 0;
  for (;;) {
    const char* value_start = p;
    if (fz_json_skip_value_token(&p, 0) != 0) {
      return -1;
    }
    const char* value_end = p;
    if (at == index) {
      *out_start = value_start;
      *out_end = value_end;
      return 1;
    }
    at++;
    p = fz_json_ws(p);
    if (*p == ',') {
      p = fz_json_ws(p + 1);
      continue;
    }
    if (*p == ']') {
      return 0;
    }
    return -1;
  }
}


static int fz_send_all(int fd, const char* data, size_t len) {
  size_t sent = 0;
  while (sent < len) {
    ssize_t wrote = send(fd, data + sent, len - sent, 0);
    if (wrote < 0) {
      if (errno == EINTR) {
        continue;
      }
      return -1;
    }
    if (wrote == 0) {
      return -1;
    }
    sent += (size_t)wrote;
  }
  return 0;
}

static const char* fz_http_reason(int status_code) {
  switch (status_code) {
    case 200: return "OK";
    case 201: return "Created";
    case 202: return "Accepted";
    case 204: return "No Content";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 409: return "Conflict";
    case 422: return "Unprocessable Entity";
    case 429: return "Too Many Requests";
    case 500: return "Internal Server Error";
    case 502: return "Bad Gateway";
    case 503: return "Service Unavailable";
    default: return "OK";
  }
}

static fz_conn_state* fz_conn_state_for(int fd, int create_if_missing) {
  fz_conn_state* free_slot = NULL;
  for (int i = 0; i < FZ_MAX_CONN_STATES; i++) {
    if (fz_conn_states[i].in_use && fz_conn_states[i].fd == fd) {
      return &fz_conn_states[i];
    }
    if (!fz_conn_states[i].in_use && free_slot == NULL) {
      free_slot = &fz_conn_states[i];
    }
  }
  if (!create_if_missing || free_slot == NULL) {
    return NULL;
  }
  memset(free_slot, 0, sizeof(*free_slot));
  free_slot->in_use = 1;
  free_slot->fd = fd;
  return free_slot;
}

static void fz_conn_state_drop(int fd) {
  pthread_mutex_lock(&fz_conn_lock);
  for (int i = 0; i < FZ_MAX_CONN_STATES; i++) {
    if (fz_conn_states[i].in_use && fz_conn_states[i].fd == fd) {
      fz_conn_state_reset_request_body(&fz_conn_states[i]);
      memset(&fz_conn_states[i], 0, sizeof(fz_conn_states[i]));
      break;
    }
  }
  pthread_mutex_unlock(&fz_conn_lock);
}

static int fz_find_header_end(const char* buf, int len) {
  for (int i = 0; i + 3 < len; i++) {
    if (buf[i] == '\r' && buf[i + 1] == '\n' && buf[i + 2] == '\r' && buf[i + 3] == '\n') {
      return i + 4;
    }
  }
  return -1;
}

static int fz_contains_ci(const char* hay, size_t hay_len, const char* needle) {
  size_t needle_len = strlen(needle);
  if (needle_len == 0 || hay_len < needle_len) {
    return 0;
  }
  for (size_t i = 0; i + needle_len <= hay_len; i++) {
    size_t j = 0;
    while (j < needle_len) {
      char a = (char)tolower((unsigned char)hay[i + j]);
      char b = (char)tolower((unsigned char)needle[j]);
      if (a != b) {
        break;
      }
      j++;
    }
    if (j == needle_len) {
      return 1;
    }
  }
  return 0;
}

static int64_t fz_parse_content_length(const char* headers, int header_len) {
  const char* cursor = headers;
  const char* end = headers + header_len;
  while (cursor < end) {
    const char* line_end = strstr(cursor, "\r\n");
    if (line_end == NULL || line_end > end) {
      break;
    }
    if (line_end == cursor) {
      break;
    }
    size_t line_len = (size_t)(line_end - cursor);
    if (line_len >= 15 && strncasecmp(cursor, "Content-Length:", 15) == 0) {
      const char* value = cursor + 15;
      while (value < line_end && (*value == ' ' || *value == '\t')) {
        value++;
      }
      char tmp[32];
      size_t max = (size_t)(line_end - value);
      if (max >= sizeof(tmp)) {
        max = sizeof(tmp) - 1;
      }
      memcpy(tmp, value, max);
      tmp[max] = '\0';
      char* parse_end = NULL;
      long long parsed = strtoll(tmp, &parse_end, 10);
      if (parse_end != tmp && parsed >= 0) {
        return parsed;
      }
    }
    cursor = line_end + 2;
  }
  return -1;
}

static int fz_parse_keep_alive(const char* headers, int header_len, const char* version, int version_len) {
  int keep_alive = (version_len >= 8 && strncasecmp(version, "HTTP/1.1", 8) == 0) ? 1 : 0;
  const char* cursor = headers;
  const char* end = headers + header_len;
  while (cursor < end) {
    const char* line_end = strstr(cursor, "\r\n");
    if (line_end == NULL || line_end > end) {
      break;
    }
    if (line_end == cursor) {
      break;
    }
    size_t line_len = (size_t)(line_end - cursor);
    if (line_len >= 11 && strncasecmp(cursor, "Connection:", 11) == 0) {
      const char* value = cursor + 11;
      while (value < line_end && (*value == ' ' || *value == '\t')) {
        value++;
      }
      size_t value_len = (size_t)(line_end - value);
      if (fz_contains_ci(value, value_len, "close")) {
        keep_alive = 0;
      } else if (fz_contains_ci(value, value_len, "keep-alive")) {
        keep_alive = 1;
      }
      break;
    }
    cursor = line_end + 2;
  }
  return keep_alive;
}

static int fz_parse_chunked_flag(const char* headers, int header_len) {
  const char* cursor = headers;
  const char* end = headers + header_len;
  while (cursor < end) {
    const char* line_end = strstr(cursor, "\r\n");
    if (line_end == NULL || line_end > end) {
      break;
    }
    if (line_end == cursor) {
      break;
    }
    size_t line_len = (size_t)(line_end - cursor);
    if (line_len >= 18 && strncasecmp(cursor, "Transfer-Encoding:", 18) == 0) {
      const char* value = cursor + 18;
      while (value < line_end && (*value == ' ' || *value == '\t')) {
        value++;
      }
      if (fz_contains_ci(value, (size_t)(line_end - value), "chunked")) {
        return 1;
      }
    }
    cursor = line_end + 2;
  }
  return 0;
}

static int fz_send_http_response_state(
    fz_conn_state* state,
    int status_code,
    const char* content_type,
    const char* body,
    int close_after) {
  if (state == NULL || state->fd < 0) {
    return -1;
  }
  if (content_type == NULL || content_type[0] == '\0') {
    content_type = "text/plain; charset=utf-8";
  }
  if (body == NULL) {
    body = "";
  }
  int body_len = (int)strlen(body);
  const char* reason = fz_http_reason(status_code);
  if (!close_after && state->request_body_eof == 0) {
    close_after = 1;
  }
  fz_bytes_buf header;
  fz_bytes_buf_init(&header);
  char status_line[256];
  int status_len = snprintf(
      status_line,
      sizeof(status_line),
      "HTTP/1.1 %d %s\r\n"
      "Content-Type: %s\r\n"
      "Content-Length: %d\r\n"
      "Connection: %s\r\n",
      status_code,
      reason,
      content_type,
      body_len,
      close_after ? "close" : "keep-alive");
  if (status_len <= 0 || fz_bytes_buf_append(&header, status_line, (size_t)status_len) != 0) {
    fz_bytes_buf_free(&header);
    return -1;
  }
  for (int i = 0; i < state->response_header_count; i++) {
    const char* key = fz_lookup_string(state->response_header_key_ids[i]);
    const char* value = fz_lookup_string(state->response_header_value_ids[i]);
    if (key == NULL || key[0] == '\0' || value == NULL) {
      continue;
    }
    if (strncasecmp(key, "content-type", 12) == 0
        || strncasecmp(key, "content-length", 14) == 0
        || strncasecmp(key, "connection", 10) == 0) {
      continue;
    }
    if (fz_bytes_buf_append(&header, key, strlen(key)) != 0
        || fz_bytes_buf_append(&header, ": ", 2) != 0
        || fz_bytes_buf_append(&header, value, strlen(value)) != 0
        || fz_bytes_buf_append(&header, "\r\n", 2) != 0) {
      fz_bytes_buf_free(&header);
      return -1;
    }
  }
  if (fz_bytes_buf_append(&header, "\r\n", 2) != 0) {
    fz_bytes_buf_free(&header);
    return -1;
  }
  if (fz_send_all(state->fd, header.data == NULL ? "" : header.data, header.len) != 0) {
    fz_bytes_buf_free(&header);
    return -1;
  }
  fz_bytes_buf_free(&header);
  if (body_len > 0 && fz_send_all(state->fd, body, (size_t)body_len) != 0) {
    return -1;
  }
  fz_conn_state_reset_response_headers(state);
  if (close_after) {
    shutdown(state->fd, SHUT_RDWR);
    close(state->fd);
    fz_conn_state_drop(state->fd);
  }
  return 0;
}

static int fz_send_http_response(int conn_fd, int status_code, const char* content_type, const char* body, int close_after) {
  fz_conn_state* state = NULL;
  pthread_mutex_lock(&fz_conn_lock);
  state = fz_conn_state_for(conn_fd, 1);
  if (state != NULL) {
    state->fd = conn_fd;
  }
  pthread_mutex_unlock(&fz_conn_lock);
  int rc = fz_send_http_response_state(state, status_code, content_type, body, close_after);
  return rc;
}

static int fz_conn_recv_into_body_buffer(fz_conn_state* state, size_t want, int timeout_ms) {
  if (state == NULL || state->fd < 0) {
    return -1;
  }
  while ((state->request_body_buf_len - state->request_body_buf_pos) < want) {
    char tmp[4096];
    ssize_t got = recv(state->fd, tmp, sizeof(tmp), 0);
    if (got < 0) {
      if (errno == EINTR) {
        continue;
      }
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        if (fz_wait_for_fd_event(state->fd, POLLIN, timeout_ms) == 0) {
          continue;
        }
      }
      return -1;
    }
    if (got == 0) {
      return 0;
    }
    size_t unread = state->request_body_buf_len - state->request_body_buf_pos;
    char* next = (char*)malloc(unread + (size_t)got + 1);
    if (next == NULL) {
      return -1;
    }
    if (unread > 0 && state->request_body_buf != NULL) {
      memcpy(next, state->request_body_buf + state->request_body_buf_pos, unread);
    }
    memcpy(next + unread, tmp, (size_t)got);
    next[unread + (size_t)got] = '\0';
    if (state->request_body_buf != NULL) {
      free(state->request_body_buf);
    }
    state->request_body_buf = next;
    state->request_body_buf_len = unread + (size_t)got;
    state->request_body_buf_pos = 0;
  }
  return 1;
}

static int fz_conn_read_body_chunk(fz_conn_state* state, char** out_ptr, size_t* out_len, int32_t max_bytes) {
  if (state == NULL || out_ptr == NULL || out_len == NULL) {
    return -1;
  }
  *out_ptr = NULL;
  *out_len = 0;
  if (max_bytes <= 0) {
    state->request_body_eof = 1;
    return 1;
  }
  if (state->request_body_eof) {
    return 1;
  }
  if (state->request_body_mode == 1) {
    if (state->request_body_remaining <= 0) {
      state->request_body_eof = 1;
      return 1;
    }
    size_t to_take = (size_t)max_bytes;
    if ((int64_t)to_take > state->request_body_remaining) {
      to_take = (size_t)state->request_body_remaining;
    }
    size_t have = state->request_body_buf_len - state->request_body_buf_pos;
    if (have < to_take) {
      int rc = fz_conn_recv_into_body_buffer(state, to_take, 2500);
      if (rc <= 0) {
        return -1;
      }
      have = state->request_body_buf_len - state->request_body_buf_pos;
    }
    if (have < to_take) {
      to_take = have;
    }
    char* chunk = (char*)malloc(to_take + 1);
    if (chunk == NULL) {
      return -1;
    }
    if (to_take > 0) {
      memcpy(chunk, state->request_body_buf + state->request_body_buf_pos, to_take);
      state->request_body_buf_pos += to_take;
      state->request_body_remaining -= (int64_t)to_take;
    }
    chunk[to_take] = '\0';
    if (state->request_body_buf_pos >= state->request_body_buf_len) {
      free(state->request_body_buf);
      state->request_body_buf = NULL;
      state->request_body_buf_len = 0;
      state->request_body_buf_pos = 0;
    }
    if (state->request_body_remaining <= 0) {
      state->request_body_eof = 1;
    }
    *out_ptr = chunk;
    *out_len = to_take;
    return 0;
  }
  if (state->request_body_mode == 2) {
    if (state->request_chunk_remaining == 0) {
      for (;;) {
        int ready = fz_conn_recv_into_body_buffer(state, 2, 2500);
        if (ready <= 0) {
          return -1;
        }
        char* scan = state->request_body_buf + state->request_body_buf_pos;
        size_t scan_len = state->request_body_buf_len - state->request_body_buf_pos;
        char* line_end = NULL;
        for (size_t i = 0; i + 1 < scan_len; i++) {
          if (scan[i] == '\r' && scan[i + 1] == '\n') {
            line_end = scan + i;
            break;
          }
        }
        if (line_end == NULL) {
          int more = fz_conn_recv_into_body_buffer(state, scan_len + 2, 2500);
          if (more <= 0) {
            return -1;
          }
          continue;
        }
        size_t line_len = (size_t)(line_end - scan);
        char size_line[64];
        if (line_len >= sizeof(size_line)) {
          return -1;
        }
        memcpy(size_line, scan, line_len);
        size_line[line_len] = '\0';
        char* semi = strchr(size_line, ';');
        if (semi != NULL) {
          *semi = '\0';
        }
        char* parse_end = NULL;
        long long chunk_size = strtoll(size_line, &parse_end, 16);
        if (parse_end == size_line || chunk_size < 0) {
          return -1;
        }
        state->request_body_buf_pos += line_len + 2;
        if (state->request_body_buf_pos >= state->request_body_buf_len) {
          free(state->request_body_buf);
          state->request_body_buf = NULL;
          state->request_body_buf_len = 0;
          state->request_body_buf_pos = 0;
        }
        if (chunk_size == 0) {
          (void)fz_conn_recv_into_body_buffer(state, 2, 2500);
          if (state->request_body_buf_len - state->request_body_buf_pos >= 2) {
            state->request_body_buf_pos += 2;
          }
          state->request_body_eof = 1;
          return 1;
        }
        state->request_chunk_remaining = chunk_size;
        break;
      }
    }
    size_t to_take = (size_t)max_bytes;
    if ((int64_t)to_take > state->request_chunk_remaining) {
      to_take = (size_t)state->request_chunk_remaining;
    }
    if ((state->request_body_buf_len - state->request_body_buf_pos) < to_take) {
      int rc = fz_conn_recv_into_body_buffer(state, to_take, 2500);
      if (rc <= 0) {
        return -1;
      }
    }
    char* chunk = (char*)malloc(to_take + 1);
    if (chunk == NULL) {
      return -1;
    }
    memcpy(chunk, state->request_body_buf + state->request_body_buf_pos, to_take);
    state->request_body_buf_pos += to_take;
    state->request_chunk_remaining -= (int64_t)to_take;
    chunk[to_take] = '\0';
    if (state->request_chunk_remaining == 0) {
      if ((state->request_body_buf_len - state->request_body_buf_pos) < 2) {
        int rc = fz_conn_recv_into_body_buffer(state, 2, 2500);
        if (rc <= 0) {
          free(chunk);
          return -1;
        }
      }
      state->request_body_buf_pos += 2;
    }
    if (state->request_body_buf_pos >= state->request_body_buf_len) {
      free(state->request_body_buf);
      state->request_body_buf = NULL;
      state->request_body_buf_len = 0;
      state->request_body_buf_pos = 0;
    }
    *out_ptr = chunk;
    *out_len = to_take;
    return 0;
  }
  state->request_body_eof = 1;
  return 1;
}

static int fz_conn_discard_body(fz_conn_state* state) {
  if (state == NULL) {
    return -1;
  }
  while (!state->request_body_eof) {
    char* chunk = NULL;
    size_t chunk_len = 0;
    int rc = fz_conn_read_body_chunk(state, &chunk, &chunk_len, 4096);
    if (chunk != NULL) {
      free(chunk);
    }
    if (rc < 0) {
      return -1;
    }
    if (rc > 0) {
      break;
    }
  }
  return 0;
}

static void fz_bytes_buf_init(fz_bytes_buf* buf) {
  buf->data = NULL;
  buf->len = 0;
  buf->cap = 0;
}

static void fz_bytes_buf_free(fz_bytes_buf* buf) {
  if (buf->data != NULL) {
    free(buf->data);
  }
  buf->data = NULL;
  buf->len = 0;
  buf->cap = 0;
}

static int fz_bytes_buf_append(fz_bytes_buf* buf, const char* data, size_t len) {
  if (len == 0) {
    return 0;
  }
  size_t needed = buf->len + len + 1;
  if (needed > buf->cap) {
    size_t next_cap = buf->cap == 0 ? 4096 : buf->cap;
    while (next_cap < needed) {
      next_cap *= 2;
    }
    char* next = (char*)realloc(buf->data, next_cap);
    if (next == NULL) {
      return -1;
    }
    buf->data = next;
    buf->cap = next_cap;
  }
  memcpy(buf->data + buf->len, data, len);
  buf->len += len;
  buf->data[buf->len] = '\0';
  return 0;
}

static int fz_drain_fd(int fd, fz_bytes_buf* buf) {
  if (fd < 0) {
    return 0;
  }
  char tmp[4096];
  for (;;) {
    ssize_t got = read(fd, tmp, sizeof(tmp));
    if (got > 0) {
      if (fz_bytes_buf_append(buf, tmp, (size_t)got) != 0) {
        return -1;
      }
      continue;
    }
    if (got == 0) {
      return 1;
    }
    if (errno == EINTR) {
      continue;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return 0;
    }
    return -1;
  }
}

static int fz_set_nonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0) {
    return -1;
  }
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static int fz_mark_cloexec(int fd) {
  int flags = fcntl(fd, F_GETFD, 0);
  if (flags < 0) {
    return -1;
  }
  return fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
}

static fz_proc_state* fz_proc_state_get(int32_t handle) {
  if (handle <= 0 || handle > FZ_MAX_PROC_STATES) {
    return NULL;
  }
  fz_proc_state* state = &fz_proc_states[handle - 1];
  if (!state->in_use) {
    return NULL;
  }
  return state;
}

static fz_http_stream_state* fz_http_stream_state_get(int32_t handle) {
  if (handle <= 0 || handle > FZ_MAX_HTTP_STREAMS) {
    return NULL;
  }
  fz_http_stream_state* state = &fz_http_stream_states[handle - 1];
  if (!state->in_use) {
    return NULL;
  }
  return state;
}

static void fz_proc_set_last_error(const char* msg) {
  if (msg == NULL) {
    msg = "proc error";
  }
  fz_proc_last_error_id = fz_intern_slice(msg, strlen(msg));
}

static int32_t fz_proc_state_alloc(pid_t pid, int stdout_fd, int stderr_fd) {
  for (int i = 0; i < FZ_MAX_PROC_STATES; i++) {
    if (!fz_proc_states[i].in_use) {
      fz_proc_states[i].in_use = 1;
      fz_proc_states[i].pid = pid;
      fz_proc_states[i].stdout_fd = stdout_fd;
      fz_proc_states[i].stderr_fd = stderr_fd;
      fz_proc_states[i].done = 0;
      fz_proc_states[i].exit_notified = 0;
      fz_proc_states[i].exit_code = -1;
      fz_proc_states[i].stdout_read_pos = 0;
      fz_proc_states[i].stderr_read_pos = 0;
      fz_proc_states[i].stdout_id = 0;
      fz_proc_states[i].stderr_id = 0;
      fz_bytes_buf_init(&fz_proc_states[i].stdout_buf);
      fz_bytes_buf_init(&fz_proc_states[i].stderr_buf);
      return i + 1;
    }
  }
  return -1;
}

static int32_t fz_http_stream_state_alloc(pid_t pid, int stdout_fd, int stderr_fd, int32_t status_code) {
  for (int i = 0; i < FZ_MAX_HTTP_STREAMS; i++) {
    if (!fz_http_stream_states[i].in_use) {
      memset(&fz_http_stream_states[i], 0, sizeof(fz_http_stream_states[i]));
      fz_http_stream_states[i].in_use = 1;
      fz_http_stream_states[i].pid = pid;
      fz_http_stream_states[i].stdout_fd = stdout_fd;
      fz_http_stream_states[i].stderr_fd = stderr_fd;
      fz_http_stream_states[i].done = 0;
      fz_http_stream_states[i].eof = 0;
      fz_http_stream_states[i].closed = 0;
      fz_http_stream_states[i].exit_code = -1;
      fz_http_stream_states[i].status_code = status_code;
      fz_http_stream_states[i].error_id = fz_intern_slice("", 0);
      fz_http_stream_states[i].stdout_read_pos = 0;
      fz_bytes_buf_init(&fz_http_stream_states[i].stdout_buf);
      fz_bytes_buf_init(&fz_http_stream_states[i].stderr_buf);
      return i + 1;
    }
  }
  return -1;
}

static fz_websocket_state* fz_websocket_state_get(int32_t handle) {
  if (handle <= 0 || handle > FZ_MAX_WEBSOCKETS) {
    return NULL;
  }
  fz_websocket_state* state = &fz_websocket_states[handle - 1];
  return state->in_use ? state : NULL;
}

static int32_t fz_websocket_state_alloc(int fd) {
  for (int i = 0; i < FZ_MAX_WEBSOCKETS; i++) {
    if (!fz_websocket_states[i].in_use) {
      memset(&fz_websocket_states[i], 0, sizeof(fz_websocket_states[i]));
      fz_websocket_states[i].in_use = 1;
      fz_websocket_states[i].fd = fd;
      fz_websocket_states[i].last_kind_id = fz_intern_slice("open", 4);
      fz_websocket_states[i].last_error_id = fz_intern_slice("", 0);
      return i + 1;
    }
  }
  return -1;
}

static void fz_sha1_compute(const uint8_t* data, size_t len, uint8_t out[20]) {
  uint32_t h0 = 0x67452301u;
  uint32_t h1 = 0xEFCDAB89u;
  uint32_t h2 = 0x98BADCFEu;
  uint32_t h3 = 0x10325476u;
  uint32_t h4 = 0xC3D2E1F0u;
  uint64_t bit_len = (uint64_t)len * 8u;
  size_t padded_len = len + 1 + 8;
  size_t rem = padded_len % 64;
  if (rem != 0) {
    padded_len += 64 - rem;
  }
  uint8_t* buf = (uint8_t*)calloc(padded_len, 1);
  if (buf == NULL) {
    memset(out, 0, 20);
    return;
  }
  memcpy(buf, data, len);
  buf[len] = 0x80u;
  for (int i = 0; i < 8; i++) {
    buf[padded_len - 1 - i] = (uint8_t)(bit_len >> (i * 8));
  }
  for (size_t offset = 0; offset < padded_len; offset += 64) {
    uint32_t w[80];
    for (int i = 0; i < 16; i++) {
      size_t at = offset + (size_t)i * 4;
      w[i] = ((uint32_t)buf[at] << 24) | ((uint32_t)buf[at + 1] << 16)
          | ((uint32_t)buf[at + 2] << 8) | (uint32_t)buf[at + 3];
    }
    for (int i = 16; i < 80; i++) {
      uint32_t x = w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16];
      w[i] = (x << 1) | (x >> 31);
    }
    uint32_t a = h0;
    uint32_t b = h1;
    uint32_t c = h2;
    uint32_t d = h3;
    uint32_t e = h4;
    for (int i = 0; i < 80; i++) {
      uint32_t f;
      uint32_t k;
      if (i < 20) {
        f = (b & c) | ((~b) & d);
        k = 0x5A827999u;
      } else if (i < 40) {
        f = b ^ c ^ d;
        k = 0x6ED9EBA1u;
      } else if (i < 60) {
        f = (b & c) | (b & d) | (c & d);
        k = 0x8F1BBCDCu;
      } else {
        f = b ^ c ^ d;
        k = 0xCA62C1D6u;
      }
      uint32_t temp = ((a << 5) | (a >> 27)) + f + e + k + w[i];
      e = d;
      d = c;
      c = (b << 30) | (b >> 2);
      b = a;
      a = temp;
    }
    h0 += a;
    h1 += b;
    h2 += c;
    h3 += d;
    h4 += e;
  }
  free(buf);
  uint32_t words[5] = {h0, h1, h2, h3, h4};
  for (int i = 0; i < 5; i++) {
    out[i * 4] = (uint8_t)(words[i] >> 24);
    out[i * 4 + 1] = (uint8_t)(words[i] >> 16);
    out[i * 4 + 2] = (uint8_t)(words[i] >> 8);
    out[i * 4 + 3] = (uint8_t)(words[i]);
  }
}

static char* fz_base64_encode(const uint8_t* data, size_t len) {
  static const char table[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  size_t out_len = ((len + 2) / 3) * 4;
  char* out = (char*)malloc(out_len + 1);
  if (out == NULL) {
    return NULL;
  }
  size_t j = 0;
  for (size_t i = 0; i < len; i += 3) {
    uint32_t octet_a = data[i];
    uint32_t octet_b = i + 1 < len ? data[i + 1] : 0;
    uint32_t octet_c = i + 2 < len ? data[i + 2] : 0;
    uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;
    out[j++] = table[(triple >> 18) & 0x3F];
    out[j++] = table[(triple >> 12) & 0x3F];
    out[j++] = i + 1 < len ? table[(triple >> 6) & 0x3F] : '=';
    out[j++] = i + 2 < len ? table[triple & 0x3F] : '=';
  }
  out[j] = '\0';
  return out;
}

static int fz_websocket_recv_exact(int fd, uint8_t* buf, size_t len) {
  size_t used = 0;
  while (used < len) {
    ssize_t got = recv(fd, buf + used, len - used, 0);
    if (got < 0) {
      if (errno == EINTR) {
        continue;
      }
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        if (fz_wait_for_fd_event(fd, POLLIN, 2500) == 0) {
          continue;
        }
      }
      return -1;
    }
    if (got == 0) {
      return -1;
    }
    used += (size_t)got;
  }
  return 0;
}

static int fz_websocket_write_frame(int fd, uint8_t opcode, const char* payload, size_t payload_len) {
  if (fd < 0) {
    return -1;
  }
  uint8_t header[10];
  size_t header_len = 0;
  header[header_len++] = (uint8_t)(0x80u | (opcode & 0x0Fu));
  if (payload_len < 126) {
    header[header_len++] = (uint8_t)payload_len;
  } else if (payload_len <= 0xFFFFu) {
    header[header_len++] = 126;
    header[header_len++] = (uint8_t)((payload_len >> 8) & 0xFFu);
    header[header_len++] = (uint8_t)(payload_len & 0xFFu);
  } else {
    header[header_len++] = 127;
    for (int i = 7; i >= 0; i--) {
      header[header_len++] = (uint8_t)((payload_len >> (i * 8)) & 0xFFu);
    }
  }
  if (fz_send_all(fd, (const char*)header, header_len) != 0) {
    return -1;
  }
  if (payload_len > 0 && payload != NULL && fz_send_all(fd, payload, payload_len) != 0) {
    return -1;
  }
  return 0;
}

static int fz_websocket_read_frame(
    fz_websocket_state* ws,
    int32_t max_bytes,
    int32_t* out_kind_id,
    int32_t* out_close_code,
    int32_t* out_error_id) {
  if (ws == NULL || ws->fd < 0 || out_kind_id == NULL || out_close_code == NULL || out_error_id == NULL) {
    return fz_intern_slice("", 0);
  }
  *out_close_code = 0;
  *out_error_id = fz_intern_slice("", 0);
  uint8_t hdr[2];
  if (fz_websocket_recv_exact(ws->fd, hdr, 2) != 0) {
    *out_kind_id = fz_intern_slice("error", 5);
    *out_error_id = fz_intern_slice("websocket read failed", 21);
    return fz_intern_slice("", 0);
  }
  uint8_t opcode = hdr[0] & 0x0Fu;
  int fin = (hdr[0] & 0x80u) != 0;
  int masked = (hdr[1] & 0x80u) != 0;
  uint64_t payload_len = hdr[1] & 0x7Fu;
  if (payload_len == 126) {
    uint8_t ext[2];
    if (fz_websocket_recv_exact(ws->fd, ext, 2) != 0) {
      *out_kind_id = fz_intern_slice("error", 5);
      *out_error_id = fz_intern_slice("websocket extended length read failed", 37);
      return fz_intern_slice("", 0);
    }
    payload_len = ((uint64_t)ext[0] << 8) | (uint64_t)ext[1];
  } else if (payload_len == 127) {
    uint8_t ext[8];
    if (fz_websocket_recv_exact(ws->fd, ext, 8) != 0) {
      *out_kind_id = fz_intern_slice("error", 5);
      *out_error_id = fz_intern_slice("websocket 64-bit length read failed", 35);
      return fz_intern_slice("", 0);
    }
    payload_len = 0;
    for (int i = 0; i < 8; i++) {
      payload_len = (payload_len << 8) | (uint64_t)ext[i];
    }
  }
  uint8_t mask[4] = {0};
  if (masked) {
    if (fz_websocket_recv_exact(ws->fd, mask, 4) != 0) {
      *out_kind_id = fz_intern_slice("error", 5);
      *out_error_id = fz_intern_slice("websocket mask read failed", 26);
      return fz_intern_slice("", 0);
    }
  }
  if (max_bytes > 0 && payload_len > (uint64_t)max_bytes) {
    *out_kind_id = fz_intern_slice("error", 5);
    *out_error_id = fz_intern_slice("websocket frame exceeds max_bytes", 33);
    return fz_intern_slice("", 0);
  }
  uint8_t* payload = NULL;
  if (payload_len > 0) {
    payload = (uint8_t*)malloc((size_t)payload_len);
    if (payload == NULL) {
      *out_kind_id = fz_intern_slice("error", 5);
      *out_error_id = fz_intern_slice("websocket payload alloc failed", 30);
      return fz_intern_slice("", 0);
    }
    if (fz_websocket_recv_exact(ws->fd, payload, (size_t)payload_len) != 0) {
      free(payload);
      *out_kind_id = fz_intern_slice("error", 5);
      *out_error_id = fz_intern_slice("websocket payload read failed", 29);
      return fz_intern_slice("", 0);
    }
    if (masked) {
      for (uint64_t i = 0; i < payload_len; i++) {
        payload[i] ^= mask[i % 4];
      }
    }
  }
  if (!fin) {
    free(payload);
    *out_kind_id = fz_intern_slice("error", 5);
    *out_error_id = fz_intern_slice("fragmented websocket frames unsupported", 39);
    return fz_intern_slice("", 0);
  }
  if (opcode == 0x8u) {
    *out_kind_id = fz_intern_slice("close", 5);
    if (payload_len >= 2) {
      *out_close_code = ((int32_t)payload[0] << 8) | (int32_t)payload[1];
    }
    free(payload);
    ws->closed = 1;
    return fz_intern_slice("", 0);
  }
  if (opcode == 0x9u) {
    *out_kind_id = fz_intern_slice("ping", 4);
  } else if (opcode == 0xAu) {
    *out_kind_id = fz_intern_slice("pong", 4);
  } else if (opcode == 0x2u) {
    *out_kind_id = fz_intern_slice("binary", 6);
  } else {
    *out_kind_id = fz_intern_slice("text", 4);
  }
  int32_t out = fz_intern_slice((const char*)(payload == NULL ? (uint8_t*)"" : payload), (size_t)payload_len);
  free(payload);
  return out;
}

static void fz_proc_finalize(fz_proc_state* state, int exit_code) {
  if (state->stdout_fd >= 0) {
    (void)fz_drain_fd(state->stdout_fd, &state->stdout_buf);
    close(state->stdout_fd);
    state->stdout_fd = -1;
  }
  if (state->stderr_fd >= 0) {
    (void)fz_drain_fd(state->stderr_fd, &state->stderr_buf);
    close(state->stderr_fd);
    state->stderr_fd = -1;
  }
  state->stdout_id = fz_intern_slice(
      state->stdout_buf.data == NULL ? "" : state->stdout_buf.data,
      state->stdout_buf.data == NULL ? 0 : state->stdout_buf.len);
  state->stderr_id = fz_intern_slice(
      state->stderr_buf.data == NULL ? "" : state->stderr_buf.data,
      state->stderr_buf.data == NULL ? 0 : state->stderr_buf.len);
  state->exit_code = exit_code;
  state->done = 1;
}

static void fz_spawn_join_all(void) {
  for (int i = 0; i < FZ_MAX_SPAWN_THREADS; i++) {
    pthread_t thread;
    int should_join = 0;
    pthread_mutex_lock(&fz_spawn_lock);
    fz_spawn_state* state = &fz_spawn_states[i];
    if (state->in_use && !state->detached && !state->joined) {
      state->joined = 1;
      thread = state->thread;
      should_join = 1;
    }
    pthread_mutex_unlock(&fz_spawn_lock);
    if (should_join) {
      (void)pthread_join(thread, NULL);
    }
  }
}

static void fz_spawn_register_atexit(void) {
  const char* max_active = fz_env_get_bootstrapped("FZ_SPAWN_MAX_ACTIVE");
  if (max_active != NULL && max_active[0] != '\0') {
    int parsed = atoi(max_active);
    if (parsed > 0 && parsed <= FZ_MAX_SPAWN_THREADS) {
      fz_spawn_max_active = parsed;
    }
  }
  (void)atexit(fz_spawn_join_all);
}

static fz_spawn_state* fz_spawn_state_by_handle_locked(int32_t handle) {
  for (int i = 0; i < FZ_MAX_SPAWN_THREADS; i++) {
    if (fz_spawn_states[i].in_use && fz_spawn_states[i].handle == handle) {
      return &fz_spawn_states[i];
    }
  }
  return NULL;
}

static fz_spawn_state* fz_spawn_state_alloc_locked(void) {
  for (int i = 0; i < FZ_MAX_SPAWN_THREADS; i++) {
    if (!fz_spawn_states[i].in_use) {
      return &fz_spawn_states[i];
    }
  }
  return NULL;
}

static fz_task_group_state* fz_task_group_by_id_locked(int32_t group_id) {
  for (int i = 0; i < 256; i++) {
    if (fz_task_groups[i].in_use && fz_task_groups[i].id == group_id) {
      return &fz_task_groups[i];
    }
  }
  return NULL;
}

static fz_task_group_state* fz_task_group_alloc_locked(void) {
  for (int i = 0; i < 256; i++) {
    if (!fz_task_groups[i].in_use) {
      return &fz_task_groups[i];
    }
  }
  return NULL;
}

static void* fz_spawn_thread_main(void* arg) {
  fz_spawn_ctx* ctx = (fz_spawn_ctx*)arg;
  if (ctx == NULL) {
    return NULL;
  }
  int32_t handle = ctx->handle;
  free(ctx);

  fz_task_entry_fn entry = NULL;
  int32_t context_id = 0;
  int32_t group_id = 0;
  int cancelled = 0;
  pthread_mutex_lock(&fz_spawn_lock);
  fz_spawn_state* state = fz_spawn_state_by_handle_locked(handle);
  if (state != NULL) {
    state->started = 1;
    entry = fz_task_entries[state->task_ref - 1];
    context_id = state->context_id;
    group_id = state->group_id;
    cancelled = state->cancelled;
  }
  pthread_mutex_unlock(&fz_spawn_lock);

  int32_t result = -1;
  fz_tls_task_context = context_id;
  fz_tls_task_handle = handle;
  fz_tls_async_deadline_ms = 0;
  fz_tls_async_cancelled = cancelled ? 1 : 0;
  if (!cancelled && entry != NULL) {
    result = entry();
  } else if (cancelled) {
    result = -2;
  }
  fz_tls_task_handle = 0;
  fz_tls_async_deadline_ms = 0;
  fz_tls_async_cancelled = 0;
  fz_tls_task_context = 0;

  pthread_mutex_lock(&fz_spawn_lock);
  state = fz_spawn_state_by_handle_locked(handle);
  if (state != NULL) {
    state->finished = 1;
    state->result = result;
    if (fz_spawn_active_count > 0) {
      fz_spawn_active_count--;
    }
    if (group_id > 0) {
      fz_task_group_state* group = fz_task_group_by_id_locked(group_id);
      if (group != NULL && group->active_count > 0) {
        group->active_count--;
      }
    }
    if (state->detached) {
      memset(state, 0, sizeof(*state));
    }
  }
  pthread_mutex_unlock(&fz_spawn_lock);
  return NULL;
}

static int32_t fz_native_spawn_impl(int32_t task_ref, int32_t context_id, int32_t group_id) {
  if (task_ref <= 0 || task_ref > fz_task_entry_count) {
    return -1;
  }
  if (fz_task_entries[task_ref - 1] == NULL) {
    return -1;
  }

  pthread_once(&fz_spawn_atexit_once, fz_spawn_register_atexit);
  pthread_mutex_lock(&fz_spawn_lock);
  if (fz_spawn_active_count >= fz_spawn_max_active) {
    pthread_mutex_unlock(&fz_spawn_lock);
    return -1;
  }
  if (group_id > 0 && fz_task_group_by_id_locked(group_id) == NULL) {
    pthread_mutex_unlock(&fz_spawn_lock);
    return -1;
  }
  fz_spawn_state* state = fz_spawn_state_alloc_locked();
  if (state == NULL) {
    pthread_mutex_unlock(&fz_spawn_lock);
    return -1;
  }
  memset(state, 0, sizeof(*state));
  state->in_use = 1;
  state->handle = fz_next_spawn_handle++;
  state->task_ref = task_ref;
  state->context_id = context_id;
  state->group_id = group_id;
  if (group_id > 0) {
    fz_task_group_state* group = fz_task_group_by_id_locked(group_id);
    if (group != NULL) {
      group->active_count++;
    }
  }
  fz_spawn_active_count++;
  int32_t handle = state->handle;
  pthread_mutex_unlock(&fz_spawn_lock);

  fz_spawn_ctx* ctx = (fz_spawn_ctx*)malloc(sizeof(fz_spawn_ctx));
  if (ctx == NULL) {
    pthread_mutex_lock(&fz_spawn_lock);
    state = fz_spawn_state_by_handle_locked(handle);
    if (state != NULL) {
      if (state->group_id > 0) {
        fz_task_group_state* group = fz_task_group_by_id_locked(state->group_id);
        if (group != NULL && group->active_count > 0) {
          group->active_count--;
        }
      }
      memset(state, 0, sizeof(*state));
    }
    if (fz_spawn_active_count > 0) {
      fz_spawn_active_count--;
    }
    pthread_mutex_unlock(&fz_spawn_lock);
    return -1;
  }
  ctx->handle = handle;

  pthread_t thread;
  if (pthread_create(&thread, NULL, fz_spawn_thread_main, ctx) != 0) {
    free(ctx);
    pthread_mutex_lock(&fz_spawn_lock);
    state = fz_spawn_state_by_handle_locked(handle);
    if (state != NULL) {
      if (state->group_id > 0) {
        fz_task_group_state* group = fz_task_group_by_id_locked(state->group_id);
        if (group != NULL && group->active_count > 0) {
          group->active_count--;
        }
      }
      memset(state, 0, sizeof(*state));
    }
    if (fz_spawn_active_count > 0) {
      fz_spawn_active_count--;
    }
    pthread_mutex_unlock(&fz_spawn_lock);
    return -1;
  }

  pthread_mutex_lock(&fz_spawn_lock);
  state = fz_spawn_state_by_handle_locked(handle);
  if (state != NULL) {
    state->thread = thread;
  }
  pthread_mutex_unlock(&fz_spawn_lock);
  return handle;
}

int32_t fz_native_env_get(int32_t key_id) {
  const char* key = fz_lookup_string(key_id);
  if (key == NULL || key[0] == '\0') {
    return 0;
  }
  const char* value = fz_env_get_bootstrapped(key);
  if (value == NULL) {
    value = "";
  }
  return fz_intern_slice(value, strlen(value));
}

uintptr_t fz_native_alloc(uintptr_t size) {
  size_t bytes = (size_t)size;
  void* raw = bytes == 0 ? malloc(1) : malloc(bytes);
  return (uintptr_t)raw;
}

void fz_native_free(uintptr_t ptr) {
  if (ptr == 0) {
    return;
  }
  free((void*)ptr);
}

void fz_native_mem_freeze(void) {}

void fz_native_mem_unfreeze(void) {}

int32_t fz_native_time_now(void) {
  return (int32_t)fz_now_ms();
}

int32_t fz_native_http_header(int32_t key_id, int32_t value_id) {
  if (key_id <= 0 || value_id <= 0) {
    return -1;
  }
  pthread_mutex_lock(&fz_http_lock);
  if (fz_http_header_count >= FZ_MAX_HTTP_HEADERS) {
    pthread_mutex_unlock(&fz_http_lock);
    return -1;
  }
  fz_http_headers[fz_http_header_count].key_id = key_id;
  fz_http_headers[fz_http_header_count].value_id = value_id;
  fz_http_header_count++;
  pthread_mutex_unlock(&fz_http_lock);
  return 0;
}

int32_t fz_native_json_escape(int32_t input_id) {
  const char* input = fz_lookup_string(input_id);
  char* escaped = fz_json_escape_owned(input);
  if (escaped == NULL) {
    return 0;
  }
  return fz_intern_owned(escaped);
}

int32_t fz_native_json_str(int32_t input_id) {
  const char* input = fz_lookup_string(input_id);
  char* escaped = fz_json_escape_owned(input);
  if (escaped == NULL) {
    return 0;
  }
  size_t len = strlen(escaped);
  char* wrapped = (char*)malloc(len + 3);
  if (wrapped == NULL) {
    free(escaped);
    return 0;
  }
  wrapped[0] = '\"';
  if (len > 0) {
    memcpy(wrapped + 1, escaped, len);
  }
  wrapped[len + 1] = '\"';
  wrapped[len + 2] = '\0';
  free(escaped);
  return fz_intern_owned(wrapped);
}

int32_t fz_native_json_raw(int32_t input_id) {
  const char* input = fz_lookup_string(input_id);
  if (input == NULL || input[0] == '\0') {
    return fz_intern_slice("null", 4);
  }
  return fz_intern_slice(input, strlen(input));
}

static int32_t fz_native_json_array_from_values(const int32_t* ids, int value_count) {
  if (ids == NULL || value_count <= 0) {
    return fz_intern_slice("[]", 2);
  }
  size_t total = 3;
  for (int i = 0; i < value_count; i++) {
    const char* value = fz_lookup_string(ids[i]);
    total += strlen(value == NULL || value[0] == '\0' ? "null" : value) + 1;
  }
  char* out = (char*)malloc(total);
  if (out == NULL) {
    return 0;
  }
  size_t used = 0;
  out[used++] = '[';
  for (int i = 0; i < value_count; i++) {
    if (i > 0) {
      out[used++] = ',';
    }
    const char* value = fz_lookup_string(ids[i]);
    if (value == NULL || value[0] == '\0') {
      value = "null";
    }
    size_t len = strlen(value);
    if (len > 0) {
      memcpy(out + used, value, len);
      used += len;
    }
  }
  out[used++] = ']';
  out[used] = '\0';
  return fz_intern_owned(out);
}

static int32_t fz_native_json_object_from_pairs(const int32_t* ids, int pair_count) {
  if (ids == NULL || pair_count <= 0) {
    return fz_intern_slice("{}", 2);
  }
  char** escaped_keys = (char**)calloc((size_t)pair_count, sizeof(char*));
  if (escaped_keys == NULL) {
    return 0;
  }
  size_t total = 3;
  for (int i = 0; i < pair_count; i++) {
    const char* key = fz_lookup_string(ids[i * 2]);
    const char* raw_value = fz_lookup_string(ids[(i * 2) + 1]);
    if (raw_value == NULL || raw_value[0] == '\0') {
      raw_value = "null";
    }
    escaped_keys[i] = fz_json_escape_owned(key);
    if (escaped_keys[i] == NULL) {
      for (int j = 0; j <= i; j++) {
        free(escaped_keys[j]);
      }
      free(escaped_keys);
      return 0;
    }
    total += strlen(escaped_keys[i]) + strlen(raw_value) + 5;
  }
  char* body = (char*)malloc(total);
  if (body == NULL) {
    for (int i = 0; i < pair_count; i++) {
      free(escaped_keys[i]);
    }
    free(escaped_keys);
    return 0;
  }
  size_t used = 0;
  body[used++] = '{';
  for (int i = 0; i < pair_count; i++) {
    if (i > 0) {
      body[used++] = ',';
    }
    body[used++] = '\"';
    size_t key_len = strlen(escaped_keys[i]);
    memcpy(body + used, escaped_keys[i], key_len);
    used += key_len;
    body[used++] = '\"';
    body[used++] = ':';
    const char* raw_value = fz_lookup_string(ids[(i * 2) + 1]);
    if (raw_value == NULL || raw_value[0] == '\0') {
      raw_value = "null";
    }
    size_t value_len = strlen(raw_value);
    memcpy(body + used, raw_value, value_len);
    used += value_len;
  }
  body[used++] = '}';
  body[used] = '\0';
  for (int i = 0; i < pair_count; i++) {
    free(escaped_keys[i]);
  }
  free(escaped_keys);
  return fz_intern_owned(body);
}

static int32_t fz_runtime_list_new(void) {
  pthread_mutex_lock(&fz_list_lock);
  int32_t handle = fz_list_alloc();
  pthread_mutex_unlock(&fz_list_lock);
  return handle;
}

static int32_t fz_runtime_list_push(int32_t handle, int32_t value_id) {
  const char* value = fz_lookup_string(value_id);
  pthread_mutex_lock(&fz_list_lock);
  fz_list_state* list = fz_list_get(handle);
  int ok = list != NULL && fz_list_push_cstr(list, value) == 0 ? 0 : -1;
  pthread_mutex_unlock(&fz_list_lock);
  return ok;
}

static int32_t fz_runtime_list_pop(int32_t handle) {
  pthread_mutex_lock(&fz_list_lock);
  fz_list_state* list = fz_list_get(handle);
  if (list == NULL || list->count <= 0) {
    pthread_mutex_unlock(&fz_list_lock);
    return fz_intern_slice("", 0);
  }
  char* item = list->items[list->count - 1];
  list->items[list->count - 1] = NULL;
  list->count--;
  int32_t id = fz_intern_slice(item == NULL ? "" : item, item == NULL ? 0 : strlen(item));
  free(item);
  pthread_mutex_unlock(&fz_list_lock);
  return id;
}

static int32_t fz_runtime_list_len(int32_t handle) {
  pthread_mutex_lock(&fz_list_lock);
  fz_list_state* list = fz_list_get(handle);
  int32_t len = list == NULL ? -1 : list->count;
  pthread_mutex_unlock(&fz_list_lock);
  return len;
}

static int32_t fz_runtime_list_get(int32_t handle, int32_t index) {
  pthread_mutex_lock(&fz_list_lock);
  fz_list_state* list = fz_list_get(handle);
  if (list == NULL || index < 0 || index >= list->count) {
    pthread_mutex_unlock(&fz_list_lock);
    return fz_intern_slice("", 0);
  }
  const char* item = list->items[index] == NULL ? "" : list->items[index];
  int32_t id = fz_intern_slice(item, strlen(item));
  pthread_mutex_unlock(&fz_list_lock);
  return id;
}

static int32_t fz_runtime_list_set(int32_t handle, int32_t index, int32_t value_id) {
  const char* value = fz_lookup_string(value_id);
  if (value == NULL) value = "";
  pthread_mutex_lock(&fz_list_lock);
  fz_list_state* list = fz_list_get(handle);
  if (list == NULL || index < 0 || index >= list->count) {
    pthread_mutex_unlock(&fz_list_lock);
    return -1;
  }
  char* dup = strdup(value);
  if (dup == NULL) {
    pthread_mutex_unlock(&fz_list_lock);
    return -1;
  }
  free(list->items[index]);
  list->items[index] = dup;
  pthread_mutex_unlock(&fz_list_lock);
  return 0;
}

static int32_t fz_runtime_list_clear(int32_t handle) {
  pthread_mutex_lock(&fz_list_lock);
  fz_list_state* list = fz_list_get(handle);
  if (list == NULL) {
    pthread_mutex_unlock(&fz_list_lock);
    return -1;
  }
  for (int i = 0; i < list->count; i++) {
    free(list->items[i]);
    list->items[i] = NULL;
  }
  list->count = 0;
  pthread_mutex_unlock(&fz_list_lock);
  return 0;
}

static int32_t fz_runtime_list_join(int32_t handle, int32_t sep_id) {
  const char* sep = fz_lookup_string(sep_id);
  if (sep == NULL) sep = "";
  pthread_mutex_lock(&fz_list_lock);
  fz_list_state* list = fz_list_get(handle);
  if (list == NULL) {
    pthread_mutex_unlock(&fz_list_lock);
    return fz_intern_slice("", 0);
  }
  size_t sep_len = strlen(sep);
  size_t total = 1;
  for (int i = 0; i < list->count; i++) {
    total += strlen(list->items[i] == NULL ? "" : list->items[i]);
    if (i > 0) total += sep_len;
  }
  char* out = (char*)malloc(total);
  if (out == NULL) {
    pthread_mutex_unlock(&fz_list_lock);
    return 0;
  }
  size_t used = 0;
  for (int i = 0; i < list->count; i++) {
    if (i > 0 && sep_len > 0) {
      memcpy(out + used, sep, sep_len);
      used += sep_len;
    }
    const char* item = list->items[i] == NULL ? "" : list->items[i];
    size_t len = strlen(item);
    if (len > 0) {
      memcpy(out + used, item, len);
      used += len;
    }
  }
  out[used] = '\0';
  pthread_mutex_unlock(&fz_list_lock);
  return fz_intern_owned(out);
}

static int32_t fz_runtime_map_new(void) {
  pthread_mutex_lock(&fz_map_lock);
  int32_t handle = fz_map_alloc();
  pthread_mutex_unlock(&fz_map_lock);
  return handle;
}

static int32_t fz_runtime_map_set(int32_t handle, int32_t key_id, int32_t value_id) {
  const char* key = fz_lookup_string(key_id);
  const char* value = fz_lookup_string(value_id);
  if (key == NULL || key[0] == '\0') {
    return -1;
  }
  if (value == NULL) value = "";
  pthread_mutex_lock(&fz_map_lock);
  fz_map_state* map = fz_map_get(handle);
  if (map == NULL) {
    pthread_mutex_unlock(&fz_map_lock);
    return -1;
  }
  int idx = fz_map_find_index(map, key);
  if (idx >= 0) {
    char* dup = strdup(value);
    if (dup == NULL) {
      pthread_mutex_unlock(&fz_map_lock);
      return -1;
    }
    free(map->values[idx]);
    map->values[idx] = dup;
    pthread_mutex_unlock(&fz_map_lock);
    return 0;
  }
  if (map->count >= FZ_MAX_MAP_ENTRIES) {
    pthread_mutex_unlock(&fz_map_lock);
    return -1;
  }
  map->keys[map->count] = strdup(key);
  map->values[map->count] = strdup(value);
  if (map->keys[map->count] == NULL || map->values[map->count] == NULL) {
    free(map->keys[map->count]);
    free(map->values[map->count]);
    map->keys[map->count] = NULL;
    map->values[map->count] = NULL;
    pthread_mutex_unlock(&fz_map_lock);
    return -1;
  }
  map->count++;
  pthread_mutex_unlock(&fz_map_lock);
  return 0;
}

static int32_t fz_runtime_map_get(int32_t handle, int32_t key_id) {
  const char* key = fz_lookup_string(key_id);
  pthread_mutex_lock(&fz_map_lock);
  fz_map_state* map = fz_map_get(handle);
  if (map == NULL || key == NULL) {
    pthread_mutex_unlock(&fz_map_lock);
    return fz_intern_slice("", 0);
  }
  int idx = fz_map_find_index(map, key);
  const char* value = (idx >= 0 && map->values[idx] != NULL) ? map->values[idx] : "";
  int32_t out = fz_intern_slice(value, strlen(value));
  pthread_mutex_unlock(&fz_map_lock);
  return out;
}

static int32_t fz_runtime_map_has(int32_t handle, int32_t key_id) {
  const char* key = fz_lookup_string(key_id);
  pthread_mutex_lock(&fz_map_lock);
  fz_map_state* map = fz_map_get(handle);
  int ok = (map != NULL && key != NULL && fz_map_find_index(map, key) >= 0) ? 1 : 0;
  pthread_mutex_unlock(&fz_map_lock);
  return ok;
}

static int32_t fz_runtime_map_delete(int32_t handle, int32_t key_id) {
  const char* key = fz_lookup_string(key_id);
  pthread_mutex_lock(&fz_map_lock);
  fz_map_state* map = fz_map_get(handle);
  if (map == NULL || key == NULL) {
    pthread_mutex_unlock(&fz_map_lock);
    return -1;
  }
  int idx = fz_map_find_index(map, key);
  if (idx < 0) {
    pthread_mutex_unlock(&fz_map_lock);
    return 0;
  }
  free(map->keys[idx]);
  free(map->values[idx]);
  for (int i = idx; i + 1 < map->count; i++) {
    map->keys[i] = map->keys[i + 1];
    map->values[i] = map->values[i + 1];
  }
  map->count--;
  map->keys[map->count] = NULL;
  map->values[map->count] = NULL;
  pthread_mutex_unlock(&fz_map_lock);
  return 1;
}

static int32_t fz_runtime_map_keys(int32_t handle) {
  pthread_mutex_lock(&fz_list_lock);
  int32_t list_handle = fz_list_alloc();
  fz_list_state* list = fz_list_get(list_handle);
  pthread_mutex_unlock(&fz_list_lock);
  if (list == NULL) {
    return -1;
  }
  pthread_mutex_lock(&fz_map_lock);
  fz_map_state* map = fz_map_get(handle);
  if (map == NULL) {
    pthread_mutex_unlock(&fz_map_lock);
    return -1;
  }
  pthread_mutex_lock(&fz_list_lock);
  for (int i = 0; i < map->count; i++) {
    (void)fz_list_push_cstr(list, map->keys[i] == NULL ? "" : map->keys[i]);
  }
  pthread_mutex_unlock(&fz_list_lock);
  pthread_mutex_unlock(&fz_map_lock);
  return list_handle;
}

static int32_t fz_runtime_map_len(int32_t handle) {
  pthread_mutex_lock(&fz_map_lock);
  fz_map_state* map = fz_map_get(handle);
  int32_t len = map == NULL ? -1 : map->count;
  pthread_mutex_unlock(&fz_map_lock);
  return len;
}


#define FZ_MAX_GPU_DEVICES 16
#define FZ_MAX_GPU_BUFFERS 4096
#define FZ_MAX_GPU_PIPELINES 256
#define FZ_MAX_GPU_EVENTS 4096
#define FZ_GPU_SLICE_TAG 4101

typedef struct {
  int in_use;
  void* device;
  void* command_queue;
} fz_gpu_device_state;

typedef struct {
  int in_use;
  int32_t device_handle;
  int32_t element_size;
  int32_t len;
  void* buffer;
} fz_gpu_buffer_state;

typedef struct {
  int in_use;
  int32_t device_handle;
  int32_t source_id;
  int32_t kernel_name_id;
  uint64_t last_used_epoch;
  void* pipeline;
} fz_gpu_pipeline_state;

typedef struct {
  int in_use;
  int32_t device_handle;
  void* command_buffer;
} fz_gpu_event_state;

static fz_gpu_device_state fz_gpu_devices[FZ_MAX_GPU_DEVICES];
static fz_gpu_buffer_state fz_gpu_buffers[FZ_MAX_GPU_BUFFERS];
static fz_gpu_pipeline_state fz_gpu_pipelines[FZ_MAX_GPU_PIPELINES];
static fz_gpu_event_state fz_gpu_events[FZ_MAX_GPU_EVENTS];
static int32_t fz_gpu_device_count_cached = -1;
static uint64_t fz_gpu_pipeline_epoch = 0;
static pthread_mutex_t fz_gpu_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_once_t fz_gpu_init_once = PTHREAD_ONCE_INIT;

static void fz_gpu_runtime_init(void);
static int32_t fz_gpu_buffer_alloc_slot(void);
static int32_t fz_gpu_pipeline_alloc_slot(void);
static int32_t fz_gpu_pipeline_evict_lru_slot(void);
static int32_t fz_gpu_event_alloc_slot(void);

#if defined(__APPLE__) && defined(__OBJC__)
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

static void fz_gpu_runtime_init(void) {
  @autoreleasepool {
    int count = 0;
    NSArray<id<MTLDevice>>* devices = MTLCopyAllDevices();
    if (devices != nil && [devices count] > 0) {
      NSUInteger limit = [devices count] < FZ_MAX_GPU_DEVICES ? [devices count] : FZ_MAX_GPU_DEVICES;
      for (NSUInteger index = 0; index < limit; index++) {
        id<MTLDevice> device = [devices objectAtIndex:index];
        if (device == nil) {
          continue;
        }
        [device retain];
        fz_gpu_devices[count].in_use = 1;
        fz_gpu_devices[count].device = (void*)device;
        count++;
      }
      [devices release];
    }
    if (count == 0) {
      id<MTLDevice> device = MTLCreateSystemDefaultDevice();
      if (device != nil) {
        [device retain];
        fz_gpu_devices[count].in_use = 1;
        fz_gpu_devices[count].device = (void*)device;
        count++;
        [device release];
      }
    }
    fz_gpu_device_count_cached = count;
  }
}

static id<MTLDevice> fz_gpu_device_for_handle(int32_t handle) {
  if (handle <= 0 || handle > FZ_MAX_GPU_DEVICES) {
    return nil;
  }
  fz_gpu_device_state* state = &fz_gpu_devices[handle - 1];
  if (!state->in_use || state->device == NULL) {
    return nil;
  }
  return (id<MTLDevice>)state->device;
}

static id<MTLBuffer> fz_gpu_buffer_for_handle(int32_t handle) {
  if (handle <= 0 || handle > FZ_MAX_GPU_BUFFERS) {
    return nil;
  }
  fz_gpu_buffer_state* state = &fz_gpu_buffers[handle - 1];
  if (!state->in_use || state->buffer == NULL) {
    return nil;
  }
  return (id<MTLBuffer>)state->buffer;
}

static id<MTLCommandQueue> fz_gpu_queue_for_device(int32_t handle) {
  if (handle <= 0 || handle > FZ_MAX_GPU_DEVICES) {
    return nil;
  }
  fz_gpu_device_state* state = &fz_gpu_devices[handle - 1];
  if (!state->in_use || state->device == NULL) {
    return nil;
  }
  if (state->command_queue == NULL) {
    id<MTLDevice> device = (id<MTLDevice>)state->device;
    id<MTLCommandQueue> queue = [device newCommandQueue];
    if (queue == nil) {
      return nil;
    }
    state->command_queue = (void*)queue;
  }
  return (id<MTLCommandQueue>)state->command_queue;
}
#else
static void fz_gpu_runtime_init(void) {
  fz_gpu_device_count_cached = 0;
}
#endif

static int32_t fz_gpu_buffer_alloc_slot(void) {
  for (int i = 0; i < FZ_MAX_GPU_BUFFERS; i++) {
    if (!fz_gpu_buffers[i].in_use) {
      memset(&fz_gpu_buffers[i], 0, sizeof(fz_gpu_buffers[i]));
      fz_gpu_buffers[i].in_use = 1;
      return i + 1;
    }
  }
  return 0;
}

static int32_t fz_gpu_pipeline_alloc_slot(void) {
  for (int i = 0; i < FZ_MAX_GPU_PIPELINES; i++) {
    if (!fz_gpu_pipelines[i].in_use) {
      memset(&fz_gpu_pipelines[i], 0, sizeof(fz_gpu_pipelines[i]));
      fz_gpu_pipelines[i].in_use = 1;
      return i + 1;
    }
  }
  return 0;
}

static int32_t fz_gpu_pipeline_evict_lru_slot(void) {
  int32_t slot = 0;
  uint64_t best_epoch = UINT64_MAX;
  for (int i = 0; i < FZ_MAX_GPU_PIPELINES; i++) {
    fz_gpu_pipeline_state* state = &fz_gpu_pipelines[i];
    if (state->in_use && state->pipeline != NULL && state->last_used_epoch < best_epoch) {
      best_epoch = state->last_used_epoch;
      slot = i + 1;
    }
  }
  if (slot <= 0) {
    return 0;
  }
#if defined(__APPLE__) && defined(__OBJC__)
  id<MTLComputePipelineState> pipeline = (id<MTLComputePipelineState>)fz_gpu_pipelines[slot - 1].pipeline;
  if (pipeline != nil) {
    [pipeline release];
  }
#endif
  memset(&fz_gpu_pipelines[slot - 1], 0, sizeof(fz_gpu_pipelines[slot - 1]));
  fz_gpu_pipelines[slot - 1].in_use = 1;
  return slot;
}

static int32_t fz_gpu_event_alloc_slot(void) {
  for (int i = 0; i < FZ_MAX_GPU_EVENTS; i++) {
    if (!fz_gpu_events[i].in_use) {
      memset(&fz_gpu_events[i], 0, sizeof(fz_gpu_events[i]));
      fz_gpu_events[i].in_use = 1;
      return i + 1;
    }
  }
  return 0;
}

int32_t fz_native_gpu_device_count(void) {
  pthread_once(&fz_gpu_init_once, fz_gpu_runtime_init);
  if (fz_gpu_device_count_cached < 0) {
    return 0;
  }
  return fz_gpu_device_count_cached;
}

int32_t fz_native_gpu_default_device(void) {
  pthread_once(&fz_gpu_init_once, fz_gpu_runtime_init);
  if (fz_gpu_device_count_cached <= 0) {
    fz_set_last_error(ENODEV, 3, "gpu.default_device failed: no Metal device available");
    return 0;
  }
  fz_set_last_error(0, 0, "");
  return 1;
}

int32_t fz_native_gpu_device_name(int32_t device_handle) {
  pthread_once(&fz_gpu_init_once, fz_gpu_runtime_init);
#if defined(__APPLE__) && defined(__OBJC__)
  @autoreleasepool {
    id<MTLDevice> device = fz_gpu_device_for_handle(device_handle);
    if (device == nil) {
      fz_set_last_error(EINVAL, 3, "gpu.device_name failed: invalid device handle");
      return 0;
    }
    NSString* name = [device name];
    const char* utf8 = name == nil ? "" : [name UTF8String];
    fz_set_last_error(0, 0, "");
    return fz_intern_slice(utf8 == NULL ? "" : utf8, utf8 == NULL ? 0 : strlen(utf8));
  }
#else
  (void)device_handle;
  fz_set_last_error(ENOTSUP, 3, "gpu.device_name failed: Metal runtime unavailable on this host");
  return 0;
#endif
}

int64_t fz_native_gpu_device_memory_bytes(int32_t device_handle) {
  pthread_once(&fz_gpu_init_once, fz_gpu_runtime_init);
#if defined(__APPLE__) && defined(__OBJC__)
  @autoreleasepool {
    id<MTLDevice> device = fz_gpu_device_for_handle(device_handle);
    if (device == nil) {
      fz_set_last_error(EINVAL, 3, "gpu.device_memory_bytes failed: invalid device handle");
      return 0;
    }
    uint64_t bytes = 0;
    if ([device respondsToSelector:@selector(recommendedMaxWorkingSetSize)]) {
      bytes = [device recommendedMaxWorkingSetSize];
    }
    fz_set_last_error(0, 0, "");
    return (int64_t)bytes;
  }
#else
  (void)device_handle;
  fz_set_last_error(ENOTSUP, 3, "gpu.device_memory_bytes failed: Metal runtime unavailable on this host");
  return 0;
#endif
}

static int32_t fz_native_gpu_buffer_new(
    int32_t device_handle,
    int32_t len,
    int32_t element_size,
    const char* context,
    const char* alloc_failure_context,
    const void* host_data
) {
  pthread_once(&fz_gpu_init_once, fz_gpu_runtime_init);
  if (len < 0) {
    fz_set_last_error(EINVAL, 3, context);
    return 0;
  }
#if defined(__APPLE__) && defined(__OBJC__)
  @autoreleasepool {
    id<MTLDevice> device = fz_gpu_device_for_handle(device_handle);
    if (device == nil) {
      fz_set_last_error(EINVAL, 3, "gpu.alloc failed: invalid device handle");
      return 0;
    }
    NSUInteger bytes = (NSUInteger)((uint64_t)(uint32_t)len * (uint64_t)(uint32_t)element_size);
    id<MTLBuffer> buffer = [device newBufferWithLength:bytes options:MTLResourceStorageModeShared];
    if (buffer == nil) {
      fz_set_last_error(ENOMEM, 3, alloc_failure_context);
      return 0;
    }
    if (bytes > 0 && host_data != NULL) {
      void* contents = [buffer contents];
      if (contents == NULL) {
        [buffer release];
        fz_set_last_error(EIO, 3, "gpu.upload failed: Metal buffer has no CPU-visible contents");
        return 0;
      }
      memcpy(contents, host_data, bytes);
    }
    pthread_mutex_lock(&fz_gpu_lock);
    int32_t handle = fz_gpu_buffer_alloc_slot();
    if (handle > 0) {
      fz_gpu_buffer_state* state = &fz_gpu_buffers[handle - 1];
      state->device_handle = device_handle;
      state->element_size = element_size;
      state->len = len;
      state->buffer = (void*)buffer;
    }
    pthread_mutex_unlock(&fz_gpu_lock);
    if (handle <= 0) {
      [buffer release];
      fz_set_last_error(ENOSPC, 3, "gpu.alloc failed: GPU buffer registry full");
      return 0;
    }
    fz_set_last_error(0, 0, "");
    return handle;
  }
#else
  (void)device_handle;
  (void)element_size;
  (void)host_data;
  fz_set_last_error(ENOTSUP, 3, alloc_failure_context);
  return 0;
#endif
}

int32_t fz_native_gpu_alloc_f32(int32_t device_handle, int32_t len) {
  return fz_native_gpu_buffer_new(
      device_handle,
      len,
      4,
      "gpu.alloc_f32 failed: len must be >= 0",
      "gpu.alloc failed: Metal buffer allocation returned nil",
      NULL);
}

int32_t fz_native_gpu_alloc_i32(int32_t device_handle, int32_t len) {
  return fz_native_gpu_buffer_new(
      device_handle,
      len,
      4,
      "gpu.alloc_i32 failed: len must be >= 0",
      "gpu.alloc failed: Metal buffer allocation returned nil",
      NULL);
}

int32_t fz_native_gpu_alloc_u32(int32_t device_handle, int32_t len) {
  return fz_native_gpu_buffer_new(
      device_handle,
      len,
      4,
      "gpu.alloc_u32 failed: len must be >= 0",
      "gpu.alloc failed: Metal buffer allocation returned nil",
      NULL);
}

static int32_t fz_native_gpu_upload_bytes(
    int32_t device_handle,
    const void* host_data,
    int32_t len,
    int32_t element_size,
    const char* len_context
) {
  if (len < 0) {
    fz_set_last_error(EINVAL, 3, len_context);
    return 0;
  }
  if (len > 0 && host_data == NULL) {
    fz_set_last_error(EINVAL, 3, "gpu.upload failed: host data pointer was null");
    return 0;
  }
  return fz_native_gpu_buffer_new(
      device_handle,
      len,
      element_size,
      len_context,
      "gpu.upload failed: Metal buffer allocation returned nil",
      host_data);
}

int32_t fz_native_gpu_upload_f32(int32_t device_handle, uintptr_t host_data, int32_t len) {
  return fz_native_gpu_upload_bytes(
      device_handle,
      (const void*)host_data,
      len,
      4,
      "gpu.upload_f32 failed: len must be >= 0");
}

int32_t fz_native_gpu_upload_i32(int32_t device_handle, uintptr_t host_data, int32_t len) {
  return fz_native_gpu_upload_bytes(
      device_handle,
      (const void*)host_data,
      len,
      4,
      "gpu.upload_i32 failed: len must be >= 0");
}

int32_t fz_native_gpu_upload_u32(int32_t device_handle, uintptr_t host_data, int32_t len) {
  return fz_native_gpu_upload_bytes(
      device_handle,
      (const void*)host_data,
      len,
      4,
      "gpu.upload_u32 failed: len must be >= 0");
}

static uintptr_t fz_native_gpu_download_bytes(
    int32_t buffer_handle,
    int32_t expected_element_size,
    int32_t element_kind,
    const char* invalid_buffer_context,
    const char* element_mismatch_context
) {
  pthread_once(&fz_gpu_init_once, fz_gpu_runtime_init);
  pthread_mutex_lock(&fz_gpu_lock);
  if (buffer_handle <= 0 || buffer_handle > FZ_MAX_GPU_BUFFERS) {
    pthread_mutex_unlock(&fz_gpu_lock);
    fz_set_last_error(EINVAL, 3, invalid_buffer_context);
    return 0;
  }
  fz_gpu_buffer_state* state = &fz_gpu_buffers[buffer_handle - 1];
  if (!state->in_use || state->buffer == NULL) {
    pthread_mutex_unlock(&fz_gpu_lock);
    fz_set_last_error(EINVAL, 3, invalid_buffer_context);
    return 0;
  }
  if (state->element_size != expected_element_size) {
    pthread_mutex_unlock(&fz_gpu_lock);
    fz_set_last_error(EINVAL, 3, element_mismatch_context);
    return 0;
  }
  int32_t len = state->len;
#if defined(__APPLE__) && defined(__OBJC__)
  id<MTLBuffer> buffer = (id<MTLBuffer>)state->buffer;
  [buffer retain];
  pthread_mutex_unlock(&fz_gpu_lock);
  void* contents = [buffer contents];
  if (len > 0 && contents == NULL) {
    [buffer release];
    fz_set_last_error(EIO, 3, "gpu.download failed: Metal buffer has no CPU-visible contents");
    return 0;
  }
  pthread_mutex_lock(&fz_collections_lock);
  int32_t handle = fz_numeric_vec_alloc();
  if (handle > 0) {
    fz_numeric_vec_state* vec = fz_numeric_vec_get((uintptr_t)handle);
    if (vec != NULL) {
      vec->element_kind = element_kind;
      uint32_t* words = (uint32_t*)contents;
      for (int32_t index = 0; index < len; index++) {
        if (fz_numeric_vec_push_bits32(vec, words[index]) != 0) {
          handle = -1;
          vec->in_use = 0;
          break;
        }
      }
    } else {
      handle = -1;
    }
  }
  pthread_mutex_unlock(&fz_collections_lock);
  [buffer release];
  if (handle <= 0) {
    fz_set_last_error(ENOSPC, 3, "gpu.download failed: numeric vector registry full");
    return 0;
  }
  fz_set_last_error(0, 0, "");
  return (uintptr_t)handle;
#else
  pthread_mutex_unlock(&fz_gpu_lock);
  fz_set_last_error(ENOTSUP, 3, "gpu.download failed: Metal runtime unavailable on this host");
  return 0;
#endif
}

uintptr_t fz_native_gpu_download_f32(int32_t buffer_handle) {
  return fz_native_gpu_download_bytes(
      buffer_handle,
      4,
      1,
      "gpu.download_f32 failed: invalid GPU buffer handle",
      "gpu.download_f32 failed: buffer element type did not match f32");
}

uintptr_t fz_native_gpu_download_i32(int32_t buffer_handle) {
  return fz_native_gpu_download_bytes(
      buffer_handle,
      4,
      2,
      "gpu.download_i32 failed: invalid GPU buffer handle",
      "gpu.download_i32 failed: buffer element type did not match i32");
}

uintptr_t fz_native_gpu_download_u32(int32_t buffer_handle) {
  return fz_native_gpu_download_bytes(
      buffer_handle,
      4,
      3,
      "gpu.download_u32 failed: invalid GPU buffer handle",
      "gpu.download_u32 failed: buffer element type did not match u32");
}

int32_t fz_native_gpu_buffer_free(int32_t buffer_handle) {
  pthread_once(&fz_gpu_init_once, fz_gpu_runtime_init);
  if (buffer_handle <= 0 || buffer_handle > FZ_MAX_GPU_BUFFERS) {
    fz_set_last_error(EINVAL, 3, "gpu.free failed: invalid GPU buffer handle");
    return -1;
  }
  pthread_mutex_lock(&fz_gpu_lock);
  fz_gpu_buffer_state* state = &fz_gpu_buffers[buffer_handle - 1];
  if (!state->in_use || state->buffer == NULL) {
    pthread_mutex_unlock(&fz_gpu_lock);
    fz_set_last_error(EINVAL, 3, "gpu.free failed: invalid GPU buffer handle");
    return -1;
  }
#if defined(__APPLE__) && defined(__OBJC__)
  id<MTLBuffer> buffer = (id<MTLBuffer>)state->buffer;
  [buffer release];
#endif
  memset(state, 0, sizeof(*state));
  pthread_mutex_unlock(&fz_gpu_lock);
  fz_set_last_error(0, 0, "");
  return 0;
}

uint64_t fz_native_gpu_slice(int32_t buffer_handle, int32_t offset, int32_t len) {
  pthread_once(&fz_gpu_init_once, fz_gpu_runtime_init);
  if (offset < 0 || len < 0) {
    fz_set_last_error(EINVAL, 3, "gpu.slice failed: offset/len must be >= 0");
    return 0;
  }
  pthread_mutex_lock(&fz_gpu_lock);
  if (buffer_handle <= 0 || buffer_handle > FZ_MAX_GPU_BUFFERS) {
    pthread_mutex_unlock(&fz_gpu_lock);
    fz_set_last_error(EINVAL, 3, "gpu.slice failed: invalid GPU buffer handle");
    return 0;
  }
  fz_gpu_buffer_state* state = &fz_gpu_buffers[buffer_handle - 1];
  if (!state->in_use || state->buffer == NULL) {
    pthread_mutex_unlock(&fz_gpu_lock);
    fz_set_last_error(EINVAL, 3, "gpu.slice failed: invalid GPU buffer handle");
    return 0;
  }
  if (offset > state->len || len > state->len - offset) {
    pthread_mutex_unlock(&fz_gpu_lock);
    fz_set_last_error(EINVAL, 3, "gpu.slice failed: slice range exceeds GPU buffer length");
    return 0;
  }
  pthread_mutex_unlock(&fz_gpu_lock);

  uint64_t handle = fz_native_agg_new(FZ_GPU_SLICE_TAG, 3);
  if (handle == 0) {
    fz_set_last_error(ENOSPC, 3, "gpu.slice failed: GPU slice registry full");
    return 0;
  }
  if (fz_native_agg_set_i64(handle, 0, (uint64_t)(uint32_t)buffer_handle) != 0
      || fz_native_agg_set_i64(handle, 1, (uint64_t)(uint32_t)offset) != 0
      || fz_native_agg_set_i64(handle, 2, (uint64_t)(uint32_t)len) != 0) {
    fz_set_last_error(EINVAL, 3, "gpu.slice failed: could not materialize slice handle");
    return 0;
  }
  fz_set_last_error(0, 0, "");
  return handle;
}

static int fz_gpu_slice_unpack(uintptr_t slice_handle, int32_t* out_buffer, int32_t* out_offset, int32_t* out_len) {
  if (slice_handle == 0 || fz_native_agg_tag((uint64_t)slice_handle) != FZ_GPU_SLICE_TAG) {
    return -1;
  }
  *out_buffer = (int32_t)fz_native_agg_get_i64((uint64_t)slice_handle, 0);
  *out_offset = (int32_t)fz_native_agg_get_i64((uint64_t)slice_handle, 1);
  *out_len = (int32_t)fz_native_agg_get_i64((uint64_t)slice_handle, 2);
  return 0;
}

#if defined(__APPLE__) && defined(__OBJC__)
static id<MTLComputePipelineState> fz_gpu_pipeline_for_kernel(
    int32_t device_handle,
    int32_t source_id,
    int32_t kernel_name_id
) {
  for (int i = 0; i < FZ_MAX_GPU_PIPELINES; i++) {
    fz_gpu_pipeline_state* state = &fz_gpu_pipelines[i];
    if (state->in_use
        && state->device_handle == device_handle
        && state->source_id == source_id
        && state->kernel_name_id == kernel_name_id
        && state->pipeline != NULL) {
      state->last_used_epoch = ++fz_gpu_pipeline_epoch;
      return (id<MTLComputePipelineState>)state->pipeline;
    }
  }
  id<MTLDevice> device = fz_gpu_device_for_handle(device_handle);
  if (device == nil) {
    fz_set_last_error(EINVAL, 3, "gpu.launch failed: invalid device handle");
    return nil;
  }
  const char* source_c = (const char*)fz_native_str_ptr(source_id);
  const char* kernel_c = (const char*)fz_native_str_ptr(kernel_name_id);
  if (source_c == NULL || kernel_c == NULL || source_c[0] == '\0' || kernel_c[0] == '\0') {
    fz_set_last_error(EINVAL, 3, "gpu.launch failed: missing kernel source or name");
    return nil;
  }
  NSString* source = [NSString stringWithUTF8String:source_c];
  NSString* kernel = [NSString stringWithUTF8String:kernel_c];
  if (source == nil || kernel == nil) {
    fz_set_last_error(EINVAL, 3, "gpu.launch failed: could not decode kernel source or name");
    return nil;
  }
  NSError* error = nil;
  id<MTLLibrary> library = [device newLibraryWithSource:source options:nil error:&error];
  if (library == nil) {
    const char* detail = error == nil ? "unknown Metal library compile error" : [[error localizedDescription] UTF8String];
    fz_set_last_error(EINVAL, 3, detail == NULL ? "gpu.launch failed: Metal library compile failed" : detail);
    return nil;
  }
  id<MTLFunction> function = [library newFunctionWithName:kernel];
  if (function == nil) {
    [library release];
    fz_set_last_error(EINVAL, 3, "gpu.launch failed: Metal kernel entry was not found");
    return nil;
  }
  id<MTLComputePipelineState> pipeline = [device newComputePipelineStateWithFunction:function error:&error];
  [function release];
  [library release];
  if (pipeline == nil) {
    const char* detail = error == nil ? "unknown Metal pipeline compile error" : [[error localizedDescription] UTF8String];
    fz_set_last_error(EINVAL, 3, detail == NULL ? "gpu.launch failed: Metal pipeline creation failed" : detail);
    return nil;
  }
  int32_t slot = fz_gpu_pipeline_alloc_slot();
  if (slot <= 0) {
    slot = fz_gpu_pipeline_evict_lru_slot();
    if (slot <= 0) {
      [pipeline release];
      fz_set_last_error(ENOSPC, 3, "gpu.launch failed: GPU pipeline registry full and no reusable slot was available");
      return nil;
    }
  }
  fz_gpu_pipeline_state* state = &fz_gpu_pipelines[slot - 1];
  state->device_handle = device_handle;
  state->source_id = source_id;
  state->kernel_name_id = kernel_name_id;
  state->last_used_epoch = ++fz_gpu_pipeline_epoch;
  state->pipeline = (void*)pipeline;
  return pipeline;
}

static int32_t fz_gpu_launch_impl(
    int32_t kernel_name_id,
    int32_t source_id,
    int32_t layout_id,
    int32_t grid,
    int32_t block,
    int argc,
    uintptr_t a0,
    uintptr_t a1,
    uintptr_t a2,
    uintptr_t a3
) {
  pthread_once(&fz_gpu_init_once, fz_gpu_runtime_init);
  if (grid <= 0 || block <= 0) {
    fz_set_last_error(EINVAL, 3, "gpu.launch failed: grid and block must be > 0");
    return 0;
  }
  const char* layout = (const char*)fz_native_str_ptr(layout_id);
  if (layout == NULL || layout[0] == '\0') {
    fz_set_last_error(EINVAL, 3, "gpu.launch failed: missing launch layout");
    return 0;
  }
  uintptr_t raw_args[4] = {a0, a1, a2, a3};
  const char* token = layout;
  int32_t device_handle = 0;
  int32_t slice_buffers[4] = {0, 0, 0, 0};
  int32_t slice_offsets[4] = {0, 0, 0, 0};
  int32_t slice_lens[4] = {0, 0, 0, 0};
  int32_t slice_element_sizes[4] = {0, 0, 0, 0};
  int slice_count = 0;
  for (int arg = 0; arg < argc; arg++) {
    const char* next = strchr(token, ',');
    size_t token_len = next == NULL ? strlen(token) : (size_t)(next - token);
    if (token_len == 0) {
      fz_set_last_error(EINVAL, 3, "gpu.launch failed: malformed launch layout");
      return 0;
    }
    if (token_len >= 6 && strncmp(token, "slice_", 6) == 0) {
      if (slice_count >= 4) {
        fz_set_last_error(EINVAL, 3, "gpu.launch failed: more than 4 slice args are unsupported");
        return 0;
      }
      if (fz_gpu_slice_unpack(raw_args[arg], &slice_buffers[slice_count], &slice_offsets[slice_count], &slice_lens[slice_count]) != 0) {
        fz_set_last_error(EINVAL, 3, "gpu.launch failed: expected a GpuSlice launch argument");
        return 0;
      }
      if (slice_buffers[slice_count] <= 0 || slice_buffers[slice_count] > FZ_MAX_GPU_BUFFERS) {
        fz_set_last_error(EINVAL, 3, "gpu.launch failed: invalid slice buffer handle");
        return 0;
      }
      fz_gpu_buffer_state* buffer_state = &fz_gpu_buffers[slice_buffers[slice_count] - 1];
      if (!buffer_state->in_use || buffer_state->buffer == NULL) {
        fz_set_last_error(EINVAL, 3, "gpu.launch failed: invalid slice buffer handle");
        return 0;
      }
      slice_element_sizes[arg] = buffer_state->element_size;
      if (device_handle == 0) {
        device_handle = buffer_state->device_handle;
      } else if (device_handle != buffer_state->device_handle) {
        fz_set_last_error(EINVAL, 3, "gpu.launch failed: all GPU slices must target the same device");
        return 0;
      }
      slice_count++;
    }
    token = next == NULL ? token + token_len : next + 1;
  }
  if (device_handle == 0) {
    device_handle = fz_native_gpu_default_device();
    if (device_handle == 0) {
      return 0;
    }
  }
  @autoreleasepool {
    id<MTLCommandQueue> queue = fz_gpu_queue_for_device(device_handle);
    if (queue == nil) {
      fz_set_last_error(EIO, 3, "gpu.launch failed: could not allocate Metal command queue");
      return 0;
    }
    id<MTLComputePipelineState> pipeline = fz_gpu_pipeline_for_kernel(device_handle, source_id, kernel_name_id);
    if (pipeline == nil) {
      return 0;
    }
    if ((NSUInteger)block > [pipeline maxTotalThreadsPerThreadgroup]) {
      char detail[160];
      snprintf(
          detail,
          sizeof(detail),
          "gpu.launch failed: block %d exceeds Metal max threads-per-threadgroup %lu",
          block,
          (unsigned long)[pipeline maxTotalThreadsPerThreadgroup]);
      fz_set_last_error(EINVAL, 3, detail);
      return 0;
    }
    id<MTLCommandBuffer> command_buffer = [queue commandBuffer];
    if (command_buffer == nil) {
      fz_set_last_error(EIO, 3, "gpu.launch failed: could not create Metal command buffer");
      return 0;
    }
    id<MTLComputeCommandEncoder> encoder = [command_buffer computeCommandEncoder];
    if (encoder == nil) {
      fz_set_last_error(EIO, 3, "gpu.launch failed: could not create Metal compute encoder");
      return 0;
    }
    [encoder setComputePipelineState:pipeline];
    token = layout;
    NSUInteger buffer_index = 0;
    for (int arg = 0; arg < argc; arg++) {
      const char* next = strchr(token, ',');
      size_t token_len = next == NULL ? strlen(token) : (size_t)(next - token);
      if (token_len >= 6 && strncmp(token, "slice_", 6) == 0) {
        int32_t buffer_handle = 0;
        int32_t offset = 0;
        int32_t len = 0;
        if (fz_gpu_slice_unpack(raw_args[arg], &buffer_handle, &offset, &len) != 0) {
          [encoder endEncoding];
          fz_set_last_error(EINVAL, 3, "gpu.launch failed: expected a GpuSlice launch argument");
          return 0;
        }
        id<MTLBuffer> buffer = fz_gpu_buffer_for_handle(buffer_handle);
        if (buffer == nil) {
          [encoder endEncoding];
          fz_set_last_error(EINVAL, 3, "gpu.launch failed: invalid slice buffer handle");
          return 0;
        }
        NSUInteger byte_offset = (NSUInteger)((uint64_t)(uint32_t)offset * (uint64_t)(uint32_t)slice_element_sizes[arg]);
        uint32_t len_u = (uint32_t)len;
        [encoder setBuffer:buffer offset:byte_offset atIndex:buffer_index++];
        [encoder setBytes:&len_u length:sizeof(uint32_t) atIndex:buffer_index++];
      } else if (token_len == 3 && strncmp(token, "i32", 3) == 0) {
        int32_t value = (int32_t)raw_args[arg];
        [encoder setBytes:&value length:sizeof(int32_t) atIndex:buffer_index++];
      } else if (token_len == 3 && strncmp(token, "u32", 3) == 0) {
        uint32_t value = (uint32_t)raw_args[arg];
        [encoder setBytes:&value length:sizeof(uint32_t) atIndex:buffer_index++];
      } else if (token_len == 3 && strncmp(token, "f32", 3) == 0) {
        union { uint32_t bits; float value; } cast;
        cast.bits = (uint32_t)raw_args[arg];
        [encoder setBytes:&cast.value length:sizeof(float) atIndex:buffer_index++];
      } else {
        [encoder endEncoding];
        fz_set_last_error(EINVAL, 3, "gpu.launch failed: unsupported Metal launch param layout");
        return 0;
      }
      token = next == NULL ? token + token_len : next + 1;
    }
    MTLSize threads_per_threadgroup = MTLSizeMake((NSUInteger)block, 1, 1);
    MTLSize threadgroups_per_grid = MTLSizeMake((NSUInteger)grid, 1, 1);
    [encoder dispatchThreadgroups:threadgroups_per_grid threadsPerThreadgroup:threads_per_threadgroup];
    [encoder endEncoding];
    [command_buffer retain];
    [command_buffer commit];
    pthread_mutex_lock(&fz_gpu_lock);
    int32_t event_handle = fz_gpu_event_alloc_slot();
    if (event_handle > 0) {
      fz_gpu_event_state* event = &fz_gpu_events[event_handle - 1];
      event->device_handle = device_handle;
      event->command_buffer = (void*)command_buffer;
    }
    pthread_mutex_unlock(&fz_gpu_lock);
    if (event_handle <= 0) {
      [command_buffer release];
      fz_set_last_error(ENOSPC, 3, "gpu.launch failed: GPU event registry full");
      return 0;
    }
    fz_set_last_error(0, 0, "");
    return event_handle;
  }
}
#endif

int32_t fz_native_gpu_launch0(int32_t kernel_name_id, int32_t source_id, int32_t layout_id, int32_t grid, int32_t block) {
#if defined(__APPLE__) && defined(__OBJC__)
  return fz_gpu_launch_impl(kernel_name_id, source_id, layout_id, grid, block, 0, 0, 0, 0, 0);
#else
  (void)kernel_name_id; (void)source_id; (void)layout_id; (void)grid; (void)block;
  fz_set_last_error(ENOTSUP, 3, "gpu.launch failed: Metal runtime unavailable on this host");
  return 0;
#endif
}

int32_t fz_native_gpu_launch1(int32_t kernel_name_id, int32_t source_id, int32_t layout_id, int32_t grid, int32_t block, uintptr_t a0) {
#if defined(__APPLE__) && defined(__OBJC__)
  return fz_gpu_launch_impl(kernel_name_id, source_id, layout_id, grid, block, 1, a0, 0, 0, 0);
#else
  (void)kernel_name_id; (void)source_id; (void)layout_id; (void)grid; (void)block; (void)a0;
  fz_set_last_error(ENOTSUP, 3, "gpu.launch failed: Metal runtime unavailable on this host");
  return 0;
#endif
}

int32_t fz_native_gpu_launch2(int32_t kernel_name_id, int32_t source_id, int32_t layout_id, int32_t grid, int32_t block, uintptr_t a0, uintptr_t a1) {
#if defined(__APPLE__) && defined(__OBJC__)
  return fz_gpu_launch_impl(kernel_name_id, source_id, layout_id, grid, block, 2, a0, a1, 0, 0);
#else
  (void)kernel_name_id; (void)source_id; (void)layout_id; (void)grid; (void)block; (void)a0; (void)a1;
  fz_set_last_error(ENOTSUP, 3, "gpu.launch failed: Metal runtime unavailable on this host");
  return 0;
#endif
}

int32_t fz_native_gpu_launch3(int32_t kernel_name_id, int32_t source_id, int32_t layout_id, int32_t grid, int32_t block, uintptr_t a0, uintptr_t a1, uintptr_t a2) {
#if defined(__APPLE__) && defined(__OBJC__)
  return fz_gpu_launch_impl(kernel_name_id, source_id, layout_id, grid, block, 3, a0, a1, a2, 0);
#else
  (void)kernel_name_id; (void)source_id; (void)layout_id; (void)grid; (void)block; (void)a0; (void)a1; (void)a2;
  fz_set_last_error(ENOTSUP, 3, "gpu.launch failed: Metal runtime unavailable on this host");
  return 0;
#endif
}

int32_t fz_native_gpu_launch4(int32_t kernel_name_id, int32_t source_id, int32_t layout_id, int32_t grid, int32_t block, uintptr_t a0, uintptr_t a1, uintptr_t a2, uintptr_t a3) {
#if defined(__APPLE__) && defined(__OBJC__)
  return fz_gpu_launch_impl(kernel_name_id, source_id, layout_id, grid, block, 4, a0, a1, a2, a3);
#else
  (void)kernel_name_id; (void)source_id; (void)layout_id; (void)grid; (void)block; (void)a0; (void)a1; (void)a2; (void)a3;
  fz_set_last_error(ENOTSUP, 3, "gpu.launch failed: Metal runtime unavailable on this host");
  return 0;
#endif
}

int32_t fz_native_gpu_wait(int32_t event_handle) {
  pthread_once(&fz_gpu_init_once, fz_gpu_runtime_init);
  if (event_handle <= 0 || event_handle > FZ_MAX_GPU_EVENTS) {
    fz_set_last_error(EINVAL, 3, "gpu.wait failed: invalid GPU event handle");
    return -1;
  }
  pthread_mutex_lock(&fz_gpu_lock);
  fz_gpu_event_state* state = &fz_gpu_events[event_handle - 1];
  if (!state->in_use || state->command_buffer == NULL) {
    pthread_mutex_unlock(&fz_gpu_lock);
    fz_set_last_error(EINVAL, 3, "gpu.wait failed: invalid GPU event handle");
    return -1;
  }
#if defined(__APPLE__) && defined(__OBJC__)
  id<MTLCommandBuffer> command_buffer = (id<MTLCommandBuffer>)state->command_buffer;
  [command_buffer retain];
  memset(state, 0, sizeof(*state));
  pthread_mutex_unlock(&fz_gpu_lock);
  [command_buffer waitUntilCompleted];
  MTLCommandBufferStatus status = [command_buffer status];
  NSError* error = [command_buffer error];
  [command_buffer release];
  if (status == MTLCommandBufferStatusError) {
    const char* detail = error == nil ? "gpu.wait failed: Metal command buffer completed with error" : [[error localizedDescription] UTF8String];
    fz_set_last_error(EIO, 3, detail == NULL ? "gpu.wait failed: Metal command buffer completed with error" : detail);
    return -1;
  }
  fz_set_last_error(0, 0, "");
  return 0;
#else
  pthread_mutex_unlock(&fz_gpu_lock);
  fz_set_last_error(ENOTSUP, 3, "gpu.wait failed: Metal runtime unavailable on this host");
  return -1;
#endif
}

int32_t fz_native_gpu_wait_async(int32_t event_handle) {
  return fz_native_gpu_wait(event_handle);
}

static int32_t fz_native_str_concat_parts(const char** parts, int count) {
  if (count <= 0) {
    return fz_intern_slice("", 0);
  }
  size_t total = 1;
  for (int i = 0; i < count; i++) {
    const char* part = parts[i] == NULL ? "" : parts[i];
    total += strlen(part);
  }
  char* out = (char*)malloc(total);
  if (out == NULL) {
    return 0;
  }
  size_t used = 0;
  for (int i = 0; i < count; i++) {
    const char* part = parts[i] == NULL ? "" : parts[i];
    size_t len = strlen(part);
    if (len > 0) {
      memcpy(out + used, part, len);
      used += len;
    }
  }
  out[used] = '\0';
  return fz_intern_owned(out);
}

int32_t fz_native_str_concat2(int32_t a_id, int32_t b_id) {
  const char* parts[2] = {fz_lookup_string(a_id), fz_lookup_string(b_id)};
  return fz_native_str_concat_parts(parts, 2);
}

int32_t fz_native_str_concat3(int32_t a_id, int32_t b_id, int32_t c_id) {
  const char* parts[3] = {fz_lookup_string(a_id), fz_lookup_string(b_id), fz_lookup_string(c_id)};
  return fz_native_str_concat_parts(parts, 3);
}

int32_t fz_native_str_concat4(int32_t a_id, int32_t b_id, int32_t c_id, int32_t d_id) {
  const char* parts[4] = {
      fz_lookup_string(a_id), fz_lookup_string(b_id), fz_lookup_string(c_id), fz_lookup_string(d_id)};
  return fz_native_str_concat_parts(parts, 4);
}

int32_t fz_native_str_from_i32(int32_t value) {
  char rendered[32];
  snprintf(rendered, sizeof(rendered), "%d", value);
  return fz_intern_slice(rendered, strlen(rendered));
}

int32_t fz_native_str_from_bool(int32_t value) {
  const char* rendered = value == 0 ? "false" : "true";
  return fz_intern_slice(rendered, strlen(rendered));
}

int32_t fz_native_str_repeat(int32_t value_id, int32_t count) {
  const char* value = fz_lookup_string(value_id);
  if (value == NULL || count <= 0) {
    return fz_intern_slice("", 0);
  }
  size_t value_len = strlen(value);
  if (value_len == 0) {
    return fz_intern_slice("", 0);
  }
  size_t total = value_len * (size_t)count;
  char* out = (char*)malloc(total + 1);
  if (out == NULL) {
    return 0;
  }
  size_t used = 0;
  for (int32_t i = 0; i < count; i++) {
    memcpy(out + used, value, value_len);
    used += value_len;
  }
  out[used] = '\0';
  return fz_intern_owned(out);
}

int32_t fz_native_str_contains(int32_t value_id, int32_t needle_id) {
  const char* value = fz_lookup_string(value_id);
  const char* needle = fz_lookup_string(needle_id);
  if (value == NULL || needle == NULL) {
    return 0;
  }
  return strstr(value, needle) != NULL ? 1 : 0;
}

int32_t fz_native_str_starts_with(int32_t value_id, int32_t prefix_id) {
  const char* value = fz_lookup_string(value_id);
  const char* prefix = fz_lookup_string(prefix_id);
  if (value == NULL || prefix == NULL) {
    return 0;
  }
  size_t prefix_len = strlen(prefix);
  return strncmp(value, prefix, prefix_len) == 0 ? 1 : 0;
}

int32_t fz_native_str_ends_with(int32_t value_id, int32_t suffix_id) {
  const char* value = fz_lookup_string(value_id);
  const char* suffix = fz_lookup_string(suffix_id);
  if (value == NULL || suffix == NULL) {
    return 0;
  }
  size_t value_len = strlen(value);
  size_t suffix_len = strlen(suffix);
  if (suffix_len > value_len) {
    return 0;
  }
  return memcmp(value + (value_len - suffix_len), suffix, suffix_len) == 0 ? 1 : 0;
}

int32_t fz_native_str_trim(int32_t value_id) {
  const char* value = fz_lookup_string(value_id);
  if (value == NULL) {
    return fz_intern_slice("", 0);
  }
  const unsigned char* start = (const unsigned char*)value;
  while (*start != '\0' && isspace(*start)) {
    start++;
  }
  const unsigned char* end = start + strlen((const char*)start);
  while (end > start && isspace(*(end - 1))) {
    end--;
  }
  return fz_intern_slice((const char*)start, (size_t)(end - start));
}

int32_t fz_native_str_replace(int32_t value_id, int32_t from_id, int32_t to_id) {
  const char* value = fz_lookup_string(value_id);
  const char* from = fz_lookup_string(from_id);
  const char* to = fz_lookup_string(to_id);
  if (value == NULL) value = "";
  if (from == NULL) from = "";
  if (to == NULL) to = "";

  size_t value_len = strlen(value);
  size_t from_len = strlen(from);
  size_t to_len = strlen(to);
  if (from_len == 0) {
    return fz_intern_slice(value, value_len);
  }

  size_t occurrences = 0;
  const char* cursor = value;
  while ((cursor = strstr(cursor, from)) != NULL) {
    occurrences++;
    cursor += from_len;
  }
  if (occurrences == 0) {
    return fz_intern_slice(value, value_len);
  }
  size_t out_len = value_len;
  if (to_len >= from_len) {
    out_len += occurrences * (to_len - from_len);
  } else {
    out_len -= occurrences * (from_len - to_len);
  }
  char* out = (char*)malloc(out_len + 1);
  if (out == NULL) {
    return 0;
  }
  const char* src = value;
  char* dst = out;
  while (1) {
    const char* hit = strstr(src, from);
    if (hit == NULL) {
      size_t tail = strlen(src);
      memcpy(dst, src, tail);
      dst += tail;
      break;
    }
    size_t prefix = (size_t)(hit - src);
    memcpy(dst, src, prefix);
    dst += prefix;
    if (to_len > 0) {
      memcpy(dst, to, to_len);
      dst += to_len;
    }
    src = hit + from_len;
  }
  *dst = '\0';
  return fz_intern_owned(out);
}

int32_t fz_native_str_len(int32_t value_id) {
  const char* value = fz_lookup_string(value_id);
  return value == NULL ? 0 : (int32_t)strlen(value);
}

int32_t fz_native_str_visible_len_ansi(int32_t value_id) {
  const unsigned char* value = (const unsigned char*)fz_lookup_string(value_id);
  if (value == NULL) {
    return 0;
  }
  int32_t visible = 0;
  size_t index = 0;
  while (value[index] != '\0') {
    if (value[index] == 0x1b && value[index + 1] == '[') {
      index += 2;
      while (value[index] != '\0' && value[index] != 'm') {
        index++;
      }
      if (value[index] == 'm') {
        index++;
      }
      continue;
    }
    visible++;
    index++;
  }
  return visible;
}

int32_t fz_native_str_slice(int32_t value_id, int32_t start, int32_t end_exclusive) {
  const char* value = fz_lookup_string(value_id);
  if (value == NULL) {
    return fz_intern_slice("", 0);
  }
  int32_t value_len = (int32_t)strlen(value);
  int32_t begin = start < 0 ? 0 : start;
  if (begin > value_len) {
    begin = value_len;
  }
  int32_t end = end_exclusive < 0 ? 0 : end_exclusive;
  if (end > value_len) {
    end = value_len;
  }
  if (end <= begin) {
    return fz_intern_slice("", 0);
  }
  return fz_intern_slice(value + begin, (size_t)(end - begin));
}

int32_t fz_native_str_upper_ascii(int32_t value_id) {
  const char* value = fz_lookup_string(value_id);
  if (value == NULL) {
    return fz_intern_slice("", 0);
  }
  size_t len = strlen(value);
  char* out = (char*)malloc(len + 1);
  if (out == NULL) {
    return 0;
  }
  for (size_t i = 0; i < len; i++) {
    unsigned char ch = (unsigned char)value[i];
    out[i] = (char)(ch >= 'a' && ch <= 'z' ? (ch - ('a' - 'A')) : ch);
  }
  out[len] = '\0';
  return fz_intern_owned(out);
}

int32_t fz_native_str_lower_ascii(int32_t value_id) {
  const char* value = fz_lookup_string(value_id);
  if (value == NULL) {
    return fz_intern_slice("", 0);
  }
  size_t len = strlen(value);
  char* out = (char*)malloc(len + 1);
  if (out == NULL) {
    return 0;
  }
  for (size_t i = 0; i < len; i++) {
    unsigned char ch = (unsigned char)value[i];
    out[i] = (char)(ch >= 'A' && ch <= 'Z' ? (ch + ('a' - 'A')) : ch);
  }
  out[len] = '\0';
  return fz_intern_owned(out);
}

int32_t fz_native_str_split(int32_t value_id, int32_t sep_id) {
  const char* value = fz_lookup_string(value_id);
  const char* sep = fz_lookup_string(sep_id);
  if (value == NULL) value = "";
  if (sep == NULL) sep = "";
  int32_t list = fz_runtime_list_new();
  if (list < 0) {
    return -1;
  }
  size_t sep_len = strlen(sep);
  if (sep_len == 0) {
    (void)fz_runtime_list_push(list, fz_intern_slice(value, strlen(value)));
    return list;
  }
  const char* cursor = value;
  while (1) {
    const char* hit = strstr(cursor, sep);
    if (hit == NULL) {
      (void)fz_runtime_list_push(list, fz_intern_slice(cursor, strlen(cursor)));
      break;
    }
    (void)fz_runtime_list_push(list, fz_intern_slice(cursor, (size_t)(hit - cursor)));
    cursor = hit + sep_len;
  }
  return list;
}

int32_t fz_native_list_new(void) { return fz_runtime_list_new(); }
int32_t fz_native_list_push(int32_t handle, int32_t value_id) {
  return fz_runtime_list_push(handle, value_id);
}
int32_t fz_native_list_pop(int32_t handle) { return fz_runtime_list_pop(handle); }
int32_t fz_native_list_len(int32_t handle) { return fz_runtime_list_len(handle); }
int32_t fz_native_list_get(int32_t handle, int32_t index) {
  return fz_runtime_list_get(handle, index);
}
int32_t fz_native_list_set(int32_t handle, int32_t index, int32_t value_id) {
  return fz_runtime_list_set(handle, index, value_id);
}
int32_t fz_native_list_clear(int32_t handle) { return fz_runtime_list_clear(handle); }
int32_t fz_native_list_join(int32_t handle, int32_t sep_id) {
  return fz_runtime_list_join(handle, sep_id);
}

int32_t fz_native_map_new(void) { return fz_runtime_map_new(); }
int32_t fz_native_map_set(int32_t handle, int32_t key_id, int32_t value_id) {
  return fz_runtime_map_set(handle, key_id, value_id);
}
int32_t fz_native_map_get(int32_t handle, int32_t key_id) {
  return fz_runtime_map_get(handle, key_id);
}
int32_t fz_native_map_has(int32_t handle, int32_t key_id) {
  return fz_runtime_map_has(handle, key_id);
}
int32_t fz_native_map_delete(int32_t handle, int32_t key_id) {
  return fz_runtime_map_delete(handle, key_id);
}
int32_t fz_native_map_keys(int32_t handle) { return fz_runtime_map_keys(handle); }
int32_t fz_native_map_len(int32_t handle) { return fz_runtime_map_len(handle); }

static int fz_http_header_key_matches(const char* header_line, const char* key) {
  if (header_line == NULL || key == NULL) {
    return 0;
  }
  const char* colon = strchr(header_line, ':');
  if (colon == NULL) {
    return 0;
  }
  size_t key_len = strlen(key);
  size_t header_key_len = (size_t)(colon - header_line);
  while (header_key_len > 0 && isspace((unsigned char)header_line[header_key_len - 1])) {
    header_key_len--;
  }
  return header_key_len == key_len && strncasecmp(header_line, key, key_len) == 0;
}

static int fz_http_header_upsert(char** header_buf, int* header_count, const char* key, const char* value) {
  if (header_buf == NULL || header_count == NULL || key == NULL || key[0] == '\0') {
    return -1;
  }
  if (value == NULL) {
    value = "";
  }
  size_t n = strlen(key) + strlen(value) + 3;
  char* kv = (char*)malloc(n);
  if (kv == NULL) {
    return -1;
  }
  snprintf(kv, n, "%s: %s", key, value);
  for (int i = 0; i < *header_count; i++) {
    if (fz_http_header_key_matches(header_buf[i], key)) {
      free(header_buf[i]);
      header_buf[i] = kv;
      return 0;
    }
  }
  if (*header_count >= FZ_MAX_HTTP_HEADERS) {
    free(kv);
    return -1;
  }
  header_buf[*header_count] = kv;
  (*header_count)++;
  return 0;
}

int32_t fz_native_json_from_list(int32_t list_handle) {
  pthread_mutex_lock(&fz_list_lock);
  fz_list_state* list = fz_list_get(list_handle);
  if (list == NULL) {
    pthread_mutex_unlock(&fz_list_lock);
    return fz_intern_slice("[]", 2);
  }
  size_t total = 3;
  for (int i = 0; i < list->count; i++) {
    const char* raw = list->items[i];
    total += strlen(raw == NULL || raw[0] == '\0' ? "null" : raw) + 1;
  }
  char* out = (char*)malloc(total);
  if (out == NULL) {
    pthread_mutex_unlock(&fz_list_lock);
    return 0;
  }
  size_t used = 0;
  out[used++] = '[';
  for (int i = 0; i < list->count; i++) {
    if (i > 0) out[used++] = ',';
    const char* raw = list->items[i];
    if (raw == NULL || raw[0] == '\0') {
      raw = "null";
    }
    size_t n = strlen(raw);
    memcpy(out + used, raw, n);
    used += n;
  }
  out[used++] = ']';
  out[used] = '\0';
  pthread_mutex_unlock(&fz_list_lock);
  return fz_intern_owned(out);
}

int32_t fz_native_json_from_map(int32_t map_handle) {
  pthread_mutex_lock(&fz_map_lock);
  fz_map_state* map = fz_map_get(map_handle);
  if (map == NULL) {
    pthread_mutex_unlock(&fz_map_lock);
    return fz_intern_slice("{}", 2);
  }
  size_t total = 3;
  for (int i = 0; i < map->count; i++) {
    char* k = fz_json_escape_owned(map->keys[i] == NULL ? "" : map->keys[i]);
    const char* raw = map->values[i];
    if (k != NULL) {
      total += strlen(k) + strlen(raw == NULL || raw[0] == '\0' ? "null" : raw) + 5;
    }
    free(k);
  }
  char* out = (char*)malloc(total);
  if (out == NULL) {
    pthread_mutex_unlock(&fz_map_lock);
    return 0;
  }
  size_t used = 0;
  out[used++] = '{';
  for (int i = 0; i < map->count; i++) {
    if (i > 0) out[used++] = ',';
    char* k = fz_json_escape_owned(map->keys[i] == NULL ? "" : map->keys[i]);
    if (k == NULL) {
      free(k);
      out[used++] = '\"';
      out[used++] = '\"';
      out[used++] = ':';
      memcpy(out + used, "null", 4);
      used += 4;
      continue;
    }
    out[used++] = '\"';
    size_t kn = strlen(k);
    memcpy(out + used, k, kn);
    used += kn;
    out[used++] = '\"';
    out[used++] = ':';
    const char* raw = map->values[i];
    if (raw == NULL || raw[0] == '\0') {
      raw = "null";
    }
    size_t vn = strlen(raw);
    memcpy(out + used, raw, vn);
    used += vn;
    free(k);
  }
  out[used++] = '}';
  out[used] = '\0';
  pthread_mutex_unlock(&fz_map_lock);
  return fz_intern_owned(out);
}

int32_t fz_native_json_to_list(int32_t json_id) {
  int32_t value_id = fz_json_value_get_id(json_id);
  const char* raw = fz_lookup_string(value_id);
  char** items = NULL;
  int count = 0;
  if (fz_parse_json_string_array(raw, &items, &count) != 0) {
    return -1;
  }
  pthread_mutex_lock(&fz_list_lock);
  int32_t handle = fz_list_alloc();
  fz_list_state* list = fz_list_get(handle);
  if (list != NULL) {
    for (int i = 0; i < count; i++) {
      (void)fz_list_push_cstr(list, items[i] == NULL ? "" : items[i]);
    }
  }
  pthread_mutex_unlock(&fz_list_lock);
  fz_free_string_list(items, count);
  return handle;
}

int32_t fz_native_json_to_map(int32_t json_id) {
  int32_t value_id = fz_json_value_get_id(json_id);
  const char* raw = fz_lookup_string(value_id);
  char** pairs = NULL;
  int count = 0;
  if (fz_parse_json_env_object(raw, &pairs, &count) != 0) {
    return -1;
  }
  pthread_mutex_lock(&fz_map_lock);
  int32_t handle = fz_map_alloc();
  fz_map_state* map = fz_map_get(handle);
  if (map != NULL) {
    for (int i = 0; i < count && map->count < FZ_MAX_MAP_ENTRIES; i++) {
      char* eq = strchr(pairs[i], '=');
      if (eq == NULL) continue;
      *eq = '\0';
      map->keys[map->count] = strdup(pairs[i]);
      map->values[map->count] = strdup(eq + 1);
      if (map->keys[map->count] != NULL && map->values[map->count] != NULL) {
        map->count++;
      }
    }
  }
  pthread_mutex_unlock(&fz_map_lock);
  fz_free_string_list(pairs, count);
  return handle;
}

int32_t fz_native_json_keys(int32_t json_value_handle) {
  int32_t value_id = fz_json_value_get_id(json_value_handle);
  const char* raw = fz_lookup_string(value_id);
  if (raw == NULL) {
    return -1;
  }
  const char* p = fz_json_ws(raw);
  if (p == NULL || *p != '{') {
    return -1;
  }
  pthread_mutex_lock(&fz_list_lock);
  int32_t handle = fz_list_alloc();
  fz_list_state* list = fz_list_get(handle);
  pthread_mutex_unlock(&fz_list_lock);
  if (list == NULL) {
    return -1;
  }
  p = fz_json_ws(p + 1);
  if (*p == '}') {
    return handle;
  }
  for (;;) {
    char* key = NULL;
    if (fz_json_parse_string(&p, &key) != 0) {
      free(key);
      return -1;
    }
    pthread_mutex_lock(&fz_list_lock);
    list = fz_list_get(handle);
    if (list != NULL) {
      (void)fz_list_push_cstr(list, key == NULL ? "" : key);
    }
    pthread_mutex_unlock(&fz_list_lock);
    free(key);
    p = fz_json_ws(p);
    if (*p != ':') {
      return -1;
    }
    p = fz_json_ws(p + 1);
    if (fz_json_skip_value_token(&p, 0) != 0) {
      return -1;
    }
    p = fz_json_ws(p);
    if (*p == ',') {
      p = fz_json_ws(p + 1);
      continue;
    }
    if (*p == '}') {
      return handle;
    }
    return -1;
  }
}

int32_t fz_native_json_parse(int32_t json_id) {
  const char* raw = fz_lookup_string(json_id);
  const char* start = NULL;
  const char* end = NULL;
  if (fz_json_parse_value_slice(raw, &start, &end) != 0) {
    return -1;
  }
  return fz_json_value_alloc_from_slice(start, end);
}

static int fz_json_parse_index_key(const char* key, int* out_index) {
  if (key == NULL || key[0] == '\0' || out_index == NULL) {
    return -1;
  }
  long value = 0;
  for (const unsigned char* p = (const unsigned char*)key; *p != '\0'; p++) {
    if (!isdigit(*p)) {
      return -1;
    }
    value = (value * 10) + (*p - '0');
    if (value > INT_MAX) {
      return -1;
    }
  }
  *out_index = (int)value;
  return 0;
}

int32_t fz_native_json_get(int32_t json_value_handle, int32_t key_id) {
  int32_t value_id = fz_json_value_get_id(json_value_handle);
  const char* raw = fz_lookup_string(value_id);
  const char* key = fz_lookup_string(key_id);
  const char* start = NULL;
  const char* end = NULL;
  const char* root = fz_json_ws(raw);
  int rc = -1;
  if (root != NULL && *root == '[') {
    int index = -1;
    if (fz_json_parse_index_key(key, &index) == 0) {
      rc = fz_json_array_lookup(raw, index, &start, &end);
    } else {
      rc = 0;
    }
  } else {
    rc = fz_json_object_lookup(raw, key == NULL ? "" : key, &start, &end);
  }
  if (rc <= 0) {
    return -1;
  }
  return fz_json_value_alloc_from_slice(start, end);
}

int32_t fz_native_json_get_str(int32_t json_value_handle, int32_t key_id) {
  const char* key = fz_lookup_string(key_id);
  int32_t child = fz_native_json_get(json_value_handle, key_id);
  if (child <= 0) {
    if (key != NULL && strcmp(key, "raw") == 0) {
      int32_t value_id = fz_json_value_get_id(json_value_handle);
      const char* raw = fz_lookup_string(value_id);
      if (raw == NULL) {
        return fz_intern_slice("", 0);
      }
      return fz_intern_slice(raw, strlen(raw));
    }
    return fz_intern_slice("", 0);
  }
  int32_t value_id = fz_json_value_get_id(child);
  const char* raw = fz_lookup_string(value_id);
  if (raw == NULL) {
    return fz_intern_slice("", 0);
  }
  const char* p = raw;
  char* out = NULL;
  if (fz_json_parse_string(&p, &out) != 0) {
    return fz_intern_slice("", 0);
  }
  p = fz_json_ws(p);
  if (*p != '\0' || out == NULL) {
    free(out);
    return fz_intern_slice("", 0);
  }
  return fz_intern_owned(out);
}

int32_t fz_native_json_has(int32_t json_value_handle, int32_t key_id) {
  int32_t value_id = fz_json_value_get_id(json_value_handle);
  const char* raw = fz_lookup_string(value_id);
  const char* key = fz_lookup_string(key_id);
  const char* start = NULL;
  const char* end = NULL;
  const char* root = fz_json_ws(raw);
  int rc = -1;
  if (root != NULL && *root == '[') {
    int index = -1;
    if (fz_json_parse_index_key(key, &index) == 0) {
      rc = fz_json_array_lookup(raw, index, &start, &end);
    } else {
      rc = 0;
    }
  } else {
    rc = fz_json_object_lookup(raw, key == NULL ? "" : key, &start, &end);
  }
  return rc > 0 ? 1 : 0;
}

int32_t fz_native_json_path(int32_t json_value_handle, int32_t path_id) {
  int32_t current = json_value_handle;
  const char* path = fz_lookup_string(path_id);
  if (path == NULL || path[0] == '\0') {
    return current;
  }
  const char* p = fz_json_ws(path);
  if (*p == '$') {
    p++;
  }
  if (*p == '.') {
    p++;
  }

  while (*p != '\0') {
    p = fz_json_ws(p);
    if (*p == '\0') {
      break;
    }
    if (*p == '.') {
      p++;
      continue;
    }
    if (*p == '[') {
      p++;
      int idx = 0;
      if (!isdigit((unsigned char)*p)) {
        return -1;
      }
      while (isdigit((unsigned char)*p)) {
        idx = (idx * 10) + (*p - '0');
        p++;
      }
      if (*p != ']') {
        return -1;
      }
      p++;
      int32_t value_id = fz_json_value_get_id(current);
      const char* raw = fz_lookup_string(value_id);
      const char* start = NULL;
      const char* end = NULL;
      int rc = fz_json_array_lookup(raw, idx, &start, &end);
      if (rc <= 0) {
        return -1;
      }
      current = fz_json_value_alloc_from_slice(start, end);
      if (current <= 0) {
        return -1;
      }
      continue;
    }
    const char* key_start = p;
    while (*p != '\0' && *p != '.' && *p != '[' && !isspace((unsigned char)*p)) {
      p++;
    }
    if (p == key_start) {
      return -1;
    }
    int32_t key_id = fz_intern_slice(key_start, (size_t)(p - key_start));
    current = fz_native_json_get(current, key_id);
    if (current <= 0) {
      return -1;
    }
  }

  return current;
}

static void fz_http_set_last_result(int status_code, const char* body, const char* err) {
  if (body == NULL) {
    body = "";
  }
  if (err == NULL) {
    err = "";
  }
  pthread_mutex_lock(&fz_http_lock);
  fz_http_last_status = status_code;
  fz_http_last_body_id = fz_intern_slice(body, strlen(body));
  fz_http_last_error_id = fz_intern_slice(err, strlen(err));
  pthread_mutex_unlock(&fz_http_lock);
}

static int fz_http_extract_status(char* payload, size_t payload_len, int* status_code, size_t* body_len) {
  if (status_code != NULL) {
    *status_code = 0;
  }
  if (body_len != NULL) {
    *body_len = payload_len;
  }
  if (payload == NULL || payload_len == 0) {
    return -1;
  }
  ssize_t i = (ssize_t)payload_len - 1;
  while (i >= 0 && (payload[i] == '\n' || payload[i] == '\r' || payload[i] == ' ' || payload[i] == '\t')) {
    i--;
  }
  if (i < 2) {
    return -1;
  }
  if (!isdigit((unsigned char)payload[i]) || !isdigit((unsigned char)payload[i - 1]) || !isdigit((unsigned char)payload[i - 2])) {
    return -1;
  }
  int parsed = (payload[i - 2] - '0') * 100 + (payload[i - 1] - '0') * 10 + (payload[i] - '0');
  ssize_t j = i - 3;
  while (j >= 0 && (payload[j] == '\n' || payload[j] == '\r')) {
    j--;
  }
  if (status_code != NULL) {
    *status_code = parsed;
  }
  if (body_len != NULL) {
    *body_len = (size_t)(j + 1);
  }
  return 0;
}

static int fz_http_parse_status_line(const char* header_block, size_t header_len) {
  if (header_block == NULL || header_len == 0) {
    return 0;
  }
  const char* line_end = NULL;
  for (size_t i = 0; i + 1 < header_len; i++) {
    if (header_block[i] == '\r' && header_block[i + 1] == '\n') {
      line_end = header_block + i;
      break;
    }
  }
  if (line_end == NULL) {
    return 0;
  }
  const char* first_space = memchr(header_block, ' ', (size_t)(line_end - header_block));
  if (first_space == NULL || (line_end - first_space) < 4) {
    return 0;
  }
  if (!isdigit((unsigned char)first_space[1]) || !isdigit((unsigned char)first_space[2])
      || !isdigit((unsigned char)first_space[3])) {
    return 0;
  }
  return (first_space[1] - '0') * 100 + (first_space[2] - '0') * 10 + (first_space[3] - '0');
}

static void fz_bytes_buf_consume_prefix(fz_bytes_buf* buf, size_t count) {
  if (buf == NULL || count == 0) {
    return;
  }
  if (count >= buf->len) {
    buf->len = 0;
    if (buf->data != NULL) {
      buf->data[0] = '\0';
    }
    return;
  }
  memmove(buf->data, buf->data + count, buf->len - count);
  buf->len -= count;
  buf->data[buf->len] = '\0';
}

static int fz_http_stream_read_response_headers(
    int stdout_fd,
    fz_bytes_buf* body_buf,
    int* status_code_out) {
  if (status_code_out != NULL) {
    *status_code_out = 0;
  }
  fz_bytes_buf scratch;
  fz_bytes_buf_init(&scratch);
  for (;;) {
    for (size_t i = 0; i + 3 < scratch.len; i++) {
      if (scratch.data[i] == '\r' && scratch.data[i + 1] == '\n' && scratch.data[i + 2] == '\r'
          && scratch.data[i + 3] == '\n') {
        size_t header_len = i + 4;
        int status = fz_http_parse_status_line(scratch.data, header_len);
        if (status > 0 && status < 200 && status != 101) {
          fz_bytes_buf_consume_prefix(&scratch, header_len);
          break;
        }
        if (status_code_out != NULL) {
          *status_code_out = status;
        }
        if (scratch.len > header_len) {
          if (fz_bytes_buf_append(body_buf, scratch.data + header_len, scratch.len - header_len)
              != 0) {
            fz_bytes_buf_free(&scratch);
            return -1;
          }
        }
        fz_bytes_buf_free(&scratch);
        return status > 0 ? 0 : -1;
      }
    }

    char tmp[4096];
    ssize_t got = read(stdout_fd, tmp, sizeof(tmp));
    if (got > 0) {
      if (fz_bytes_buf_append(&scratch, tmp, (size_t)got) != 0) {
        fz_bytes_buf_free(&scratch);
        return -1;
      }
      continue;
    }
    if (got == 0) {
      fz_bytes_buf_free(&scratch);
      return -1;
    }
    if (errno == EINTR) {
      continue;
    }
    fz_bytes_buf_free(&scratch);
    return -1;
  }
}

static void fz_http_stream_refresh_error_locked(fz_http_stream_state* state, const char* fallback) {
  if (state == NULL) {
    return;
  }
  const char* msg = fallback == NULL ? "" : fallback;
  if (state->stderr_buf.data != NULL && state->stderr_buf.len > 0) {
    msg = state->stderr_buf.data;
  }
  state->error_id = fz_intern_slice(msg, strlen(msg));
}

static void fz_http_stream_compact_stdout_locked(fz_http_stream_state* state) {
  if (state == NULL || state->stdout_read_pos == 0) {
    return;
  }
  if (state->stdout_read_pos >= state->stdout_buf.len) {
    state->stdout_buf.len = 0;
    state->stdout_read_pos = 0;
    if (state->stdout_buf.data != NULL) {
      state->stdout_buf.data[0] = '\0';
    }
    return;
  }
  memmove(
      state->stdout_buf.data,
      state->stdout_buf.data + state->stdout_read_pos,
      state->stdout_buf.len - state->stdout_read_pos);
  state->stdout_buf.len -= state->stdout_read_pos;
  state->stdout_buf.data[state->stdout_buf.len] = '\0';
  state->stdout_read_pos = 0;
}

static void fz_http_stream_finish_locked(fz_http_stream_state* state, int wait_blocking) {
  if (state == NULL || state->done) {
    return;
  }
  int status = 0;
  pid_t waited = waitpid(state->pid, &status, wait_blocking ? 0 : WNOHANG);
  if (waited <= 0) {
    return;
  }
  state->done = 1;
  if (WIFEXITED(status)) {
    state->exit_code = WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    state->exit_code = 128 + WTERMSIG(status);
  } else {
    state->exit_code = -1;
  }
  if (state->stderr_fd >= 0) {
    (void)fz_drain_fd(state->stderr_fd, &state->stderr_buf);
  }
  if (state->exit_code != 0 && (state->stderr_buf.data == NULL || state->stderr_buf.len == 0)) {
    char msg[128];
    snprintf(msg, sizeof(msg), "http stream transport exited=%d", state->exit_code);
    fz_http_stream_refresh_error_locked(state, msg);
  } else {
    fz_http_stream_refresh_error_locked(state, "");
  }
}

static int fz_http_stream_drain_locked(fz_http_stream_state* state) {
  if (state == NULL || state->closed) {
    return -1;
  }
  if (fz_async_current_task_cancelled()) {
    if (!state->done && state->pid > 0) {
      kill(state->pid, SIGTERM);
      fz_http_stream_finish_locked(state, 1);
    }
    state->eof = 1;
    fz_http_stream_refresh_error_locked(state, "http stream cancelled");
    return -1;
  }
  if (fz_async_deadline_expired()) {
    if (!state->done && state->pid > 0) {
      kill(state->pid, SIGTERM);
      fz_http_stream_finish_locked(state, 1);
    }
    state->eof = 1;
    fz_http_stream_refresh_error_locked(state, "http stream deadline expired");
    return -1;
  }
  size_t before_stdout = state->stdout_buf.len;
  size_t before_stderr = state->stderr_buf.len;
  int before_eof = state->eof;
  int before_done = state->done;
  if (state->stdout_fd >= 0) {
    int rc = fz_drain_fd(state->stdout_fd, &state->stdout_buf);
    if (rc < 0) {
      fz_http_stream_refresh_error_locked(state, "http stream stdout read failed");
      return -1;
    }
    if (rc == 1) {
      close(state->stdout_fd);
      state->stdout_fd = -1;
      state->eof = 1;
    }
  }
  if (state->stderr_fd >= 0) {
    int rc = fz_drain_fd(state->stderr_fd, &state->stderr_buf);
    if (rc < 0) {
      fz_http_stream_refresh_error_locked(state, "http stream stderr read failed");
      return -1;
    }
    if (rc == 1) {
      close(state->stderr_fd);
      state->stderr_fd = -1;
    }
  }
  fz_http_stream_finish_locked(state, 0);
  if (state->stderr_buf.len != before_stderr || (state->done && !before_done)) {
    fz_http_stream_refresh_error_locked(state, "");
  }
  if (state->stdout_buf.len != before_stdout || state->eof != before_eof || state->done != before_done
      || state->stderr_buf.len != before_stderr) {
    return 1;
  }
  return 0;
}

static int32_t fz_http_stream_consume_chunk_locked(fz_http_stream_state* state, int32_t max_bytes) {
  if (state == NULL) {
    return fz_intern_slice("", 0);
  }
  size_t unread = state->stdout_buf.len > state->stdout_read_pos
      ? (state->stdout_buf.len - state->stdout_read_pos)
      : 0;
  if (unread == 0) {
    if (state->eof) {
      return fz_intern_slice("", 0);
    }
    return 0;
  }
  size_t limit = max_bytes <= 0 ? unread : (size_t)max_bytes;
  if (limit > unread) {
    limit = unread;
  }
  int32_t out = fz_intern_slice(state->stdout_buf.data + state->stdout_read_pos, limit);
  state->stdout_read_pos += limit;
  fz_http_stream_compact_stdout_locked(state);
  return out;
}

static int32_t fz_http_stream_consume_line_locked(fz_http_stream_state* state) {
  if (state == NULL) {
    return fz_intern_slice("", 0);
  }
  size_t unread = state->stdout_buf.len > state->stdout_read_pos
      ? (state->stdout_buf.len - state->stdout_read_pos)
      : 0;
  if (unread == 0) {
    return state->eof ? fz_intern_slice("", 0) : 0;
  }
  char* start = state->stdout_buf.data + state->stdout_read_pos;
  char* newline = memchr(start, '\n', unread);
  if (newline == NULL) {
    if (!state->eof) {
      return 0;
    }
    size_t len = unread;
    if (len > 0 && start[len - 1] == '\r') {
      len--;
    }
    int32_t out = fz_intern_slice(start, len);
    state->stdout_read_pos += unread;
    fz_http_stream_compact_stdout_locked(state);
    return out;
  }
  size_t len = (size_t)(newline - start);
  if (len > 0 && start[len - 1] == '\r') {
    len--;
  }
  int32_t out = fz_intern_slice(start, len);
  state->stdout_read_pos += (size_t)(newline - start) + 1;
  fz_http_stream_compact_stdout_locked(state);
  return out;
}

static int32_t fz_http_stream_close_locked(fz_http_stream_state* state) {
  if (state == NULL) {
    return -1;
  }
  if (!state->done && state->pid > 0) {
    kill(state->pid, SIGTERM);
    fz_http_stream_finish_locked(state, 1);
  }
  if (state->stdout_fd >= 0) {
    close(state->stdout_fd);
    state->stdout_fd = -1;
  }
  if (state->stderr_fd >= 0) {
    close(state->stderr_fd);
    state->stderr_fd = -1;
  }
  fz_bytes_buf_free(&state->stdout_buf);
  fz_bytes_buf_free(&state->stderr_buf);
  memset(state, 0, sizeof(*state));
  return 0;
}

static int32_t fz_native_http_post_json_inner(int32_t endpoint_id, int32_t body_id, int return_body) {
  (void)pthread_once(&fz_env_bootstrap_once, fz_env_bootstrap);
  const char* endpoint = fz_lookup_string(endpoint_id);
  const char* body = fz_lookup_string(body_id);
  if (endpoint == NULL || endpoint[0] == '\0') {
    fz_last_exit_class = 3;
    fz_set_last_error(EINVAL, 3, "http_post_json failed: endpoint is empty");
    fz_http_set_last_result(0, "", "http_post_json: empty endpoint");
    return return_body ? fz_intern_slice("", 0) : -1;
  }
  if (body == NULL || body[0] == '\0') {
    body = "{}";
  }

  char* header_buf[FZ_MAX_HTTP_HEADERS];
  int header_count = 0;
  pthread_mutex_lock(&fz_http_lock);
  for (int i = 0; i < fz_http_header_count && i < FZ_MAX_HTTP_HEADERS; i++) {
    const char* key = fz_lookup_string(fz_http_headers[i].key_id);
    const char* value = fz_lookup_string(fz_http_headers[i].value_id);
    if (key == NULL || key[0] == '\0') {
      continue;
    }
    if (value == NULL) {
      value = "";
    }
    (void)fz_http_header_upsert(header_buf, &header_count, key, value);
  }
  fz_http_header_count = 0;
  pthread_mutex_unlock(&fz_http_lock);

  (void)fz_http_header_upsert(header_buf, &header_count, "content-type", "application/json");

  int max_args = 20 + (header_count * 2);
  char** argv = (char**)calloc((size_t)max_args, sizeof(char*));
  if (argv == NULL) {
    for (int i = 0; i < header_count; i++) free(header_buf[i]);
    fz_last_exit_class = 3;
    fz_set_last_error(ENOMEM, 3, "http_post_json failed: argv alloc failed");
    fz_http_set_last_result(0, "", "http_post_json: alloc failed");
    return return_body ? fz_intern_slice("", 0) : -1;
  }
  int ai = 0;
  argv[ai++] = "curl";
  argv[ai++] = "-sS";
  argv[ai++] = "-X";
  argv[ai++] = "POST";
  argv[ai++] = (char*)endpoint;
  for (int i = 0; i < header_count; i++) {
    argv[ai++] = "-H";
    argv[ai++] = header_buf[i];
  }
  argv[ai++] = "--data";
  argv[ai++] = (char*)body;
  argv[ai++] = "--connect-timeout";
  argv[ai++] = "10";
  argv[ai++] = "--max-time";
  argv[ai++] = "60";
  argv[ai++] = "-w";
  argv[ai++] = "\n%{http_code}";
  argv[ai++] = NULL;

  int out_pipe[2];
  int err_pipe[2];
  if (pipe(out_pipe) != 0) {
    free(argv);
    for (int i = 0; i < header_count; i++) free(header_buf[i]);
    fz_last_exit_class = 3;
    fz_set_last_error(errno, 3, "http_post_json failed: pipe failed");
    fz_http_set_last_result(0, "", "http_post_json: pipe failed");
    return return_body ? fz_intern_slice("", 0) : -1;
  }
  if (pipe(err_pipe) != 0) {
    close(out_pipe[0]);
    close(out_pipe[1]);
    free(argv);
    for (int i = 0; i < header_count; i++) free(header_buf[i]);
    fz_last_exit_class = 3;
    fz_set_last_error(errno, 3, "http_post_json failed: stderr pipe failed");
    fz_http_set_last_result(0, "", "http_post_json: stderr pipe failed");
    return return_body ? fz_intern_slice("", 0) : -1;
  }

  pid_t pid = fork();
  if (pid < 0) {
    close(out_pipe[0]);
    close(out_pipe[1]);
    close(err_pipe[0]);
    close(err_pipe[1]);
    free(argv);
    for (int i = 0; i < header_count; i++) free(header_buf[i]);
    fz_last_exit_class = 3;
    fz_set_last_error(errno, 3, "http_post_json failed: fork failed");
    fz_http_set_last_result(0, "", "http_post_json: fork failed");
    return return_body ? fz_intern_slice("", 0) : -1;
  }

  if (pid == 0) {
    (void)dup2(out_pipe[1], STDOUT_FILENO);
    (void)dup2(err_pipe[1], STDERR_FILENO);
    close(out_pipe[0]);
    close(out_pipe[1]);
    close(err_pipe[0]);
    close(err_pipe[1]);
    execvp("curl", argv);
    argv[0] = "/usr/bin/curl";
    execv("/usr/bin/curl", argv);
    argv[0] = "/opt/homebrew/bin/curl";
    execv("/opt/homebrew/bin/curl", argv);
    dprintf(STDERR_FILENO, "http_post_json failed: unable to exec curl (%s)\n", strerror(errno));
    _exit(127);
  }

  close(out_pipe[1]);
  close(err_pipe[1]);
  fz_bytes_buf out;
  fz_bytes_buf_init(&out);
  fz_bytes_buf err;
  fz_bytes_buf_init(&err);
  for (;;) {
    char tmp[4096];
    ssize_t got = read(out_pipe[0], tmp, sizeof(tmp));
    if (got > 0) {
      if (fz_bytes_buf_append(&out, tmp, (size_t)got) != 0) {
        break;
      }
      continue;
    }
    if (got == 0) {
      break;
    }
    if (errno == EINTR) {
      continue;
    }
    break;
  }
  close(out_pipe[0]);
  for (;;) {
    char tmp[4096];
    ssize_t got = read(err_pipe[0], tmp, sizeof(tmp));
    if (got > 0) {
      if (fz_bytes_buf_append(&err, tmp, (size_t)got) != 0) {
        break;
      }
      continue;
    }
    if (got == 0) {
      break;
    }
    if (errno == EINTR) {
      continue;
    }
    break;
  }
  close(err_pipe[0]);

  int status = 0;
  int waited = waitpid(pid, &status, 0);
  free(argv);
  for (int i = 0; i < header_count; i++) free(header_buf[i]);
  if (waited < 0) {
    fz_last_exit_class = 3;
    fz_set_last_error(errno, 3, "http_post_json failed: waitpid failed");
    fz_http_set_last_result(0, "", "http_post_json: waitpid failed");
    fz_bytes_buf_free(&out);
    fz_bytes_buf_free(&err);
    return return_body ? fz_intern_slice("", 0) : -1;
  }
  fz_last_exit_class = fz_exit_class_from_status(0, status, 0);

  int status_code = 0;
  size_t body_len = out.len;
  int parsed_status = fz_http_extract_status(out.data, out.len, &status_code, &body_len);
  const char* body_text = out.data == NULL ? "" : out.data;
  const char* err_text = err.data == NULL ? "" : err.data;
  char saved = '\0';
  if (out.data != NULL && body_len < out.len) {
    saved = out.data[body_len];
    out.data[body_len] = '\0';
  }
  int32_t body_value_id = fz_intern_slice(body_text, strlen(body_text));
  if (out.data != NULL && body_len < out.len) {
    out.data[body_len] = saved;
  }
  int transport_status = status_code > 0 ? status_code : 599;
  if (WIFEXITED(status) && WEXITSTATUS(status) == 0 && parsed_status == 0) {
    fz_http_set_last_result(status_code, body_text, err_text);
    fz_set_last_error(0, 0, "");
    fz_bytes_buf_free(&out);
    fz_bytes_buf_free(&err);
    return return_body ? body_value_id : 0;
  }

  if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
    const char* msg = "http_post_json failed: missing HTTP status trailer from transport";
    fz_http_set_last_result(transport_status, body_text, msg);
    fz_set_last_error(transport_status, 3, msg);
    int32_t fallback = strlen(body_text) > 0 ? body_value_id : fz_intern_slice(msg, strlen(msg));
    fz_bytes_buf_free(&out);
    fz_bytes_buf_free(&err);
    return return_body ? fallback : transport_status;
  }
  if (WIFEXITED(status)) {
    char msg[256];
    snprintf(
        msg,
        sizeof(msg),
        "http_post_json failed: curl exit=%d endpoint=%s",
        WEXITSTATUS(status),
        endpoint);
    const char* err_msg = (err_text[0] != '\0') ? err_text : msg;
    const char* body_for_failure = (body_text[0] != '\0') ? body_text : err_msg;
    int32_t failure_body_id = fz_intern_slice(body_for_failure, strlen(body_for_failure));
    fz_http_set_last_result(transport_status, body_for_failure, err_msg);
    fz_set_last_error(WEXITSTATUS(status), 3, msg);
    fz_bytes_buf_free(&out);
    fz_bytes_buf_free(&err);
    return return_body ? failure_body_id : WEXITSTATUS(status);
  }
  if (WIFSIGNALED(status)) {
    char msg[256];
    snprintf(
        msg,
        sizeof(msg),
        "http_post_json failed: curl terminated by signal=%d endpoint=%s",
        WTERMSIG(status),
        endpoint);
    const char* err_msg = (err_text[0] != '\0') ? err_text : msg;
    const char* body_for_failure = (body_text[0] != '\0') ? body_text : err_msg;
    int32_t failure_body_id = fz_intern_slice(body_for_failure, strlen(body_for_failure));
    fz_http_set_last_result(transport_status, body_for_failure, err_msg);
    fz_set_last_error(128 + WTERMSIG(status), 3, msg);
    fz_bytes_buf_free(&out);
    fz_bytes_buf_free(&err);
    return return_body ? failure_body_id : (128 + WTERMSIG(status));
  }
  {
    const char* msg = "http_post_json failed: unknown child status";
    const char* err_msg = (err_text[0] != '\0') ? err_text : msg;
    const char* body_for_failure = (body_text[0] != '\0') ? body_text : err_msg;
    int32_t failure_body_id = fz_intern_slice(body_for_failure, strlen(body_for_failure));
    fz_http_set_last_result(transport_status, body_for_failure, err_msg);
    fz_set_last_error(-1, 3, msg);
    fz_bytes_buf_free(&out);
    fz_bytes_buf_free(&err);
    return return_body ? failure_body_id : -1;
  }
}

int32_t fz_native_http_post_json(int32_t endpoint_id, int32_t body_id) {
  return fz_native_http_post_json_inner(endpoint_id, body_id, 0);
}

int32_t fz_native_http_post_json_capture(int32_t endpoint_id, int32_t body_id) {
  return fz_native_http_post_json_inner(endpoint_id, body_id, 1);
}

int32_t fz_native_http_request_stream(int32_t method_id, int32_t endpoint_id, int32_t body_id) {
  (void)pthread_once(&fz_env_bootstrap_once, fz_env_bootstrap);
  const char* method = fz_lookup_string(method_id);
  const char* endpoint = fz_lookup_string(endpoint_id);
  const char* body = fz_lookup_string(body_id);
  if (method == NULL || method[0] == '\0') {
    method = "GET";
  }
  if (endpoint == NULL || endpoint[0] == '\0') {
    fz_last_exit_class = 3;
    fz_set_last_error(EINVAL, 3, "http_request_stream failed: endpoint is empty");
    fz_http_set_last_result(0, "", "http_request_stream: empty endpoint");
    return -1;
  }
  if (body == NULL) {
    body = "";
  }

  char* header_buf[FZ_MAX_HTTP_HEADERS];
  int header_count = 0;
  pthread_mutex_lock(&fz_http_lock);
  for (int i = 0; i < fz_http_header_count && i < FZ_MAX_HTTP_HEADERS; i++) {
    const char* key = fz_lookup_string(fz_http_headers[i].key_id);
    const char* value = fz_lookup_string(fz_http_headers[i].value_id);
    if (key == NULL || key[0] == '\0') {
      continue;
    }
    if (value == NULL) {
      value = "";
    }
    (void)fz_http_header_upsert(header_buf, &header_count, key, value);
  }
  fz_http_header_count = 0;
  pthread_mutex_unlock(&fz_http_lock);

  int has_body = body[0] != '\0';
  int max_args = 24 + (header_count * 2);
  char** argv = (char**)calloc((size_t)max_args, sizeof(char*));
  if (argv == NULL) {
    for (int i = 0; i < header_count; i++) free(header_buf[i]);
    fz_last_exit_class = 3;
    fz_set_last_error(ENOMEM, 3, "http_request_stream failed: argv alloc failed");
    fz_http_set_last_result(0, "", "http_request_stream: alloc failed");
    return -1;
  }
  int ai = 0;
  argv[ai++] = "curl";
  argv[ai++] = "-sS";
  argv[ai++] = "-N";
  argv[ai++] = "-X";
  argv[ai++] = (char*)method;
  argv[ai++] = (char*)endpoint;
  argv[ai++] = "-D";
  argv[ai++] = "-";
  for (int i = 0; i < header_count; i++) {
    argv[ai++] = "-H";
    argv[ai++] = header_buf[i];
  }
  if (has_body) {
    argv[ai++] = "--data";
    argv[ai++] = (char*)body;
  }
  argv[ai++] = "--connect-timeout";
  argv[ai++] = "10";
  argv[ai++] = "--max-time";
  argv[ai++] = "300";
  argv[ai++] = NULL;

  int out_pipe[2];
  int err_pipe[2];
  if (pipe(out_pipe) != 0) {
    free(argv);
    for (int i = 0; i < header_count; i++) free(header_buf[i]);
    fz_last_exit_class = 3;
    fz_set_last_error(errno, 3, "http_request_stream failed: pipe failed");
    fz_http_set_last_result(0, "", "http_request_stream: pipe failed");
    return -1;
  }
  if (pipe(err_pipe) != 0) {
    close(out_pipe[0]);
    close(out_pipe[1]);
    free(argv);
    for (int i = 0; i < header_count; i++) free(header_buf[i]);
    fz_last_exit_class = 3;
    fz_set_last_error(errno, 3, "http_request_stream failed: stderr pipe failed");
    fz_http_set_last_result(0, "", "http_request_stream: stderr pipe failed");
    return -1;
  }

  pid_t pid = fork();
  if (pid < 0) {
    close(out_pipe[0]);
    close(out_pipe[1]);
    close(err_pipe[0]);
    close(err_pipe[1]);
    free(argv);
    for (int i = 0; i < header_count; i++) free(header_buf[i]);
    fz_last_exit_class = 3;
    fz_set_last_error(errno, 3, "http_request_stream failed: fork failed");
    fz_http_set_last_result(0, "", "http_request_stream: fork failed");
    return -1;
  }

  if (pid == 0) {
    (void)dup2(out_pipe[1], STDOUT_FILENO);
    (void)dup2(err_pipe[1], STDERR_FILENO);
    close(out_pipe[0]);
    close(out_pipe[1]);
    close(err_pipe[0]);
    close(err_pipe[1]);
    execvp("curl", argv);
    argv[0] = "/usr/bin/curl";
    execv("/usr/bin/curl", argv);
    argv[0] = "/opt/homebrew/bin/curl";
    execv("/opt/homebrew/bin/curl", argv);
    dprintf(STDERR_FILENO, "http_request_stream failed: unable to exec curl (%s)\n", strerror(errno));
    _exit(127);
  }

  close(out_pipe[1]);
  close(err_pipe[1]);
  fz_bytes_buf initial_body;
  fz_bytes_buf_init(&initial_body);
  int status_code = 0;
  int headers_ok = fz_http_stream_read_response_headers(out_pipe[0], &initial_body, &status_code);
  if (headers_ok != 0) {
    close(out_pipe[0]);
    close(err_pipe[0]);
    int status = 0;
    (void)waitpid(pid, &status, 0);
    free(argv);
    for (int i = 0; i < header_count; i++) free(header_buf[i]);
    fz_last_exit_class = 3;
    fz_set_last_error(-1, 3, "http_request_stream failed: invalid response headers");
    fz_http_set_last_result(0, "", "http_request_stream: invalid response headers");
    fz_bytes_buf_free(&initial_body);
    return -1;
  }

  (void)fz_set_nonblocking(out_pipe[0]);
  (void)fz_set_nonblocking(err_pipe[0]);
  pthread_mutex_lock(&fz_http_lock);
  int32_t handle = fz_http_stream_state_alloc(pid, out_pipe[0], err_pipe[0], status_code);
  if (handle > 0) {
    fz_http_stream_state* state = fz_http_stream_state_get(handle);
    if (state != NULL && initial_body.len > 0) {
      (void)fz_bytes_buf_append(&state->stdout_buf, initial_body.data, initial_body.len);
    }
    fz_http_last_status = status_code;
    fz_http_last_body_id = fz_intern_slice("", 0);
    fz_http_last_error_id = fz_intern_slice("", 0);
  }
  pthread_mutex_unlock(&fz_http_lock);
  free(argv);
  for (int i = 0; i < header_count; i++) free(header_buf[i]);
  fz_bytes_buf_free(&initial_body);
  if (handle <= 0) {
    close(out_pipe[0]);
    close(err_pipe[0]);
    kill(pid, SIGTERM);
    (void)waitpid(pid, NULL, 0);
    fz_last_exit_class = 3;
    fz_set_last_error(ENOMEM, 3, "http_request_stream failed: stream state alloc failed");
    fz_http_set_last_result(status_code, "", "http_request_stream: stream state alloc failed");
    return -1;
  }
  fz_set_last_error(0, 0, "");
  return handle;
}

int32_t fz_native_http_post_json_stream(int32_t endpoint_id, int32_t body_id) {
  (void)fz_native_http_header(
      fz_intern_slice("content-type", 12), fz_intern_slice("application/json", 16));
  return fz_native_http_request_stream(fz_intern_slice("POST", 4), endpoint_id, body_id);
}

int32_t fz_native_http_stream_read(int32_t handle, int32_t max_bytes) {
  for (;;) {
    pthread_mutex_lock(&fz_http_lock);
    fz_http_stream_state* state = fz_http_stream_state_get(handle);
    if (state == NULL || state->closed) {
      pthread_mutex_unlock(&fz_http_lock);
      return fz_intern_slice("", 0);
    }
    int32_t out = fz_http_stream_consume_chunk_locked(state, max_bytes);
    if (fz_lookup_string(out)[0] != '\0') {
      pthread_mutex_unlock(&fz_http_lock);
      return out;
    }
    if (state->eof) {
      pthread_mutex_unlock(&fz_http_lock);
      return fz_intern_slice("", 0);
    }
    int progress = fz_http_stream_drain_locked(state);
    pthread_mutex_unlock(&fz_http_lock);
    if (progress < 0) {
      return fz_intern_slice("", 0);
    }
    if (progress == 0) {
      int sleep_ms = fz_async_effective_timeout_ms(1);
      if (sleep_ms <= 0) {
        pthread_mutex_lock(&fz_http_lock);
        state = fz_http_stream_state_get(handle);
        if (state != NULL && !state->closed) {
          (void)fz_http_stream_drain_locked(state);
        }
        pthread_mutex_unlock(&fz_http_lock);
        return fz_intern_slice("", 0);
      }
      usleep((useconds_t)sleep_ms * 1000);
    }
  }
}

int32_t fz_native_http_stream_read_line(int32_t handle) {
  for (;;) {
    pthread_mutex_lock(&fz_http_lock);
    fz_http_stream_state* state = fz_http_stream_state_get(handle);
    if (state == NULL || state->closed) {
      pthread_mutex_unlock(&fz_http_lock);
      return fz_intern_slice("", 0);
    }
    int32_t out = fz_http_stream_consume_line_locked(state);
    if (out != 0) {
      pthread_mutex_unlock(&fz_http_lock);
      return out;
    }
    if (state->eof) {
      pthread_mutex_unlock(&fz_http_lock);
      return fz_intern_slice("", 0);
    }
    int progress = fz_http_stream_drain_locked(state);
    pthread_mutex_unlock(&fz_http_lock);
    if (progress < 0) {
      return fz_intern_slice("", 0);
    }
    if (progress == 0) {
      int sleep_ms = fz_async_effective_timeout_ms(1);
      if (sleep_ms <= 0) {
        pthread_mutex_lock(&fz_http_lock);
        state = fz_http_stream_state_get(handle);
        if (state != NULL && !state->closed) {
          (void)fz_http_stream_drain_locked(state);
        }
        pthread_mutex_unlock(&fz_http_lock);
        return fz_intern_slice("", 0);
      }
      usleep((useconds_t)sleep_ms * 1000);
    }
  }
}

int32_t fz_native_http_stream_eof(int32_t handle) {
  pthread_mutex_lock(&fz_http_lock);
  fz_http_stream_state* state = fz_http_stream_state_get(handle);
  if (state == NULL || state->closed) {
    pthread_mutex_unlock(&fz_http_lock);
    return 1;
  }
  (void)fz_http_stream_drain_locked(state);
  size_t unread = state->stdout_buf.len > state->stdout_read_pos
      ? (state->stdout_buf.len - state->stdout_read_pos)
      : 0;
  int eof = state->eof && unread == 0 ? 1 : 0;
  pthread_mutex_unlock(&fz_http_lock);
  return eof;
}

int32_t fz_native_http_stream_status(int32_t handle) {
  pthread_mutex_lock(&fz_http_lock);
  fz_http_stream_state* state = fz_http_stream_state_get(handle);
  int32_t status = state == NULL ? 0 : state->status_code;
  pthread_mutex_unlock(&fz_http_lock);
  return status;
}

int32_t fz_native_http_stream_error(int32_t handle) {
  pthread_mutex_lock(&fz_http_lock);
  fz_http_stream_state* state = fz_http_stream_state_get(handle);
  if (state == NULL) {
    pthread_mutex_unlock(&fz_http_lock);
    return fz_intern_slice("", 0);
  }
  (void)fz_http_stream_drain_locked(state);
  int32_t error_id = state->error_id;
  pthread_mutex_unlock(&fz_http_lock);
  return error_id;
}

int32_t fz_native_http_stream_close(int32_t handle) {
  pthread_mutex_lock(&fz_http_lock);
  fz_http_stream_state* state = fz_http_stream_state_get(handle);
  int32_t rc = fz_http_stream_close_locked(state);
  pthread_mutex_unlock(&fz_http_lock);
  return rc;
}

int32_t fz_native_http_last_status(void) {
  pthread_mutex_lock(&fz_http_lock);
  int32_t value = fz_http_last_status;
  pthread_mutex_unlock(&fz_http_lock);
  return value;
}

int32_t fz_native_http_last_body(void) {
  pthread_mutex_lock(&fz_http_lock);
  int32_t value = fz_http_last_body_id;
  pthread_mutex_unlock(&fz_http_lock);
  return value;
}

int32_t fz_native_http_last_error(void) {
  pthread_mutex_lock(&fz_http_lock);
  int32_t value = fz_http_last_error_id;
  pthread_mutex_unlock(&fz_http_lock);
  return value;
}

int32_t fz_native_error_code(void) {
  return fz_last_error_code;
}

int32_t fz_native_error_class(void) {
  return fz_last_error_class;
}

int32_t fz_native_error_message(void) {
  return fz_last_error_message_id;
}

int32_t fz_native_error_context(int32_t ctx_id) {
  const char* ctx = fz_lookup_string(ctx_id);
  const char* msg = fz_lookup_string(fz_last_error_message_id);
  if (ctx == NULL || ctx[0] == '\0') {
    return 0;
  }
  if (msg == NULL) {
    msg = "";
  }
  size_t n = strlen(msg) + strlen(ctx) + 4;
  char* out = (char*)malloc(n);
  if (out == NULL) {
    return -1;
  }
  snprintf(out, n, "%s: %s", msg, ctx);
  fz_last_error_message_id = fz_intern_owned(out);
  return 0;
}


static int32_t fz_log_level_value(const char* level) {
  if (level == NULL) return 0;
  if (strcmp(level, "error") == 0) return 2;
  if (strcmp(level, "warn") == 0) return 1;
  return 0;
}

static FILE* fz_log_stream(void) {
  return fz_log_sink == 1 ? stderr : stdout;
}

static int32_t fz_log_emit(const char* level, const char* message, const char* fields) {
  if (level == NULL) level = "info";
  if (message == NULL) message = "";
  if (fields == NULL) fields = "{}";
  if (!fz_log_enabled) {
    return 0;
  }
  if (fz_log_level_value(level) < fz_log_min_level) {
    return 0;
  }
  int64_t ts = fz_now_ms();
  FILE* stream = fz_log_stream();
  if (fz_log_json) {
    fprintf(stream, "{\"ts\":%lld,\"level\":\"%s\",\"msg\":\"", (long long)ts, level);
    for (const char* p = message; *p; p++) {
      if (*p == '"' || *p == '\\') fputc('\\', stream);
      fputc(*p, stream);
    }
    fprintf(stream, "\",\"fields\":%s}\n", fields[0] == '\0' ? "{}" : fields);
  } else if (fields[0] != '\0' && strcmp(fields, "{}") != 0) {
    fprintf(stream, "[%lld] %s %s | fields=%s\n", (long long)ts, level, message, fields);
  } else {
    fprintf(stream, "[%lld] %s %s\n", (long long)ts, level, message);
  }
  fflush(stream);
  return 0;
}

int32_t fz_native_log_info(int32_t message_id, int32_t fields_id) {
  return fz_log_emit("info", fz_lookup_string(message_id), fz_lookup_string(fields_id));
}

int32_t fz_native_log_warn(int32_t message_id, int32_t fields_id) {
  return fz_log_emit("warn", fz_lookup_string(message_id), fz_lookup_string(fields_id));
}

int32_t fz_native_log_error(int32_t message_id, int32_t fields_id) {
  return fz_log_emit("error", fz_lookup_string(message_id), fz_lookup_string(fields_id));
}

int32_t fz_native_log_fields_map(int32_t map_handle) {
  return fz_native_json_from_map(map_handle);
}

int32_t fz_native_log_set_json(int32_t enabled) {
  fz_log_json = enabled != 0 ? 1 : 0;
  return 0;
}

int32_t fz_native_log_set_enabled(int32_t enabled) {
  fz_log_enabled = enabled != 0 ? 1 : 0;
  return 0;
}

int32_t fz_native_log_set_level(int32_t level_id) {
  const char* level = fz_lookup_string(level_id);
  if (level == NULL || level[0] == '\0' || strcmp(level, "info") == 0) {
    fz_log_min_level = 0;
    return 0;
  }
  if (strcmp(level, "warn") == 0) {
    fz_log_min_level = 1;
    return 0;
  }
  if (strcmp(level, "error") == 0) {
    fz_log_min_level = 2;
    return 0;
  }
  fz_set_last_error(EINVAL, 3, "log.set_level failed: expected info, warn, or error");
  return -1;
}

int32_t fz_native_log_set_sink(int32_t sink_id) {
  const char* sink = fz_lookup_string(sink_id);
  if (sink == NULL || sink[0] == '\0' || strcmp(sink, "stdout") == 0) {
    fz_log_sink = 0;
    return 0;
  }
  if (strcmp(sink, "stderr") == 0) {
    fz_log_sink = 1;
    return 0;
  }
  fz_set_last_error(EINVAL, 3, "log.set_sink failed: expected stdout or stderr");
  return -1;
}

int32_t fz_native_log_correlation_id(int32_t conn_fd) {
  return fz_native_net_request_id(conn_fd);
}

int32_t fz_native_time_sleep_ms(int32_t ms) {
  if (ms > 0) {
    usleep((useconds_t)ms * 1000);
  }
  return 0;
}

int32_t fz_native_time_elapsed_ms(int32_t start_ms) {
  int64_t now = fz_now_ms();
  return (int32_t)(now - (int64_t)start_ms);
}

int32_t fz_native_time_deadline_after(int32_t delta_ms) {
  int64_t now = fz_now_ms();
  return (int32_t)(now + (int64_t)delta_ms);
}

int32_t fz_native_crypto_random_hex(int32_t len_bytes) {
  if (len_bytes < 0) {
    fz_set_last_error(EINVAL, 3, "crypto.random_hex failed: len must be >= 0");
    return fz_intern_slice("", 0);
  }
  size_t len = (size_t)len_bytes;
  uint8_t* raw = len == 0 ? NULL : (uint8_t*)malloc(len);
  if (len > 0 && raw == NULL) {
    fz_set_last_error(ENOMEM, 3, "crypto.random_hex failed: alloc failed");
    return fz_intern_slice("", 0);
  }
  if (fz_crypto_fill_random(raw, len) != 0) {
    if (raw != NULL) {
      fz_crypto_memzero(raw, len);
    }
    free(raw);
    fz_set_last_error(errno == 0 ? EIO : errno, 3, "crypto.random_hex failed: entropy unavailable");
    return fz_intern_slice("", 0);
  }
  char* encoded = fz_crypto_hex_encode(raw == NULL ? (const uint8_t*)"" : raw, len);
  if (raw != NULL) {
    fz_crypto_memzero(raw, len);
  }
  free(raw);
  if (encoded == NULL) {
    fz_set_last_error(ENOMEM, 3, "crypto.random_hex failed: hex encode alloc failed");
    return fz_intern_slice("", 0);
  }
  fz_set_last_error(0, 0, "");
  return fz_intern_owned(encoded);
}

int32_t fz_native_crypto_random_base64(int32_t len_bytes) {
  if (len_bytes < 0) {
    fz_set_last_error(EINVAL, 3, "crypto.random_base64 failed: len must be >= 0");
    return fz_intern_slice("", 0);
  }
  size_t len = (size_t)len_bytes;
  uint8_t* raw = len == 0 ? NULL : (uint8_t*)malloc(len);
  if (len > 0 && raw == NULL) {
    fz_set_last_error(ENOMEM, 3, "crypto.random_base64 failed: alloc failed");
    return fz_intern_slice("", 0);
  }
  if (fz_crypto_fill_random(raw, len) != 0) {
    if (raw != NULL) {
      fz_crypto_memzero(raw, len);
    }
    free(raw);
    fz_set_last_error(errno == 0 ? EIO : errno, 3, "crypto.random_base64 failed: entropy unavailable");
    return fz_intern_slice("", 0);
  }
  char* encoded = fz_crypto_base64_encode_alloc(raw == NULL ? (const uint8_t*)"" : raw, len);
  if (raw != NULL) {
    fz_crypto_memzero(raw, len);
  }
  free(raw);
  if (encoded == NULL) {
    fz_set_last_error(ENOMEM, 3, "crypto.random_base64 failed: base64 encode alloc failed");
    return fz_intern_slice("", 0);
  }
  fz_set_last_error(0, 0, "");
  return fz_intern_owned(encoded);
}

int32_t fz_native_crypto_sha256(int32_t input_id) {
  const char* input = fz_lookup_string(input_id);
  size_t len = input == NULL ? 0 : strlen(input);
  uint8_t digest[32];
  fz_sha256_hash((const uint8_t*)(input == NULL ? "" : input), len, digest);
  char* encoded = fz_crypto_hex_encode(digest, sizeof(digest));
  fz_crypto_memzero(digest, sizeof(digest));
  if (encoded == NULL) {
    fz_set_last_error(ENOMEM, 3, "crypto.sha256 failed: hex encode alloc failed");
    return fz_intern_slice("", 0);
  }
  fz_set_last_error(0, 0, "");
  return fz_intern_owned(encoded);
}

int32_t fz_native_crypto_hmac_sha256(int32_t key_id, int32_t data_id) {
  const char* key = fz_lookup_string(key_id);
  const char* data = fz_lookup_string(data_id);
  uint8_t digest[32];
  fz_hmac_sha256_hash(
      (const uint8_t*)(key == NULL ? "" : key),
      key == NULL ? 0 : strlen(key),
      (const uint8_t*)(data == NULL ? "" : data),
      data == NULL ? 0 : strlen(data),
      digest);
  char* encoded = fz_crypto_hex_encode(digest, sizeof(digest));
  fz_crypto_memzero(digest, sizeof(digest));
  if (encoded == NULL) {
    fz_set_last_error(ENOMEM, 3, "crypto.hmac_sha256 failed: hex encode alloc failed");
    return fz_intern_slice("", 0);
  }
  fz_set_last_error(0, 0, "");
  return fz_intern_owned(encoded);
}

int32_t fz_native_crypto_constant_time_eq(int32_t left_id, int32_t right_id) {
  const char* left = fz_lookup_string(left_id);
  const char* right = fz_lookup_string(right_id);
  size_t left_len = left == NULL ? 0 : strlen(left);
  size_t right_len = right == NULL ? 0 : strlen(right);
  size_t max_len = left_len > right_len ? left_len : right_len;
  unsigned char diff = (unsigned char)(left_len ^ right_len);
  for (size_t i = 0; i < max_len; i++) {
    unsigned char a = i < left_len ? (unsigned char)left[i] : 0;
    unsigned char b = i < right_len ? (unsigned char)right[i] : 0;
    diff |= (unsigned char)(a ^ b);
  }
  fz_set_last_error(0, 0, "");
  return diff == 0 ? 1 : 0;
}

int32_t fz_native_crypto_base64_encode(int32_t input_id) {
  const char* input = fz_lookup_string(input_id);
  size_t len = input == NULL ? 0 : strlen(input);
  char* encoded = fz_crypto_base64_encode_alloc((const uint8_t*)(input == NULL ? "" : input), len);
  if (encoded == NULL) {
    fz_set_last_error(ENOMEM, 3, "crypto.base64_encode failed: alloc failed");
    return fz_intern_slice("", 0);
  }
  fz_set_last_error(0, 0, "");
  return fz_intern_owned(encoded);
}

int32_t fz_native_crypto_base64_decode(int32_t input_id) {
  const char* input = fz_lookup_string(input_id);
  uint8_t* decoded = NULL;
  size_t decoded_len = 0;
  if (fz_crypto_base64_decode_alloc(input == NULL ? "" : input, &decoded, &decoded_len) != 0) {
    fz_set_last_error(EINVAL, 3, "crypto.base64_decode failed: invalid base64 input");
    return fz_intern_slice("", 0);
  }
  if (decoded != NULL && memchr(decoded, '\0', decoded_len) != NULL) {
    fz_crypto_memzero(decoded, decoded_len);
    free(decoded);
    fz_set_last_error(EINVAL, 3, "crypto.base64_decode failed: decoded bytes are not text-safe");
    return fz_intern_slice("", 0);
  }
  int32_t out = fz_intern_slice((const char*)(decoded == NULL ? (const uint8_t*)"" : decoded), decoded_len);
  if (decoded != NULL) {
    fz_crypto_memzero(decoded, decoded_len);
  }
  free(decoded);
  fz_set_last_error(0, 0, "");
  return out;
}

int32_t fz_native_crypto_base64_url_encode(int32_t input_id) {
  const char* input = fz_lookup_string(input_id);
  size_t len = input == NULL ? 0 : strlen(input);
  char* encoded =
      fz_crypto_base64_url_encode_alloc((const uint8_t*)(input == NULL ? "" : input), len);
  if (encoded == NULL) {
    fz_set_last_error(ENOMEM, 3, "crypto.base64_url_encode failed: alloc failed");
    return fz_intern_slice("", 0);
  }
  fz_set_last_error(0, 0, "");
  return fz_intern_owned(encoded);
}

int32_t fz_native_crypto_base64_url_decode(int32_t input_id) {
  const char* input = fz_lookup_string(input_id);
  uint8_t* decoded = NULL;
  size_t decoded_len = 0;
  if (fz_crypto_base64_url_decode_alloc(input == NULL ? "" : input, &decoded, &decoded_len) != 0) {
    fz_set_last_error(EINVAL, 3, "crypto.base64_url_decode failed: invalid base64url input");
    return fz_intern_slice("", 0);
  }
  if (decoded != NULL && memchr(decoded, '\0', decoded_len) != NULL) {
    fz_crypto_memzero(decoded, decoded_len);
    free(decoded);
    fz_set_last_error(EINVAL, 3, "crypto.base64_url_decode failed: decoded bytes are not text-safe");
    return fz_intern_slice("", 0);
  }
  int32_t out = fz_intern_slice((const char*)(decoded == NULL ? (const uint8_t*)"" : decoded), decoded_len);
  if (decoded != NULL) {
    fz_crypto_memzero(decoded, decoded_len);
  }
  free(decoded);
  fz_set_last_error(0, 0, "");
  return out;
}

int32_t fz_native_time_interval(int32_t period_ms) {
  if (period_ms <= 0) {
    return -1;
  }
  pthread_mutex_lock(&fz_time_lock);
  int32_t handle = -1;
  for (int i = 0; i < FZ_MAX_INTERVALS; i++) {
    if (!fz_intervals[i].in_use) {
      fz_intervals[i].in_use = 1;
      fz_intervals[i].period_ms = period_ms;
      fz_intervals[i].next_ms = fz_now_ms() + period_ms;
      handle = i + 1;
      break;
    }
  }
  pthread_mutex_unlock(&fz_time_lock);
  return handle;
}

int32_t fz_native_time_tick(int32_t handle) {
  if (handle <= 0 || handle > FZ_MAX_INTERVALS) {
    return -1;
  }
  pthread_mutex_lock(&fz_time_lock);
  fz_interval_state* interval = &fz_intervals[handle - 1];
  if (!interval->in_use) {
    pthread_mutex_unlock(&fz_time_lock);
    return -1;
  }
  int64_t now = fz_now_ms();
  int64_t wait_ms = interval->next_ms - now;
  if (wait_ms > 0) {
    pthread_mutex_unlock(&fz_time_lock);
    usleep((useconds_t)wait_ms * 1000);
    pthread_mutex_lock(&fz_time_lock);
    interval = &fz_intervals[handle - 1];
  }
  now = fz_now_ms();
  interval->next_ms = now + interval->period_ms;
  pthread_mutex_unlock(&fz_time_lock);
  return 0;
}

int32_t fz_native_fs_open(int32_t path_id) {
  const char* path = fz_lookup_string(path_id);
  if (path == NULL || path[0] == '\0') {
    fz_set_last_error(EINVAL, 3, "fs.open failed: path must not be empty");
    return -1;
  }
  int fd = open(path, O_CREAT | O_RDWR, 0644);
  if (fd < 0) {
    fz_set_last_error(errno, 3, "fs.open failed");
    return -1;
  }
  (void)fz_mark_cloexec(fd);
  return fd;
}

int32_t fz_native_fs_close(int32_t handle) {
  if (handle < 0) {
    fz_set_last_error(EINVAL, 3, "fs.close failed: invalid handle");
    return -1;
  }
  if (close(handle) != 0) {
    fz_set_last_error(errno, 3, "fs.close failed");
    return -1;
  }
  return 0;
}

int32_t fz_native_fs_write(int32_t handle, int32_t content_id) {
  const char* content = fz_lookup_string(content_id);
  if (handle < 0) {
    fz_set_last_error(EINVAL, 3, "fs.write failed: invalid handle");
    return -1;
  }
  if (content == NULL) content = "";
  if (lseek(handle, 0, SEEK_END) < 0) {
    fz_set_last_error(errno, 3, "fs.write failed: seek failed");
    return -1;
  }
  size_t left = strlen(content);
  const char* p = content;
  while (left > 0) {
    ssize_t wrote = write(handle, p, left);
    if (wrote < 0) {
      if (errno == EINTR) continue;
      fz_set_last_error(errno, 3, "fs.write failed");
      return -1;
    }
    if (wrote == 0) {
      break;
    }
    p += wrote;
    left -= (size_t)wrote;
  }
  return 0;
}

int32_t fz_native_fs_read(int32_t handle, int32_t limit) {
  if (handle < 0) {
    fz_set_last_error(EINVAL, 3, "fs.read failed: invalid handle");
    return fz_intern_slice("", 0);
  }
  if (limit < 0) {
    fz_set_last_error(EINVAL, 3, "fs.read failed: limit must be >= 0");
    return fz_intern_slice("", 0);
  }
  if (lseek(handle, 0, SEEK_SET) < 0) {
    fz_set_last_error(errno, 3, "fs.read failed: seek failed");
    return fz_intern_slice("", 0);
  }
  size_t cap = (size_t)limit;
  if (cap > 1048576) cap = 1048576;
  fz_bytes_buf buf;
  fz_bytes_buf_init(&buf);
  char tmp[4096];
  while (buf.len < cap) {
    size_t chunk = sizeof(tmp);
    if (cap - buf.len < chunk) {
      chunk = cap - buf.len;
    }
    if (chunk == 0) {
      break;
    }
    ssize_t got = read(handle, tmp, chunk);
    if (got > 0) {
      if (fz_bytes_buf_append(&buf, tmp, (size_t)got) != 0) {
        fz_set_last_error(ENOMEM, 3, "fs.read failed: buffer alloc failed");
        break;
      }
      continue;
    }
    if (got == 0) break;
    if (errno == EINTR) continue;
    fz_set_last_error(errno, 3, "fs.read failed");
    break;
  }
  int32_t out = fz_intern_slice(buf.data == NULL ? "" : buf.data, buf.len);
  fz_bytes_buf_free(&buf);
  return out;
}

int32_t fz_native_fs_flush(int32_t handle) {
  if (handle < 0) {
    fz_set_last_error(EINVAL, 3, "fs.flush failed: invalid handle");
    return -1;
  }
  return fsync(handle) == 0 ? 0 : -1;
}

int32_t fz_native_fs_fsync(int32_t handle) {
  if (handle < 0) {
    fz_set_last_error(EINVAL, 3, "fs.fsync failed: invalid handle");
    return -1;
  }
  if (fsync(handle) != 0) {
    fz_set_last_error(errno, 3, "fs.fsync failed");
    return -1;
  }
  return 0;
}

int32_t fz_native_fs_lock(int32_t handle) {
  if (handle < 0) {
    fz_set_last_error(EINVAL, 3, "fs.lock failed: invalid handle");
    return -1;
  }
  if (lockf(handle, F_LOCK, 0) != 0) {
    fz_set_last_error(errno, 3, "fs.lock failed");
    return -1;
  }
  return 0;
}

int32_t fz_native_fs_atomic_write(int32_t path_id, int32_t body_id) {
  const char* path = fz_lookup_string(path_id);
  const char* payload = fz_lookup_string(body_id);
  if (path == NULL || path[0] == '\0') {
    fz_set_last_error(EINVAL, 3, "fs.atomic_write failed: invalid path");
    return -1;
  }
  if (payload == NULL) {
    payload = "";
  }
  char tmp_path[2048];
  int written = snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
  if (written <= 0 || (size_t)written >= sizeof(tmp_path)) {
    fz_set_last_error(ENAMETOOLONG, 3, "fs.atomic_write failed: temp path too long");
    return -1;
  }
  int fd = open(tmp_path, O_CREAT | O_TRUNC | O_WRONLY, 0644);
  if (fd < 0) {
    fz_set_last_error(errno, 3, "fs.atomic_write failed: open temp file");
    return -1;
  }
  size_t left = strlen(payload);
  const char* cursor = payload;
  while (left > 0) {
    ssize_t n = write(fd, cursor, left);
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      close(fd);
      fz_set_last_error(errno, 3, "fs.atomic_write failed: write temp file");
      return -1;
    }
    if (n == 0) {
      break;
    }
    cursor += n;
    left -= (size_t)n;
  }
  if (fsync(fd) != 0) {
    int err = errno;
    close(fd);
    fz_set_last_error(err, 3, "fs.atomic_write failed: fsync temp file");
    return -1;
  }
  close(fd);
  if (rename(tmp_path, path) != 0) {
    fz_set_last_error(errno, 3, "fs.atomic_write failed: rename temp file");
    return -1;
  }
  return 0;
}

int32_t fz_native_fs_read_file(int32_t path_id) {
  const char* path = fz_lookup_string(path_id);
  if (path == NULL || path[0] == '\0') {
    return fz_intern_slice("", 0);
  }
  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    fz_set_last_error(errno, 3, "fs.read_file failed");
    return fz_intern_slice("", 0);
  }
  fz_bytes_buf buf;
  fz_bytes_buf_init(&buf);
  char tmp[4096];
  for (;;) {
    ssize_t got = read(fd, tmp, sizeof(tmp));
    if (got > 0) {
      if (fz_bytes_buf_append(&buf, tmp, (size_t)got) != 0) {
        break;
      }
      continue;
    }
    if (got == 0) break;
    if (errno == EINTR) continue;
    break;
  }
  close(fd);
  int32_t out = fz_intern_slice(buf.data == NULL ? "" : buf.data, buf.len);
  fz_bytes_buf_free(&buf);
  return out;
}

int32_t fz_native_fs_write_file(int32_t path_id, int32_t content_id) {
  const char* path = fz_lookup_string(path_id);
  const char* content = fz_lookup_string(content_id);
  if (path == NULL || path[0] == '\0') {
    return -1;
  }
  if (content == NULL) content = "";
  int fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0644);
  if (fd < 0) {
    fz_set_last_error(errno, 3, "fs.write_file open failed");
    return -1;
  }
  size_t left = strlen(content);
  const char* p = content;
  while (left > 0) {
    ssize_t wrote = write(fd, p, left);
    if (wrote < 0) {
      if (errno == EINTR) continue;
      close(fd);
      fz_set_last_error(errno, 3, "fs.write_file write failed");
      return -1;
    }
    if (wrote == 0) break;
    p += wrote;
    left -= (size_t)wrote;
  }
  close(fd);
  return 0;
}

static int fz_storage_write_atomic_path(const char* path, const char* content) {
  if (path == NULL || path[0] == '\0') {
    return -1;
  }
  if (content == NULL) {
    content = "";
  }
  char tmp_path[2048];
  int written = snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
  if (written <= 0 || (size_t)written >= sizeof(tmp_path)) {
    return -1;
  }
  int fd = open(tmp_path, O_CREAT | O_TRUNC | O_WRONLY, 0644);
  if (fd < 0) {
    return -1;
  }
  size_t left = strlen(content);
  const char* p = content;
  while (left > 0) {
    ssize_t n = write(fd, p, left);
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      close(fd);
      return -1;
    }
    if (n == 0) {
      break;
    }
    p += n;
    left -= (size_t)n;
  }
  if (fsync(fd) != 0) {
    close(fd);
    return -1;
  }
  close(fd);
  return rename(tmp_path, path) == 0 ? 0 : -1;
}

int32_t fz_native_storage_append(int32_t path_id, int32_t line_id) {
  const char* path = fz_lookup_string(path_id);
  const char* line = fz_lookup_string(line_id);
  if (path == NULL || path[0] == '\0') {
    return -1;
  }
  if (line == NULL) {
    line = "";
  }
  int fd = open(path, O_CREAT | O_APPEND | O_WRONLY, 0644);
  if (fd < 0) {
    return -1;
  }
  size_t len = strlen(line);
  if (len > 0 && write(fd, line, len) < 0) {
    close(fd);
    return -1;
  }
  if (write(fd, "\n", 1) < 0) {
    close(fd);
    return -1;
  }
  close(fd);
  return 0;
}

int32_t fz_native_storage_atomic_append(int32_t path_id, int32_t line_id) {
  const char* path = fz_lookup_string(path_id);
  const char* line = fz_lookup_string(line_id);
  if (path == NULL || path[0] == '\0') {
    return -1;
  }
  if (line == NULL) {
    line = "";
  }
  int32_t existing_id = fz_native_fs_read_file(path_id);
  const char* existing = fz_lookup_string(existing_id);
  if (existing == NULL) {
    existing = "";
  }
  size_t existing_len = strlen(existing);
  size_t line_len = strlen(line);
  char* payload = (char*)malloc(existing_len + line_len + 3);
  if (payload == NULL) {
    return -1;
  }
  size_t used = 0;
  if (existing_len > 0) {
    memcpy(payload + used, existing, existing_len);
    used += existing_len;
    if (payload[used - 1] != '\n') {
      payload[used++] = '\n';
    }
  }
  if (line_len > 0) {
    memcpy(payload + used, line, line_len);
    used += line_len;
  }
  payload[used++] = '\n';
  payload[used] = '\0';
  int rc = fz_storage_write_atomic_path(path, payload);
  free(payload);
  return rc == 0 ? 0 : -1;
}

int32_t fz_native_storage_kv_open(int32_t path_id) {
  const char* path = fz_lookup_string(path_id);
  if (path == NULL || path[0] == '\0') {
    return -1;
  }
  pthread_mutex_lock(&fz_storage_kv_lock);
  for (int i = 0; i < FZ_MAX_STORAGE_KV; i++) {
    if (!fz_storage_kv[i].in_use) {
      continue;
    }
    const char* existing_path = fz_lookup_string(fz_storage_kv[i].path_id);
    if (existing_path == NULL || strcmp(existing_path, path) != 0) {
      continue;
    }
    int32_t kv_handle = fz_storage_kv_alloc();
    fz_storage_kv_state* kv = fz_storage_kv_get(kv_handle);
    if (kv != NULL) {
      kv->path_id = fz_storage_kv[i].path_id;
      kv->map_handle = fz_storage_kv[i].map_handle;
    }
    pthread_mutex_unlock(&fz_storage_kv_lock);
    return kv == NULL ? -1 : kv_handle;
  }
  pthread_mutex_unlock(&fz_storage_kv_lock);
  int32_t map_handle = fz_runtime_map_new();
  int32_t file_json_id = fz_native_fs_read_file(path_id);
  const char* raw = fz_lookup_string(file_json_id);
  if (raw != NULL && raw[0] != '\0') {
    int32_t parsed_handle = fz_native_json_to_map(file_json_id);
    if (parsed_handle > 0) {
      map_handle = parsed_handle;
    }
  }
  pthread_mutex_lock(&fz_storage_kv_lock);
  int32_t kv_handle = fz_storage_kv_alloc();
  fz_storage_kv_state* kv = fz_storage_kv_get(kv_handle);
  if (kv != NULL) {
    kv->path_id = path_id;
    kv->map_handle = map_handle;
  }
  pthread_mutex_unlock(&fz_storage_kv_lock);
  return kv == NULL ? -1 : kv_handle;
}

int32_t fz_native_storage_kv_close(int32_t kv_handle) {
  pthread_mutex_lock(&fz_storage_kv_lock);
  fz_storage_kv_state* kv = fz_storage_kv_get(kv_handle);
  if (kv == NULL) {
    pthread_mutex_unlock(&fz_storage_kv_lock);
    return -1;
  }
  memset(kv, 0, sizeof(*kv));
  pthread_mutex_unlock(&fz_storage_kv_lock);
  return 0;
}

int32_t fz_native_storage_kv_get(int32_t kv_handle, int32_t key_id) {
  pthread_mutex_lock(&fz_storage_kv_lock);
  fz_storage_kv_state* kv = fz_storage_kv_get(kv_handle);
  if (kv == NULL) {
    pthread_mutex_unlock(&fz_storage_kv_lock);
    return fz_intern_slice("", 0);
  }
  int32_t map_handle = kv->map_handle;
  pthread_mutex_unlock(&fz_storage_kv_lock);
  return fz_runtime_map_get(map_handle, key_id);
}

int32_t fz_native_storage_kv_put(int32_t kv_handle, int32_t key_id, int32_t value_id) {
  pthread_mutex_lock(&fz_storage_kv_lock);
  fz_storage_kv_state* kv = fz_storage_kv_get(kv_handle);
  if (kv == NULL) {
    pthread_mutex_unlock(&fz_storage_kv_lock);
    return -1;
  }
  int32_t path_id = kv->path_id;
  int32_t map_handle = kv->map_handle;
  pthread_mutex_unlock(&fz_storage_kv_lock);
  int rc = fz_runtime_map_set(map_handle, key_id, value_id);
  if (rc != 0) {
    return -1;
  }
  int32_t json_id = fz_native_json_from_map(map_handle);
  const char* path = fz_lookup_string(path_id);
  const char* content = fz_lookup_string(json_id);
  return fz_storage_write_atomic_path(path, content) == 0 ? 0 : -1;
}

static int fz_fs_read_lstat(const char* path, struct stat* st, const char* context) {
  if (path == NULL || path[0] == '\0' || st == NULL) {
    errno = EINVAL;
    fz_set_last_error(EINVAL, 3, context);
    return -1;
  }
  if (lstat(path, st) == 0) {
    return 0;
  }
  fz_set_last_error(errno, 3, context);
  return -1;
}

static char* fz_fs_join_owned(const char* left, const char* right) {
  if (left == NULL) left = "";
  if (right == NULL) right = "";
  size_t left_len = strlen(left);
  size_t right_len = strlen(right);
  int need_sep = left_len > 0 && left[left_len - 1] != '/';
  char* out = (char*)malloc(left_len + right_len + (need_sep ? 2 : 1));
  if (out == NULL) {
    errno = ENOMEM;
    return NULL;
  }
  strcpy(out, left);
  if (need_sep) strcat(out, "/");
  strcat(out, right);
  return out;
}

static int fz_fs_mkdirs_owned(char* path) {
  if (path == NULL || path[0] == '\0') {
    errno = EINVAL;
    return -1;
  }
  size_t len = strlen(path);
  if (len == 0) {
    errno = EINVAL;
    return -1;
  }
  for (size_t i = 1; i < len; i++) {
    if (path[i] != '/') continue;
    path[i] = '\0';
    if (path[0] != '\0' && mkdir(path, 0755) != 0 && errno != EEXIST) {
      path[i] = '/';
      return -1;
    }
    path[i] = '/';
  }
  if (mkdir(path, 0755) != 0 && errno != EEXIST) {
    return -1;
  }
  return 0;
}

static int fz_fs_ensure_parent_dirs(const char* path, const char* context) {
  if (path == NULL || path[0] == '\0') {
    fz_set_last_error(EINVAL, 3, context);
    return -1;
  }
  const char* slash = strrchr(path, '/');
  if (slash == NULL) {
    return 0;
  }
  size_t len = (size_t)(slash - path);
  if (len == 0) {
    return 0;
  }
  char* parent = (char*)malloc(len + 1);
  if (parent == NULL) {
    fz_set_last_error(ENOMEM, 3, context);
    return -1;
  }
  memcpy(parent, path, len);
  parent[len] = '\0';
  int rc = fz_fs_mkdirs_owned(parent);
  if (rc != 0) {
    fz_set_last_error(errno, 3, context);
  }
  free(parent);
  return rc;
}

static int fz_fs_copy_file_path(const char* src, const char* dst, const char* context) {
  struct stat st;
  if (fz_fs_read_lstat(src, &st, context) != 0) {
    return -1;
  }
  if (!S_ISREG(st.st_mode)) {
    fz_set_last_error(EINVAL, 3, context);
    return -1;
  }
  if (fz_fs_ensure_parent_dirs(dst, context) != 0) {
    return -1;
  }
  int in_fd = open(src, O_RDONLY);
  if (in_fd < 0) {
    fz_set_last_error(errno, 3, context);
    return -1;
  }
  int out_fd = open(dst, O_CREAT | O_TRUNC | O_WRONLY, st.st_mode & 0777 ? st.st_mode & 0777 : 0644);
  if (out_fd < 0) {
    close(in_fd);
    fz_set_last_error(errno, 3, context);
    return -1;
  }
  char buf[8192];
  int rc = 0;
  for (;;) {
    ssize_t got = read(in_fd, buf, sizeof(buf));
    if (got == 0) {
      break;
    }
    if (got < 0) {
      if (errno == EINTR) continue;
      rc = -1;
      break;
    }
    char* p = buf;
    ssize_t left = got;
    while (left > 0) {
      ssize_t wrote = write(out_fd, p, (size_t)left);
      if (wrote < 0) {
        if (errno == EINTR) continue;
        rc = -1;
        left = 0;
        break;
      }
      p += wrote;
      left -= wrote;
    }
    if (rc != 0) {
      break;
    }
  }
  if (rc == 0 && fsync(out_fd) != 0) {
    rc = -1;
  }
  int saved_errno = errno;
  close(in_fd);
  close(out_fd);
  if (rc != 0) {
    errno = saved_errno;
    fz_set_last_error(errno, 3, context);
    return -1;
  }
  return 0;
}

static int fz_fs_remove_path(const char* path, const char* context) {
  struct stat st;
  if (fz_fs_read_lstat(path, &st, context) != 0) {
    return -1;
  }
  if (S_ISDIR(st.st_mode) && !S_ISLNK(st.st_mode)) {
    DIR* dir = opendir(path);
    if (dir == NULL) {
      fz_set_last_error(errno, 3, context);
      return -1;
    }
    int rc = 0;
    struct dirent* ent = NULL;
    while ((ent = readdir(dir)) != NULL) {
      if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
      char* child = fz_fs_join_owned(path, ent->d_name);
      if (child == NULL) {
        rc = -1;
        break;
      }
      if (fz_fs_remove_path(child, context) != 0) {
        free(child);
        rc = -1;
        break;
      }
      free(child);
    }
    int saved_errno = errno;
    closedir(dir);
    if (rc != 0) {
      errno = saved_errno;
      return -1;
    }
    if (rmdir(path) != 0) {
      fz_set_last_error(errno, 3, context);
      return -1;
    }
    return 0;
  }
  if (unlink(path) != 0) {
    fz_set_last_error(errno, 3, context);
    return -1;
  }
  return 0;
}

static int fz_fs_copy_tree_path(const char* src, const char* dst, const char* context) {
  struct stat st;
  if (fz_fs_read_lstat(src, &st, context) != 0) {
    return -1;
  }
  if (S_ISLNK(st.st_mode)) {
    fz_set_last_error(EINVAL, 3, context);
    return -1;
  }
  if (S_ISREG(st.st_mode)) {
    return fz_fs_copy_file_path(src, dst, context);
  }
  if (!S_ISDIR(st.st_mode)) {
    fz_set_last_error(EINVAL, 3, context);
    return -1;
  }
  char* dst_owned = strdup(dst);
  if (dst_owned == NULL) {
    fz_set_last_error(ENOMEM, 3, context);
    return -1;
  }
  if (fz_fs_mkdirs_owned(dst_owned) != 0) {
    free(dst_owned);
    fz_set_last_error(errno, 3, context);
    return -1;
  }
  free(dst_owned);
  DIR* dir = opendir(src);
  if (dir == NULL) {
    fz_set_last_error(errno, 3, context);
    return -1;
  }
  int rc = 0;
  struct dirent* ent = NULL;
  while ((ent = readdir(dir)) != NULL) {
    if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
    char* src_child = fz_fs_join_owned(src, ent->d_name);
    char* dst_child = fz_fs_join_owned(dst, ent->d_name);
    if (src_child == NULL || dst_child == NULL) {
      free(src_child);
      free(dst_child);
      errno = ENOMEM;
      rc = -1;
      break;
    }
    if (fz_fs_copy_tree_path(src_child, dst_child, context) != 0) {
      free(src_child);
      free(dst_child);
      rc = -1;
      break;
    }
    free(src_child);
    free(dst_child);
  }
  int saved_errno = errno;
  closedir(dir);
  if (rc != 0) {
    errno = saved_errno;
    return -1;
  }
  return 0;
}

static int fz_compare_cstr_ptrs(const void* left, const void* right) {
  const char* const* a = (const char* const*)left;
  const char* const* b = (const char* const*)right;
  const char* av = (a != NULL && *a != NULL) ? *a : "";
  const char* bv = (b != NULL && *b != NULL) ? *b : "";
  return strcmp(av, bv);
}

int32_t fz_native_fs_mkdir(int32_t path_id) {
  const char* path = fz_lookup_string(path_id);
  if (path == NULL || path[0] == '\0') return -1;
  if (mkdir(path, 0755) == 0 || errno == EEXIST) return 0;
  return -1;
}

int32_t fz_native_fs_exists(int32_t path_id) {
  const char* path = fz_lookup_string(path_id);
  if (path == NULL || path[0] == '\0') return 0;
  struct stat st;
  return lstat(path, &st) == 0 ? 1 : 0;
}

int32_t fz_native_fs_is_file(int32_t path_id) {
  const char* path = fz_lookup_string(path_id);
  struct stat st;
  if (fz_fs_read_lstat(path, &st, "fs.is_file failed") != 0) return 0;
  return S_ISREG(st.st_mode) ? 1 : 0;
}

int32_t fz_native_fs_is_dir(int32_t path_id) {
  const char* path = fz_lookup_string(path_id);
  struct stat st;
  if (fz_fs_read_lstat(path, &st, "fs.is_dir failed") != 0) return 0;
  return S_ISDIR(st.st_mode) ? 1 : 0;
}

int32_t fz_native_fs_is_symlink(int32_t path_id) {
  const char* path = fz_lookup_string(path_id);
  struct stat st;
  if (fz_fs_read_lstat(path, &st, "fs.is_symlink failed") != 0) return 0;
  return S_ISLNK(st.st_mode) ? 1 : 0;
}

int32_t fz_native_fs_stat_size(int32_t path_id) {
  const char* path = fz_lookup_string(path_id);
  struct stat st;
  if (fz_fs_read_lstat(path, &st, "fs.stat_size failed") != 0) return -1;
  return (int32_t)st.st_size;
}

int32_t fz_native_fs_stat_mtime(int32_t path_id) {
  const char* path = fz_lookup_string(path_id);
  struct stat st;
  if (fz_fs_read_lstat(path, &st, "fs.stat_mtime failed") != 0) return -1;
  return (int32_t)st.st_mtime;
}

int32_t fz_native_fs_listdir(int32_t path_id) {
  const char* path = fz_lookup_string(path_id);
  if (path == NULL || path[0] == '\0') return -1;
  DIR* dir = opendir(path);
  if (dir == NULL) {
    fz_set_last_error(errno, 3, "fs.listdir failed");
    return -1;
  }
  pthread_mutex_lock(&fz_list_lock);
  int32_t list_handle = fz_list_alloc();
  fz_list_state* list = fz_list_get(list_handle);
  if (list != NULL) {
    struct dirent* ent = NULL;
    while ((ent = readdir(dir)) != NULL) {
      if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
      (void)fz_list_push_cstr(list, ent->d_name);
    }
    if (list->count > 1) {
      qsort(list->items, (size_t)list->count, sizeof(char*), fz_compare_cstr_ptrs);
    }
  }
  pthread_mutex_unlock(&fz_list_lock);
  closedir(dir);
  return list_handle;
}

int32_t fz_native_fs_remove_file(int32_t path_id) {
  const char* path = fz_lookup_string(path_id);
  if (path == NULL || path[0] == '\0') return -1;
  return unlink(path) == 0 ? 0 : -1;
}

int32_t fz_native_fs_remove(int32_t path_id) {
  const char* path = fz_lookup_string(path_id);
  return fz_fs_remove_path(path, "fs.remove failed");
}

int32_t fz_native_fs_temp_file(int32_t prefix_id) {
  const char* prefix = fz_lookup_string(prefix_id);
  if (prefix == NULL || prefix[0] == '\0') prefix = "fz";
  char tmpl[512];
  snprintf(tmpl, sizeof(tmpl), "/tmp/%s-XXXXXX", prefix);
  int fd = mkstemp(tmpl);
  if (fd < 0) return fz_intern_slice("", 0);
  close(fd);
  return fz_intern_slice(tmpl, strlen(tmpl));
}

int32_t fz_native_fs_copy_file(int32_t src_id, int32_t dst_id) {
  const char* src = fz_lookup_string(src_id);
  const char* dst = fz_lookup_string(dst_id);
  return fz_fs_copy_file_path(src, dst, "fs.copy_file failed");
}

int32_t fz_native_fs_copy_tree(int32_t src_id, int32_t dst_id) {
  const char* src = fz_lookup_string(src_id);
  const char* dst = fz_lookup_string(dst_id);
  return fz_fs_copy_tree_path(src, dst, "fs.copy_tree failed");
}

int32_t fz_native_path_join(int32_t left_id, int32_t right_id) {
  const char* left = fz_lookup_string(left_id);
  const char* right = fz_lookup_string(right_id);
  if (left == NULL) left = "";
  if (right == NULL) right = "";
  size_t left_len = strlen(left);
  size_t right_len = strlen(right);
  int need_sep = left_len > 0 && left[left_len - 1] != '/';
  char* out = (char*)malloc(left_len + right_len + (need_sep ? 2 : 1));
  if (out == NULL) return 0;
  strcpy(out, left);
  if (need_sep) strcat(out, "/");
  strcat(out, right);
  return fz_intern_owned(out);
}

int32_t fz_native_path_normalize(int32_t path_id) {
  const char* path = fz_lookup_string(path_id);
  if (path == NULL) path = "";
  char* out = strdup(path);
  if (out == NULL) return 0;
  size_t w = 0;
  for (size_t r = 0; out[r] != '\0'; r++) {
    if (out[r] == '/' && w > 0 && out[w - 1] == '/') continue;
    out[w++] = out[r];
  }
  if (w > 1 && out[w - 1] == '/') w--;
  out[w] = '\0';
  return fz_intern_owned(out);
}

static const char* fz_path_last_segment(const char* path) {
  const char* last = strrchr(path, '/');
  if (last == NULL) return path;
  if (last[1] == '\0') return last;
  return last + 1;
}

int32_t fz_native_path_basename(int32_t path_id) {
  int32_t normalized_id = fz_native_path_normalize(path_id);
  const char* normalized = fz_lookup_string(normalized_id);
  if (normalized == NULL || normalized[0] == '\0') return fz_intern_slice(".", 1);
  if (strcmp(normalized, "/") == 0) return fz_intern_slice("/", 1);
  const char* base = fz_path_last_segment(normalized);
  if (base[0] == '\0') return fz_intern_slice(".", 1);
  return fz_intern_slice(base, strlen(base));
}

int32_t fz_native_path_dirname(int32_t path_id) {
  int32_t normalized_id = fz_native_path_normalize(path_id);
  const char* normalized = fz_lookup_string(normalized_id);
  if (normalized == NULL || normalized[0] == '\0') return fz_intern_slice(".", 1);
  if (strcmp(normalized, "/") == 0) return fz_intern_slice("/", 1);
  const char* last = strrchr(normalized, '/');
  if (last == NULL) return fz_intern_slice(".", 1);
  if (last == normalized) return fz_intern_slice("/", 1);
  return fz_intern_slice(normalized, (size_t)(last - normalized));
}

int32_t fz_native_path_stem(int32_t path_id) {
  int32_t base_id = fz_native_path_basename(path_id);
  const char* base = fz_lookup_string(base_id);
  if (base == NULL || base[0] == '\0' || strcmp(base, "/") == 0 || strcmp(base, ".") == 0) {
    return fz_intern_slice(base == NULL ? "" : base, base == NULL ? 0 : strlen(base));
  }
  const char* dot = strrchr(base, '.');
  if (dot == NULL || dot == base) return fz_intern_slice(base, strlen(base));
  return fz_intern_slice(base, (size_t)(dot - base));
}

int32_t fz_native_path_extension(int32_t path_id) {
  int32_t base_id = fz_native_path_basename(path_id);
  const char* base = fz_lookup_string(base_id);
  if (base == NULL || base[0] == '\0' || strcmp(base, "/") == 0 || strcmp(base, ".") == 0) {
    return fz_intern_slice("", 0);
  }
  const char* dot = strrchr(base, '.');
  if (dot == NULL || dot == base || dot[1] == '\0') return fz_intern_slice("", 0);
  return fz_intern_slice(dot + 1, strlen(dot + 1));
}

int32_t fz_native_net_bind(void) {
  (void)pthread_once(&fz_env_bootstrap_once, fz_env_bootstrap);
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    char msg[256];
    snprintf(msg, sizeof(msg), "http.bind failed: socket() errno=%d (%s)", errno, strerror(errno));
    fz_set_last_error(errno, 3, msg);
    return -1;
  }
  (void)fz_mark_cloexec(fd);
  int yes = 1;
  (void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = fz_default_addr();
  addr.sin_port = htons((uint16_t)fz_default_port());
  if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
    char host[64];
    const char* rendered = inet_ntop(AF_INET, &addr.sin_addr, host, sizeof(host));
    if (rendered == NULL) {
      rendered = fz_default_host_name();
    }
    int bind_port = (int)ntohs(addr.sin_port);
    char msg[320];
    snprintf(
        msg,
        sizeof(msg),
        "http.bind failed on %s:%d errno=%d (%s); set FZ_HOST/FZ_PORT or AGENT_HOST/AGENT_PORT",
        rendered,
        bind_port,
        errno,
        strerror(errno));
    fz_set_last_error(errno, 3, msg);
    close(fd);
    return -1;
  }
  pthread_mutex_lock(&fz_listener_lock);
  fz_listener_fd = fd;
  pthread_mutex_unlock(&fz_listener_lock);
  fz_set_last_error(0, 0, "");
  return fd;
}

int32_t fz_native_net_listen(int32_t fd) {
  int listener = fd;
  if (listener < 0) {
    pthread_mutex_lock(&fz_listener_lock);
    listener = fz_listener_fd;
    pthread_mutex_unlock(&fz_listener_lock);
  }
  if (listener < 0) {
    fz_set_last_error(EINVAL, 3, "http.listen failed: no listener fd (call http.bind first)");
    return -1;
  }
  if (listen(listener, 128) != 0) {
    char msg[256];
    snprintf(
        msg,
        sizeof(msg),
        "http.listen failed fd=%d backlog=128 errno=%d (%s)",
        listener,
        errno,
        strerror(errno));
    fz_set_last_error(errno, 3, msg);
    return -1;
  }
  fz_log_bind_target(listener);
  fz_set_last_error(0, 0, "");
  return 0;
}

int32_t fz_native_net_accept(void) {
  int listener = -1;
  pthread_mutex_lock(&fz_listener_lock);
  listener = fz_listener_fd;
  pthread_mutex_unlock(&fz_listener_lock);
  if (listener < 0) {
    fz_set_last_error(EINVAL, 3, "http.accept failed: listener not initialized");
    return -1;
  }
  struct sockaddr_in peer;
  socklen_t peer_len = sizeof(peer);
  int conn_fd = accept(listener, (struct sockaddr*)&peer, &peer_len);
  if (conn_fd < 0) {
    if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
      char msg[256];
      snprintf(
          msg,
          sizeof(msg),
          "http.accept failed listener=%d errno=%d (%s)",
          listener,
          errno,
          strerror(errno));
      fz_set_last_error(errno, 3, msg);
    }
    return -1;
  }
  (void)fz_mark_cloexec(conn_fd);
  char peer_addr[64];
  const char* rendered = inet_ntop(AF_INET, &peer.sin_addr, peer_addr, sizeof(peer_addr));
  if (rendered == NULL) {
    rendered = "";
  }
  pthread_mutex_lock(&fz_conn_lock);
  fz_conn_state* state = fz_conn_state_for(conn_fd, 1);
  if (state != NULL) {
    fz_conn_state_reset_request_body(state);
    fz_conn_state_reset_response_headers(state);
    state->remote_addr_id = fz_intern_slice(rendered, strlen(rendered));
    state->request_id = 0;
    state->header_count = 0;
    state->query_count = 0;
    state->param_count = 0;
  }
  pthread_mutex_unlock(&fz_conn_lock);
  fz_set_last_error(0, 0, "");
  return conn_fd;
}

static int fz_wait_for_fd_event(int fd, short events, int timeout_ms) {
  struct pollfd pfd;
  pfd.fd = fd;
  pfd.events = events;
  pfd.revents = 0;
  for (;;) {
    if (fz_async_current_task_cancelled()) {
      errno = ECANCELED;
      return -1;
    }
    if (fz_async_deadline_expired()) {
      errno = ETIMEDOUT;
      return -1;
    }
    int effective_timeout_ms = fz_async_effective_timeout_ms(timeout_ms);
    int ready = poll(&pfd, 1, effective_timeout_ms);
    if (ready > 0) {
      return 0;
    }
    if (ready == 0) {
      errno = ETIMEDOUT;
      return -1;
    }
    if (errno == EINTR) {
      continue;
    }
    return -1;
  }
}

int32_t fz_native_net_poll_register(int32_t fd) {
  if (fd < 0) {
    fz_set_last_error(EINVAL, 3, "http.poll_register failed: invalid handle");
    return -1;
  }
  pthread_mutex_lock(&fz_net_poll_lock);
  for (int i = 0; i < FZ_MAX_NET_POLL_WATCHES; i++) {
    if (fz_net_poll_watches[i].in_use && fz_net_poll_watches[i].fd == fd) {
      fz_net_poll_watches[i].events = POLLIN;
      pthread_mutex_unlock(&fz_net_poll_lock);
      fz_set_last_error(0, 0, "");
      return 0;
    }
  }
  for (int i = 0; i < FZ_MAX_NET_POLL_WATCHES; i++) {
    if (!fz_net_poll_watches[i].in_use) {
      fz_net_poll_watches[i].in_use = 1;
      fz_net_poll_watches[i].fd = fd;
      fz_net_poll_watches[i].events = POLLIN;
      pthread_mutex_unlock(&fz_net_poll_lock);
      fz_set_last_error(0, 0, "");
      return 0;
    }
  }
  pthread_mutex_unlock(&fz_net_poll_lock);
  fz_set_last_error(ENOSPC, 3, "http.poll_register failed: poll watch queue full");
  return -1;
}

int32_t fz_native_net_read_headers(int32_t conn_fd) {
  if (conn_fd < 0) {
    fz_set_last_error(EINVAL, 3, "http.read_headers failed: invalid connection handle");
    return -1;
  }
  char* req = (char*)malloc(FZ_MAX_HTTP_READ + 1);
  if (req == NULL) {
    fz_set_last_error(ENOMEM, 3, "http.read_headers failed: allocation failed");
    return -1;
  }
  int total = 0;
  int header_end = -1;
  while (total < FZ_MAX_HTTP_READ) {
    ssize_t got = recv(conn_fd, req + total, (size_t)(FZ_MAX_HTTP_READ - total), 0);
    if (got < 0) {
      if (errno == EINTR) {
        continue;
      }
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        if (fz_wait_for_fd_event(conn_fd, POLLIN, 2500) == 0) {
          continue;
        }
      }
      char msg[256];
      snprintf(
          msg,
          sizeof(msg),
          "http.read_headers failed fd=%d errno=%d (%s)",
          conn_fd,
          errno,
          strerror(errno));
      fz_set_last_error(errno, 3, msg);
      free(req);
      return -1;
    }
    if (got == 0) {
      if (total == 0) {
        fz_set_last_error(
            ECONNRESET,
            3,
            "http.read_headers failed: peer closed before a complete request was received");
        free(req);
        return -1;
      }
      break;
    }
    total += (int)got;
    req[total] = '\0';
    if (header_end < 0) {
      header_end = fz_find_header_end(req, total);
      if (header_end >= 0) {
        break;
      }
    }
  }
  if (header_end < 0) {
    fz_set_last_error(
        EPROTO,
        3,
        "http.read_headers failed: request headers were incomplete or malformed");
    free(req);
    return -1;
  }

  const char* line_end = strstr(req, "\r\n");
  if (line_end == NULL) {
    fz_set_last_error(EPROTO, 3, "http.read_headers failed: missing request line terminator");
    free(req);
    return -1;
  }
  const char* sp1 = memchr(req, ' ', (size_t)(line_end - req));
  if (sp1 == NULL) {
    fz_set_last_error(EPROTO, 3, "http.read_headers failed: malformed request method/path");
    free(req);
    return -1;
  }
  const char* sp2 = memchr(sp1 + 1, ' ', (size_t)(line_end - (sp1 + 1)));
  if (sp2 == NULL) {
    fz_set_last_error(EPROTO, 3, "http.read_headers failed: malformed request path/version");
    free(req);
    return -1;
  }

  size_t method_len = (size_t)(sp1 - req);
  size_t path_len = (size_t)(sp2 - (sp1 + 1));
  const char* version = sp2 + 1;
  int version_len = (int)(line_end - version);
  const char* raw_path = sp1 + 1;
  const char* query_mark = memchr(raw_path, '?', path_len);
  size_t clean_path_len = query_mark == NULL ? path_len : (size_t)(query_mark - raw_path);

  int32_t method_id = fz_intern_slice(req, method_len);
  int32_t path_id = fz_intern_slice(raw_path, clean_path_len);
  int keep_alive = fz_parse_keep_alive(req, header_end, version, version_len);
  int64_t content_length = fz_parse_content_length(req, header_end);
  int chunked = fz_parse_chunked_flag(req, header_end);
  size_t prefetched_body_len = total > header_end ? (size_t)(total - header_end) : 0;

  pthread_mutex_lock(&fz_conn_lock);
  fz_conn_state* state = fz_conn_state_for(conn_fd, 1);
  if (state != NULL) {
    fz_conn_state_reset_request_body(state);
    fz_conn_state_reset_response_headers(state);
    state->method_id = method_id;
    state->path_id = path_id;
    state->keep_alive = keep_alive;
    state->header_count = 0;
    state->query_count = 0;
    state->param_count = 0;
    state->request_headers_ready = 1;
    state->request_body_active = 1;
    if (chunked) {
      state->request_body_mode = 2;
      state->request_body_remaining = -1;
      state->request_chunk_remaining = 0;
      state->request_body_eof = 0;
    } else if (content_length > 0) {
      state->request_body_mode = 1;
      state->request_body_remaining = content_length;
      state->request_chunk_remaining = 0;
      state->request_body_eof = 0;
    } else {
      state->request_body_mode = 0;
      state->request_body_remaining = 0;
      state->request_chunk_remaining = 0;
      state->request_body_eof = 1;
    }
    if (prefetched_body_len > 0) {
      state->request_body_buf = (char*)malloc(prefetched_body_len + 1);
      if (state->request_body_buf != NULL) {
        memcpy(state->request_body_buf, req + header_end, prefetched_body_len);
        state->request_body_buf[prefetched_body_len] = '\0';
        state->request_body_buf_len = prefetched_body_len;
        state->request_body_buf_pos = 0;
      }
    }
    if (state->request_body_mode == 1 && state->request_body_remaining >= 0) {
      state->request_body_remaining -= (int64_t)prefetched_body_len;
      if (state->request_body_remaining <= 0) {
        state->request_body_remaining = 0;
        state->request_body_eof = 1;
      }
    }
    fz_conn_request_counter += 1;
    char rid[64];
    snprintf(rid, sizeof(rid), "req-%d", fz_conn_request_counter);
    state->request_id = fz_intern_slice(rid, strlen(rid));
    const char* cursor = line_end + 2;
    while (cursor < req + header_end && state->header_count < FZ_MAX_CONN_META) {
      const char* next = strstr(cursor, "\r\n");
      if (next == NULL || next <= cursor) break;
      const char* colon = memchr(cursor, ':', (size_t)(next - cursor));
      if (colon != NULL) {
        const char* v = colon + 1;
        while (v < next && (*v == ' ' || *v == '\t')) v++;
        state->header_key_ids[state->header_count] = fz_intern_slice(cursor, (size_t)(colon - cursor));
        state->header_value_ids[state->header_count] = fz_intern_slice(v, (size_t)(next - v));
        state->header_count++;
      }
      cursor = next + 2;
    }
    if (query_mark != NULL) {
      const char* q = query_mark + 1;
      const char* q_end = raw_path + path_len;
      while (q < q_end && state->query_count < FZ_MAX_CONN_META) {
        const char* amp = memchr(q, '&', (size_t)(q_end - q));
        const char* token_end = amp == NULL ? q_end : amp;
        const char* eq = memchr(q, '=', (size_t)(token_end - q));
        if (eq == NULL) {
          state->query_key_ids[state->query_count] = fz_intern_slice(q, (size_t)(token_end - q));
          state->query_value_ids[state->query_count] = fz_intern_slice("", 0);
          state->query_count++;
        } else {
          state->query_key_ids[state->query_count] = fz_intern_slice(q, (size_t)(eq - q));
          state->query_value_ids[state->query_count] = fz_intern_slice(eq + 1, (size_t)(token_end - (eq + 1)));
          state->query_count++;
        }
        if (amp == NULL) break;
        q = amp + 1;
      }
    }
  }
  pthread_mutex_unlock(&fz_conn_lock);

  fz_set_last_error(0, 0, "");
  free(req);
  return 0;
}

int32_t fz_native_net_read(int32_t conn_fd) {
  if (fz_native_net_read_headers(conn_fd) != 0) {
    return -1;
  }
  pthread_mutex_lock(&fz_conn_lock);
  fz_conn_state* state = fz_conn_state_for(conn_fd, 0);
  if (state == NULL) {
    pthread_mutex_unlock(&fz_conn_lock);
    fz_set_last_error(EINVAL, 3, "http.read failed: connection state unavailable");
    return -1;
  }
  fz_bytes_buf body;
  fz_bytes_buf_init(&body);
  size_t prefetched = state->request_body_buf_len - state->request_body_buf_pos;
  if (state->request_body_mode == 1 && prefetched > 0 && state->request_body_remaining <= 0) {
    state->request_body_remaining = (int64_t)prefetched;
    state->request_body_eof = 0;
  }
  while (!state->request_body_eof) {
    char* chunk = NULL;
    size_t chunk_len = 0;
    int rc = fz_conn_read_body_chunk(state, &chunk, &chunk_len, 4096);
    if (rc < 0) {
      if (chunk != NULL) {
        free(chunk);
      }
      fz_bytes_buf_free(&body);
      pthread_mutex_unlock(&fz_conn_lock);
      fz_set_last_error(EIO, 3, "http.read failed while streaming request body");
      return -1;
    }
    if (chunk != NULL) {
      if (fz_bytes_buf_append(&body, chunk, chunk_len) != 0) {
        free(chunk);
        fz_bytes_buf_free(&body);
        pthread_mutex_unlock(&fz_conn_lock);
        fz_set_last_error(ENOMEM, 3, "http.read failed buffering request body");
        return -1;
      }
      free(chunk);
    }
    if (rc > 0) {
      break;
    }
  }
  state->body_id = fz_intern_slice(body.data == NULL ? "" : body.data, body.len);
  state->request_body_fully_buffered = 1;
  state->request_body_active = 0;
  fz_bytes_buf_free(&body);
  pthread_mutex_unlock(&fz_conn_lock);
  fz_set_last_error(0, 0, "");
  return 0;
}

int32_t fz_native_net_poll_next(void) {
  struct pollfd pfds[FZ_MAX_NET_POLL_WATCHES];
  int slots[FZ_MAX_NET_POLL_WATCHES];
  int count = 0;
  pthread_mutex_lock(&fz_net_poll_lock);
  for (int i = 0; i < FZ_MAX_NET_POLL_WATCHES; i++) {
    if (!fz_net_poll_watches[i].in_use) {
      continue;
    }
    pfds[count].fd = fz_net_poll_watches[i].fd;
    pfds[count].events = fz_net_poll_watches[i].events | POLLERR | POLLHUP;
    pfds[count].revents = 0;
    slots[count] = i;
    count++;
  }
  pthread_mutex_unlock(&fz_net_poll_lock);

  if (count == 0) {
    fz_set_last_error(EINVAL, 3, "http.poll_next failed: no registered sockets");
    return -1;
  }

  for (;;) {
    int ready = poll(pfds, (nfds_t)count, 2500);
    if (ready > 0) {
      for (int i = 0; i < count; i++) {
        if (pfds[i].revents == 0) {
          continue;
        }
        int fd = pfds[i].fd;
        pthread_mutex_lock(&fz_net_poll_lock);
        fz_net_poll_watches[slots[i]].in_use = 0;
        fz_net_poll_watches[slots[i]].fd = -1;
        fz_net_poll_watches[slots[i]].events = 0;
        pthread_mutex_unlock(&fz_net_poll_lock);
        fz_set_last_error(0, 0, "");
        return fd;
      }
      fz_set_last_error(EIO, 3, "http.poll_next failed: poll reported readiness without events");
      return -1;
    }
    if (ready == 0) {
      fz_set_last_error(ETIMEDOUT, 3, "http.poll_next timed out waiting for socket readiness");
      return -1;
    }
    if (errno == EINTR) {
      continue;
    }
    char msg[256];
    snprintf(msg, sizeof(msg), "http.poll_next failed errno=%d (%s)", errno, strerror(errno));
    fz_set_last_error(errno, 3, msg);
    return -1;
  }
}

int32_t fz_native_net_method(int32_t conn_fd) {
  pthread_mutex_lock(&fz_conn_lock);
  fz_conn_state* state = fz_conn_state_for(conn_fd, 0);
  int32_t value = state == NULL ? 0 : state->method_id;
  pthread_mutex_unlock(&fz_conn_lock);
  return value;
}

int32_t fz_native_net_path(int32_t conn_fd) {
  pthread_mutex_lock(&fz_conn_lock);
  fz_conn_state* state = fz_conn_state_for(conn_fd, 0);
  int32_t value = state == NULL ? 0 : state->path_id;
  pthread_mutex_unlock(&fz_conn_lock);
  return value;
}

int32_t fz_native_net_body(int32_t conn_fd) {
  pthread_mutex_lock(&fz_conn_lock);
  fz_conn_state* state = fz_conn_state_for(conn_fd, 0);
  int32_t value = state == NULL ? 0 : state->body_id;
  pthread_mutex_unlock(&fz_conn_lock);
  return value;
}

int32_t fz_native_net_body_read(int32_t conn_fd, int32_t max_bytes) {
  pthread_mutex_lock(&fz_conn_lock);
  fz_conn_state* state = fz_conn_state_for(conn_fd, 0);
  if (state == NULL) {
    pthread_mutex_unlock(&fz_conn_lock);
    return fz_intern_slice("", 0);
  }
  size_t prefetched = state->request_body_buf_len - state->request_body_buf_pos;
  if (state->request_body_mode == 1 && prefetched > 0 && state->request_body_remaining <= 0) {
    state->request_body_remaining = (int64_t)prefetched;
    state->request_body_eof = 0;
  }
  if (state->request_body_fully_buffered) {
    const char* body = fz_lookup_string(state->body_id);
    if (body == NULL) {
      pthread_mutex_unlock(&fz_conn_lock);
      return fz_intern_slice("", 0);
    }
    size_t total = strlen(body);
    size_t start = state->request_body_buf_pos;
    if (start >= total || max_bytes <= 0) {
      state->request_body_eof = 1;
      pthread_mutex_unlock(&fz_conn_lock);
      return fz_intern_slice("", 0);
    }
    size_t take = (size_t)max_bytes;
    if (take > total - start) {
      take = total - start;
    }
    state->request_body_buf_pos += take;
    if (state->request_body_buf_pos >= total) {
      state->request_body_eof = 1;
    }
    int32_t out = fz_intern_slice(body + start, take);
    pthread_mutex_unlock(&fz_conn_lock);
    return out;
  }
  char* chunk = NULL;
  size_t chunk_len = 0;
  int rc = fz_conn_read_body_chunk(state, &chunk, &chunk_len, max_bytes);
  if (rc < 0) {
    pthread_mutex_unlock(&fz_conn_lock);
    if (chunk != NULL) {
      free(chunk);
    }
    return fz_intern_slice("", 0);
  }
  int32_t out = fz_intern_slice(chunk == NULL ? "" : chunk, chunk_len);
  if (chunk != NULL) {
    free(chunk);
  }
  pthread_mutex_unlock(&fz_conn_lock);
  return out;
}

int32_t fz_native_net_body_eof(int32_t conn_fd) {
  pthread_mutex_lock(&fz_conn_lock);
  fz_conn_state* state = fz_conn_state_for(conn_fd, 0);
  int32_t value = (state == NULL || state->request_body_eof) ? 1 : 0;
  pthread_mutex_unlock(&fz_conn_lock);
  return value;
}

int32_t fz_native_net_body_discard(int32_t conn_fd) {
  pthread_mutex_lock(&fz_conn_lock);
  fz_conn_state* state = fz_conn_state_for(conn_fd, 0);
  int rc = state == NULL ? -1 : fz_conn_discard_body(state);
  pthread_mutex_unlock(&fz_conn_lock);
  return rc;
}

int32_t fz_native_net_body_json(int32_t conn_fd) {
  int32_t body_id = fz_native_net_body(conn_fd);
  return fz_native_json_parse(body_id);
}

int32_t fz_native_net_body_bind(int32_t conn_fd) {
  int32_t out_map = fz_runtime_map_new();
  if (out_map < 0) {
    return -1;
  }
  int32_t body = fz_native_net_body_json(conn_fd);
  if (body <= 0) {
    (void)fz_runtime_map_set(
        out_map,
        fz_intern_slice("__error", 7),
        fz_intern_slice("invalid JSON body", 17));
    return out_map;
  }
  int32_t body_id = fz_json_value_get_id(body);
  const char* raw = fz_lookup_string(body_id);
  const char* p = fz_json_ws(raw);
  if (p == NULL || *p != '{') {
    (void)fz_runtime_map_set(
        out_map,
        fz_intern_slice("__error", 7),
        fz_intern_slice("body must be JSON object", 24));
    return out_map;
  }
  p = fz_json_ws(p + 1);
  if (*p == '}') {
    return out_map;
  }
  for (;;) {
    char* key = NULL;
    if (fz_json_parse_string(&p, &key) != 0) {
      (void)fz_runtime_map_set(
          out_map,
          fz_intern_slice("__error", 7),
          fz_intern_slice("invalid JSON object key", 23));
      free(key);
      return out_map;
    }
    p = fz_json_ws(p);
    if (*p != ':') {
      (void)fz_runtime_map_set(
          out_map,
          fz_intern_slice("__error", 7),
          fz_intern_slice("invalid JSON object syntax", 26));
      free(key);
      return out_map;
    }
    p = fz_json_ws(p + 1);
    const char* value_start = p;
    if (fz_json_skip_value_token(&p, 0) != 0) {
      (void)fz_runtime_map_set(
          out_map,
          fz_intern_slice("__error", 7),
          fz_intern_slice("invalid JSON object value", 25));
      free(key);
      return out_map;
    }
    const char* value_end = p;
    int32_t key_id = fz_intern_slice(key == NULL ? "" : key, strlen(key == NULL ? "" : key));
    free(key);

    const char* q = value_start;
    char* string_value = NULL;
    int decoded = fz_json_parse_string(&q, &string_value) == 0 && fz_json_ws(q) == value_end;
    if (decoded) {
      int32_t value_id = fz_intern_slice(string_value == NULL ? "" : string_value, strlen(string_value == NULL ? "" : string_value));
      free(string_value);
      (void)fz_runtime_map_set(out_map, key_id, value_id);
    } else {
      free(string_value);
      int32_t value_id = fz_intern_slice(value_start, (size_t)(value_end - value_start));
      (void)fz_runtime_map_set(out_map, key_id, value_id);
    }
    p = fz_json_ws(p);
    if (*p == ',') {
      p = fz_json_ws(p + 1);
      continue;
    }
    if (*p == '}') {
      return out_map;
    }
    (void)fz_runtime_map_set(
        out_map,
        fz_intern_slice("__error", 7),
        fz_intern_slice("invalid JSON object terminator", 30));
    return out_map;
  }
}

int32_t fz_native_net_header(int32_t conn_fd, int32_t key_id) {
  const char* key = fz_lookup_string(key_id);
  pthread_mutex_lock(&fz_conn_lock);
  fz_conn_state* state = fz_conn_state_for(conn_fd, 0);
  if (state == NULL || key == NULL) {
    pthread_mutex_unlock(&fz_conn_lock);
    return fz_intern_slice("", 0);
  }
  for (int i = 0; i < state->header_count; i++) {
    const char* k = fz_lookup_string(state->header_key_ids[i]);
    if (k != NULL && strcasecmp(k, key) == 0) {
      int32_t value = state->header_value_ids[i];
      pthread_mutex_unlock(&fz_conn_lock);
      return value;
    }
  }
  pthread_mutex_unlock(&fz_conn_lock);
  return fz_intern_slice("", 0);
}

int32_t fz_native_net_query(int32_t conn_fd, int32_t key_id) {
  const char* key = fz_lookup_string(key_id);
  pthread_mutex_lock(&fz_conn_lock);
  fz_conn_state* state = fz_conn_state_for(conn_fd, 0);
  if (state == NULL || key == NULL) {
    pthread_mutex_unlock(&fz_conn_lock);
    return fz_intern_slice("", 0);
  }
  for (int i = 0; i < state->query_count; i++) {
    const char* k = fz_lookup_string(state->query_key_ids[i]);
    if (k != NULL && strcmp(k, key) == 0) {
      int32_t value = state->query_value_ids[i];
      pthread_mutex_unlock(&fz_conn_lock);
      return value;
    }
  }
  pthread_mutex_unlock(&fz_conn_lock);
  return fz_intern_slice("", 0);
}

int32_t fz_native_net_param(int32_t conn_fd, int32_t key_id) {
  const char* key = fz_lookup_string(key_id);
  pthread_mutex_lock(&fz_conn_lock);
  fz_conn_state* state = fz_conn_state_for(conn_fd, 0);
  if (state == NULL || key == NULL) {
    pthread_mutex_unlock(&fz_conn_lock);
    return fz_intern_slice("", 0);
  }
  for (int i = 0; i < state->param_count; i++) {
    const char* k = fz_lookup_string(state->param_key_ids[i]);
    if (k != NULL && strcmp(k, key) == 0) {
      int32_t value = state->param_value_ids[i];
      pthread_mutex_unlock(&fz_conn_lock);
      return value;
    }
  }
  pthread_mutex_unlock(&fz_conn_lock);
  return fz_intern_slice("", 0);
}

int32_t fz_native_net_headers(int32_t conn_fd) {
  pthread_mutex_lock(&fz_conn_lock);
  fz_conn_state* state = fz_conn_state_for(conn_fd, 0);
  if (state == NULL) {
    pthread_mutex_unlock(&fz_conn_lock);
    return -1;
  }
  pthread_mutex_lock(&fz_list_lock);
  int32_t list_handle = fz_list_alloc();
  fz_list_state* list = fz_list_get(list_handle);
  if (list != NULL) {
    for (int i = 0; i < state->header_count; i++) {
      const char* k = fz_lookup_string(state->header_key_ids[i]);
      const char* v = fz_lookup_string(state->header_value_ids[i]);
      size_t n = strlen(k == NULL ? "" : k) + strlen(v == NULL ? "" : v) + 3;
      char* kv = (char*)malloc(n);
      if (kv == NULL) continue;
      snprintf(kv, n, "%s:%s", k == NULL ? "" : k, v == NULL ? "" : v);
      (void)fz_list_push_cstr(list, kv);
      free(kv);
    }
  }
  pthread_mutex_unlock(&fz_list_lock);
  pthread_mutex_unlock(&fz_conn_lock);
  return list_handle;
}

int32_t fz_native_net_request_id(int32_t conn_fd) {
  pthread_mutex_lock(&fz_conn_lock);
  fz_conn_state* state = fz_conn_state_for(conn_fd, 0);
  int32_t value = state == NULL ? 0 : state->request_id;
  pthread_mutex_unlock(&fz_conn_lock);
  return value;
}

int32_t fz_native_net_remote_addr(int32_t conn_fd) {
  pthread_mutex_lock(&fz_conn_lock);
  fz_conn_state* state = fz_conn_state_for(conn_fd, 0);
  int32_t value = state == NULL ? 0 : state->remote_addr_id;
  pthread_mutex_unlock(&fz_conn_lock);
  return value;
}

int32_t fz_native_net_response_header_set(int32_t conn_fd, int32_t key_id, int32_t value_id) {
  const char* key = fz_lookup_string(key_id);
  if (key == NULL || key[0] == '\0') {
    return -1;
  }
  pthread_mutex_lock(&fz_conn_lock);
  fz_conn_state* state = fz_conn_state_for(conn_fd, 0);
  if (state == NULL) {
    pthread_mutex_unlock(&fz_conn_lock);
    return -1;
  }
  for (int i = 0; i < state->response_header_count; i++) {
    const char* existing = fz_lookup_string(state->response_header_key_ids[i]);
    if (existing != NULL && strcasecmp(existing, key) == 0) {
      state->response_header_value_ids[i] = value_id;
      pthread_mutex_unlock(&fz_conn_lock);
      return 0;
    }
  }
  if (state->response_header_count >= FZ_MAX_CONN_META) {
    pthread_mutex_unlock(&fz_conn_lock);
    return -1;
  }
  state->response_header_key_ids[state->response_header_count] = key_id;
  state->response_header_value_ids[state->response_header_count] = value_id;
  state->response_header_count++;
  pthread_mutex_unlock(&fz_conn_lock);
  return 0;
}

int32_t fz_native_net_response_header_add(int32_t conn_fd, int32_t key_id, int32_t value_id) {
  const char* key = fz_lookup_string(key_id);
  if (key == NULL || key[0] == '\0') {
    return -1;
  }
  pthread_mutex_lock(&fz_conn_lock);
  fz_conn_state* state = fz_conn_state_for(conn_fd, 0);
  if (state == NULL || state->response_header_count >= FZ_MAX_CONN_META) {
    pthread_mutex_unlock(&fz_conn_lock);
    return -1;
  }
  state->response_header_key_ids[state->response_header_count] = key_id;
  state->response_header_value_ids[state->response_header_count] = value_id;
  state->response_header_count++;
  pthread_mutex_unlock(&fz_conn_lock);
  return 0;
}

int32_t fz_native_net_response_header_clear(int32_t conn_fd) {
  pthread_mutex_lock(&fz_conn_lock);
  fz_conn_state* state = fz_conn_state_for(conn_fd, 0);
  if (state != NULL) {
    fz_conn_state_reset_response_headers(state);
  }
  pthread_mutex_unlock(&fz_conn_lock);
  return 0;
}

int32_t fz_native_net_websocket_accept(int32_t conn_fd) {
  pthread_mutex_lock(&fz_conn_lock);
  fz_conn_state* state = fz_conn_state_for(conn_fd, 0);
  if (state == NULL) {
    pthread_mutex_unlock(&fz_conn_lock);
    return -1;
  }
  const char* upgrade = "";
  const char* connection = "";
  const char* ws_key = "";
  const char* ws_version = "";
  for (int i = 0; i < state->header_count; i++) {
    const char* key = fz_lookup_string(state->header_key_ids[i]);
    const char* value = fz_lookup_string(state->header_value_ids[i]);
    if (key == NULL || value == NULL) {
      continue;
    }
    if (strcasecmp(key, "upgrade") == 0) {
      upgrade = value;
    } else if (strcasecmp(key, "connection") == 0) {
      connection = value;
    } else if (strcasecmp(key, "sec-websocket-key") == 0) {
      ws_key = value;
    } else if (strcasecmp(key, "sec-websocket-version") == 0) {
      ws_version = value;
    }
  }
  if (upgrade == NULL || strcasecmp(upgrade, "websocket") != 0
      || connection == NULL || fz_contains_ci(connection, strlen(connection), "upgrade") == 0
      || ws_key == NULL || ws_key[0] == '\0'
      || ws_version == NULL || strcmp(ws_version, "13") != 0) {
    pthread_mutex_unlock(&fz_conn_lock);
    return -1;
  }
  const char* guid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  size_t concat_len = strlen(ws_key) + strlen(guid);
  char* concat = (char*)malloc(concat_len + 1);
  if (concat == NULL) {
    pthread_mutex_unlock(&fz_conn_lock);
    return -1;
  }
  snprintf(concat, concat_len + 1, "%s%s", ws_key, guid);
  uint8_t digest[20];
  fz_sha1_compute((const uint8_t*)concat, concat_len, digest);
  free(concat);
  char* accept_value = fz_base64_encode(digest, sizeof(digest));
  if (accept_value == NULL) {
    pthread_mutex_unlock(&fz_conn_lock);
    return -1;
  }
  if (state->response_header_count + 3 <= FZ_MAX_CONN_META) {
    state->response_header_key_ids[state->response_header_count] = fz_intern_slice("Upgrade", 7);
    state->response_header_value_ids[state->response_header_count] = fz_intern_slice("websocket", 9);
    state->response_header_count++;
    state->response_header_key_ids[state->response_header_count] = fz_intern_slice("Connection", 10);
    state->response_header_value_ids[state->response_header_count] = fz_intern_slice("Upgrade", 7);
    state->response_header_count++;
    state->response_header_key_ids[state->response_header_count] = fz_intern_slice("Sec-WebSocket-Accept", 20);
    state->response_header_value_ids[state->response_header_count] = fz_intern_slice(accept_value, strlen(accept_value));
    state->response_header_count++;
  }
  free(accept_value);
  int rc = fz_send_http_response_state(state, 101, "", "", 0);
  if (rc != 0) {
    pthread_mutex_unlock(&fz_conn_lock);
    return -1;
  }
  int32_t handle = fz_websocket_state_alloc(conn_fd);
  pthread_mutex_unlock(&fz_conn_lock);
  return handle;
}

int32_t fz_native_net_websocket_read(int32_t ws_handle, int32_t max_bytes) {
  fz_websocket_state* ws = fz_websocket_state_get(ws_handle);
  if (ws == NULL) {
    return fz_intern_slice("", 0);
  }
  int32_t kind_id = 0;
  int32_t close_code = 0;
  int32_t error_id = 0;
  int32_t payload_id = fz_websocket_read_frame(ws, max_bytes, &kind_id, &close_code, &error_id);
  ws->last_kind_id = kind_id;
  ws->close_code = close_code;
  ws->last_error_id = error_id;
  return payload_id;
}

int32_t fz_native_net_websocket_kind(int32_t ws_handle) {
  fz_websocket_state* ws = fz_websocket_state_get(ws_handle);
  return ws == NULL ? fz_intern_slice("error", 5) : ws->last_kind_id;
}

int32_t fz_native_net_websocket_close_code(int32_t ws_handle) {
  fz_websocket_state* ws = fz_websocket_state_get(ws_handle);
  return ws == NULL ? 0 : ws->close_code;
}

int32_t fz_native_net_websocket_error(int32_t ws_handle) {
  fz_websocket_state* ws = fz_websocket_state_get(ws_handle);
  return ws == NULL ? fz_intern_slice("websocket not found", 19) : ws->last_error_id;
}

int32_t fz_native_net_websocket_write_text(int32_t ws_handle, int32_t payload_id) {
  fz_websocket_state* ws = fz_websocket_state_get(ws_handle);
  const char* payload = fz_lookup_string(payload_id);
  return ws == NULL ? -1 : fz_websocket_write_frame(ws->fd, 0x1u, payload, strlen(payload == NULL ? "" : payload));
}

int32_t fz_native_net_websocket_write_binary(int32_t ws_handle, int32_t payload_id) {
  fz_websocket_state* ws = fz_websocket_state_get(ws_handle);
  const char* payload = fz_lookup_string(payload_id);
  return ws == NULL ? -1 : fz_websocket_write_frame(ws->fd, 0x2u, payload, strlen(payload == NULL ? "" : payload));
}

int32_t fz_native_net_websocket_ping(int32_t ws_handle, int32_t payload_id) {
  fz_websocket_state* ws = fz_websocket_state_get(ws_handle);
  const char* payload = fz_lookup_string(payload_id);
  return ws == NULL ? -1 : fz_websocket_write_frame(ws->fd, 0x9u, payload, strlen(payload == NULL ? "" : payload));
}

int32_t fz_native_net_websocket_pong(int32_t ws_handle, int32_t payload_id) {
  fz_websocket_state* ws = fz_websocket_state_get(ws_handle);
  const char* payload = fz_lookup_string(payload_id);
  return ws == NULL ? -1 : fz_websocket_write_frame(ws->fd, 0xAu, payload, strlen(payload == NULL ? "" : payload));
}

int32_t fz_native_net_websocket_close(int32_t ws_handle, int32_t code, int32_t reason_id) {
  fz_websocket_state* ws = fz_websocket_state_get(ws_handle);
  if (ws == NULL) {
    return -1;
  }
  const char* reason = fz_lookup_string(reason_id);
  size_t reason_len = strlen(reason == NULL ? "" : reason);
  char* payload = (char*)malloc(reason_len + 2);
  if (payload == NULL) {
    return -1;
  }
  payload[0] = (char)((code >> 8) & 0xFF);
  payload[1] = (char)(code & 0xFF);
  if (reason_len > 0) {
    memcpy(payload + 2, reason, reason_len);
  }
  int rc = fz_websocket_write_frame(ws->fd, 0x8u, payload, reason_len + 2);
  free(payload);
  ws->closed = 1;
  return rc;
}

static int fz_route_match_path_and_capture(fz_conn_state* state, const char* pattern) {
  if (state == NULL || pattern == NULL) {
    return 0;
  }
  const char* path = fz_lookup_string(state->path_id);
  if (path == NULL) path = "";
  state->param_count = 0;
  const char* p = path;
  const char* t = pattern;
  while (*p == '/') p++;
  while (*t == '/') t++;
  for (;;) {
    const char* p_end = strchr(p, '/');
    const char* t_end = strchr(t, '/');
    size_t p_len = p_end == NULL ? strlen(p) : (size_t)(p_end - p);
    size_t t_len = t_end == NULL ? strlen(t) : (size_t)(t_end - t);
    if (p_len == 0 && t_len == 0) return 1;
    if (p_len == 0 || t_len == 0) return 0;
    if (t[0] == ':') {
      if (state->param_count < FZ_MAX_ROUTE_PARAMS) {
        state->param_key_ids[state->param_count] = fz_intern_slice(t + 1, t_len - 1);
        state->param_value_ids[state->param_count] = fz_intern_slice(p, p_len);
        state->param_count++;
      }
    } else if (p_len != t_len || strncmp(p, t, p_len) != 0) {
      return 0;
    }
    if (p_end == NULL && t_end == NULL) return 1;
    if (p_end == NULL || t_end == NULL) return 0;
    p = p_end + 1;
    t = t_end + 1;
  }
}

int32_t fz_native_route_match(int32_t conn_fd, int32_t method_id, int32_t pattern_id) {
  const char* method = fz_lookup_string(method_id);
  const char* pattern = fz_lookup_string(pattern_id);
  pthread_mutex_lock(&fz_conn_lock);
  fz_conn_state* state = fz_conn_state_for(conn_fd, 0);
  if (state == NULL) {
    pthread_mutex_unlock(&fz_conn_lock);
    return 0;
  }
  if (method != NULL && method[0] != '\0') {
    const char* req_method = fz_lookup_string(state->method_id);
    if (req_method == NULL || strcmp(req_method, method) != 0) {
      pthread_mutex_unlock(&fz_conn_lock);
      return 0;
    }
  }
  int ok = fz_route_match_path_and_capture(state, pattern == NULL ? "" : pattern);
  pthread_mutex_unlock(&fz_conn_lock);
  return ok ? 1 : 0;
}

int32_t fz_native_route_write_404(int32_t conn_fd) {
  return fz_native_net_write(conn_fd, 404, fz_intern_slice("not found", 9));
}

int32_t fz_native_route_write_405(int32_t conn_fd) {
  return fz_native_net_write(conn_fd, 405, fz_intern_slice("method not allowed", 18));
}

int32_t fz_native_net_write_response(
    int32_t conn_fd,
    int32_t status_code,
    int32_t content_type_id,
    int32_t body_id,
    int32_t close_after) {
  const char* content_type = fz_lookup_string(content_type_id);
  const char* body = fz_lookup_string(body_id);
  return fz_send_http_response(conn_fd, status_code, content_type, body, close_after != 0);
}

int32_t fz_native_net_write(int32_t conn_fd, int32_t status_code, int32_t body_id) {
  int close_after = 1;
  pthread_mutex_lock(&fz_conn_lock);
  fz_conn_state* state = fz_conn_state_for(conn_fd, 0);
  if (state != NULL) {
    close_after = state->keep_alive ? 0 : 1;
  }
  pthread_mutex_unlock(&fz_conn_lock);
  return fz_send_http_response(
      conn_fd,
      status_code,
      "text/plain; charset=utf-8",
      fz_lookup_string(body_id),
      close_after);
}

static char* fz_json_escape_string_bytes(const char* raw) {
  if (raw == NULL) {
    raw = "";
  }
  size_t len = strlen(raw);
  size_t cap = (len * 6) + 1;
  char* out = (char*)malloc(cap);
  if (out == NULL) {
    return NULL;
  }
  size_t used = 0;
  for (size_t i = 0; i < len; i++) {
    unsigned char ch = (unsigned char)raw[i];
    if (ch == '\"' || ch == '\\') {
      out[used++] = '\\';
      out[used++] = (char)ch;
      continue;
    }
    switch (ch) {
      case '\b':
        out[used++] = '\\';
        out[used++] = 'b';
        break;
      case '\f':
        out[used++] = '\\';
        out[used++] = 'f';
        break;
      case '\n':
        out[used++] = '\\';
        out[used++] = 'n';
        break;
      case '\r':
        out[used++] = '\\';
        out[used++] = 'r';
        break;
      case '\t':
        out[used++] = '\\';
        out[used++] = 't';
        break;
      default:
        if (ch < 0x20) {
          (void)snprintf(out + used, cap - used, "\\u%04x", (unsigned int)ch);
          used += 6;
        } else {
          out[used++] = (char)ch;
        }
        break;
    }
  }
  out[used] = '\0';
  return out;
}

static int32_t fz_json_wrap_invalid_payload(const char* raw) {
  char* escaped = fz_json_escape_string_bytes(raw);
  if (escaped == NULL) {
    const char* fallback =
        "{\"error\":\"invalid_json_payload\",\"message\":\"http.write_json could not allocate sanitize buffer\"}";
    return fz_intern_slice(fallback, strlen(fallback));
  }
  const char* prefix =
      "{\"error\":\"invalid_json_payload\",\"message\":\"http.write_json sanitized non-JSON body\",\"raw\":\"";
  const char* suffix = "\"}";
  size_t total = strlen(prefix) + strlen(escaped) + strlen(suffix) + 1;
  char* wrapped = (char*)malloc(total);
  if (wrapped == NULL) {
    free(escaped);
    const char* fallback =
        "{\"error\":\"invalid_json_payload\",\"message\":\"http.write_json sanitize alloc failed\"}";
    return fz_intern_slice(fallback, strlen(fallback));
  }
  snprintf(wrapped, total, "%s%s%s", prefix, escaped, suffix);
  free(escaped);
  return fz_intern_owned(wrapped);
}

int32_t fz_native_net_write_json(int32_t conn_fd, int32_t status_code, int32_t body_id) {
  const char* body = fz_lookup_string(body_id);
  if (body == NULL || body[0] == '\0') {
    body = "null";
  }
  const char* send_body = body;
  int32_t replacement_id = 0;
  const char* start = NULL;
  const char* end = NULL;
  if (fz_json_parse_value_slice(body, &start, &end) != 0) {
    replacement_id = fz_json_wrap_invalid_payload(body);
    send_body = fz_lookup_string(replacement_id);
    fz_set_last_error(
        EINVAL,
        3,
        "http.write_json received invalid JSON body; response was sanitized");
  } else {
    fz_set_last_error(0, 0, "");
  }
  int close_after = 1;
  pthread_mutex_lock(&fz_conn_lock);
  fz_conn_state* state = fz_conn_state_for(conn_fd, 0);
  if (state != NULL) {
    close_after = state->keep_alive ? 0 : 1;
  }
  pthread_mutex_unlock(&fz_conn_lock);
  return fz_send_http_response(
      conn_fd,
      status_code,
      "application/json",
      send_body,
      close_after);
}

int32_t fz_native_close(int32_t fd) {
  if (fd >= 0) {
    shutdown(fd, SHUT_RDWR);
    close(fd);
  }
  for (int i = 0; i < FZ_MAX_WEBSOCKETS; i++) {
    if (fz_websocket_states[i].in_use && fz_websocket_states[i].fd == fd) {
      memset(&fz_websocket_states[i], 0, sizeof(fz_websocket_states[i]));
    }
  }
  fz_conn_state_drop(fd);
  return 0;
}

static const char* fz_json_skip_ws(const char* p) {
  while (p != NULL && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) {
    p++;
  }
  return p;
}

static int fz_json_parse_string(const char** cursor, char** out) {
  if (cursor == NULL || *cursor == NULL || out == NULL) {
    return -1;
  }
  const char* p = fz_json_skip_ws(*cursor);
  if (p == NULL || *p != '\"') {
    return -1;
  }
  p++;
  size_t cap = 32;
  size_t len = 0;
  char* buf = (char*)malloc(cap);
  if (buf == NULL) {
    return -1;
  }
  while (*p != '\0') {
    char ch = *p++;
    if (ch == '\"') {
      buf[len] = '\0';
      *out = buf;
      *cursor = p;
      return 0;
    }
    if (ch == '\\') {
      char esc = *p++;
      if (esc == '\0') {
        free(buf);
        return -1;
      }
      switch (esc) {
        case '\"': ch = '\"'; break;
        case '\\': ch = '\\'; break;
        case '/': ch = '/'; break;
        case 'b': ch = '\b'; break;
        case 'f': ch = '\f'; break;
        case 'n': ch = '\n'; break;
        case 'r': ch = '\r'; break;
        case 't': ch = '\t'; break;
        case 'u':
          for (int i = 0; i < 4; i++) {
            if (!isxdigit((unsigned char)p[i])) {
              free(buf);
              return -1;
            }
          }
          p += 4;
          ch = '?';
          break;
        default:
          free(buf);
          return -1;
      }
    }
    if (len + 2 > cap) {
      cap *= 2;
      char* next = (char*)realloc(buf, cap);
      if (next == NULL) {
        free(buf);
        return -1;
      }
      buf = next;
    }
    buf[len++] = ch;
  }
  free(buf);
  return -1;
}

static int fz_parse_json_string_array(const char* raw, char*** out_items, int* out_count) {
  if (out_items == NULL || out_count == NULL) {
    return -1;
  }
  *out_items = NULL;
  *out_count = 0;
  if (raw == NULL || raw[0] == '\0') {
    return 0;
  }
  const char* p = fz_json_skip_ws(raw);
  if (*p != '[') {
    return -1;
  }
  p = fz_json_skip_ws(p + 1);
  int cap = 4;
  int count = 0;
  char** items = (char**)calloc((size_t)cap, sizeof(char*));
  if (items == NULL) {
    return -1;
  }
  if (*p == ']') {
    *out_items = items;
    *out_count = 0;
    return 0;
  }
  for (;;) {
    char* item = NULL;
    if (fz_json_parse_string(&p, &item) != 0) {
      for (int i = 0; i < count; i++) free(items[i]);
      free(items);
      return -1;
    }
    if (count >= cap) {
      cap *= 2;
      char** next = (char**)realloc(items, (size_t)cap * sizeof(char*));
      if (next == NULL) {
        free(item);
        for (int i = 0; i < count; i++) free(items[i]);
        free(items);
        return -1;
      }
      items = next;
    }
    items[count++] = item;
    p = fz_json_skip_ws(p);
    if (*p == ',') {
      p = fz_json_skip_ws(p + 1);
      continue;
    }
    if (*p == ']') {
      break;
    }
    for (int i = 0; i < count; i++) free(items[i]);
    free(items);
    return -1;
  }
  *out_items = items;
  *out_count = count;
  return 0;
}

static int fz_parse_json_env_object(const char* raw, char*** out_items, int* out_count) {
  if (out_items == NULL || out_count == NULL) {
    return -1;
  }
  *out_items = NULL;
  *out_count = 0;
  if (raw == NULL || raw[0] == '\0') {
    return 0;
  }
  const char* p = fz_json_skip_ws(raw);
  if (*p != '{') {
    return -1;
  }
  p = fz_json_skip_ws(p + 1);
  int cap = 4;
  int count = 0;
  char** entries = (char**)calloc((size_t)cap, sizeof(char*));
  if (entries == NULL) {
    return -1;
  }
  if (*p == '}') {
    *out_items = entries;
    *out_count = 0;
    return 0;
  }
  for (;;) {
    char* key = NULL;
    char* value = NULL;
    if (fz_json_parse_string(&p, &key) != 0) {
      for (int i = 0; i < count; i++) free(entries[i]);
      free(entries);
      return -1;
    }
    p = fz_json_skip_ws(p);
    if (*p != ':') {
      free(key);
      for (int i = 0; i < count; i++) free(entries[i]);
      free(entries);
      return -1;
    }
    p = fz_json_skip_ws(p + 1);
    if (fz_json_parse_string(&p, &value) != 0) {
      free(key);
      for (int i = 0; i < count; i++) free(entries[i]);
      free(entries);
      return -1;
    }
    size_t n = strlen(key) + strlen(value) + 2;
    char* joined = (char*)malloc(n);
    if (joined == NULL) {
      free(key);
      free(value);
      for (int i = 0; i < count; i++) free(entries[i]);
      free(entries);
      return -1;
    }
    snprintf(joined, n, "%s=%s", key, value);
    free(key);
    free(value);
    if (count >= cap) {
      cap *= 2;
      char** next = (char**)realloc(entries, (size_t)cap * sizeof(char*));
      if (next == NULL) {
        free(joined);
        for (int i = 0; i < count; i++) free(entries[i]);
        free(entries);
        return -1;
      }
      entries = next;
    }
    entries[count++] = joined;
    p = fz_json_skip_ws(p);
    if (*p == ',') {
      p = fz_json_skip_ws(p + 1);
      continue;
    }
    if (*p == '}') {
      break;
    }
    for (int i = 0; i < count; i++) free(entries[i]);
    free(entries);
    return -1;
  }
  *out_items = entries;
  *out_count = count;
  return 0;
}

static void fz_free_string_list(char** items, int count) {
  if (items == NULL) {
    return;
  }
  for (int i = 0; i < count; i++) {
    free(items[i]);
  }
  free(items);
}


static int fz_env_key_match(const char* entry, const char* key, size_t key_len) {
  if (entry == NULL || key == NULL) {
    return 0;
  }
  return strncmp(entry, key, key_len) == 0 && entry[key_len] == '=';
}

static char** fz_clone_env_with_overrides(char** overrides, int override_count) {
  int base_count = 0;
  while (environ != NULL && environ[base_count] != NULL) {
    base_count++;
  }
  int cap = base_count + override_count + 1;
  char** envp = (char**)calloc((size_t)cap, sizeof(char*));
  if (envp == NULL) {
    return NULL;
  }
  int count = 0;
  for (int i = 0; i < base_count; i++) {
    envp[count] = strdup(environ[i]);
    if (envp[count] == NULL) {
      for (int j = 0; j < count; j++) free(envp[j]);
      free(envp);
      return NULL;
    }
    count++;
  }
  for (int i = 0; i < override_count; i++) {
    const char* item = overrides[i];
    const char* eq = item == NULL ? NULL : strchr(item, '=');
    if (eq == NULL || eq == item) {
      continue;
    }
    size_t key_len = (size_t)(eq - item);
    int replaced = 0;
    for (int j = 0; j < count; j++) {
      if (fz_env_key_match(envp[j], item, key_len)) {
        char* dup = strdup(item);
        if (dup == NULL) {
          continue;
        }
        free(envp[j]);
        envp[j] = dup;
        replaced = 1;
        break;
      }
    }
    if (!replaced && count < cap - 1) {
      envp[count] = strdup(item);
      if (envp[count] != NULL) {
        count++;
      }
    }
  }
  envp[count] = NULL;
  return envp;
}

static void fz_free_env(char** envp) {
  if (envp == NULL) {
    return;
  }
  for (int i = 0; envp[i] != NULL; i++) {
    free(envp[i]);
  }
  free(envp);
}

static int32_t fz_native_proc_spawn_argv(
    const char* executable,
    char* const* argv,
    char* const* envp,
    const char* stdin_payload) {
  if (executable == NULL || executable[0] == '\0' || argv == NULL || argv[0] == NULL) {
    fz_last_exit_class = 3;
    fz_proc_set_last_error("proc_spawn: invalid argv");
    return -1;
  }

  int out_pipe[2];
  int err_pipe[2];
  int in_pipe[2] = {-1, -1};
  if (pipe(out_pipe) != 0) {
    fz_last_exit_class = 3;
    fz_proc_set_last_error("proc_spawn: stdout pipe failed");
    return -1;
  }
  if (pipe(err_pipe) != 0) {
    close(out_pipe[0]);
    close(out_pipe[1]);
    fz_last_exit_class = 3;
    fz_proc_set_last_error("proc_spawn: stderr pipe failed");
    return -1;
  }
  if (stdin_payload != NULL && pipe(in_pipe) != 0) {
    close(out_pipe[0]);
    close(out_pipe[1]);
    close(err_pipe[0]);
    close(err_pipe[1]);
    fz_last_exit_class = 3;
    fz_proc_set_last_error("proc_spawn: stdin pipe failed");
    return -1;
  }
  (void)fz_mark_cloexec(out_pipe[0]);
  (void)fz_mark_cloexec(err_pipe[0]);

  posix_spawn_file_actions_t file_actions;
  if (posix_spawn_file_actions_init(&file_actions) != 0) {
    close(out_pipe[0]);
    close(out_pipe[1]);
    close(err_pipe[0]);
    close(err_pipe[1]);
    if (in_pipe[0] >= 0) {
      close(in_pipe[0]);
      close(in_pipe[1]);
    }
    fz_last_exit_class = 3;
    fz_proc_set_last_error("proc_spawn: file actions init failed");
    return -1;
  }
  int file_actions_ok = 1;
  if (posix_spawn_file_actions_adddup2(&file_actions, out_pipe[1], STDOUT_FILENO) != 0) {
    file_actions_ok = 0;
  }
  if (file_actions_ok
      && posix_spawn_file_actions_adddup2(&file_actions, err_pipe[1], STDERR_FILENO) != 0) {
    file_actions_ok = 0;
  }
  if (file_actions_ok && in_pipe[0] >= 0
      && posix_spawn_file_actions_adddup2(&file_actions, in_pipe[0], STDIN_FILENO) != 0) {
    file_actions_ok = 0;
  }
  if (file_actions_ok
      && posix_spawn_file_actions_addclose(&file_actions, out_pipe[0]) != 0) {
    file_actions_ok = 0;
  }
  if (file_actions_ok
      && posix_spawn_file_actions_addclose(&file_actions, out_pipe[1]) != 0) {
    file_actions_ok = 0;
  }
  if (file_actions_ok
      && posix_spawn_file_actions_addclose(&file_actions, err_pipe[0]) != 0) {
    file_actions_ok = 0;
  }
  if (file_actions_ok
      && posix_spawn_file_actions_addclose(&file_actions, err_pipe[1]) != 0) {
    file_actions_ok = 0;
  }
  if (file_actions_ok && in_pipe[0] >= 0
      && posix_spawn_file_actions_addclose(&file_actions, in_pipe[0]) != 0) {
    file_actions_ok = 0;
  }
  if (file_actions_ok && in_pipe[1] >= 0
      && posix_spawn_file_actions_addclose(&file_actions, in_pipe[1]) != 0) {
    file_actions_ok = 0;
  }
  if (!file_actions_ok) {
    (void)posix_spawn_file_actions_destroy(&file_actions);
    close(out_pipe[0]);
    close(out_pipe[1]);
    close(err_pipe[0]);
    close(err_pipe[1]);
    if (in_pipe[0] >= 0) {
      close(in_pipe[0]);
      close(in_pipe[1]);
    }
    fz_last_exit_class = 3;
    fz_proc_set_last_error("proc_spawn: file actions setup failed");
    return -1;
  }

  pid_t pid = 0;
  int spawn_rc = posix_spawnp(
      &pid,
      executable,
      &file_actions,
      NULL,
      argv,
      envp == NULL ? environ : envp);
  (void)posix_spawn_file_actions_destroy(&file_actions);
  if (spawn_rc != 0 || pid <= 0) {
    close(out_pipe[0]);
    close(out_pipe[1]);
    close(err_pipe[0]);
    close(err_pipe[1]);
    if (in_pipe[0] >= 0) {
      close(in_pipe[0]);
      close(in_pipe[1]);
    }
    fz_last_exit_class = 3;
    fz_proc_set_last_error("proc_spawn: posix_spawnp failed");
    return -1;
  }

  if (in_pipe[0] >= 0) {
    close(in_pipe[0]);
    size_t remaining = strlen(stdin_payload);
    const char* cursor = stdin_payload;
    while (remaining > 0) {
      ssize_t wrote = write(in_pipe[1], cursor, remaining);
      if (wrote < 0) {
        if (errno == EINTR) {
          continue;
        }
        break;
      }
      if (wrote == 0) {
        break;
      }
      cursor += wrote;
      remaining -= (size_t)wrote;
    }
    close(in_pipe[1]);
  }

  close(out_pipe[1]);
  close(err_pipe[1]);
  (void)fz_set_nonblocking(out_pipe[0]);
  (void)fz_set_nonblocking(err_pipe[0]);

  pthread_mutex_lock(&fz_proc_lock);
  int32_t handle = fz_proc_state_alloc(pid, out_pipe[0], err_pipe[0]);
  pthread_mutex_unlock(&fz_proc_lock);
  if (handle < 0) {
    kill(pid, SIGKILL);
    close(out_pipe[0]);
    close(err_pipe[0]);
    fz_last_exit_class = 3;
    fz_proc_set_last_error("proc_spawn: state allocation failed");
    return -1;
  }
  fz_proc_set_last_error("");
  return handle;
}

static int fz_proc_poll_streams(fz_proc_state* state, int timeout_ms) {
  if (state == NULL) {
    return -1;
  }
  struct pollfd pfds[2];
  int count = 0;
  if (state->stdout_fd >= 0) {
    pfds[count].fd = state->stdout_fd;
    pfds[count].events = POLLIN | POLLHUP | POLLERR;
    pfds[count].revents = 0;
    count++;
  }
  if (state->stderr_fd >= 0) {
    pfds[count].fd = state->stderr_fd;
    pfds[count].events = POLLIN | POLLHUP | POLLERR;
    pfds[count].revents = 0;
    count++;
  }
  if (count == 0) {
    if (timeout_ms > 0) {
      usleep((useconds_t)timeout_ms * 1000);
    }
    return 0;
  }
  for (;;) {
    int ready = poll(pfds, (nfds_t)count, timeout_ms);
    if (ready >= 0) {
      return ready;
    }
    if (errno == EINTR) {
      continue;
    }
    return -1;
  }
}

int32_t fz_native_proc_spawn(int32_t command_id) {
  const char* command = fz_lookup_string(command_id);
  if (command == NULL || command[0] == '\0') {
    fz_last_exit_class = 3;
    fz_proc_set_last_error("proc_spawn: empty command");
    return -1;
  }
  char* const argv[] = {"sh", "-lc", (char*)command, NULL};
  return fz_native_proc_spawn_argv("sh", argv, environ, NULL);
}

static int fz_clone_list_items(int32_t list_handle, char*** out_items, int* out_count) {
  if (out_items == NULL || out_count == NULL) {
    return -1;
  }
  *out_items = NULL;
  *out_count = 0;
  if (list_handle <= 0) {
    return 0;
  }
  pthread_mutex_lock(&fz_list_lock);
  fz_list_state* list = fz_list_get(list_handle);
  if (list == NULL || list->count <= 0) {
    pthread_mutex_unlock(&fz_list_lock);
    return 0;
  }
  int count = list->count;
  char** items = (char**)calloc((size_t)count, sizeof(char*));
  if (items == NULL) {
    pthread_mutex_unlock(&fz_list_lock);
    return -1;
  }
  for (int i = 0; i < count; i++) {
    const char* src = list->items[i] == NULL ? "" : list->items[i];
    items[i] = strdup(src);
    if (items[i] == NULL) {
      for (int j = 0; j < i; j++) {
        free(items[j]);
      }
      free(items);
      pthread_mutex_unlock(&fz_list_lock);
      return -1;
    }
  }
  pthread_mutex_unlock(&fz_list_lock);
  *out_items = items;
  *out_count = count;
  return 0;
}

static int fz_clone_map_entries_as_env(int32_t map_handle, char*** out_items, int* out_count) {
  if (out_items == NULL || out_count == NULL) {
    return -1;
  }
  *out_items = NULL;
  *out_count = 0;
  if (map_handle <= 0) {
    return 0;
  }
  pthread_mutex_lock(&fz_map_lock);
  fz_map_state* map = fz_map_get(map_handle);
  if (map == NULL || map->count <= 0) {
    pthread_mutex_unlock(&fz_map_lock);
    return 0;
  }
  int count = map->count;
  char** entries = (char**)calloc((size_t)count, sizeof(char*));
  if (entries == NULL) {
    pthread_mutex_unlock(&fz_map_lock);
    return -1;
  }
  for (int i = 0; i < count; i++) {
    const char* key = map->keys[i] == NULL ? "" : map->keys[i];
    const char* value = map->values[i] == NULL ? "" : map->values[i];
    size_t n = strlen(key) + strlen(value) + 2;
    entries[i] = (char*)malloc(n);
    if (entries[i] == NULL) {
      for (int j = 0; j < i; j++) {
        free(entries[j]);
      }
      free(entries);
      pthread_mutex_unlock(&fz_map_lock);
      return -1;
    }
    snprintf(entries[i], n, "%s=%s", key, value);
  }
  pthread_mutex_unlock(&fz_map_lock);
  *out_items = entries;
  *out_count = count;
  return 0;
}

int32_t fz_native_proc_spawnl(
    int32_t command_id,
    int32_t args_list_id,
    int32_t env_map_id,
    int32_t stdin_id) {
  const char* command = fz_lookup_string(command_id);
  const char* stdin_payload = fz_lookup_string(stdin_id);
  if (command == NULL || command[0] == '\0') {
    fz_last_exit_class = 3;
    fz_proc_set_last_error("proc_spawnl: empty command");
    return -1;
  }

  char** arg_items = NULL;
  int arg_count = 0;
  if (fz_clone_list_items(args_list_id, &arg_items, &arg_count) != 0) {
    fz_last_exit_class = 3;
    fz_proc_set_last_error("proc_spawnl: args_list clone failed");
    return -1;
  }

  char** env_items = NULL;
  int env_count = 0;
  if (fz_clone_map_entries_as_env(env_map_id, &env_items, &env_count) != 0) {
    fz_free_string_list(arg_items, arg_count);
    fz_last_exit_class = 3;
    fz_proc_set_last_error("proc_spawnl: env_map clone failed");
    return -1;
  }

  int argv_count = arg_count + 2;
  char** argv = (char**)calloc((size_t)argv_count, sizeof(char*));
  if (argv == NULL) {
    fz_free_string_list(arg_items, arg_count);
    fz_free_string_list(env_items, env_count);
    fz_last_exit_class = 3;
    fz_proc_set_last_error("proc_spawnl: argv alloc failed");
    return -1;
  }
  argv[0] = (char*)command;
  for (int i = 0; i < arg_count; i++) {
    argv[i + 1] = arg_items[i];
  }
  argv[argv_count - 1] = NULL;

  char** envp = fz_clone_env_with_overrides(env_items, env_count);
  int32_t handle = fz_native_proc_spawn_argv(
      command,
      argv,
      envp == NULL ? environ : envp,
      (stdin_payload == NULL || stdin_payload[0] == '\0') ? NULL : stdin_payload);

  fz_free_env(envp);
  free(argv);
  fz_free_string_list(arg_items, arg_count);
  fz_free_string_list(env_items, env_count);
  return handle;
}

int32_t fz_native_proc_wait(int32_t handle, int32_t timeout_ms) {
  pthread_mutex_lock(&fz_proc_lock);
  fz_proc_state* state = fz_proc_state_get(handle);
  if (state == NULL) {
    pthread_mutex_unlock(&fz_proc_lock);
    fz_proc_set_last_error("proc_wait: invalid handle");
    return -1;
  }
  if (state->done) {
    pthread_mutex_unlock(&fz_proc_lock);
    return 0;
  }

  int64_t start = fz_now_ms();
  int status = 0;
  int timed_out = 0;
  for (;;) {
    if (fz_async_current_task_cancelled()) {
      kill(state->pid, SIGKILL);
      (void)waitpid(state->pid, &status, 0);
      timed_out = 1;
      break;
    }
    if (fz_async_deadline_expired()) {
      kill(state->pid, SIGKILL);
      (void)waitpid(state->pid, &status, 0);
      timed_out = 1;
      break;
    }
    if (fz_drain_fd(state->stdout_fd, &state->stdout_buf) < 0) {
      pthread_mutex_unlock(&fz_proc_lock);
      fz_proc_set_last_error("proc_wait: stdout drain failed");
      return -1;
    }
    if (fz_drain_fd(state->stderr_fd, &state->stderr_buf) < 0) {
      pthread_mutex_unlock(&fz_proc_lock);
      fz_proc_set_last_error("proc_wait: stderr drain failed");
      return -1;
    }

    pid_t waited = waitpid(state->pid, &status, WNOHANG);
    if (waited == state->pid) {
      break;
    }
    if (waited < 0) {
      pthread_mutex_unlock(&fz_proc_lock);
      fz_proc_set_last_error("proc_wait: waitpid failed");
      return -1;
    }
    if (timeout_ms == 0) {
      pthread_mutex_unlock(&fz_proc_lock);
      return 1;
    }
    if (timeout_ms > 0) {
      int64_t elapsed = fz_now_ms() - start;
      if (elapsed >= timeout_ms) {
        kill(state->pid, SIGKILL);
        (void)waitpid(state->pid, &status, 0);
        timed_out = 1;
        break;
      }
      int remaining_ms = (int)(timeout_ms - elapsed);
      int poll_timeout_ms = remaining_ms > 50 ? 50 : remaining_ms;
      poll_timeout_ms = fz_async_effective_timeout_ms(poll_timeout_ms);
      if (fz_proc_poll_streams(state, poll_timeout_ms) < 0) {
        pthread_mutex_unlock(&fz_proc_lock);
        fz_proc_set_last_error("proc_wait: stream poll failed");
        return -1;
      }
    } else {
      int poll_timeout_ms = fz_async_effective_timeout_ms(50);
      if (fz_proc_poll_streams(state, poll_timeout_ms) < 0) {
        pthread_mutex_unlock(&fz_proc_lock);
        fz_proc_set_last_error("proc_wait: stream poll failed");
        return -1;
      }
    }
    if (timeout_ms > 0 && (fz_now_ms() - start) >= timeout_ms) {
      kill(state->pid, SIGKILL);
      (void)waitpid(state->pid, &status, 0);
      timed_out = 1;
      break;
    }
  }

  int exit_code = -1;
  if (timed_out) {
    exit_code = -124;
  } else if (WIFEXITED(status)) {
    exit_code = WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    exit_code = 128 + WTERMSIG(status);
  }
  fz_last_exit_class = fz_exit_class_from_status(timed_out, status, 0);
  fz_proc_finalize(state, exit_code);
  pthread_mutex_unlock(&fz_proc_lock);
  fz_proc_set_last_error("");
  return 0;
}

int32_t fz_native_proc_run(int32_t command_id) {
  int32_t handle = fz_native_proc_spawn(command_id);
  if (handle < 0) {
    return -1;
  }
  int32_t waited = fz_native_proc_wait(handle, fz_proc_default_timeout_ms);
  if (waited < 0) {
    return -1;
  }
  pthread_mutex_lock(&fz_proc_lock);
  fz_proc_state* state = fz_proc_state_get(handle);
  int32_t exit_code = (state == NULL) ? -1 : state->exit_code;
  pthread_mutex_unlock(&fz_proc_lock);
  return exit_code;
}

int32_t fz_native_proc_runl(
    int32_t command_id,
    int32_t args_list_id,
    int32_t env_map_id,
    int32_t stdin_id) {
  int32_t handle = fz_native_proc_spawnl(command_id, args_list_id, env_map_id, stdin_id);
  if (handle < 0) {
    return -1;
  }
  int32_t waited = fz_native_proc_wait(handle, fz_proc_default_timeout_ms);
  if (waited < 0) {
    return -1;
  }
  pthread_mutex_lock(&fz_proc_lock);
  fz_proc_state* state = fz_proc_state_get(handle);
  int32_t exit_code = (state == NULL) ? -1 : state->exit_code;
  pthread_mutex_unlock(&fz_proc_lock);
  return exit_code;
}

int32_t fz_native_proc_argv_new(void) { return fz_runtime_list_new(); }
int32_t fz_native_proc_argv_push(int32_t argv_list_id, int32_t value_id) {
  return fz_runtime_list_push(argv_list_id, value_id);
}
int32_t fz_native_proc_env_new(void) { return fz_runtime_map_new(); }
int32_t fz_native_proc_env_set(int32_t env_map_id, int32_t key_id, int32_t value_id) {
  return fz_runtime_map_set(env_map_id, key_id, value_id);
}
int32_t fz_native_proc_spawn_cmd(
    int32_t command_id,
    int32_t argv_list_id,
    int32_t env_map_id,
    int32_t stdin_id) {
  return fz_native_proc_spawnl(command_id, argv_list_id, env_map_id, stdin_id);
}
int32_t fz_native_proc_run_cmd(
    int32_t command_id,
    int32_t argv_list_id,
    int32_t env_map_id,
    int32_t stdin_id) {
  return fz_native_proc_runl(command_id, argv_list_id, env_map_id, stdin_id);
}

int32_t fz_native_proc_poll(int32_t handle) {
  int wait_result = fz_native_proc_wait(handle, 0);
  if (wait_result < 0) {
    return -1;
  }
  return wait_result == 0 ? 1 : 0;
}

static int32_t fz_native_proc_read_stream_chunk(int32_t handle, int32_t max_bytes, int use_stdout) {
  if (max_bytes <= 0) {
    max_bytes = 4096;
  }
  pthread_mutex_lock(&fz_proc_lock);
  fz_proc_state* state = fz_proc_state_get(handle);
  if (state == NULL) {
    pthread_mutex_unlock(&fz_proc_lock);
    return fz_intern_slice("", 0);
  }
  if (!state->done) {
    (void)fz_drain_fd(state->stdout_fd, &state->stdout_buf);
    (void)fz_drain_fd(state->stderr_fd, &state->stderr_buf);
  }
  fz_bytes_buf* buf = use_stdout ? &state->stdout_buf : &state->stderr_buf;
  size_t* cursor = use_stdout ? &state->stdout_read_pos : &state->stderr_read_pos;
  size_t remaining = buf->len > *cursor ? (buf->len - *cursor) : 0;
  size_t take = remaining < (size_t)max_bytes ? remaining : (size_t)max_bytes;
  int32_t out = fz_intern_slice(buf->data == NULL ? "" : (buf->data + *cursor), take);
  *cursor += take;
  pthread_mutex_unlock(&fz_proc_lock);
  return out;
}

int32_t fz_native_proc_read_stdout(int32_t handle, int32_t max_bytes) {
  return fz_native_proc_read_stream_chunk(handle, max_bytes, 1);
}

int32_t fz_native_proc_read_stderr(int32_t handle, int32_t max_bytes) {
  return fz_native_proc_read_stream_chunk(handle, max_bytes, 0);
}

int32_t fz_native_proc_event(int32_t handle) {
  int wait_result = fz_native_proc_wait(handle, 0);
  if (wait_result < 0) {
    return -1;
  }
  if (wait_result > 0) {
    return 0;
  }
  pthread_mutex_lock(&fz_proc_lock);
  fz_proc_state* state = fz_proc_state_get(handle);
  if (state == NULL) {
    pthread_mutex_unlock(&fz_proc_lock);
    return -1;
  }
  int emit = state->exit_notified ? 0 : 1;
  state->exit_notified = 1;
  pthread_mutex_unlock(&fz_proc_lock);
  return emit;
}

int32_t fz_native_proc_stdout(int32_t handle) {
  int wait_result = fz_native_proc_wait(handle, 0);
  if (wait_result < 0) {
    return fz_proc_last_error_id;
  }
  pthread_mutex_lock(&fz_proc_lock);
  fz_proc_state* state = fz_proc_state_get(handle);
  int32_t value = (state == NULL) ? 0 : state->stdout_id;
  pthread_mutex_unlock(&fz_proc_lock);
  return value;
}

int32_t fz_native_proc_stderr(int32_t handle) {
  int wait_result = fz_native_proc_wait(handle, 0);
  if (wait_result < 0) {
    return fz_proc_last_error_id;
  }
  pthread_mutex_lock(&fz_proc_lock);
  fz_proc_state* state = fz_proc_state_get(handle);
  int32_t value = (state == NULL) ? 0 : state->stderr_id;
  pthread_mutex_unlock(&fz_proc_lock);
  return value;
}

int32_t fz_native_proc_exit_code(int32_t handle) {
  int wait_result = fz_native_proc_wait(handle, 0);
  if (wait_result < 0) {
    return -1;
  }
  if (wait_result > 0) {
    return -2;
  }
  pthread_mutex_lock(&fz_proc_lock);
  fz_proc_state* state = fz_proc_state_get(handle);
  int32_t value = (state == NULL) ? -1 : state->exit_code;
  pthread_mutex_unlock(&fz_proc_lock);
  return value;
}

int32_t fz_native_proc_exec_timeout(int32_t timeout_ms) {
  if (timeout_ms > 0) {
    fz_proc_default_timeout_ms = timeout_ms;
  }
  return 0;
}

int32_t fz_native_proc_close(int32_t handle) {
  int32_t waited = fz_native_proc_wait(handle, fz_proc_default_timeout_ms);
  if (waited < 0) {
    return -1;
  }
  pthread_mutex_lock(&fz_proc_lock);
  fz_proc_state* state = fz_proc_state_get(handle);
  if (state == NULL) {
    pthread_mutex_unlock(&fz_proc_lock);
    fz_proc_set_last_error("proc_close: invalid handle");
    return -1;
  }
  fz_bytes_buf_free(&state->stdout_buf);
  fz_bytes_buf_free(&state->stderr_buf);
  memset(state, 0, sizeof(*state));
  pthread_mutex_unlock(&fz_proc_lock);
  fz_proc_set_last_error("");
  return 0;
}

int32_t fz_native_proc_exit_class(void) {
  return fz_last_exit_class;
}

int32_t fz_native_spawn(int32_t task_ref) {
  return fz_native_spawn_impl(task_ref, 0, 0);
}

int32_t fz_native_spawn_ctx(int32_t task_ref, int32_t context_id) {
  return fz_native_spawn_impl(task_ref, context_id, 0);
}

int32_t fz_native_join(int32_t handle) {
  pthread_t thread;
  pthread_mutex_lock(&fz_spawn_lock);
  fz_spawn_state* state = fz_spawn_state_by_handle_locked(handle);
  if (state == NULL || !state->in_use) {
    pthread_mutex_unlock(&fz_spawn_lock);
    return -1;
  }
  if (state->detached) {
    pthread_mutex_unlock(&fz_spawn_lock);
    return -2;
  }
  if (state->joined && state->finished) {
    int32_t result = state->result;
    memset(state, 0, sizeof(*state));
    pthread_mutex_unlock(&fz_spawn_lock);
    return result;
  }
  state->joined = 1;
  thread = state->thread;
  pthread_mutex_unlock(&fz_spawn_lock);

  (void)pthread_join(thread, NULL);

  pthread_mutex_lock(&fz_spawn_lock);
  state = fz_spawn_state_by_handle_locked(handle);
  if (state == NULL || !state->in_use) {
    pthread_mutex_unlock(&fz_spawn_lock);
    return -1;
  }
  int32_t result = state->result;
  memset(state, 0, sizeof(*state));
  pthread_mutex_unlock(&fz_spawn_lock);
  return result;
}

int32_t fz_native_detach(int32_t handle) {
  pthread_t thread;
  int should_detach = 0;
  pthread_mutex_lock(&fz_spawn_lock);
  fz_spawn_state* state = fz_spawn_state_by_handle_locked(handle);
  if (state == NULL || !state->in_use) {
    pthread_mutex_unlock(&fz_spawn_lock);
    return -1;
  }
  if (state->detached) {
    pthread_mutex_unlock(&fz_spawn_lock);
    return 0;
  }
  state->detached = 1;
  if (state->finished) {
    memset(state, 0, sizeof(*state));
    pthread_mutex_unlock(&fz_spawn_lock);
    return 0;
  }
  thread = state->thread;
  should_detach = 1;
  pthread_mutex_unlock(&fz_spawn_lock);
  if (should_detach) {
    (void)pthread_detach(thread);
  }
  return 0;
}

int32_t fz_native_cancel_task(int32_t handle) {
  pthread_t thread;
  int should_join = 0;
  pthread_mutex_lock(&fz_spawn_lock);
  fz_spawn_state* state = fz_spawn_state_by_handle_locked(handle);
  if (state == NULL || !state->in_use) {
    pthread_mutex_unlock(&fz_spawn_lock);
    return -1;
  }
  state->cancelled = 1;
  if (!state->detached && !state->joined && !state->finished) {
    state->joined = 1;
    thread = state->thread;
    should_join = !pthread_equal(thread, pthread_self());
  } else if (!state->detached && !state->joined && state->finished) {
    state->joined = 1;
    thread = state->thread;
    should_join = !pthread_equal(thread, pthread_self());
  }
  pthread_mutex_unlock(&fz_spawn_lock);
  if (should_join) {
    (void)pthread_join(thread, NULL);
  }
  pthread_mutex_lock(&fz_spawn_lock);
  state = fz_spawn_state_by_handle_locked(handle);
  if (state != NULL && state->in_use && !state->detached) {
    memset(state, 0, sizeof(*state));
  }
  pthread_mutex_unlock(&fz_spawn_lock);
  return 0;
}

int32_t fz_native_task_result(int32_t handle) {
  pthread_mutex_lock(&fz_spawn_lock);
  fz_spawn_state* state = fz_spawn_state_by_handle_locked(handle);
  if (state == NULL || !state->in_use) {
    pthread_mutex_unlock(&fz_spawn_lock);
    return -1;
  }
  int32_t result = state->finished ? state->result : -2;
  pthread_mutex_unlock(&fz_spawn_lock);
  return result;
}

int32_t fz_native_task_context_id(void) {
  return fz_tls_task_context;
}

int32_t fz_native_task_group_begin(void) {
  pthread_mutex_lock(&fz_spawn_lock);
  fz_task_group_state* group = fz_task_group_alloc_locked();
  if (group == NULL) {
    pthread_mutex_unlock(&fz_spawn_lock);
    return -1;
  }
  memset(group, 0, sizeof(*group));
  group->in_use = 1;
  group->id = fz_next_task_group_id++;
  int32_t group_id = group->id;
  pthread_mutex_unlock(&fz_spawn_lock);
  return group_id;
}

int32_t fz_native_task_group_spawn(int32_t group_id, int32_t task_ref) {
  return fz_native_spawn_impl(task_ref, 0, group_id);
}

int32_t fz_native_task_group_spawn_n(int32_t group_id, int32_t task_ref, int32_t n) {
  if (n <= 0) {
    return 0;
  }
  for (int32_t i = 0; i < n; i++) {
    if (fz_native_task_group_spawn(group_id, task_ref) < 0) {
      return -1;
    }
  }
  return 0;
}

int32_t fz_native_task_group_join(int32_t group_id) {
  int32_t first_failure = 0;
  for (;;) {
    int32_t next_handle = 0;
    pthread_mutex_lock(&fz_spawn_lock);
    fz_task_group_state* group = fz_task_group_by_id_locked(group_id);
    if (group == NULL || !group->in_use) {
      pthread_mutex_unlock(&fz_spawn_lock);
      return -1;
    }
    for (int i = 0; i < FZ_MAX_SPAWN_THREADS; i++) {
      fz_spawn_state* state = &fz_spawn_states[i];
      if (state->in_use && state->group_id == group_id && !state->detached) {
        next_handle = state->handle;
        break;
      }
    }
    if (next_handle == 0) {
      group->in_use = 0;
      pthread_mutex_unlock(&fz_spawn_lock);
      return first_failure;
    }
    pthread_mutex_unlock(&fz_spawn_lock);
    int32_t joined = fz_native_join(next_handle);
    if (joined < 0) {
      return joined;
    }
    if (first_failure == 0 && joined != 0) {
      first_failure = joined;
    }
  }
}

int32_t fz_native_task_group_cancel(int32_t group_id) {
  pthread_t threads[FZ_MAX_SPAWN_THREADS];
  int thread_count = 0;
  pthread_mutex_lock(&fz_spawn_lock);
  fz_task_group_state* group = fz_task_group_by_id_locked(group_id);
  if (group == NULL || !group->in_use) {
    pthread_mutex_unlock(&fz_spawn_lock);
    return -1;
  }
  for (int i = 0; i < FZ_MAX_SPAWN_THREADS; i++) {
    fz_spawn_state* state = &fz_spawn_states[i];
    if (state->in_use && state->group_id == group_id) {
      state->cancelled = 1;
      if (!state->detached && !state->joined) {
        state->joined = 1;
        threads[thread_count++] = state->thread;
      }
    }
  }
  pthread_mutex_unlock(&fz_spawn_lock);
  for (int i = 0; i < thread_count; i++) {
    if (!pthread_equal(threads[i], pthread_self())) {
      (void)pthread_join(threads[i], NULL);
    }
  }
  pthread_mutex_lock(&fz_spawn_lock);
  for (int i = 0; i < FZ_MAX_SPAWN_THREADS; i++) {
    fz_spawn_state* state = &fz_spawn_states[i];
    if (state->in_use && state->group_id == group_id && !state->detached) {
      memset(state, 0, sizeof(*state));
    }
  }
  group = fz_task_group_by_id_locked(group_id);
  if (group != NULL) {
    group->in_use = 0;
    group->active_count = 0;
  }
  pthread_mutex_unlock(&fz_spawn_lock);
  return 0;
}

int32_t fz_native_task_group_join_all(int32_t group_id) {
  return fz_native_task_group_join(group_id);
}

int32_t fz_native_task_parallel_map(int32_t list_handle, int32_t task_ref) {
  int32_t count = fz_runtime_list_len(list_handle);
  if (count < 0) {
    return -1;
  }
  int32_t group_id = fz_native_task_group_begin();
  if (group_id < 0) {
    return -1;
  }
  if (fz_native_task_group_spawn_n(group_id, task_ref, count) < 0) {
    (void)fz_native_task_group_cancel(group_id);
    return -1;
  }
  return fz_native_task_group_join_all(group_id);
}

int32_t fz_native_timeout(int32_t timeout_ms) {
  if (timeout_ms < 0) {
    return -1;
  }
  fz_tls_async_deadline_ms = fz_now_ms() + (int64_t)timeout_ms;
  return 0;
}

int32_t fz_native_deadline(int32_t deadline_ms) {
  fz_tls_async_deadline_ms = (int64_t)deadline_ms;
  return 0;
}

int32_t fz_native_cancel(void) {
  fz_tls_async_cancelled = 1;
  return 0;
}

int32_t fz_native_recv(void) {
  if (fz_async_current_task_cancelled()) {
    return -1;
  }
  if (fz_async_deadline_expired()) {
    return -1;
  }
  return 0;
}

int32_t fz_native_yield(void) {
  sched_yield();
  return 0;
}

int32_t fz_native_checkpoint(void) {
  sched_yield();
  return 0;
}

int32_t fz_native_assert_eq_i32(int32_t left, int32_t right) {
  if (left != right) {
    fprintf(stderr, "assert.eq_i32 failed: left=%d right=%d\n", left, right);
    return -1;
  }
  return 0;
}

int32_t fz_native_pulse(void) {
  sched_yield();
  return 0;
}

int32_t fz_host_init(void) {
  pthread_mutex_lock(&fz_host_lock);
  fz_host_initialized = 1;
  for (int i = 0; i < 64; i++) {
    fz_host_callbacks[i] = NULL;
  }
  fz_set_last_error(0, 0, "");
  pthread_mutex_unlock(&fz_host_lock);
  return 0;
}

int32_t fz_host_shutdown(void) {
  pthread_mutex_lock(&fz_host_lock);
  fz_host_initialized = 0;
  fz_set_last_error(0, 0, "");
  pthread_mutex_unlock(&fz_host_lock);
  return 0;
}

int32_t fz_host_cleanup(void) {
  pthread_mutex_lock(&fz_host_lock);
  for (int i = 0; i < 64; i++) {
    fz_host_callbacks[i] = NULL;
  }
  fz_set_last_error(0, 0, "");
  pthread_mutex_unlock(&fz_host_lock);
  return 0;
}

int32_t fz_host_register_callback_i32(int32_t slot, fz_callback_i32_v0 cb) {
  if (slot < 0 || slot >= 64 || cb == NULL) {
    fz_set_last_error(EINVAL, 3, "fz_host_register_callback_i32 failed: invalid slot or callback");
    return -1;
  }
  pthread_mutex_lock(&fz_host_lock);
  if (!fz_host_initialized) {
    fz_set_last_error(EINVAL, 3, "fz_host_register_callback_i32 failed: host runtime not initialized");
    pthread_mutex_unlock(&fz_host_lock);
    return -2;
  }
  fz_host_callbacks[slot] = cb;
  fz_set_last_error(0, 0, "");
  pthread_mutex_unlock(&fz_host_lock);
  return 0;
}

int32_t fz_host_invoke_callback_i32(int32_t slot, int32_t arg) {
  if (slot < 0 || slot >= 64) {
    fz_set_last_error(EINVAL, 3, "fz_host_invoke_callback_i32 failed: invalid slot");
    return -1;
  }
  pthread_mutex_lock(&fz_host_lock);
  fz_callback_i32_v0 cb = fz_host_callbacks[slot];
  pthread_mutex_unlock(&fz_host_lock);
  if (cb == NULL) {
    fz_set_last_error(EINVAL, 3, "fz_host_invoke_callback_i32 failed: callback not registered");
    return -2;
  }
  fz_set_last_error(0, 0, "");
  return cb(arg);
}

int32_t fz_host_last_error_code(void) {
  return fz_last_error_code;
}

int32_t fz_host_last_error_class(void) {
  return fz_last_error_class;
}

const char* fz_host_last_error_message(void) {
  const char* msg = fz_lookup_string(fz_last_error_message_id);
  return msg == NULL ? "" : msg;
}

#define FZ_MAX_PROC_ARGS 512

static pthread_once_t fz_proc_args_once = PTHREAD_ONCE_INIT;
static int32_t fz_proc_arg_count = 0;
static int32_t fz_proc_arg_ids[FZ_MAX_PROC_ARGS];
static pthread_mutex_t fz_term_lock = PTHREAD_MUTEX_INITIALIZER;
static int32_t fz_term_last_stdin_eof = 0;

static void fz_proc_capture_args(void) {
  fz_proc_arg_count = 0;
#ifdef __APPLE__
  int* argc_ptr = _NSGetArgc();
  char*** argv_ptr = _NSGetArgv();
  int argc = argc_ptr == NULL ? 0 : *argc_ptr;
  char** argv = argv_ptr == NULL ? NULL : *argv_ptr;
  if (argc <= 0 || argv == NULL) {
    return;
  }
  if (argc > FZ_MAX_PROC_ARGS) {
    argc = FZ_MAX_PROC_ARGS;
  }
  for (int i = 0; i < argc; i++) {
    const char* item = argv[i] == NULL ? "" : argv[i];
    fz_proc_arg_ids[i] = fz_intern_slice(item, strlen(item));
  }
  fz_proc_arg_count = argc;
#elif defined(__linux__)
  FILE* file = fopen("/proc/self/cmdline", "rb");
  if (file == NULL) {
    return;
  }
  size_t cap = 4096;
  size_t len = 0;
  char* raw = (char*)malloc(cap);
  if (raw == NULL) {
    fclose(file);
    return;
  }
  for (;;) {
    if (len == cap) {
      size_t next_cap = cap * 2;
      char* next = (char*)realloc(raw, next_cap);
      if (next == NULL) {
        free(raw);
        fclose(file);
        return;
      }
      raw = next;
      cap = next_cap;
    }
    size_t got = fread(raw + len, 1, cap - len, file);
    len += got;
    if (got == 0) {
      break;
    }
  }
  fclose(file);
  size_t start = 0;
  int count = 0;
  while (start < len && count < FZ_MAX_PROC_ARGS) {
    size_t end = start;
    while (end < len && raw[end] != '\0') {
      end++;
    }
    fz_proc_arg_ids[count++] = fz_intern_slice(raw + start, end - start);
    start = end + 1;
  }
  fz_proc_arg_count = count;
  free(raw);
#endif
}

int32_t fz_native_proc_argv_count(void) {
  pthread_once(&fz_proc_args_once, fz_proc_capture_args);
  return fz_proc_arg_count;
}

int32_t fz_native_proc_argv_get(int32_t index) {
  pthread_once(&fz_proc_args_once, fz_proc_capture_args);
  if (index < 0 || index >= fz_proc_arg_count || index >= FZ_MAX_PROC_ARGS) {
    return fz_intern_slice("", 0);
  }
  return fz_proc_arg_ids[index];
}

int32_t fz_native_term_read_line(void) {
  pthread_mutex_lock(&fz_term_lock);
  fz_term_last_stdin_eof = 0;
  size_t cap = 256;
  size_t len = 0;
  char* buffer = (char*)malloc(cap);
  if (buffer == NULL) {
    pthread_mutex_unlock(&fz_term_lock);
    return 0;
  }
  int ch = 0;
  while ((ch = fgetc(stdin)) != EOF) {
    if (ch == '\n') {
      break;
    }
    if (len + 1 >= cap) {
      size_t next_cap = cap * 2;
      char* next = (char*)realloc(buffer, next_cap);
      if (next == NULL) {
        free(buffer);
        pthread_mutex_unlock(&fz_term_lock);
        return 0;
      }
      buffer = next;
      cap = next_cap;
    }
    buffer[len++] = (char)ch;
  }
  if (ch == EOF) {
    fz_term_last_stdin_eof = 1;
  }
  if (len > 0 && buffer[len - 1] == '\r') {
    len--;
  }
  buffer[len] = '\0';
  int32_t out = fz_intern_owned(buffer);
  if (out == 0) {
    out = fz_intern_slice("", 0);
  }
  pthread_mutex_unlock(&fz_term_lock);
  return out;
}

int32_t fz_native_term_stdin_eof(void) {
  pthread_mutex_lock(&fz_term_lock);
  int32_t eof = fz_term_last_stdin_eof;
  pthread_mutex_unlock(&fz_term_lock);
  return eof;
}

int32_t fz_native_term_write(int32_t text_id) {
  const char* text = fz_lookup_string(text_id);
  if (text == NULL) {
    text = "";
  }
  if (fputs(text, stdout) == EOF) {
    return -1;
  }
  return fflush(stdout) == 0 ? 0 : -1;
}

int32_t fz_native_term_write_err(int32_t text_id) {
  const char* text = fz_lookup_string(text_id);
  if (text == NULL) {
    text = "";
  }
  if (fputs(text, stderr) == EOF) {
    return -1;
  }
  return fflush(stderr) == 0 ? 0 : -1;
}

int32_t fz_native_term_stdin_is_tty(void) {
  return isatty(STDIN_FILENO) ? 1 : 0;
}

int32_t fz_native_term_stdout_is_tty(void) {
  return isatty(STDOUT_FILENO) ? 1 : 0;
}
