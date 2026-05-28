export class J2FzError extends Error {
  constructor(message: string, options?: { cause?: unknown }) {
    super(message, options);
    this.name = new.target.name;
  }
}

export class AbiParseError extends J2FzError {}
export class AbiValidationError extends J2FzError {}
export class AbiMismatchError extends J2FzError {}
export class SymbolLoadError extends J2FzError {}
export class TypeMarshalingError extends J2FzError {}
export class OwnershipError extends J2FzError {}
export class CallbackError extends J2FzError {}
export class AsyncInteropError extends J2FzError {}
export class NativeBoundaryError extends J2FzError {}
