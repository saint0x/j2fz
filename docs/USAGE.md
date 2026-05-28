# j2fz Usage

`j2fz` exposes three distinct runtime entry paths:

- `loadFozzyModule()`
  Use when you already know the absolute library and manifest paths.
- `loadFozzyPackage()`
  Use when you want `j2fz` to discover the generated package layout.
- generated package entrypoints
  Use `createBindings()` for explicit paths or `createDiscoveredBindings()` from generated packages.

## Direct Runtime Loading

```ts
import { loadFozzyModule } from "j2fz";

const module = loadFozzyModule({
  paths: {
    sharedLibrary: "/absolute/path/to/libdemo.dylib",
    abiManifest: "/absolute/path/to/abi.manifest.json",
  },
  package: {
    name: "demo.bridge",
    version: "1.0.0",
  },
});

const hash32 = module.exports.get("hash32");
const result = hash32?.call(Buffer.from("hello"), 5n);
module.dispose();
```

## Generated Package Loading

Generated packages export two entrypoints:

- `createBindings(options)`
  Explicit runtime paths.
- `createDiscoveredBindings(options?)`
  Package-local artifact discovery rooted at the generated package directory.

```ts
import { createBindings, createDiscoveredBindings } from "demo.bridge-j2fz";

const explicit = createBindings({
  paths: {
    sharedLibrary: "/absolute/path/to/libdemo.dylib",
    abiManifest: "/absolute/path/to/abi.manifest.json",
  },
});

const discovered = createDiscoveredBindings();
```

## Diagnostics

Diagnostics are optional and disabled by default.

```ts
import { loadFozzyModule } from "j2fz";

const events: string[] = [];

const module = loadFozzyModule({
  paths: {
    sharedLibrary: "/absolute/path/to/libdemo.dylib",
    abiManifest: "/absolute/path/to/abi.manifest.json",
  },
  diagnostics: {
    mode: "debug",
    onEvent(event) {
      events.push(event.kind);
    },
  },
});
```

Current emitted diagnostics cover:

- package discovery
- manifest parsing
- library load
- export binding
- sync call lifecycle
- async lifecycle
- callback registration, invocation, completion, and disposal
- module disposal
