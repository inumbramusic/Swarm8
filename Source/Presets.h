// Swarm8 — factory presets. Values are real (not normalised). Anything not listed = default.
#pragma once
#include "Params.h"
#include <vector>
#include <utility>

namespace swarm
{
struct Preset
{
    const char* name;
    std::vector<std::pair<const char*, float>> values;
};

inline const std::vector<Preset>& factoryPresets()
{
    static const std::vector<Preset> presets = {
        { "Classic Swarm", {   // the signature move: one note, mod wheel/ribbon fans it into a cluster
            { "glide", 0.15f }, { "swRange", 12.f }, { "swLag", 1.5f }, { "drift", 0.3f },
            { "cutA", 900.f }, { "resA", 0.35f }, { "envA", 0.25f },
            { "cutB", 1500.f }, { "resB", 0.25f }, { "envB", 0.15f },
            { "drive", 8.f }, { "atk", 0.25f }, { "rel", 2.5f },
            { "revMix", 0.3f }, { "revSize", 0.7f } } },

        { "Dorm Room Dread", { // low, dark, uneasy drone that breathes on its own
            { "swarm", 0.12f }, { "swRange", 5.f }, { "swLag", 3.f }, { "drift", 0.5f },
            { "cutA", 420.f }, { "resA", 0.55f }, { "envA", 0.1f },
            { "cutB", 650.f }, { "resB", 0.45f }, { "envB", 0.05f },
            { "lfoRate", 0.07f }, { "lfoShape", 4.f }, { "lfoSwarm", 0.18f }, { "lfoCut", 0.6f },
            { "drive", 10.f }, { "atk", 2.0f }, { "rel", 5.f },
            { "revMix", 0.45f }, { "revSize", 0.8f }, { "revDamp", 0.65f } } },

        { "Rising Swarm", {    // everything fans UPWARD from the root, slowly
            { "swMode", 1.f }, { "swRange", 24.f }, { "swLag", 4.f }, { "swCurve", 0.3f },
            { "cutA", 1600.f }, { "resA", 0.4f }, { "cutB", 2400.f }, { "resB", 0.3f },
            { "atk", 0.6f }, { "rel", 3.f }, { "revMix", 0.35f }, { "revSize", 0.75f } } },

        { "Collapse", {        // starts as a wide cloud, swarm falls down onto the root
            { "swMode", 2.f }, { "swRange", 19.f }, { "swLag", 5.f }, { "swarm", 1.f },
            { "cutA", 1100.f }, { "resA", 0.45f }, { "envA", 0.2f },
            { "cutB", 1700.f }, { "resB", 0.35f },
            { "atk", 0.8f }, { "rel", 4.f }, { "revMix", 0.4f }, { "revSize", 0.8f } } },

        { "Unison Wall", {     // tight detune — massive supersaw-style unison
            { "swarm", 0.35f }, { "swRange", 0.6f }, { "swCurve", -0.4f }, { "drift", 0.4f },
            { "cutA", 3200.f }, { "resA", 0.12f }, { "envA", 0.15f },
            { "cutB", 4200.f }, { "resB", 0.1f }, { "envB", 0.1f },
            { "drive", 4.f }, { "width", 1.f }, { "atk", 0.05f }, { "rel", 1.5f },
            { "revMix", 0.3f } } },

        { "Ladder Scream", {   // 8-pole series filter, high resonance, driven
            { "route", 2.f }, { "drive", 18.f }, { "swarm", 0.2f }, { "swRange", 7.f },
            { "cutA", 380.f }, { "resA", 0.88f }, { "envA", 0.55f }, { "atkA", 0.4f }, { "decA", 2.5f }, { "susA", 0.2f },
            { "cutB", 900.f }, { "resB", 0.6f }, { "envB", 0.3f },
            { "atk", 0.02f }, { "rel", 1.8f }, { "revMix", 0.25f } } },

        { "Fifths & Octaves", { // scatter mode, wide range — chordal clusters
            { "swMode", 3.f }, { "swRange", 24.f }, { "swLag", 2.f },
            { "cutA", 1800.f }, { "resA", 0.25f }, { "cutB", 2600.f }, { "resB", 0.2f },
            { "atk", 0.4f }, { "rel", 3.f }, { "revMix", 0.35f } } },

        { "Sub Rumble Bass", { // 4 oscillators, tight, punchy filter env
            { "oscs", 4.f }, { "swarm", 0.05f }, { "swRange", 1.f }, { "drift", 0.15f },
            { "glide", 0.04f }, { "route", 1.f },
            { "cutA", 160.f }, { "resA", 0.4f }, { "envA", 0.5f }, { "decA", 0.35f }, { "susA", 0.05f },
            { "cutB", 260.f }, { "resB", 0.3f }, { "envB", 0.35f }, { "decB", 0.5f }, { "susB", 0.1f },
            { "drive", 12.f }, { "width", 0.2f }, { "atk", 0.003f }, { "rel", 0.3f },
            { "revMix", 0.05f }, { "volume", -4.f } } },

        { "Cluster Bloom", {   // semitone cluster that blooms slowly into a huge space
            { "swMode", 3.f }, { "swRange", 3.f }, { "swLag", 2.5f }, { "swarm", 0.6f },
            { "cutA", 1300.f }, { "resA", 0.3f }, { "envA", 0.35f }, { "atkA", 2.5f }, { "decA", 4.f },
            { "cutB", 2000.f }, { "resB", 0.2f }, { "envB", 0.25f }, { "atkB", 3.5f },
            { "atk", 2.5f }, { "rel", 6.f }, { "revMix", 0.6f }, { "revSize", 0.92f } } },

        { "Siren Ribbon", {    // made for the pitch ribbon / iPad XY pad
            { "ribBase", 36.f }, { "ribRange", 4.f }, { "swarm", 0.08f }, { "swRange", 4.f },
            { "cutA", 1400.f }, { "resA", 0.6f }, { "cutB", 2200.f }, { "resB", 0.5f },
            { "lfoRate", 5.5f }, { "lfoPitch", 0.15f },
            { "atk", 0.1f }, { "rel", 2.f }, { "revMix", 0.4f } } },

        { "Glass Choir", {     // bright, parallel filters, chorusy swarm
            { "route", 1.f }, { "swarm", 0.5f }, { "swRange", 0.35f }, { "drift", 0.45f },
            { "cutA", 5500.f }, { "resA", 0.2f }, { "envA", -0.2f },
            { "cutB", 2500.f }, { "resB", 0.35f }, { "envB", 0.1f },
            { "atk", 1.5f }, { "rel", 4.f }, { "width", 0.9f },
            { "revMix", 0.55f }, { "revSize", 0.85f }, { "revDamp", 0.35f } } },

        { "Lost Signal", {     // random filter + swarm jumps
            { "swarm", 0.2f }, { "swRange", 9.f }, { "swLag", 0.15f },
            { "lfoShape", 3.f }, { "lfoRate", 6.f }, { "lfoCut", 1.6f }, { "lfoSwarm", 0.3f },
            { "cutA", 1000.f }, { "resA", 0.7f }, { "cutB", 1600.f }, { "resB", 0.6f },
            { "atk", 0.05f }, { "rel", 2.f }, { "revMix", 0.4f } } },

        { "Slow Unravel", {    // ultra-slow lag: notes come apart over many seconds
            { "swRange", 7.f }, { "swCurve", -0.5f }, { "swLag", 8.f }, { "swarm", 1.f },
            { "cutA", 800.f }, { "resA", 0.35f }, { "envA", 0.3f }, { "atkA", 4.f }, { "decA", 8.f }, { "susA", 0.5f },
            { "cutB", 1200.f }, { "resB", 0.3f },
            { "atk", 3.f }, { "rel", 7.f }, { "revMix", 0.55f }, { "revSize", 0.9f } } },

        { "Pulse Drift", {     // square-LFO filter gating against a drifting swarm
            { "lfoShape", 2.f }, { "lfoRate", 4.f }, { "lfoCut", 2.f },
            { "swarm", 0.25f }, { "swRange", 5.f }, { "drift", 0.5f },
            { "cutA", 500.f }, { "resA", 0.55f }, { "cutB", 900.f }, { "resB", 0.45f },
            { "atk", 0.2f }, { "rel", 2.f }, { "revMix", 0.35f } } },

        // ---- beyond the original: alternate waveforms -------------------------
        { "PWM Swarm", {       // pulse waves with LFO pulse-width modulation
            { "wave1", 1.f }, { "shape", 0.35f }, { "shapeLfo", 0.6f }, { "lfoRate", 0.4f },
            { "swRange", 12.f }, { "swLag", 1.5f },
            { "cutA", 1400.f }, { "resA", 0.3f }, { "envA", 0.2f }, { "cutB", 2000.f }, { "resB", 0.25f },
            { "atk", 0.3f }, { "rel", 2.5f }, { "revMix", 0.35f } } },

        { "Sync Hive", {       // hard-sync swarm; ride Shape (CC22) for the classic sync sweep
            { "wave1", 4.f }, { "shape", 0.3f }, { "shapeLfo", 0.35f }, { "lfoRate", 0.15f }, { "lfoShape", 1.f },
            { "swRange", 7.f }, { "swLag", 1.f }, { "drive", 10.f },
            { "cutA", 2500.f }, { "resA", 0.25f }, { "cutB", 3500.f }, { "resB", 0.2f },
            { "atk", 0.05f }, { "rel", 1.8f }, { "revMix", 0.3f } } },

        { "Folded Swarm", {    // wavefolded sines: soft to snarling with Shape
            { "wave1", 5.f }, { "shape", 0.45f }, { "shapeLfo", 0.25f }, { "lfoRate", 0.2f },
            { "swRange", 12.f }, { "swLag", 2.f }, { "route", 1.f },
            { "cutA", 3000.f }, { "resA", 0.2f }, { "cutB", 1500.f }, { "resB", 0.4f },
            { "atk", 0.4f }, { "rel", 3.f }, { "revMix", 0.4f } } },

        { "FM Glass Cloud", {  // FM tones swarmed into a shimmering cloud
            { "wave1", 6.f }, { "shape", 0.35f }, { "swarm", 0.4f }, { "swRange", 0.5f }, { "drift", 0.4f },
            { "route", 1.f }, { "cutA", 7000.f }, { "resA", 0.1f }, { "cutB", 4000.f }, { "resB", 0.2f },
            { "atk", 1.2f }, { "rel", 5.f }, { "width", 1.f },
            { "revMix", 0.55f }, { "revSize", 0.9f }, { "revDamp", 0.3f } } },

        { "Sine Beating", {    // pure sines: the swarm becomes slow beating and interference
            { "wave1", 3.f }, { "swRange", 0.4f }, { "swarm", 0.3f }, { "swLag", 3.f }, { "drift", 0.6f },
            { "route", 1.f }, { "cutA", 8000.f }, { "resA", 0.f }, { "cutB", 8000.f }, { "resB", 0.f },
            { "envA", 0.f }, { "envB", 0.f }, { "sub", 0.3f },
            { "atk", 1.5f }, { "rel", 4.f }, { "revMix", 0.45f } } },

        { "Hollow Triangles", { // triangles + sub: soft, organ-ish swarm pad
            { "wave1", 2.f }, { "wave2", 1.f }, { "waveMix", 0.25f }, { "shape", 0.2f }, { "sub", 0.45f },
            { "swRange", 5.f }, { "swLag", 1.5f },
            { "cutA", 2200.f }, { "resA", 0.2f }, { "cutB", 1600.f }, { "resB", 0.3f },
            { "atk", 0.6f }, { "rel", 3.f }, { "revMix", 0.4f } } },

        { "Saw | Pulse Split", { // saws to Filter A (left), pulses to Filter B (right)
            { "altWaves", 1.f }, { "wave1", 0.f }, { "wave2", 1.f }, { "shape", 0.25f }, { "shapeLfo", 0.3f },
            { "route", 0.f }, { "width", 1.f }, { "swRange", 12.f }, { "swLag", 1.5f },
            { "cutA", 900.f }, { "resA", 0.45f }, { "envA", 0.3f }, { "cutB", 2400.f }, { "resB", 0.3f },
            { "atk", 0.2f }, { "rel", 2.5f }, { "revMix", 0.35f } } },

        { "Storm Noise", {     // noise + swarm + resonant filters for risers and wind
            { "noise", 0.6f }, { "swarm", 0.5f }, { "swRange", 24.f }, { "swMode", 3.f }, { "swLag", 4.f },
            { "cutA", 600.f }, { "resA", 0.8f }, { "envA", 0.5f }, { "atkA", 4.f }, { "decA", 6.f },
            { "cutB", 1200.f }, { "resB", 0.75f }, { "envB", 0.4f }, { "atkB", 5.f },
            { "lfoShape", 4.f }, { "lfoRate", 0.3f }, { "lfoCut", 1.f },
            { "atk", 2.f }, { "rel", 5.f }, { "revMix", 0.5f } } },

        { "Init", {} },
    };
    return presets;
}

// Fills v[] with defaults and then the preset's overrides.
inline void presetValues (const Preset& pr, float* v)
{
    defaultValues (v);
    for (auto& kv : pr.values)
    {
        const int idx = findParam (kv.first);
        if (idx >= 0) v[idx] = kv.second;
    }
}
} // namespace swarm
