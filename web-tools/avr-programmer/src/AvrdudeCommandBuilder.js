const AVRDUDE_CONF_PATH = "/tmp/avrdude.conf";

export class AvrdudeCommandBuilder {
  constructor({ programmer = "buspirate", port = "/dev/null", baudRate = 115200, spiFrequency = 1 } = {}) {
    this.programmer = programmer;
    this.port = port;
    this.baudRate = baudRate;
    this.spiFrequency = spiFrequency;
  }

  detectSignature(partId) {
    return this.base(partId, ["-v"]);
  }

  readMemory(partId, memory, outputPath, format = "i") {
    return this.base(partId, ["-U", `${memory}:r:${outputPath}:${format}`]);
  }

  writeMemory(partId, memory, inputPath, format = "i", { noErase = false } = {}) {
    const args = noErase ? ["-D"] : [];
    return this.base(partId, [...args, "-U", `${memory}:w:${inputPath}:${format}`]);
  }

  verifyMemory(partId, memory, inputPath, format = "i") {
    return this.base(partId, ["-V", "-U", `${memory}:v:${inputPath}:${format}`]);
  }

  eraseChip(partId) {
    return this.base(partId, ["-e"]);
  }

  readFuse(partId, fuseName, outputPath) {
    return this.base(partId, ["-U", `${fuseName}:r:${outputPath}:h`]);
  }

  writeFuse(partId, fuseName, value) {
    return this.base(partId, ["-U", `${fuseName}:w:0x${value.toString(16)}:m`]);
  }

  base(partId, extraArgs = []) {
    const args = [
      "avrdude",
      "-C", AVRDUDE_CONF_PATH,
      "-c", this.programmer,
      "-P", this.port,
      "-p", partId,
      "-b", String(this.baudRate),
      "-x", `spifreq=${this.spiFrequency}`,
      ...extraArgs,
    ];
    return args;
  }
}

export function argsToString(args) {
  return args.map((arg) => (/\s/.test(arg) ? JSON.stringify(arg) : arg)).join(" ");
}
