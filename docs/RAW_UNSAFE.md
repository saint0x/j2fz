# Raw Unsafe API

The raw unsafe runtime exists for expert consumers who need explicit symbol binding outside the safe/generated contract surface.

Import path:

```ts
import { loadUnsafeRawModule } from "j2fz/runtime/raw";
```

## What It Does

- loads the shared library and manifest
- lets you bind arbitrary explicit symbols by prototype string parts
- lets you pass explicitly wrapped raw pointer values

## What It Does Not Do

- it does not protect you from ABI misuse
- it does not infer ownership
- it does not make raw pointers safe
- it is not re-exported from the default `j2fz` entrypoint

## Example

```ts
import { loadUnsafeRawModule } from "j2fz/runtime/raw";

const raw = loadUnsafeRawModule({
  paths: {
    sharedLibrary: "/absolute/path/to/libdemo.dylib",
    abiManifest: "/absolute/path/to/abi.manifest.json",
  },
});

const addOne = raw.bindFunction({
  symbol: "add_one",
  result: "int32_t",
  params: ["int32_t"],
});

const value = addOne.call(41);
raw.dispose();
```

For pointer out-params, use explicit C-like qualifiers in the raw signature:

```ts
const writeValue = raw.bindFunction({
  symbol: "write_value",
  result: "int32_t",
  params: ["_Out_ int32_t *"],
});
```

## Contract Boundary

Use the raw module only when:

- the safe/generated surface does not cover the symbol you need
- you understand the underlying C ABI contract
- you are prepared to manage ownership and pointer correctness yourself
