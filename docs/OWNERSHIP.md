# Ownership Model

`j2fz` preserves the ownership contract declared by the Fozzy ABI manifest.

## Ownership Classes

- `value`
  Plain scalars or copyable return values.
- `borrowed`
  Data owned by the caller for the duration of the call.
- `owned`
  Native-owned results surfaced as disposable JS handles.
- `out`
  Caller-provided mutable storage written by native code.
- `inout`
  Caller-provided mutable storage both read and written by native code.

## Borrowed Buffers

Borrowed byte-region arguments currently accept:

- `Buffer`
- `Uint8Array`
- `string`

For `ptr_len` contracts, `j2fz` rejects unrelated objects instead of guessing.

## Owned Native Pointers

Owned pointer returns are wrapped in handles with:

- `pointer`
- `disposed`
- `dispose()`
- `decodeBytes(length)`
- `decodeString()` only for string-like pointer types

Rules:

- using a disposed handle throws `OwnershipError`
- decoding a non-string handle with `decodeString()` throws `OwnershipError`
- negative byte lengths throw `TypeMarshalingError`
- finalizers are not the primary lifecycle contract

## Raw Unsafe Escape Hatch

The safe runtime does not expose raw pointer fabrication.

If you need raw pointer handling, use the explicit unsafe module:

```ts
import { loadUnsafeRawModule } from "j2fz/runtime/raw";
```

That API is intentionally separate from the default safe surface.
