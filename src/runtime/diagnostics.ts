import type { DiagnosticEvent, DiagnosticsOptions } from "../types/public.js";

export interface DiagnosticEmitter {
  (event: DiagnosticEvent): void;
  readonly enabled: boolean;
}

export function createDiagnosticEmitter(options: DiagnosticsOptions | undefined): DiagnosticEmitter {
  const mode = options?.mode ?? "silent";
  const onEvent = options?.onEvent;

  if (mode === "silent" && onEvent === undefined) {
    const emit: DiagnosticEmitter = Object.assign(
      () => {},
      { enabled: false as const },
    );
    return emit;
  }

  const emit: DiagnosticEmitter = Object.assign(
    (event: DiagnosticEvent) => {
      if (mode === "debug") {
        console.error(`[j2fz:${event.kind}] ${event.message}`);
      }
      onEvent?.(event);
    },
    { enabled: true as const },
  );
  return emit;
}
