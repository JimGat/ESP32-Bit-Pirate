export class SerialError extends Error {
  constructor(message, cause = undefined) {
    super(message);
    this.name = "SerialError";
    this.cause = cause;
  }
}

export class SerialTimeoutError extends SerialError {
  constructor(message = "Serial read timed out") {
    super(message);
    this.name = "SerialTimeoutError";
  }
}

export class SerialUnsupportedError extends SerialError {
  constructor() {
    super("Web Serial is not available in this browser.");
    this.name = "SerialUnsupportedError";
  }
}
