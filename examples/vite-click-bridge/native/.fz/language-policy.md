# Language Policy

- Schema: `fozzylang.language_policy.v1`
- Language tier: `core_v1`
- Experimental opt-in required: `true`
- Change policy: `additive_only`

## Syntax Freeze

- `fn`: function declarations
- `let`: immutable bindings
- `let mut`: mutable bindings
- `struct`: struct declarations
- `enum`: enum declarations
- `match`: pattern matching
- `trait`: trait declarations
- `impl`: impl blocks
- `async`: async declarations
- `await`: async suspension
- `rpc`: rpc declarations
- `unsafe metadata`: compiler-generated unsafe contracts
- `defer`: scope cleanup
- `use core.*`: capability and stdlib imports
- `extern`: external ABI imports
- `pubext`: public ABI exports

## Profiles

| Profile | Checks | Unsafe | Backend | Runtime Imports | Capabilities | Emit Safety Artifacts | Optimize | Optimization | Diagnostics |
|---|---|---|---|---|---|---|---|---|---|
| `dev` | `true` | `false` | `cranelift` | `declared_native_runtime_contracts_only` | `explicit_compiler_checked` | `true` | `false` | `O0` | `standard` |
| `release` | `true` | `true` | `llvm` | `declared_native_runtime_contracts_only` | `explicit_compiler_checked` | `true` | `true` | `O3` | `standard` |
| `strict` | `true` | `true` | `llvm` | `declared_native_runtime_contracts_only` | `explicit_compiler_checked` | `true` | `true` | `O2+g` | `strict` |
| `verify` | `true` | `true` | `llvm` | `declared_native_runtime_contracts_only` | `explicit_compiler_checked` | `true` | `true` | `O1+g` | `standard` |
