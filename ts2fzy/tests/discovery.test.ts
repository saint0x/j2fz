import test from "node:test";
import assert from "node:assert/strict";
import { mkdtempSync, mkdirSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";

import { discoverLibraryPaths } from "../src/runtime/discovery.js";
import {
  defaultHeaderFileNameForPackage,
  defaultSharedLibraryFileNameForPackage,
} from "../src/runtime/platform.js";

test("discoverLibraryPaths resolves default generated package layout", () => {
  const root = mkdtempSync(join(tmpdir(), "j2fz-discovery-"));
  const nativeDir = join(root, "native");
  const includeDir = join(root, "include");
  mkdirSync(nativeDir, { recursive: true });
  mkdirSync(includeDir, { recursive: true });

  const libraryPath = join(nativeDir, defaultSharedLibraryFileNameForPackage("demo.bridge"));
  const manifestPath = join(root, "abi.manifest.json");
  const headerPath = join(includeDir, defaultHeaderFileNameForPackage("demo.bridge"));

  writeFileSync(libraryPath, "binary", "utf8");
  writeFileSync(manifestPath, "{}", "utf8");
  writeFileSync(headerPath, "/* header */", "utf8");

  const resolved = discoverLibraryPaths({
    packageRoot: root,
    package: {
      name: "demo.bridge",
      version: "1.0.0",
    },
  });

  assert.equal(resolved.sharedLibrary, libraryPath);
  assert.equal(resolved.abiManifest, manifestPath);
  assert.equal(resolved.header, headerPath);
});

test("discoverLibraryPaths honors environment overrides", () => {
  const root = mkdtempSync(join(tmpdir(), "j2fz-discovery-env-"));
  const manualLibrary = join(root, "manual.dylib");
  const manualManifest = join(root, "manual.json");
  const manualHeader = join(root, "manual.h");
  writeFileSync(manualLibrary, "binary", "utf8");
  writeFileSync(manualManifest, "{}", "utf8");
  writeFileSync(manualHeader, "/* header */", "utf8");

  const resolved = discoverLibraryPaths({
    packageRoot: root,
    env: {
      J2FZ_SHARED_LIBRARY: manualLibrary,
      J2FZ_ABI_MANIFEST: manualManifest,
      J2FZ_HEADER: manualHeader,
    },
  });

  assert.equal(resolved.sharedLibrary, manualLibrary);
  assert.equal(resolved.abiManifest, manualManifest);
  assert.equal(resolved.header, manualHeader);
});

test("discoverLibraryPaths rejects unreadable required artifacts", () => {
  const root = mkdtempSync(join(tmpdir(), "j2fz-discovery-missing-"));
  assert.throws(
    () =>
      discoverLibraryPaths({
        packageRoot: root,
        package: {
          name: "demo.bridge",
          version: "1.0.0",
        },
      }),
    /shared library is not readable/,
  );
});
