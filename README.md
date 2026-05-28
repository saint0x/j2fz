# j2fz

`j2fz` is a TypeScript-first bidirectional interoperability library for Fozzy.

It treats the stabilized Fozzy native ABI as the canonical language boundary and builds a production-grade JavaScript and TypeScript developer surface on top of it.

## Core Principles

- TypeScript-first public API
- Manifest-driven generation
- Explicit ownership and disposal
- Explicit async and callback contracts
- Safe high-level API with isolated unsafe/raw escape hatches
- Bidirectional interoperability:
  - JavaScript calling Fozzy exports
  - Fozzy calling JavaScript callbacks through registered bridge shims

## Current Foundation

The repo now includes a real production foundation rather than only planning docs:

- strict ABI manifest types and validation
- Koffi-backed native loading and symbol declaration
- manifest-driven TypeScript binding generation
- callback registration and native callback roundtrip coverage
- native integration testing against a compiled fixture shared library

The current implementation center is:

- [src/runtime/manifest.ts](/Users/deepsaint/Desktop/j2fz/src/runtime/manifest.ts:1)
- [src/runtime/koffi.ts](/Users/deepsaint/Desktop/j2fz/src/runtime/koffi.ts:1)
- [src/runtime/loader.ts](/Users/deepsaint/Desktop/j2fz/src/runtime/loader.ts:1)
- [src/generator/render.ts](/Users/deepsaint/Desktop/j2fz/src/generator/render.ts:1)
- [tests/integration.test.ts](/Users/deepsaint/Desktop/j2fz/tests/integration.test.ts:1)

## Repository Layout

- `src/runtime`
  - native library loading
  - symbol resolution
  - marshaling
  - ownership
  - callback bridge
  - async bridge
- `src/generator`
  - ABI manifest ingestion
  - internal contract model
  - TypeScript/JavaScript binding generation
- `src/types`
  - shared public type contracts
- `fixtures`
  - real Fozzy-built ABI fixtures for cross-language tests
- `tests`
  - unit, integration, compatibility, and generation tests
- `docs`
  - product and architecture docs

## Validation

Current local validation:

- `npm run check`
- `npm test`

The integration suite builds a real native shared library fixture, loads it through `j2fz`, calls native exports, and round-trips a JavaScript callback through the ABI boundary.

## Status

- Project directory created
- Production checklist created
- TypeScript-first contract direction locked
- Strict TypeScript runtime/generator foundation implemented
- Native callback roundtrip integration test implemented

See [CHECKLIST.md](/Users/deepsaint/Desktop/j2fz/CHECKLIST.md:1) for the actionable implementation plan.
See [docs/ARCHITECTURE.md](/Users/deepsaint/Desktop/j2fz/docs/ARCHITECTURE.md:1) for the current implementation architecture.
