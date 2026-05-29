# fzy2ts

`fzy2ts` is the Fzy-hosted side of `j2fz`.

It gives Fzy code an explicit JavaScript host ABI instead of assuming JavaScript can be imported magically. The current design is handle-based and built for stable foreign interop:

- open a JS module by specifier
- resolve an exported JS function or value
- construct JS values explicitly
- call JS functions through value handles
- await JS promise-like results explicitly
- release modules, exports, values, and registrations explicitly

At runtime, those imports are satisfied by the `ts2fzy` native host shim and JS host sidecar rather than by compiler magic inside Fzy itself.

## Design Rules

- no implicit JavaScript imports in the language core
- explicit host ABI boundary
- explicit ownership and release
- explicit async boundary
- explicit callback/context contracts

## Package Layout

- `src/api/js_host.fzy`
  Raw host ABI declarations
- `src/runtime/js.fzy`
  Pure Fzy helpers over status/model logic and the raw host ABI namespace
- `src/model/types.fzy`
  Shared handle and ABI-shaped data types

## Current Scope

This package establishes the Fzy-side interop surface and contracts, and it now verifies cleanly under Fzy's strict production profile. It is the right place to build the reverse-direction product without overloading the JS-hosted runtime model in `ts2fzy/`.
