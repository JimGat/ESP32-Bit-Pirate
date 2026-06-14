export function parseSignature(output) {
  const patterns = [
    /Device signature\s*=\s*0x([0-9a-f]{6})/i,
    /signature\s*=\s*0x([0-9a-f]{6})/i,
  ];
  for (const pattern of patterns) {
    const match = output.match(pattern);
    if (match) {
      return `0x${match[1].toLowerCase()}`;
    }
  }
  return null;
}

export function parseAvrdudeVersion(output) {
  const match = output.match(/avrdude(?:\.exe)?:\s+Version\s+([^\s,]+)/i);
  return match?.[1] ?? null;
}

export function summarizeOutput(output) {
  const signature = parseSignature(output);
  const version = parseAvrdudeVersion(output);
  const errors = output
    .split("\n")
    .filter((line) => /error|failed|timeout|invalid/i.test(line))
    .slice(-4);

  return { signature, version, errors };
}
