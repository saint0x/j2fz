import net from "node:net";
import { unlink } from "node:fs/promises";

import { JsHostRuntime } from "./runtime.js";

interface PendingCallback {
  resolve: (value: number) => void;
  reject: (error: Error) => void;
}

interface NativeCallbackRegistration {
  readonly name: string;
  readonly invoke: (value: number) => Promise<number>;
}

export interface JsHostServerOptions {
  socketPath: string;
}

export interface JsHostServerHandle {
  readonly socketPath: string;
  close(): Promise<void>;
}

function encodeHex(value: Uint8Array): string {
  return Buffer.from(value).toString("hex");
}

function decodeHex(value: string): Uint8Array {
  return new Uint8Array(Buffer.from(value, "hex"));
}

function encodeUtf8Hex(value: string): string {
  return encodeHex(Buffer.from(value, "utf8"));
}

function decodeUtf8Hex(value: string): string {
  return Buffer.from(value, "hex").toString("utf8");
}

function expectBigIntField(value: string | undefined, field: string): bigint {
  if (value === undefined || value.length === 0) {
    throw new Error(`missing bigint field ${field}`);
  }
  return BigInt(value);
}

function expectNumberField(value: string | undefined, field: string): number {
  if (value === undefined || value.length === 0) {
    throw new Error(`missing number field ${field}`);
  }
  const parsed = Number(value);
  if (!Number.isFinite(parsed)) {
    throw new Error(`invalid number field ${field}`);
  }
  return parsed;
}

function parseHandleList(raw: string | undefined): bigint[] {
  if (raw === undefined || raw.length === 0) {
    return [];
  }
  return raw.split(",").filter((part) => part.length > 0).map((part) => BigInt(part));
}

class JsHostConnection {
  readonly #socket: net.Socket;
  readonly #runtime: JsHostRuntime;
  readonly #registrations = new Map<bigint, NativeCallbackRegistration>();
  readonly #pendingCallbacks = new Map<number, PendingCallback>();
  #closed = false;
  #buffer = "";
  #nextCallbackRequestId = 1;

  constructor(socket: net.Socket) {
    this.#socket = socket;
    this.#runtime = new JsHostRuntime({
      resolveModuleNamespace: async (specifier) => {
        if (specifier !== "j2fz:host") {
          return null;
        }
        const entries = [...this.#registrations.values()].map((registration) => [registration.name, registration.invoke] as const);
        return {
          ...Object.fromEntries(entries),
          getCallback: (name: string): ((value: number) => Promise<number>) => {
            const entry = [...this.#registrations.values()].find((registration) => registration.name === name);
            if (!entry) {
              throw new Error(`registered callback not found: ${name}`);
            }
            return entry.invoke;
          },
        };
      },
    });
    socket.setEncoding("utf8");
    socket.on("data", (chunk: string) => {
      this.#buffer += chunk;
      this.#drainBuffer();
    });
    socket.on("close", () => {
      this.#closed = true;
      for (const pending of this.#pendingCallbacks.values()) {
        pending.reject(new Error("host socket closed"));
      }
      this.#pendingCallbacks.clear();
    });
    socket.on("error", () => {
      this.#closed = true;
    });
  }

  async close(): Promise<void> {
    if (this.#closed) {
      return;
    }
    await new Promise<void>((resolve) => {
      this.#socket.end(() => resolve());
    });
  }

  #drainBuffer(): void {
    while (true) {
      const newline = this.#buffer.indexOf("\n");
      if (newline < 0) {
        return;
      }
      const line = this.#buffer.slice(0, newline);
      this.#buffer = this.#buffer.slice(newline + 1);
      if (line.length === 0) {
        continue;
      }
      void this.#handleLine(line);
    }
  }

  async #handleLine(line: string): Promise<void> {
    const parts = line.split("\t");
    const kind = parts[0];
    if (kind === "CBRES") {
      this.#handleCallbackResponse(parts);
      return;
    }
    if (kind !== "REQ") {
      return;
    }
    const requestId = parts[1];
    const op = parts[2];
    if (requestId === undefined || op === undefined) {
      return;
    }
    try {
      const payload = await this.#dispatch(op, parts.slice(3));
      this.#write(["RES", requestId, "OK", payload]);
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      this.#write(["RES", requestId, "ERR", encodeUtf8Hex(message)]);
    }
  }

  #handleCallbackResponse(parts: string[]): void {
    const requestId = expectNumberField(parts[1], "callback_request_id");
    const status = parts[2];
    const pending = this.#pendingCallbacks.get(requestId);
    if (!pending) {
      return;
    }
    this.#pendingCallbacks.delete(requestId);
    if (status !== "OK") {
      const message = decodeUtf8Hex(parts[3] ?? "");
      pending.reject(new Error(message));
      return;
    }
    pending.resolve(expectNumberField(parts[3], "callback_result"));
  }

  async #dispatch(op: string, args: string[]): Promise<string> {
    switch (op) {
      case "OPEN_MODULE": {
        const handle = await this.#runtime.openModule(decodeUtf8Hex(args[0] ?? ""));
        return handle.toString();
      }
      case "CLOSE_MODULE":
        this.#runtime.closeModule(expectBigIntField(args[0], "module_handle"));
        return "0";
      case "GET_EXPORT": {
        const handle = this.#runtime.getExport(
          expectBigIntField(args[0], "module_handle"),
          decodeUtf8Hex(args[1] ?? ""),
        );
        return handle.toString();
      }
      case "RELEASE_EXPORT":
        this.#runtime.releaseExport(expectBigIntField(args[0], "export_handle"));
        return "0";
      case "RELEASE_VALUE":
        this.#runtime.releaseValue(expectBigIntField(args[0], "value_handle"));
        return "0";
      case "CREATE_NULL":
        return this.#runtime.createNull().toString();
      case "CREATE_BOOL":
        return this.#runtime.createBool(expectNumberField(args[0], "bool_value") !== 0).toString();
      case "CREATE_I32":
        return this.#runtime.createI32(expectNumberField(args[0], "i32_value")).toString();
      case "CREATE_U64":
        return this.#runtime.createU64(expectBigIntField(args[0], "u64_value")).toString();
      case "CREATE_F64":
        return this.#runtime.createF64(expectNumberField(args[0], "f64_value")).toString();
      case "CREATE_STR":
        return this.#runtime.createString(decodeUtf8Hex(args[0] ?? "")).toString();
      case "CREATE_BYTES":
        return this.#runtime.createBytes(decodeHex(args[0] ?? "")).toString();
      case "AS_BOOL":
        return this.#runtime.asBool(expectBigIntField(args[0], "value_handle")) ? "1" : "0";
      case "AS_I32":
        return this.#runtime.asI32(expectBigIntField(args[0], "value_handle")).toString();
      case "AS_U64":
        return this.#runtime.asU64(expectBigIntField(args[0], "value_handle")).toString();
      case "AS_F64":
        return this.#runtime.asF64(expectBigIntField(args[0], "value_handle")).toString();
      case "AS_UTF8":
        return encodeHex(this.#runtime.copyUtf8(expectBigIntField(args[0], "value_handle")));
      case "COPY_UTF8":
        return encodeHex(this.#runtime.copyUtf8(expectBigIntField(args[0], "value_handle")));
      case "CALL": {
        const handle = this.#runtime.call(
          expectBigIntField(args[0], "function_handle"),
          parseHandleList(args[1]),
        );
        return handle.toString();
      }
      case "AWAIT": {
        const handle = await this.#runtime.awaitValue(expectBigIntField(args[0], "value_handle"));
        return handle.toString();
      }
      case "REGISTER_NATIVE_I32_I32": {
        const registrationHandle = expectBigIntField(args[0], "registration_handle");
        const registration: NativeCallbackRegistration = {
          name: decodeUtf8Hex(args[1] ?? ""),
          invoke: (value: number) => this.#invokeNativeCallback(registrationHandle, value),
        };
        this.#registrations.set(registrationHandle, registration);
        return registrationHandle.toString();
      }
      case "UNREGISTER_NATIVE_I32_I32":
        this.#registrations.delete(expectBigIntField(args[0], "registration_handle"));
        return "0";
      default:
        throw new Error(`unsupported host op: ${op}`);
    }
  }

  #invokeNativeCallback(registrationHandle: bigint, value: number): Promise<number> {
    const requestId = this.#nextCallbackRequestId;
    this.#nextCallbackRequestId += 1;
    return new Promise<number>((resolve, reject) => {
      this.#pendingCallbacks.set(requestId, { resolve, reject });
      this.#write(["CBREQ", requestId.toString(), registrationHandle.toString(), value.toString()]);
    });
  }

  #write(parts: string[]): void {
    if (this.#closed) {
      throw new Error("host socket is closed");
    }
    this.#socket.write(`${parts.join("\t")}\n`);
  }
}

export async function startJsHostServer(options: JsHostServerOptions): Promise<JsHostServerHandle> {
  await unlink(options.socketPath).catch(() => undefined);
  const connections = new Set<JsHostConnection>();
  const server = net.createServer((socket) => {
    const connection = new JsHostConnection(socket);
    connections.add(connection);
    socket.on("close", () => {
      connections.delete(connection);
    });
  });
  await new Promise<void>((resolve, reject) => {
    server.once("error", reject);
    server.listen(options.socketPath, () => {
      server.off("error", reject);
      resolve();
    });
  });
  return {
    socketPath: options.socketPath,
    async close(): Promise<void> {
      for (const connection of [...connections]) {
        await connection.close();
      }
      await new Promise<void>((resolve, reject) => {
        server.close((error) => {
          if (error) {
            reject(error);
            return;
          }
          resolve();
        });
      });
      await unlink(options.socketPath).catch(() => undefined);
    },
  };
}

