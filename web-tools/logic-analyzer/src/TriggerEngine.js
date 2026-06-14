const TRIGGER_LABELS = {
  low: "0",
  high: "1",
  rising: "↑",
  falling: "↓",
  edge: "↕",
};

export function getTriggerConditions(channels = []) {
  return channels
    .filter((channel) => channel.enabled && channel.trigger && channel.trigger !== "none")
    .map((channel) => ({
      channelIndex: channel.index,
      bit: channel.bit,
      name: channel.name || `D${channel.index}`,
      type: channel.trigger,
    }));
}

export function findTriggerIndex(samples, conditions = [], { maxIndex = null } = {}) {
  if (!(samples instanceof Uint8Array) || samples.length === 0 || conditions.length === 0) {
    return -1;
  }

  const needsPreviousSample = conditions.some((condition) => isEdgeTrigger(condition.type));
  const firstSample = needsPreviousSample ? 1 : 0;
  const lastSample = maxIndex == null
    ? samples.length - 1
    : Math.min(samples.length - 1, Math.floor(maxIndex));

  if (lastSample < firstSample) {
    return -1;
  }

  for (let sampleIndex = firstSample; sampleIndex <= lastSample; sampleIndex += 1) {
    const current = samples[sampleIndex];
    const previous = sampleIndex > 0 ? samples[sampleIndex - 1] : current;
    if (conditions.every((condition) => matchesCondition(previous, current, condition))) {
      return sampleIndex;
    }
  }

  return -1;
}

export function calculateTriggerSearchSampleCount(outputSampleCount, sampleMemoryBytes) {
  const outputCount = alignSampleCount(outputSampleCount);
  const memoryCount = alignSampleCount(sampleMemoryBytes);
  const maximumOutputCount = Math.floor(memoryCount / 8) * 4;

  if (outputCount > maximumOutputCount) {
    throw new Error(
      `Triggered captures are limited to ${maximumOutputCount} samples because the software trigger needs a 2× search window.`,
    );
  }

  return outputCount * 2;
}

export function alignCaptureToTrigger(capture, triggerIndex, outputSampleCount) {
  const outputCount = alignSampleCount(outputSampleCount);
  if (!capture?.packedSamples || triggerIndex < 0 || triggerIndex + outputCount > capture.packedSamples.length) {
    throw new Error("The trigger was too close to the end of the search window.");
  }

  const alignedSamples = capture.packedSamples.slice(triggerIndex, triggerIndex + outputCount);
  const triggerOffsetMs = capture.sampleRateHz > 0
    ? (triggerIndex / capture.sampleRateHz) * 1000
    : 0;
  const startedAtMs = Date.parse(capture.startedAt);

  return {
    ...capture,
    sampleCount: outputCount,
    triggerIndex: 0,
    packedSamples: alignedSamples,
    startedAt: Number.isFinite(startedAtMs)
      ? new Date(startedAtMs + triggerOffsetMs).toISOString()
      : capture.startedAt,
  };
}

export function formatTriggerSummary(conditions = []) {
  if (conditions.length === 0) {
    return "None";
  }

  return conditions
    .map((condition) => `${condition.name} ${TRIGGER_LABELS[condition.type] ?? condition.type}`)
    .join(" + ");
}

function matchesCondition(previousSample, currentSample, condition) {
  const previous = (previousSample >> condition.bit) & 1;
  const current = (currentSample >> condition.bit) & 1;

  switch (condition.type) {
    case "low":
      return current === 0;
    case "high":
      return current === 1;
    case "rising":
      return previous === 0 && current === 1;
    case "falling":
      return previous === 1 && current === 0;
    case "edge":
      return previous !== current;
    default:
      return false;
  }
}

function isEdgeTrigger(type) {
  return type === "rising" || type === "falling" || type === "edge";
}

function alignSampleCount(value) {
  const count = Math.max(4, Math.floor(Number(value) || 0));
  return Math.floor(count / 4) * 4;
}
