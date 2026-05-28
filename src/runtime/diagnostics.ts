import type { DiagnosticEvent, DiagnosticsOptions } from "../types/public.js";

export type DiagnosticEmitter = (event: DiagnosticEvent) => void;

export function createDiagnosticEmitter(options: DiagnosticsOptions | undefined): DiagnosticEmitter {
  const mode = options?.mode ?? "silent";
  const onEvent = options?.onEvent;

  if (mode === "silent" && onEvent === undefined) {
    return () => {};
  }

  return (event: DiagnosticEvent) => {
    if (mode === "debug") {
      console.error(`[j2fz:${event.kind}] ${event.message}`);
    }
    onEvent?.(event);
  };
}
