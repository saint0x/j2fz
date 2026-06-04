#ifndef FOZZY_J2FZ_VITE_CLICK_BRIDGE_H
#define FOZZY_J2FZ_VITE_CLICK_BRIDGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t (*fz_callback_i32_v0)(int32_t arg);
int32_t fz_host_init(void);
int32_t fz_host_shutdown(void);
int32_t fz_host_cleanup(void);
int32_t fz_host_last_error_code(void);
int32_t fz_host_last_error_class(void);
const char* fz_host_last_error_message(void);
int32_t fz_host_register_callback_i32(int32_t slot, fz_callback_i32_v0 cb);
int32_t fz_host_invoke_callback_i32(int32_t slot, int32_t arg);

typedef int32_t (*fz_callback_sig0_v0)(int32_t arg0);

typedef struct fzy2ts_model_types_JsModuleHandle {
    uint64_t raw;
} fzy2ts_model_types_JsModuleHandle;

typedef struct fzy2ts_model_types_JsExportHandle {
    uint64_t raw;
} fzy2ts_model_types_JsExportHandle;

typedef struct fzy2ts_model_types_JsValueHandle {
    uint64_t raw;
} fzy2ts_model_types_JsValueHandle;

typedef struct fzy2ts_model_types_JsRegistrationHandle {
    uint64_t raw;
} fzy2ts_model_types_JsRegistrationHandle;

typedef struct fzy2ts_model_types_JsUtf8View {
    uint64_t ptr;
    size_t len;
} fzy2ts_model_types_JsUtf8View;

typedef struct fzy2ts_model_types_JsArgv {
    const uint64_t* ptr_borrowed;
    size_t len;
} fzy2ts_model_types_JsArgv;

int32_t bridge_click(int32_t count);

#ifdef __cplusplus
}
#endif

#endif
