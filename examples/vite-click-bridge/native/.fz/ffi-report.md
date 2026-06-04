# FFI Report

- Imports: 29
- Exports: 1
- Pointer contract violations: 0
- Callback anchor violations: 0
- FFI-stable type violations: 0
- Async import violations: 0
- Missing panic boundary declarations: 0

## Imports

- `fzy2ts.api.js_host.j2fz_js_module_open` -> `i32` | pointer_contract_ok=true | callback_anchor_ok=true | ffi_stable_ok=true | async_forbidden=false
- `fzy2ts.api.js_host.j2fz_js_module_close` -> `i32` | pointer_contract_ok=true | callback_anchor_ok=true | ffi_stable_ok=true | async_forbidden=false
- `fzy2ts.api.js_host.j2fz_js_export_get` -> `i32` | pointer_contract_ok=true | callback_anchor_ok=true | ffi_stable_ok=true | async_forbidden=false
- `fzy2ts.api.js_host.j2fz_js_export_release` -> `i32` | pointer_contract_ok=true | callback_anchor_ok=true | ffi_stable_ok=true | async_forbidden=false
- `fzy2ts.api.js_host.j2fz_js_value_release` -> `i32` | pointer_contract_ok=true | callback_anchor_ok=true | ffi_stable_ok=true | async_forbidden=false
- `fzy2ts.api.js_host.j2fz_js_value_null` -> `i32` | pointer_contract_ok=true | callback_anchor_ok=true | ffi_stable_ok=true | async_forbidden=false
- `fzy2ts.api.js_host.j2fz_js_value_bool` -> `i32` | pointer_contract_ok=true | callback_anchor_ok=true | ffi_stable_ok=true | async_forbidden=false
- `fzy2ts.api.js_host.j2fz_js_value_i32` -> `i32` | pointer_contract_ok=true | callback_anchor_ok=true | ffi_stable_ok=true | async_forbidden=false
- `fzy2ts.api.js_host.j2fz_js_value_u64` -> `i32` | pointer_contract_ok=true | callback_anchor_ok=true | ffi_stable_ok=true | async_forbidden=false
- `fzy2ts.api.js_host.j2fz_js_value_f64` -> `i32` | pointer_contract_ok=true | callback_anchor_ok=true | ffi_stable_ok=true | async_forbidden=false
- `fzy2ts.api.js_host.j2fz_js_value_str` -> `i32` | pointer_contract_ok=true | callback_anchor_ok=true | ffi_stable_ok=true | async_forbidden=false
- `fzy2ts.api.js_host.j2fz_js_value_bytes` -> `i32` | pointer_contract_ok=true | callback_anchor_ok=true | ffi_stable_ok=true | async_forbidden=false
- `fzy2ts.api.js_host.j2fz_js_value_as_bool` -> `i32` | pointer_contract_ok=true | callback_anchor_ok=true | ffi_stable_ok=true | async_forbidden=false
- `fzy2ts.api.js_host.j2fz_js_value_as_i32` -> `i32` | pointer_contract_ok=true | callback_anchor_ok=true | ffi_stable_ok=true | async_forbidden=false
- `fzy2ts.api.js_host.j2fz_js_value_as_u64` -> `i32` | pointer_contract_ok=true | callback_anchor_ok=true | ffi_stable_ok=true | async_forbidden=false
- `fzy2ts.api.js_host.j2fz_js_value_as_f64` -> `i32` | pointer_contract_ok=true | callback_anchor_ok=true | ffi_stable_ok=true | async_forbidden=false
- `fzy2ts.api.js_host.j2fz_js_value_as_utf8` -> `i32` | pointer_contract_ok=true | callback_anchor_ok=true | ffi_stable_ok=true | async_forbidden=false
- `fzy2ts.api.js_host.j2fz_js_value_copy_utf8` -> `i32` | pointer_contract_ok=true | callback_anchor_ok=true | ffi_stable_ok=true | async_forbidden=false
- `fzy2ts.api.js_host.j2fz_js_call` -> `i32` | pointer_contract_ok=true | callback_anchor_ok=true | ffi_stable_ok=true | async_forbidden=false
- `fzy2ts.api.js_host.j2fz_js_await` -> `i32` | pointer_contract_ok=true | callback_anchor_ok=true | ffi_stable_ok=true | async_forbidden=false
- `fzy2ts.api.js_host.j2fz_js_host_register_i32_i32` -> `i32` | pointer_contract_ok=true | callback_anchor_ok=true | ffi_stable_ok=true | async_forbidden=false
- `fzy2ts.api.js_host.j2fz_js_host_registration_release` -> `i32` | pointer_contract_ok=true | callback_anchor_ok=true | ffi_stable_ok=true | async_forbidden=false
- `fzy2ts.bridge_easy.j2fz_js_easy_module_open` -> `u64` | pointer_contract_ok=true | callback_anchor_ok=true | ffi_stable_ok=true | async_forbidden=false
- `fzy2ts.bridge_easy.j2fz_js_easy_export_get` -> `u64` | pointer_contract_ok=true | callback_anchor_ok=true | ffi_stable_ok=true | async_forbidden=false
- `fzy2ts.bridge_easy.j2fz_js_easy_value_i32` -> `u64` | pointer_contract_ok=true | callback_anchor_ok=true | ffi_stable_ok=true | async_forbidden=false
- `fzy2ts.bridge_easy.j2fz_js_easy_call1` -> `u64` | pointer_contract_ok=true | callback_anchor_ok=true | ffi_stable_ok=true | async_forbidden=false
- `fzy2ts.bridge_easy.j2fz_js_easy_await` -> `u64` | pointer_contract_ok=true | callback_anchor_ok=true | ffi_stable_ok=true | async_forbidden=false
- `fzy2ts.bridge_easy.j2fz_js_easy_value_as_i32` -> `i32` | pointer_contract_ok=true | callback_anchor_ok=true | ffi_stable_ok=true | async_forbidden=false
- `fzy2ts.bridge_easy.j2fz_js_easy_host_register_i32_i32` -> `u64` | pointer_contract_ok=true | callback_anchor_ok=true | ffi_stable_ok=true | async_forbidden=false

## Exports

- `bridge_click` -> `i32` | panic_boundary_declared=true | ffi_stable_ok=true
