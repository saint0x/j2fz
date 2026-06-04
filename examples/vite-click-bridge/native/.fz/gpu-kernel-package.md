# GPU Kernel Package

- Schema: `fozzylang.gpu_kernel_package.v1`
- Status: `empty`
- Module: `lib`
- Kernels: `0`
- Functions: `0`

## Launch ABI

- ABI version: `fozzylang.gpu_launch_abi.v1`
- Package format: `kernel_ir`
- Kernel identity: `function_name`
- Grid dimensions: `x`
- Block dimensions: `x`
- Argument encoding: `fzy_native_scalar_and_handle_abi`

## Layout Classes

| Class | Value Type | Access | Wire Slots |
|---|---|---|---|
| `i32` | `i32` | `-` | `scalar_value` |
| `u32` | `u32` | `-` | `scalar_value` |
| `f32` | `f32` | `-` | `scalar_value` |
| `slice_f32_ro` | `GpuSlice<f32>` | `readonly` | `buffer_handle, i32_offset, i32_len` |
| `slice_f32_wo` | `GpuSlice<f32>` | `writeonly` | `buffer_handle, i32_offset, i32_len` |
| `slice_f32_rw` | `GpuSlice<f32>` | `readwrite` | `buffer_handle, i32_offset, i32_len` |
| `slice_i32_ro` | `GpuSlice<i32>` | `readonly` | `buffer_handle, i32_offset, i32_len` |
| `slice_i32_wo` | `GpuSlice<i32>` | `writeonly` | `buffer_handle, i32_offset, i32_len` |
| `slice_i32_rw` | `GpuSlice<i32>` | `readwrite` | `buffer_handle, i32_offset, i32_len` |
| `slice_u32_ro` | `GpuSlice<u32>` | `readonly` | `buffer_handle, i32_offset, i32_len` |
| `slice_u32_wo` | `GpuSlice<u32>` | `writeonly` | `buffer_handle, i32_offset, i32_len` |
| `slice_u32_rw` | `GpuSlice<u32>` | `readwrite` | `buffer_handle, i32_offset, i32_len` |

## Functions

| Function | Space | Params | Capabilities | Return |
|---|---|---|---|---|
