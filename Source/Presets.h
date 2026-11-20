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

        { "Poly Swarm Pad", {  // play chords; mod wheel swarms every note at once
            { "voiceMode", 1.f }, { "voices", 6.f }, { "oscs", 4.f },
            { "swarm", 0.15f }, { "swRange", 0.6f }, { "swLag", 1.5f }, { "drift", 0.4f },
            { "cutA", 1800.f }, { "resA", 0.2f }, { "envA", 0.2f }, { "atkA", 0.8f }, { "decA", 2.f },
            { "cutB", 2600.f }, { "resB", 0.15f }, { "envB", 0.1f },
            { "atk", 0.8f }, { "rel", 3.f }, { "width", 0.8f }, { "revMix", 0.45f }, { "revSize", 0.8f } } },

        { "Poly Swarm Keys", { // plucky poly with a little detune; works with sustain pedal
            { "voiceMode", 1.f }, { "voices", 8.f }, { "oscs", 3.f },
            { "swarm", 0.2f }, { "swRange", 0.3f }, { "drift", 0.3f },
            { "cutA", 900.f }, { "resA", 0.3f }, { "envA", 0.45f }, { "decA", 0.8f }, { "susA", 0.15f },
            { "cutB", 1400.f }, { "resB", 0.2f }, { "envB", 0.35f }, { "decB", 1.2f }, { "susB", 0.2f },
            { "atk", 0.004f }, { "dec", 1.5f }, { "sus", 0.35f }, { "rel", 0.9f }, { "velFilt", 0.5f }, { "volume", 0.f },
            { "revMix", 0.3f } } },

        { "Poly Cluster Strings", { // each note blooms into its own cluster
            { "voiceMode", 1.f }, { "voices", 8.f }, { "oscs", 4.f },
            { "swarm", 0.35f }, { "swRange", 3.f }, { "swMode", 3.f }, { "swLag", 2.f },
            { "cutA", 1300.f }, { "resA", 0.25f }, { "cutB", 2200.f }, { "resB", 0.2f },
            { "atk", 1.2f }, { "rel", 4.f }, { "revMix", 0.5f }, { "revSize", 0.85f } } },

        { "Chord Bend", {      // play a chord, then another: every oscillator glides to its new note
            { "voiceMode", 2.f }, { "morphTime", 2.f }, { "morphCurve", 1.f }, { "stagger", 0.35f },
            { "chordLatch", 1.f }, { "swarm", 0.06f }, { "swRange", 0.5f }, { "drift", 0.35f },
            { "cutA", 1600.f }, { "resA", 0.25f }, { "cutB", 2400.f }, { "resB", 0.2f },
            { "atk", 0.6f }, { "rel", 3.f }, { "revMix", 0.45f }, { "revSize", 0.8f } } },

        { "Chord Melt (1 Bar)", { // tempo-synced: each chord change takes one bar, staggered
            { "voiceMode", 2.f }, { "morphSync", 5.f }, { "morphCurve", 2.f }, { "stagger", 0.7f },
            { "chordSpread", 1.f }, { "chordLatch", 1.f }, { "swarm", 0.1f }, { "swRange", 1.f },
            { "cutA", 1100.f }, { "resA", 0.35f }, { "envA", 0.15f }, { "cutB", 1800.f }, { "resB", 0.3f },
            { "atk", 1.f }, { "rel", 4.f }, { "revMix", 0.5f }, { "revSize", 0.85f } } },

        { "Chord Snap Swarm", { // fast, snappy chord changes inside a wide swarm
            { "voiceMode", 2.f }, { "morphTime", 0.2f }, { "morphCurve", 3.f }, { "stagger", 0.5f },
            { "chordLatch", 1.f }, { "swarm", 0.3f }, { "swRange", 7.f }, { "swLag", 1.f },
            { "cutA", 1500.f }, { "resA", 0.4f }, { "envA", 0.2f }, { "cutB", 2200.f }, { "resB", 0.3f },
            { "atk", 0.1f }, { "rel", 2.f }, { "revMix", 0.4f } } },

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
