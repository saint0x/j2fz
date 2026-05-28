# j2fz Production Checklist

This checklist is the full production implementation plan for `j2fz`, the JavaScript-to-Fozzy bidirectional interoperability library. The project assumes the Fozzy language/runtime side has already landed the native ABI hardening required for a stable foreign boundary.

`j2fz` is TypeScript-first. The type layer is part of the product contract, not a secondary wrapper. Every supported bidirectional interop shape must have:
- a runtime implementation
- a generated TypeScript representation
- a validated ownership/lifecycle model
- a test path
- documentation

## Project Setup

✅ Create the `j2fz` project directory.

- Establish repository structure for:
  - generator
  - runtime loader
  - marshaling core
  - callback bridge
  - async bridge
  - TypeScript output
  - TypeScript-first public SDK
  - tests
  - fixtures
  - examples
  - docs
- Decide package module format strategy:
  - ESM-first
  - dual ESM/CJS
- Define release branches, tagging policy, and artifact naming.
- Define supported Node.js versions for production.
- Define supported host platforms and architectures.

## Product Identity

- Lock the product name as `j2fz` everywhere:
  - package name
  - generated metadata
  - docs
  - CI
  - release assets
- Write the product charter:
  - `j2fz` is a JavaScript bridge over the stable Fozzy native ABI
  - `j2fz` is not a compiler target
  - `j2fz` is not a replacement runtime
- Write the trust model and interoperability boundary statement.

## Native Contract Assumptions

- Record the required Fozzy-side guarantees that `j2fz` depends on:
  - stable ABI manifest schema
  - stable symbol naming policy
  - stable callback contract model
  - stable async ABI model
  - stable ownership contract model
  - stable `repr(C)` layout policy
  - stable panic boundary semantics
- Define a loader-time validation matrix for these assumptions.
- Refuse runtime startup if required ABI contract fields are missing.

## Artifact Intake

- Implement artifact discovery for:
  - shared library
  - header
  - ABI manifest
  - optional doc metadata
- Support explicit path configuration.
- Support package-local default discovery rules.
- Support environment override rules where appropriate.
- Validate that manifest package identity matches expected library identity.
- Validate that all manifest-declared symbols resolve in the native library.

## ABI Manifest Loader

- Parse Fozzy ABI manifests as the canonical source of truth.
- Validate supported ABI schema version.
- Validate package name and version fields.
- Validate panic boundary field.
- Validate export entries.
- Validate parameter contract entries.
- Validate return contract entries.
- Validate callback binding entries.
- Validate async contract entries.
- Validate `repr(C)` layout metadata.
- Validate symbol version metadata.
- Build normalized internal representations for:
  - exports
  - layouts
  - callback contracts
  - async contracts
  - ownership semantics
  - nullability semantics
  - mutability semantics
  - pointer view semantics
- Reject malformed or partially specified manifests.

## Compatibility Enforcement

- Implement strict manifest compatibility checks between:
  - generated bindings and source ABI
  - optional baseline ABI and current ABI
- Fail on:
  - schema mismatch
  - package mismatch
  - panic boundary mismatch
  - missing required exports
  - signature mismatch
  - contract mismatch
  - unsupported async model
  - unsupported callback model
- Surface compatibility failures as clear typed errors.

## Native Loader Core

- Load native shared libraries safely from explicit paths.
- Implement platform-specific library extension resolution.
- Validate library existence and readability before load.
- Handle symbol resolution errors cleanly.
- Build a raw symbol table keyed by manifest declarations.
- Refuse partially loaded modules in safe mode.
- Decide whether an unsafe/raw loader mode exists at all.
- Document raw loader risk boundaries if exposed.

## Public Runtime API

- Design the primary API for loading a Fozzy module.
- Design generated per-module APIs.
- Make TypeScript the canonical public developer surface.
- Ensure plain JavaScript consumption remains supported without weakening type-driven guarantees in generated output.
- Separate public APIs into:
  - high-level safe generated API
  - advanced contract-aware API
  - raw unsafe interop API
- Ensure the safe API never leaks raw pointers by default.
- Ensure advanced APIs remain explicit about ownership and disposal.

## TypeScript-First Contract Surface

- Define the TypeScript type model as a first-class interoperability contract.
- Ensure every generated binding has:
  - runtime implementation
  - `.d.ts` representation
  - source-map-friendly generated TS/JS structure
  - ownership and disposal documentation
- Decide whether generated output is authored as:
  - TypeScript source compiled at package build time
  - JavaScript plus generated declarations
- Prefer generated TypeScript source if it improves maintainability and traceability.
- Ensure all public exported APIs are representable in strict TypeScript.
- Ensure all unsafe/raw APIs are visibly branded in TypeScript types.
- Define the no-`any` policy for generator output.
- Define the no-implicit-lossy-coercion policy in TypeScript-facing APIs.
- Keep TypeScript types aligned with the manifest contract rather than heuristic inference.

## Type Mapping: Scalars

- Map `i8/i16/i32/u8/u16/u32` to JS `number`.
- Map `i64/u64` to JS `bigint`.
- Map `f32/f64` to JS `number`.
- Map `bool` to JS `boolean`.
- Enforce range checks before native calls.
- Reject lossy 64-bit coercions by default.
- Decide whether optional explicit narrowing helpers are supported.
- Generate TypeScript signatures that reflect these rules exactly.
- Ensure generated TypeScript signatures reject invalid integer classes at compile time wherever possible.

## Type Mapping: Pointers and Buffers

- Implement borrowed byte-region inputs from:
  - `Buffer`
  - `Uint8Array`
  - `ArrayBufferView`
- Implement explicit mutable buffer wrappers for `out` and `inout` contracts.
- Implement opaque handle wrappers for owned native pointers.
- Implement explicit raw-pointer escape hatches only in unsafe APIs.
- Enforce pointer contract interpretation from the ABI manifest.
- Enforce mutability restrictions.
- Enforce view-shape restrictions.
- Enforce nullability restrictions.
- Prevent use-after-dispose on owned handles.
- Brand owned and opaque pointer-backed handles in TypeScript so they cannot be confused with ordinary objects.

## Type Mapping: Strings

- Standardize on UTF-8 handling.
- Encode JS string inputs to temporary borrowed native buffers.
- Define exact handling for:
  - borrowed string inputs
  - owned returned string memory
  - nullable string contracts
  - explicit byte-string versus text-string contracts
- Reject implicit NUL-terminated assumptions unless the ABI contract explicitly allows them.
- Implement safe decoding failures with clear error reporting.

## Type Mapping: `repr(C)` Structs and Enums

- Generate runtime layout definitions from manifest metadata.
- Generate TypeScript interfaces or equivalent type declarations.
- Decide the exact TypeScript representation policy for:
  - fieldless enums
  - payload enums
  - fixed-size arrays
  - opaque named handles
- Implement struct encoders.
- Implement struct decoders.
- Validate expected size and alignment where practical.
- Generate enum constants or union types as appropriate.
- Define the policy for payload enums if the native ABI supports them.
- Reject unsupported layout shapes at generation time.

## Unsupported Types Policy

- Build an explicit unsupported-types matrix.
- Reject unsupported exports during generation rather than leaving them half-bound.
- Provide actionable diagnostics when a Fozzy export cannot be represented safely in JS.
- Define the extension path for future support rather than silently coercing unsupported types.

## Ownership and Resource Lifecycle

- Model ownership classes explicitly:
  - value
  - borrowed
  - owned
  - out
  - inout
- Implement owned native resource wrappers with:
  - explicit `dispose()`
  - disposed-state tracking
  - optional finalizer fallback
- Ensure finalizers are backup safety nets, not primary lifecycle policy.
- Define error behavior for double-dispose.
- Define behavior for abandoned owned resources.
- Add leak-detection diagnostics in debug/test modes.
- Ensure callback-related native resources also follow explicit lifecycle rules.
- Model disposable owned resources as explicit TypeScript interfaces or abstract base contracts.

## Error System

- Define the `j2fz` error hierarchy.
- Implement typed errors for:
  - ABI mismatch
  - symbol load failure
  - marshaling failure
  - ownership misuse
  - callback failure
  - async interop failure
  - native boundary failure
- Ensure sync calls throw synchronously.
- Ensure async calls reject Promises.
- Normalize low-level native failures into stable JS-facing error forms.
- Ensure JS exceptions never unwind across the native boundary.

## Sync Call Path

- Implement the raw sync invocation path.
- Add pre-call argument validation.
- Add post-call return validation.
- Apply ownership and disposal policies after the call returns.
- Attach debug metadata for tracing and diagnostics.
- Ensure wrapper overhead remains easy to reason about.

## Async Interop

- Implement the finalized Fozzy async ABI bridge.
- Generate Promise-based wrappers for async exports.
- Implement start/poll/await/drop orchestration.
- Guarantee cleanup on success.
- Guarantee cleanup on failure.
- Guarantee cleanup on cancellation or abandonment where possible.
- Ensure no event-loop-hostile busy spinning in JS.
- Add timeout and cancellation policies if supported by the native contract.
- Define the low-level async-handle API for advanced consumers if needed.

## Callback Bridge

- Implement callback registration from JS into native bridge shims.
- Implement callback handle objects with explicit disposal.
- Enforce signature compatibility at registration time.
- Enforce context lifetime rules.
- Prevent callback invocation after disposal.
- Define behavior for reentrant callbacks.
- Define behavior for concurrent callback invocation.
- Normalize thrown JS callback exceptions into native-compatible failure semantics.
- Add tracing and diagnostics for callback registration, invocation, and teardown.
- Generate TypeScript callback signatures from ABI metadata rather than handwritten callback declarations.
- Ensure callback context types are represented explicitly in generated TypeScript APIs.

## Generator

- Build a manifest-driven code generator.
- Generate JavaScript wrappers from ABI metadata.
- Generate TypeScript declaration files.
- Generate TypeScript source wrappers if selected as the primary output mode.
- Generate owned-handle helpers.
- Generate callback registration helpers where applicable.
- Generate async wrappers where applicable.
- Generate docs metadata from ABI fields.
- Guarantee strict-TypeScript-clean generated output.
- Guarantee generated output does not rely on `any`.
- Reject generation when the ABI contract requires unsupported features.
- Ensure generation is deterministic.
- Ensure regenerated output is stable for CI diffs.

## Generated API Ergonomics

- Design wrappers to look idiomatic in JavaScript.
- Design wrappers to feel native in TypeScript.
- Keep ownership semantics visible in names or docs where needed.
- Avoid exposing raw ABI trivia in the primary API unless necessary.
- Keep advanced and unsafe surfaces clearly separated.
- Ensure Promise-returning wrappers are natural to consume.
- Ensure TypeScript output teaches correct usage by type shape alone where possible.

## Raw Unsafe API

- Decide the exact scope of any raw unsafe surface.
- Require explicit opt-in for raw symbol calls.
- Require explicit opt-in for raw pointer handling.
- Keep raw unsafe APIs out of default imports.
- Mark raw APIs loudly in generated docs and type declarations.

## Packaging Modes

- Support embedded generated package mode.
- Support runtime loader SDK mode.
- Define packaging structure for generated modules.
- Define publishing structure for reusable runtime core versus generated outputs.
- Decide whether generator output is checked in or built during publish.

## TypeScript Tooling

- Emit strong TypeScript declarations.
- Brand opaque handles.
- Distinguish nullable and non-nullable contracts.
- Preserve `bigint` semantics for 64-bit integers.
- Model disposable resources explicitly.
- Add TS compile tests against generated output.
- Run strict TypeScript in CI.
- Enforce no implicit `any`, no unchecked indexed access, and exact optional property handling where feasible.
- Add golden tests for generated TypeScript API shape.
- Add consumer-style TypeScript fixture projects that import generated bindings.

## Observability

- Implement structured diagnostics for:
  - module load
  - manifest validation
  - symbol resolution
  - sync call execution
  - async call lifecycle
  - callback invocation
  - disposal and leak warnings
- Define observability modes:
  - silent
  - debug
  - structured logger hook
- Add correlation IDs where possible.
- Define any trace-link integration points to Fozzy-native evidence.

## Security Posture

- Validate every manifest before use.
- Restrict dynamic library loading to explicit trusted paths by default.
- Prevent pointer fabrication in safe APIs.
- Prevent unsafe symbol lookup through the high-level API.
- Validate callback registration inputs strictly.
- Define the untrusted-native-library story clearly.
- Document trust boundaries for consumers.

## Performance Work

- Benchmark scalar sync call overhead.
- Benchmark borrowed buffer call overhead.
- Benchmark owned resource creation and disposal cost.
- Benchmark callback invocation overhead.
- Benchmark async bridge overhead.
- Minimize copies where the ABI contract permits it.
- Keep safe-path overhead predictable and documented.
- Decide whether a specialized fast path is needed for hot loops.

## Test Matrix

- Add unit tests for:
  - manifest parsing
  - compatibility validation
  - scalar marshaling
  - string marshaling
  - buffer marshaling
  - struct marshaling
  - enum marshaling
  - ownership tracking
  - disposal behavior
  - error translation
  - callback lifecycle
  - async lifecycle
- Add integration tests against real Fozzy-built fixture libraries.
- Add negative tests for malformed manifests.
- Add negative tests for symbol mismatch.
- Add negative tests for callback misuse.
- Add negative tests for ownership misuse.
- Add negative tests for async handle misuse.

## Cross-Language Contract Fixtures

- Create canonical Fozzy fixture libraries specifically for `j2fz`.
- Include fixtures for:
  - scalar calls
  - strings
  - byte buffers
  - `repr(C)` structs
  - enums
  - owned resource returns
  - out and inout buffers
  - callbacks
  - async exports
  - native error paths
- Keep fixture ABI baselines under compatibility test.

## Determinism and Evidence

- Define the exact role of Fozzy deterministic evidence in `j2fz` release gates.
- Require the source Fozzy fixture libraries to pass the full Fozzy evidence chain.
- Add `j2fz` bridge-layer tests that run against those verified native artifacts.
- Record bridge test artifacts and logs for CI review.
- Define what `j2fz` can and cannot claim about deterministic replay on the JS side.

## Documentation

- Write the top-level product overview.
- Write the trust model.
- Write the ABI dependency model.
- Write the ownership model.
- Write the callback model.
- Write the async model.
- Write the error model.
- Write the TypeScript model.
- Write the packaging model.
- Write the unsafe raw API warning docs.
- Generate per-binding API docs from manifests.
- Include exact disposal expectations in every relevant doc path.

## Author Workflow

- Document the Fozzy author workflow for producing JS-ready native adapters.
- Require Fozzy authors to expose intentional ABI adapter modules rather than arbitrary internal functions.
- Document how to build the native library, generate manifests, and run compatibility gates.
- Document how to run `j2fz` generation and publish output.

## Consumer Workflow

- Document installation flow.
- Document loading flow.
- Document sync use.
- Document async use.
- Document callback registration use.
- Document owned resource disposal.
- Document troubleshooting steps for ABI mismatch and load failures.

## CI and Release

- Add CI for:
  - formatting
  - linting
  - type-checking
  - unit tests
  - integration tests
  - fixture generation
  - compatibility tests
  - package build
- Add release artifact validation.
- Add generated-output drift checks.
- Add platform matrix CI where possible.
- Add prepublish integrity checks.
- Define semver policy for:
  - runtime core
  - generator
  - generated package outputs

## Production Readiness

- Define production support policy.
- Define incident triage workflow.
- Define logging expectations for field debugging.
- Define memory/resource leak response workflow.
- Define ABI break response workflow.
- Define platform support response workflow.
- Define long-term maintenance plan for generator and runtime core.

## Exit Criteria

- Runtime loader is stable on all supported platforms.
- Manifest validation is strict and complete.
- High-level sync bindings are production-ready.
- High-level async bindings are production-ready.
- Callback bridge is production-ready.
- Ownership and disposal behavior are production-ready.
- TypeScript output is production-ready.
- Generator output is deterministic and production-ready.
- Test coverage is broad across all supported contract classes.
- Documentation is sufficient for both Fozzy authors and JS consumers.
- CI and release gates are green and repeatable.

## Done Log

✅ Create the `j2fz` directory.
✅ Create this actionable production checklist.
✅ Scaffold the TypeScript-first repository layout.
✅ Add strict TypeScript project configuration.
✅ Add manifest parsing and validation foundation.
✅ Add Koffi-backed native loader foundation.
✅ Add manifest-driven TypeScript binding generation foundation.
✅ Add native integration tests with a compiled shared-library fixture and callback roundtrip.
