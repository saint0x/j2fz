# j2fz Architecture

## Overview

`j2fz` is a manifest-driven TypeScript runtime and generator for consuming Fozzy native ABI libraries from JavaScript.

The core architectural rule is:

- Fozzy owns the native ABI contract.
- `j2fz` owns validation, loading, marshaling, TypeScript generation, and JavaScript-facing ergonomics.
- The low-level Node FFI engine is an implementation detail, not the product contract.

## Current Stack

The current implementation uses:

- Fozzy ABI manifests as the source of truth
- Koffi as the Node-native C FFI substrate
- strict TypeScript as the public contract layer

Primary implementation files:

- [src/types/abi.ts](/Users/deepsaint/Desktop/j2fz/src/types/abi.ts:1)
- [src/runtime/errors.ts](/Users/deepsaint/Desktop/j2fz/src/runtime/errors.ts:1)
- [src/runtime/discovery.ts](/Users/deepsaint/Desktop/j2fz/src/runtime/discovery.ts:1)
- [src/runtime/manifest.ts](/Users/deepsaint/Desktop/j2fz/src/runtime/manifest.ts:1)
- [src/runtime/koffi.ts](/Users/deepsaint/Desktop/j2fz/src/runtime/koffi.ts:1)
- [src/runtime/loader.ts](/Users/deepsaint/Desktop/j2fz/src/runtime/loader.ts:1)
- [src/generator/model.ts](/Users/deepsaint/Desktop/j2fz/src/generator/model.ts:1)
- [src/generator/render.ts](/Users/deepsaint/Desktop/j2fz/src/generator/render.ts:1)

## Layers

### 1. ABI Model

The ABI model layer defines the normalized representation of:

- package identity
- panic boundary
- repr(C) layouts
- exports
- parameter contracts
- return contracts
- callback bindings
- async boundaries

This layer exists in [src/types/abi.ts](/Users/deepsaint/Desktop/j2fz/src/types/abi.ts:1).

### 2. Manifest Validation

The manifest layer parses JSON ABI artifacts and rejects malformed or unsupported contracts before any native library is loaded.

This layer exists in [src/runtime/manifest.ts](/Users/deepsaint/Desktop/j2fz/src/runtime/manifest.ts:1).

Current guarantees:

- schema validation
- package field validation
- panic-boundary validation
- export validation
- callback binding validation
- async boundary validation
- repr(C) layout validation
- referenced C type validation against known builtin and manifest-declared types

### 3. Runtime Type Registry

The runtime type registry translates ABI-declared repr(C) layouts and C signatures into Koffi-compatible runtime types.

This layer exists in [src/runtime/koffi.ts](/Users/deepsaint/Desktop/j2fz/src/runtime/koffi.ts:1).

Current responsibilities:

- resolve package-local generated artifact layouts
- honor explicit environment overrides for deployment
- build Koffi struct and enum definitions
- map C scalar and pointer types into runtime FFI types
- synthesize callback prototype and callback-pointer types
- declare native functions from ABI metadata

### 4. Loader

The loader composes:

- package artifact discovery
- ABI manifest parsing
- package identity checks
- native shared library loading
- runtime type registry creation
- export binding
- callback registration
- async-handle orchestration

This layer exists in [src/runtime/loader.ts](/Users/deepsaint/Desktop/j2fz/src/runtime/loader.ts:1).

### 5. Generator

The generator converts ABI exports into TypeScript-facing binding modules.

This layer exists in:

- [src/generator/model.ts](/Users/deepsaint/Desktop/j2fz/src/generator/model.ts:1)
- [src/generator/render.ts](/Users/deepsaint/Desktop/j2fz/src/generator/render.ts:1)
- [src/generator/write.ts](/Users/deepsaint/Desktop/j2fz/src/generator/write.ts:1)

Current generator responsibilities:

- build a normalized generated-module model
- map ABI types into TypeScript surface types
- render package-ready JavaScript and declaration output
- write generated package output
- emit ownership-aware handle interfaces for owned pointer returns
- emit typed callback registration helpers
- emit binding-level disposal surface

## Current Runtime Contract

Implemented today:

- package artifact discovery and package-layout loading
- strict ABI manifest parsing
- ABI compatibility baseline comparison
- repr(C) layout registration
- sync export binding
- async-handle export orchestration
- ownership-aware argument adaptation
- owned-pointer result handles
- callback registration support
- manifest-driven TypeScript rendering
- package-ready binding artifact generation
- generated callback registration helpers
- generated binding disposal

Validated today:

- unit tests for package artifact discovery
- unit tests for manifest validation
- unit tests for generator rendering
- unit tests for Koffi declaration synthesis
- integration test using a compiled native fixture with callback roundtrip
- integration coverage for owned-pointer disposal and out-buffer writes
- integration coverage for package-layout loading via `loadFozzyPackage`

Integration coverage lives in [tests/integration.test.ts](/Users/deepsaint/Desktop/j2fz/tests/integration.test.ts:1).

## Deliberate Boundaries

`j2fz` currently treats these as separate concerns:

- native ABI correctness belongs to Fozzy
- JS binding safety and ergonomics belong to `j2fz`
- Koffi usage is encapsulated behind runtime helpers, not exposed as the public contract

This keeps the library free to evolve its Node-native substrate without changing the `j2fz` product model.

## Next Major Areas

The next production slices to build are:

- richer ownership adapters for owned/out/inout contracts
- broader async fixture coverage
- richer callback contract coverage
- docs and release workflow hardening
