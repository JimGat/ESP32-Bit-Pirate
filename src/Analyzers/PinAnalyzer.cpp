#include "PinAnalyzer.h"
#include <algorithm>
#include <sstream>
#include <cmath>

PinAnalyzer::PinAnalyzer(PinService& pinService)
: pinService(pinService) {}

void PinAnalyzer::begin(uint8_t pin_) {
    end(); // clean up if rebeginning
    pulseRing = new uint32_t[PULSE_RING];
    riseUs    = new uint32_t[RISE_RING];

    pin = pin_;
    pinService.setInput(pin);

    lastLevel = pinService.read(pin);
    startLevel = lastLevel;

    windowStartMs = millis();
    lastChangeUs = micros();

    edges = 0;
    highUs = 0;
    lowUs  = 0;
    minPulseUs = 0xFFFFFFFF;
    maxPulseUs = 0;
    basePulseUs = 0;

    bursts = 0;
    burstEdges = 0;
    maxGapUs = 0;
    maxGapWasHigh = false;
    inBurst = false;

    pulseCount = 0;
    pulseHead = 0;

    riseCount = 0;
    riseHead = 0;
    lastRiseUs = 0;
    lastHighPulseUs = 0;
}

void PinAnalyzer::end() {
    if (pulseRing) {
        delete[] pulseRing;
        pulseRing = nullptr;
    }

    if (riseUs) {
        delete[] riseUs;
        riseUs = nullptr;
    }
}

bool PinAnalyzer::shouldReport(unsigned long nowMs) const {
    return (nowMs - windowStartMs) >= ANALYZE_DURATION_MS;
}

void PinAnalyzer::sample() {
    bool v = pinService.read(pin);
    if (v == lastLevel) return;
    onEdge(v, micros());
}

void PinAnalyzer::onEdge(bool newLevel, uint32_t nowUs) {
    uint32_t dt = nowUs - lastChangeUs;

    // Accumulate time spent in last level
    if (lastLevel) highUs += dt;
    else           lowUs  += dt;

    // Pulse stats 
    if (dt < minPulseUs) minPulseUs = dt;
    if (dt > maxPulseUs) maxPulseUs = dt;

    pulseRing[pulseHead] = dt;
    pulseHead = (pulseHead + 1) % PULSE_RING;
    if (pulseCount < PULSE_RING) pulseCount++;

    // Burst detection
    const uint32_t gapThresholdUs = 5000; // 5ms
    if (dt >= gapThresholdUs) {
        if (inBurst) inBurst = false;
        bursts++;
        if (dt > maxGapUs) {
            maxGapUs = dt;
            maxGapWasHigh = lastLevel;
        }
    } else {
        inBurst = true;
        burstEdges++;
    }

    // Rising edges for servo period
    if (!lastLevel && newLevel) { // LOW->HIGH
        riseUs[riseHead] = nowUs;
        riseHead = (riseHead + 1) % RISE_RING;
        if (riseCount < RISE_RING) riseCount++;

        if (lastRiseUs != 0) {
            // period candidate is time between rising edges (collected later)
        }
        lastRiseUs = nowUs;
    }

    // Falling edge gives us HIGH pulse width
    if (lastLevel && !newLevel) { // HIGH->LOW
        lastHighPulseUs = dt; // dt was time HIGH
    }

    edges++;
    lastLevel = newLevel;
    lastChangeUs = nowUs;
}

void PinAnalyzer::closeTail(uint32_t nowUs) {
    uint32_t dt = nowUs - lastChangeUs;
    if (lastLevel) highUs += dt;
    else           lowUs  += dt;

    const uint32_t gapThresholdUs = 5000; // 5ms
    if (dt >= gapThresholdUs && dt > maxGapUs) {
        maxGapUs = dt;
        maxGapWasHigh = lastLevel;
    }

    // Don’t push tail into pulse ring
    lastChangeUs = nowUs;
}

void PinAnalyzer::resetWindow() {
    // Start a fresh window from current state
    windowStartMs = millis();
    lastChangeUs = micros();
    startLevel = lastLevel;

    edges = 0;
    highUs = 0;
    lowUs  = 0;
    minPulseUs = 0xFFFFFFFF;
    maxPulseUs = 0;

    bursts = 0;
    burstEdges = 0;
    maxGapUs = 0;
    maxGapWasHigh = false;
    inBurst = false;

    pulseCount = 0;
    pulseHead = 0;
    basePulseUs = 0;

    riseCount = 0;
    riseHead = 0;
    lastRiseUs = 0;
    lastHighPulseUs = 0;
}

void PinAnalyzer::collectPulses(std::vector<uint32_t>& out) const {
    out.clear();
    out.reserve(pulseCount);

    // Oldest -> newest
    int start = (pulseHead - pulseCount);
    if (start < 0) start += PULSE_RING;

    for (int i = 0; i < (int)pulseCount; i++) {
        int idx = (start + i) % PULSE_RING;
        out.push_back(pulseRing[idx]);
    }
}

void PinAnalyzer::collectRisePeriods(std::vector<uint32_t>& outPeriods) const {
    outPeriods.clear();
    if (riseCount < 2) return;

    // Oldest -> newest
    int start = (riseHead - riseCount);
    if (start < 0) start += RISE_RING;

    uint32_t prev = 0;
    for (int i = 0; i < (int)riseCount; i++) {
        int idx = (start + i) % RISE_RING;
        uint32_t t = riseUs[idx];
        if (prev != 0 && t > prev) outPeriods.push_back(t - prev);
        prev = t;
    }
}

void PinAnalyzer::collectHighPulses(std::vector<uint32_t>& outHighPulses) const {
    // We don’t store a ring of highs here to keep it light;
    // Instead, we infer from pulse distribution + duty + min/max.
    // If you want, we can store high pulse widths on each falling edge.
    outHighPulses.clear();
}

uint32_t PinAnalyzer::medianOf(std::vector<uint32_t>& v) {
    if (v.empty()) return 0;
    std::sort(v.begin(), v.end());
    size_t n = v.size();
    if (n & 1) return v[n/2];
    return (uint32_t)((v[n/2 - 1] + v[n/2]) / 2);
}

uint32_t PinAnalyzer::estimateBaseT(const std::vector<uint32_t>& pulses) {
    if (pulses.size() < 10) return 0;

    // Take short pulses as timing base candidate
    std::vector<uint32_t> v = pulses;
    std::sort(v.begin(), v.end());
    size_t m = std::max<size_t>(8, v.size()/4);
    v.resize(m);
    return medianOf(v);
}

float PinAnalyzer::jitterScorePct(const std::vector<uint32_t>& pulses, uint32_t ref) {
    if (pulses.size() < 10 || ref == 0) return 100.f;

    // MAD around ref
    std::vector<uint32_t> dev;
    dev.reserve(pulses.size());
    for (auto p : pulses) {
        uint32_t d = (p > ref) ? (p - ref) : (ref - p);
        dev.push_back(d);
    }
    uint32_t mad = medianOf(dev);
    float pct = (100.0f * (float)mad) / (float)ref;
    if (pct < 0.f) pct = 0.f;
    if (pct > 200.f) pct = 200.f;
    return pct;
}

int PinAnalyzer::clampInt(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

void PinAnalyzer::analyzeTimingBins(const std::vector<uint32_t>& pulses,
                                    uint32_t baseT,
                                    int& binsUsed,
                                    int& dominantPct,
                                    int& oneTPct) {
    binsUsed = 0;
    dominantPct = 0;
    oneTPct = 0;
    if (pulses.size() < 10 || baseT == 0) return;

    int bins[8] = {0,0,0,0,0,0,0,0};
    int total = 0;
    float tol = baseT * 0.25f;

    for (auto p : pulses) {
        if (p < baseT / 2) continue;

        int bestN = 0;
        float bestErr = 1e9f;
        for (int n = 1; n <= 8; n++) {
            float target = (float)baseT * n;
            float err = fabsf((float)p - target);
            if (err < bestErr) {
                bestErr = err;
                bestN = n;
            }
        }

        if (bestN > 0 && bestErr <= tol) {
            bins[bestN - 1]++;
            total++;
        }
    }

    if (total == 0) return;

    int dominant = 0;
    for (int i = 0; i < 8; i++) {
        if (bins[i] > 0) binsUsed++;
        if (bins[i] > dominant) dominant = bins[i];
    }

    dominantPct = (dominant * 100) / total;
    oneTPct = (bins[0] * 100) / total;
}

void PinAnalyzer::collectNormalPulses(const std::vector<uint32_t>& pulses,
                                      uint32_t baseT,
                                      std::vector<uint32_t>& normal) {
    normal.clear();
    if (pulses.empty()) return;

    uint32_t limit = baseT ? (baseT * 16) : 0;
    if (limit == 0) return;

    normal.reserve(pulses.size());
    for (auto p : pulses) {
        if (p >= baseT / 2 && p <= limit) {
            normal.push_back(p);
        }
    }
}

void PinAnalyzer::estimateTwoPulseClusters(const std::vector<uint32_t>& pulses,
                                           uint32_t baseT,
                                           uint32_t& clusterA,
                                           uint32_t& clusterB) {
    clusterA = 0;
    clusterB = 0;
    if (pulses.size() < 6 || baseT == 0) return;

    std::vector<uint32_t> v;
    v.reserve(pulses.size());

    uint32_t gapLimit = baseT * 64;
    for (auto p : pulses) {
        if (p >= baseT / 2 && p <= gapLimit) {
            v.push_back(p);
        }
    }

    if (v.size() < 6) return;

    std::sort(v.begin(), v.end());
    clusterA = v[v.size() / 4];
    clusterB = v[(v.size() * 3) / 4];

    if (clusterA > clusterB) {
        uint32_t t = clusterA;
        clusterA = clusterB;
        clusterB = t;
    }
}

const char* PinAnalyzer::kindToStr(SignalKind k) {
    switch (k) {
        case SignalKind::Idle: return "Idle";
        case SignalKind::NoiseOrFloating: return "Noise/Floating";
        case SignalKind::Clock: return "Clock";
        case SignalKind::PWM: return "PWM";
        case SignalKind::Servo: return "Servo";
        case SignalKind::DataLike: return "Data-like";
        case SignalKind::BurstData: return "Burst data";
        default: return "Unknown";
    }
}

PinAnalyzer::Guess PinAnalyzer::detectIdle(float approxHz, float dutyPct, uint32_t edges_, bool startLvl) const {
    Guess g;
    if (edges_ == 0 || approxHz < 0.5f) {
        g.kind = SignalKind::Idle;
        g.confidencePct = 85;
        g.note = std::string("No meaningful activity. Stayed mostly ") + (startLvl ? "HIGH" : "LOW") + ".";
        return g;
    }
    return g;
}

PinAnalyzer::Guess PinAnalyzer::detectNoiseOrFloating(const std::vector<uint32_t>& pulses, float jitterPct, uint32_t minP, uint32_t edges_) const {
    Guess g;
    if (edges_ < 5) return g;

    // Very fast tiny pulses + high jitter => noise
    if (minP <= 3 && jitterPct > 60.f) {
        g.kind = SignalKind::NoiseOrFloating;
        g.confidencePct = 65;
        g.note = "Very short pulses with high jitter.";
        return g;
    }

    // Very broad distribution (max >> median) and jitter high
    if (!pulses.empty()) {
        std::vector<uint32_t> tmp = pulses;
        uint32_t med = medianOf(tmp);
        uint32_t mx = 0;
        for (auto p : pulses) if (p > mx) mx = p;

        if (med > 0 && mx > (med * 20) && jitterPct > 80.f) {
            g.kind = SignalKind::NoiseOrFloating;
            g.confidencePct = 55;
            g.note = "Highly irregular timing.";
        }
    }
    return g;
}

PinAnalyzer::Guess PinAnalyzer::detectClockPwm(float approxHz,
                                               float dutyPct,
                                               uint32_t normalMinPulseUs,
                                               uint32_t normalMaxPulseUs,
                                               uint32_t pulseClusterAUs,
                                               uint32_t pulseClusterBUs,
                                               float normalJitterPct,
                                               uint32_t edges_) const {
    (void)approxHz;
    Guess g;
    if (edges_ < 40) return g;
    uint32_t a = pulseClusterAUs ? pulseClusterAUs : normalMinPulseUs;
    uint32_t b = pulseClusterBUs ? pulseClusterBUs : normalMaxPulseUs;

    if (a == 0 || a == 0xFFFFFFFF) return g;

    // Very short pulses are timing-rough in polling, but still classify
    // regular fast PWM/clock as signal activity instead of floating noise.
    if (a < 1) return g;

    if (a > b) { uint32_t t = a; a = b; b = t; }

    // Ratio check on clustered pulse widths, not on raw max gap.
    if (b > a * 64) return g;
    if (normalJitterPct > 35.f && b <= a * 2) return g;

    uint32_t periodUs = a + b;
    if (periodUs == 0) return g;

    const float hzFromPulses = 1000000.0f / (float)periodUs;

    bool midDuty = (dutyPct > 45.f && dutyPct < 55.f);
    bool anyDuty = (dutyPct > 5.f  && dutyPct < 95.f);

    int conf = 55;
    if (edges_ > 200)  conf += 10;
    if (edges_ > 2000) conf += 10;
    if (b <= a * 3)    conf += 10;
    if (b <= a * 2)    conf += 5;
    if (b > a * 16)    conf -= 18;
    if (a < 4)         conf -= 10;
    if (normalJitterPct < 8.f) conf += 8;
    else if (normalJitterPct > 20.f && b <= a * 2) conf -= 12;

    conf = clampInt(conf, 0, 90);

    if (hzFromPulses >= 20.f && anyDuty) {
        if (midDuty) {
            g.kind = SignalKind::Clock;
            g.confidencePct = clampInt(conf + 5, 0, 95);
            g.note = "Regular toggling, near 50% duty.";
        } else {
            g.kind = SignalKind::PWM;
            g.confidencePct = conf;
            g.note = "Regular toggling, duty not 50%.";
        }
        return g;
    }

    return Guess{};
}

PinAnalyzer::Guess PinAnalyzer::detectServo(const std::vector<uint32_t>& risePeriods, const std::vector<uint32_t>& /*highPulses*/) const {
    Guess g;
    if (risePeriods.size() < 6) return g;

    // Servo: period ~20ms (50Hz). We just detect a strong cluster near 20ms.
    int near20 = 0;
    for (auto p : risePeriods) {
        if (p > 15000 && p < 25000) near20++;
    }
    float ratio = (float)near20 / (float)risePeriods.size();
    if (ratio > 0.70f) {
        g.kind = SignalKind::Servo;
        g.confidencePct = 78;
        g.note = "Strong ~20 ms rhythm.";
        return g;
    }
    return g;
}

PinAnalyzer::Guess PinAnalyzer::detectDataLike(const std::vector<uint32_t>& pulses,
                                               uint32_t baseT,
                                               int binsUsed,
                                               int dominantPct,
                                               int oneTPct,
                                               bool hasLongGaps) const {
    Guess g;
    if (pulses.size() < 30 || baseT < 2 || baseT > 2000) return g;

    // Check how many pulses are close to N*T
    int ok = 0;
    int total = 0;
    float tol = baseT * 0.25f;

    for (auto p : pulses) {
        if (p < baseT / 2) continue;
        total++;

        int bestN = 0;
        float bestErr = 1e9f;
        for (int n = 1; n <= 8; n++) {
            float target = (float)baseT * n;
            float err = fabsf((float)p - target);
            if (err < bestErr) { bestErr = err; bestN = n; }
        }
        if (bestErr <= tol) ok++;
    }

    if (total < 20) return g;

    float score = (float)ok / (float)total;
    bool variedTiming = (binsUsed >= 2 && dominantPct < 88);
    bool stronglyVariedTiming = (binsUsed >= 3 && dominantPct < 80);
    bool pureOneT = (binsUsed <= 1 && oneTPct >= 85);

    // A perfect clock or 50% PWM also matches 1*T. That is not data by itself.
    if (pureOneT && !hasLongGaps) return g;
    if (!variedTiming && !hasLongGaps) return g;

    if (score > 0.60f) {
        int baud = (baseT > 0) ? (int)(1000000.0f / (float)baseT + 0.5f) : 0;

        // confidence shaped by score
        int conf = (int)(35 + score * 45);
        if (stronglyVariedTiming) conf += 12;
        else if (variedTiming) conf += 5;
        if (hasLongGaps) conf += 8;
        if (dominantPct > 85) conf -= 18;
        if (oneTPct > 80) conf -= 12;
        if (baud < 1200 || baud > 2000000) conf = (int)(conf * 0.7f);

        g.kind = SignalKind::DataLike;
        g.confidencePct = clampInt(conf, 0, 95);
        g.note = "";
        g.extra = "baud~" + std::to_string(baud);
        return g;
    }

    return g;
}

PinAnalyzer::Guess PinAnalyzer::detectBurstData(int bursts_, uint32_t edges_, float approxHz, float jitterPct) const {
    Guess g;
    if (edges_ < 10) return g;

    //many edges but segmented by gaps, and jitter not super low
    if (bursts_ >= 2 && approxHz > 20.f && jitterPct > 15.f) {
        g.kind = SignalKind::BurstData;
        int conf = 55 + (bursts_ > 5 ? 10 : 0);
        g.confidencePct = clampInt(conf, 0, 85);
        g.note = "Feels like data traffic.";
        g.extra = "bursts~" + std::to_string(bursts_);
        return g;
    }

    return g;
}

std::string PinAnalyzer::runPullTest() {
    // Very short tests: 40ms each, purely observational.
    auto measureStability = [&](int ms) -> uint32_t {
        uint32_t e = 0;
        bool last = pinService.read(pin);
        uint32_t t0 = micros();
        unsigned long t0ms = millis();
        while ((millis() - t0ms) < (unsigned long)ms) {
            bool v = pinService.read(pin);
            if (v != last) { e++; last = v; }
        }
        (void)t0;
        return e;
    };

    pinService.setInput(pin);
    uint32_t baseEdges = measureStability(40);
    if (baseEdges == 0xFFFFFFFF) return "";

    // Pull-up
    pinService.setInputPullup(pin);
    uint32_t upEdges = measureStability(40);
    if (upEdges == 0xFFFFFFFF) { pinService.setInput(pin); return ""; }

    // Pull-down
    pinService.setInputPullDown(pin);
    uint32_t dnEdges = measureStability(40);
    if (dnEdges == 0xFFFFFFFF) { pinService.setInput(pin); return ""; }

    // Restore
    pinService.setInput(pin);

    // Interpretation
    if (baseEdges > 20 && upEdges <= (baseEdges / 4)) {
        return "Pull-up stabilizes the line. Floating or open-drain is likely.";
    }
    if (baseEdges > 20 && dnEdges <= (baseEdges / 4)) {
        return "Pull-down  stabilizes the line. Floating input is likely.";
    }
    if (upEdges > baseEdges * 2 && dnEdges > baseEdges * 2) {
        return "Pull resistors make it worse. This line is probably driven.";
    }

    return "";
}

PinAnalyzer::Report PinAnalyzer::buildReport(bool doPullTest) {
    Report r;

    // Close tail to include the last stable segment time in high/low
    closeTail(micros());

    uint32_t elapsedMs = (uint32_t)(millis() - windowStartMs);
    if (elapsedMs == 0) elapsedMs = 1;
    r.edges = edges; // raw count
    r.edgesPerSec = (uint32_t)((uint64_t)edges * 1000ULL / elapsedMs);
    r.highUs = highUs;
    r.lowUs  = lowUs;
    r.minPulseUs = minPulseUs;
    r.maxPulseUs = maxPulseUs;
    r.bursts = bursts;
    r.burstEdges = burstEdges;
    r.maxGapUs = maxGapUs;
    r.maxGapWasHigh = maxGapWasHigh;

    uint32_t totalUs = highUs + lowUs;
    r.dutyPct = (totalUs ? (100.0f * (float)highUs) / (float)totalUs : 0.f);

    float seconds = elapsedMs / 1000.0f;
    r.approxHz = (seconds > 0.f) ? ((edges / 2.0f) / seconds) : 0.f;


    // Pulse features
    std::vector<uint32_t> pulses;
    collectPulses(pulses);

    if (!pulses.empty()) {
        std::vector<uint32_t> tmp = pulses;
        r.medianPulseUs = medianOf(tmp);
        r.basePulseUs = estimateBaseT(pulses);
        basePulseUs = r.basePulseUs;
        r.jitterPct = jitterScorePct(pulses, r.basePulseUs ? r.basePulseUs : r.medianPulseUs);

        analyzeTimingBins(pulses, r.basePulseUs, r.timingBinsUsed, r.dominantTimingBinPct, r.oneTBinPct);
        r.hasLongGaps = (r.basePulseUs > 0 && r.maxGapUs > (r.basePulseUs * 16));
        estimateTwoPulseClusters(pulses, r.basePulseUs, r.pulseClusterAUs, r.pulseClusterBUs);

        std::vector<uint32_t> normalPulses;
        collectNormalPulses(pulses, r.basePulseUs, normalPulses);
        if (!normalPulses.empty()) {
            r.normalMinPulseUs = normalPulses[0];
            r.normalMaxPulseUs = normalPulses[0];
            for (auto p : normalPulses) {
                if (p < r.normalMinPulseUs) r.normalMinPulseUs = p;
                if (p > r.normalMaxPulseUs) r.normalMaxPulseUs = p;
            }
            r.normalJitterPct = jitterScorePct(normalPulses, r.basePulseUs ? r.basePulseUs : r.medianPulseUs);
        }

        r.timingReliable = (r.minPulseUs == 0xFFFFFFFF || r.minPulseUs > 8);
    } else {
        r.medianPulseUs = 0;
        r.basePulseUs = 0;
        r.jitterPct = 100.f;
        r.normalJitterPct = 100.f;
        r.timingReliable = true;
    }

    // Rise periods (servo)
    std::vector<uint32_t> risePeriods;
    collectRisePeriods(risePeriods);

    // Guesses
    std::vector<Guess> guesses;
    guesses.reserve(8);

    auto gIdle  = detectIdle(r.approxHz, r.dutyPct, r.edges, startLevel);
    if (gIdle.confidencePct) guesses.push_back(gIdle);

    auto gNoise = detectNoiseOrFloating(pulses, r.jitterPct, r.minPulseUs, r.edges);
    if (gNoise.confidencePct) guesses.push_back(gNoise);

    auto gCP = detectClockPwm(r.approxHz,
                              r.dutyPct,
                              r.normalMinPulseUs,
                              r.normalMaxPulseUs,
                              r.pulseClusterAUs,
                              r.pulseClusterBUs,
                              r.normalJitterPct,
                              r.edges);
    if (gCP.confidencePct) guesses.push_back(gCP);

    auto gServo = detectServo(risePeriods, {});
    if (gServo.confidencePct) guesses.push_back(gServo);

    auto gData = detectDataLike(pulses,
                                r.basePulseUs,
                                r.timingBinsUsed,
                                r.dominantTimingBinPct,
                                r.oneTBinPct,
                                r.hasLongGaps);
    if (gData.confidencePct) guesses.push_back(gData);

    auto gBurst = detectBurstData(bursts, r.edges, r.approxHz, r.jitterPct);
    if (gBurst.confidencePct) guesses.push_back(gBurst);

    // If nothing matched and there is activity
    if (guesses.empty() && r.edges > 0) {
        Guess g;
        g.kind = SignalKind::Unknown;
        g.confidencePct = 30;
        g.note = "There is activity, but it doesn't match pattern.";
        guesses.push_back(g);
    }

    bool clockOrPwmStrong = (gCP.confidencePct >= 70);
    bool strongDataEvidence = (r.hasLongGaps || r.bursts >= 2 ||
                               (r.timingBinsUsed >= 3 && r.dominantTimingBinPct < 80));
    if (clockOrPwmStrong && !strongDataEvidence) {
        for (auto& g : guesses) {
            if (g.kind == SignalKind::DataLike) {
                g.confidencePct = clampInt(g.confidencePct - 35, 0, 95);
                g.note = "Weak data evidence; mostly regular timing.";
            } else if (g.kind == SignalKind::NoiseOrFloating) {
                g.confidencePct = clampInt(g.confidencePct - 30, 0, 95);
                g.note = "Short pulses, but regular signal detected.";
            }
        }
    }

    // Sort top guesses
    std::sort(guesses.begin(), guesses.end(), [](const Guess& a, const Guess& b){
        return a.confidencePct > b.confidencePct;
    });

    r.top1 = guesses.size() > 0 ? guesses[0] : Guess{};
    r.top2 = guesses.size() > 1 ? guesses[1] : Guess{};
    r.top3 = guesses.size() > 2 ? guesses[2] : Guess{};

    // Pull test
    if (doPullTest) {
        r.pullTestDone = true;
        r.pullHint = runPullTest();
    }

    return r;
}

std::string PinAnalyzer::formatWizardReport(uint8_t pin, const Report& r) const {
    std::ostringstream oss;

    oss << "[Wizard report on GPIO " << (int)pin << "]\r\n";

    auto line = [&](const std::string& s) {
        oss << "  " << s << "\r\n";
    };

    if (r.edges == 0) {
        line("I did not see any activity.");
        line(std::string("The line stayed mostly ") + (startLevel ? "HIGH." : "LOW."));
        line("This looks idle. If unexpected, check wiring.");
    } else {
        // Speed summary based on observed edges/sec
        if (r.edgesPerSec < 10)             line("Activity is very slow.");
        else if (r.edgesPerSec < 1000)      line("Activity is slow.");
        else if (r.edgesPerSec < 20000)     line("Activity is moderate.");
        else if (r.edgesPerSec < 200000)    line("Activity is fast.");
        else if (r.edgesPerSec < 450000)    line("Activity is very fast.");
        else                                line("Activity is at sampling limits.");

        // Top guesses
        {
            std::string g1 = std::string("Top guess: ") +
                kindToStr(r.top1.kind) + " (" + std::to_string(r.top1.confidencePct) + "%) — " + r.top1.note;
            if (!r.top1.extra.empty()) g1 += " [" + r.top1.extra + "]";
            line(g1);
        }

        if (r.top2.confidencePct > 0) {
            std::string g2 = std::string("Also possible: ") +
                kindToStr(r.top2.kind) + " (" + std::to_string(r.top2.confidencePct) + "%)";
            if (!r.top2.extra.empty()) g2 += " [" + r.top2.extra + "]";
            line(g2);
        }

        // What we see
        int hzInt = (int)(r.approxHz + 0.5f);
        int dutyInt = clampInt((int)(r.dutyPct + 0.5f), 0, 100);
        line("~" + std::to_string(hzInt) + " Hz, " + std::to_string(dutyInt) + "% HIGH.");

        auto isClockish = (r.top1.kind == SignalKind::PWM || r.top1.kind == SignalKind::Clock);

        if (isClockish) {
            // For PWM, jitterPct is often meaningless because the distribution is bimodal (high+low).
            if (r.pulseClusterAUs != 0 && r.pulseClusterBUs > 0) {
                uint32_t a = r.pulseClusterAUs;
                uint32_t b = r.pulseClusterBUs;
                if (a > b) { uint32_t t = a; a = b; b = t; }

                float ratio = (a > 0) ? ((float)b / (float)a) : 999.f;

                if (ratio <= 3.0f)      line("Clock-like timing is very consistent.");
                else if (ratio <= 12.0f) line("PWM-like timing (two pulse widths).");
                else                    line("PWM-like but very asymmetric duty.");
            } else {
                line("Clock/PWM-like activity.");
            }
        } else {
            if (r.jitterPct < 10.f)       line("Timing is very stable.");
            else if (r.jitterPct < 35.f)  line("Timing is somewhat stable.");
            else                          line("Timing is quite irregular.");
        }

        if (r.bursts >= 2) {
            line("It comes in bursts (" + std::to_string(r.bursts) +
                ", max gap ~" + std::to_string((int)((r.maxGapUs + 500) / 1000)) + " ms).");
        } else if (r.hasLongGaps) {
            line(std::string("Long idle gap observed, mostly ") + (r.maxGapWasHigh ? "HIGH." : "LOW."));
        }

        if (r.minPulseUs != 0xFFFFFFFF) {
            line("Min pulse ~" + std::to_string(r.minPulseUs) +
                 " us, max ~" + std::to_string(r.maxPulseUs) + " us.");
        }

        if (!r.timingReliable) {
            line("Timing limited: near polling limit.");
        }
    }

    if (r.pullTestDone && !r.pullHint.empty()) {
        line(r.pullHint);
    }

    // Technical line
    {
        std::string tech = "Report: edges/sec=" + std::to_string(r.edgesPerSec) +
                           " duty=" + std::to_string((int)(r.dutyPct + 0.5f)) + "%" +
                           " jitter~" + std::to_string((int)(r.jitterPct + 0.5f)) + "%";
        if (r.basePulseUs) tech += " baseT~" + std::to_string(r.basePulseUs) + "us";
        line(tech);
    }

    oss << "\r\n";
    return oss.str();
}
