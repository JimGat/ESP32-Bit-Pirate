const ID_CHARS = "!#$%&()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[]^_abcdefghijklmnopqrstuvwxyz{|}~";

export function writeVcd(capture) {
  const enabledChannels = capture.channels.filter((channel) => channel.enabled);
  const timescale = chooseTimescale(capture.sampleRateHz);
  const stepTicks = Math.max(1, Math.round(timescale.unitsPerSecond / capture.sampleRateHz));
  const ids = new Map(enabledChannels.map((channel, index) => [channel.index, ID_CHARS[index]]));
  const lines = [];

  lines.push("$date");
  lines.push(`  ${capture.startedAt}`);
  lines.push("$end");
  lines.push("$version");
  lines.push("  Web Logic Analyzer SUMP capture");
  lines.push("$end");
  lines.push("$comment");
  lines.push(`  sample_rate_hz=${capture.sampleRateHz}`);
  lines.push(`  sample_count=${capture.sampleCount}`);
  if (capture.deviceMetadata?.deviceName) {
    lines.push(`  device=${capture.deviceMetadata.deviceName}`);
  }
  lines.push("$end");
  lines.push(`$timescale ${timescale.value} ${timescale.unit} $end`);
  lines.push("$scope module logic $end");
  for (const channel of enabledChannels) {
    lines.push(`$var wire 1 ${ids.get(channel.index)} ${sanitizeName(channel.name)} $end`);
  }
  lines.push("$upscope $end");
  lines.push("$enddefinitions $end");
  lines.push("#0");

  let previous = 0;
  for (const channel of enabledChannels) {
    const value = (capture.packedSamples[0] >> channel.bit) & 1;
    lines.push(`${value}${ids.get(channel.index)}`);
    previous |= value << channel.bit;
  }

  for (let sampleIndex = 1; sampleIndex < capture.sampleCount; sampleIndex += 1) {
    const sample = capture.packedSamples[sampleIndex] ?? 0;
    const changed = sample ^ previous;
    if (changed === 0) {
      continue;
    }

    lines.push(`#${sampleIndex * stepTicks}`);
    for (const channel of enabledChannels) {
      if (changed & (1 << channel.bit)) {
        lines.push(`${(sample >> channel.bit) & 1}${ids.get(channel.index)}`);
      }
    }
    previous = sample;
  }

  const durationTicks = Math.max(0, (capture.sampleCount - 1) * stepTicks);
  if (lines[lines.length - 1] !== `#${durationTicks}`) {
    lines.push(`#${durationTicks}`);
  }

  return `${lines.join("\n")}\n`;
}

export function chooseTimescale(sampleRateHz) {
  if (sampleRateHz >= 1000000000) {
    return { value: 1, unit: "ps", unitsPerSecond: 1000000000000 };
  }
  if (sampleRateHz >= 1000000) {
    return { value: 1, unit: "ns", unitsPerSecond: 1000000000 };
  }
  if (sampleRateHz >= 1000) {
    return { value: 1, unit: "us", unitsPerSecond: 1000000 };
  }
  return { value: 1, unit: "ms", unitsPerSecond: 1000 };
}

function sanitizeName(name) {
  return String(name || "signal").replace(/[^A-Za-z0-9_$[\]:.]/g, "_");
}
