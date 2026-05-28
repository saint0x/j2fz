import test from "node:test";
import assert from "node:assert/strict";

import type { AbiExport } from "../src/types/abi.js";
import { OwnershipError, TypeMarshalingError } from "../src/runtime/errors.js";
import { adaptCallArgs, adaptResultValue } from "../src/runtime/ownership.js";

const EMPTY_REGISTRY = {
  layouts: new Map(),
};

function makeExport(overrides: Partial<AbiExport> = {}): AbiExport {
  return {
    name: "demo_export",
    async: false,
    symbolVersion: 1,
    params: [],
    return: {
      fzy: "void",
      c: "void",
      contract: {
        ownership: "value",
        nullability: "n/a",
        mutability: "const",
      },
    },
    contract: {
      execution: "sync",
      callbackBindings: [],
      asyncBoundary: null,
    },
    ...overrides,
  };
}

test("adaptCallArgs rejects wrong argument count", () => {
  const abiExport = makeExport({
    params: [
      {
        name: "value",
        fzy: "i32",
        c: "int32_t",
        contract: {
          ownership: "value",
          nullability: "n/a",
          mutability: "const",
          lifetimeAnchor: null,
          view: null,
        },
      },
    ],
  });

  assert.throws(() => adaptCallArgs(abiExport, []), /expected 1 args but got 0/);
});

test("adaptCallArgs rejects invalid bigint-compatible values", () => {
  const abiExport = makeExport({
    params: [
      {
        name: "len",
        fzy: "usize",
        c: "size_t",
        contract: {
          ownership: "value",
          nullability: "n/a",
          mutability: "const",
          lifetimeAnchor: null,
          view: null,
        },
      },
    ],
  });

  assert.throws(() => adaptCallArgs(abiExport, ["nope"]), /must be bigint-compatible/);
});

test("adaptCallArgs rejects invalid boolean values", () => {
  const abiExport = makeExport({
    params: [
      {
        name: "enabled",
        fzy: "bool",
        c: "bool",
        contract: {
          ownership: "value",
          nullability: "n/a",
          mutability: "const",
          lifetimeAnchor: null,
          view: null,
        },
      },
    ],
  });

  assert.throws(() => adaptCallArgs(abiExport, [1]), /must be boolean/);
});

test("adaptCallArgs rejects invalid ptr_len values", () => {
  const abiExport = makeExport({
    params: [
      {
        name: "ptr_borrowed",
        fzy: "*u8",
        c: "uint8_t*",
        contract: {
          ownership: "borrowed",
          nullability: "non_null",
          mutability: "const",
          lifetimeAnchor: "loan:ptr",
          view: {
            kind: "ptr_len",
            lengthParam: "len",
          },
        },
      },
      {
        name: "len",
        fzy: "usize",
        c: "size_t",
        contract: {
          ownership: "value",
          nullability: "n/a",
          mutability: "const",
          lifetimeAnchor: null,
          view: null,
        },
      },
    ],
  });

  assert.throws(
    () => adaptCallArgs(abiExport, [{ not: "a buffer" }, 2n]),
    /must be Uint8Array, Buffer, or string/,
  );
});

test("owned pointer handle rejects string decode for non-string pointer", () => {
  const releases: unknown[] = [];
  const abiExport = makeExport({
    name: "alloc_bytes",
    return: {
      fzy: "*u8",
      c: "uint8_t*",
      contract: {
        ownership: "owned",
        nullability: "non_null",
        mutability: "mut",
      },
    },
  });

  const handle = adaptResultValue(
    abiExport,
    Buffer.from([65, 66, 67]),
    {
      func() {
        return (value: unknown) => {
          releases.push(value);
        };
      },
    } as never,
    EMPTY_REGISTRY,
    { alloc_bytes: "alloc_bytes_free" },
  ) as {
    decodeString(): string;
    decodeBytes(length: number): Uint8Array;
    dispose(): void;
    disposed: boolean;
  };

  assert.throws(() => handle.decodeString(), OwnershipError);
  assert.deepEqual([...handle.decodeBytes(3)], [65, 66, 67]);
  handle.dispose();
  assert.equal(handle.disposed, true);
  assert.equal(releases.length, 1);
});

test("owned pointer handle rejects access after dispose", () => {
  const abiExport = makeExport({
    name: "alloc_bytes",
    return: {
      fzy: "*u8",
      c: "uint8_t*",
      contract: {
        ownership: "owned",
        nullability: "non_null",
        mutability: "mut",
      },
    },
  });

  const handle = adaptResultValue(
    abiExport,
    Buffer.from([65, 66, 67]),
    {
      func() {
        return () => {};
      },
    } as never,
    EMPTY_REGISTRY,
    { alloc_bytes: "alloc_bytes_free" },
  ) as {
    decodeBytes(length: number): Uint8Array;
    dispose(): void;
  };

  handle.dispose();
  assert.throws(() => handle.decodeBytes(1), OwnershipError);
});

test("owned pointer handle rejects invalid decode length", () => {
  const abiExport = makeExport({
    name: "alloc_bytes",
    return: {
      fzy: "*u8",
      c: "uint8_t*",
      contract: {
        ownership: "owned",
        nullability: "non_null",
        mutability: "mut",
      },
    },
  });

  const handle = adaptResultValue(
    abiExport,
    Buffer.from([65, 66, 67]),
    {
      func() {
        return () => {};
      },
    } as never,
    EMPTY_REGISTRY,
    { alloc_bytes: "alloc_bytes_free" },
  ) as {
    decodeBytes(length: number): Uint8Array;
  };

  assert.throws(() => handle.decodeBytes(-1), TypeMarshalingError);
});
