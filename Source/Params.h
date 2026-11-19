// Swarm8 — parameter table. Single source of truth for the engine, the plugin
// parameters (APVTS), the UI layout, presets, MIDI CC defaults and OSC addresses.
#pragma once

namespace swarm
{
enum class PType { Float, Bool, Choice, Int };

struct ParamDef
{
    const char* id;       // stable ID (used in presets, OSC: /swarm/<id>)
    const char* name;     // display name
    const char* group;    // UI section
    PType type;
    float min, max, def;
    float centre;         // skew centre for log-ish knobs (0 = linear)
    const char* choices;  // "A|B|C" for Choice
    const char* unit;
    int defaultCC;        // default MIDI CC (-1 = none)
};

enum P
{
    // PITCH
    pTune, pFine, pGlide, pBend, pRibBase, pRibRange, pLegato, pRibbon, pRibGate,
    // SWARM
    pSwarm, pSwRange, pSwMode, pSwCurve, pSwLag, pOscs, pDrift, pAtSwarm,
    // OSC (waveforms)
    pWave1, pWave2, pWaveMix, pAltWaves, pShape, pShapeLfo, pSub, pNoise,
    // FILTER (shared)
    pRoute, pDrive, pKeyTrack, pVelFilt,
    // FILTER A / B
    pCutA, pResA, pEnvA,
    pCutB, pResB, pEnvB,
    // ENV A / ENV B / AMP
    pAtkA, pDecA, pSusA, pRelA,
    pAtkB, pDecB, pSusB, pRelB,
    pAtk, pDec, pSus, pRel, pDrone, pVelAmp,
    // LFO
    pLfoRate, pLfoShape, pLfoPitch, pLfoSwarm, pLfoCut,
    // OUTPUT
    pWidth, pRevMix, pRevSize, pRevDamp, pVolume,
    NumParams
};

// Saw = classic Swarmatron. Shape knob = pulse width / sync ratio / fold depth / FM index.
inline constexpr const char* kWaves = "Saw|Pulse|Triangle|Sine|Sync|Fold|FM";

inline constexpr ParamDef kParams[NumParams] = {
    // id          name              group       type           min     max     def     centre choices                         unit   CC
    { "tune",     "Tune",           "PITCH",    PType::Float,  -24.f,  24.f,   0.f,    0.f,  nullptr,                        "st",   -1 },
    { "fine",     "Fine",           "PITCH",    PType::Float,  -100.f, 100.f,  0.f,    0.f,  nullptr,                        "ct",   -1 },
    { "glide",    "Glide",          "PITCH",    PType::Float,  0.f,    5.f,    0.08f,  0.4f, nullptr,                        "s",     5 },
    { "bend",     "Bend Range",     "PITCH",    PType::Float,  0.f,    24.f,   2.f,    0.f,  nullptr,                        "st",   -1 },
    { "ribBase",  "Ribbon Low",     "PITCH",    PType::Int,    12.f,   84.f,   36.f,   0.f,  nullptr,                        "note", -1 },
    { "ribRange", "Ribbon Octaves", "PITCH",    PType::Float,  0.5f,   6.f,    3.f,    0.f,  nullptr,                        "oct",  -1 },
    { "legato",   "Legato",         "PITCH",    PType::Bool,   0.f,    1.f,    1.f,    0.f,  nullptr,                        "",     -1 },
    { "ribbon",   "Pitch Ribbon",   "RIBBON",   PType::Float,  0.f,    1.f,    0.5f,   0.f,  nullptr,                        "",     16 },
    { "ribGate",  "Ribbon Touch",   "RIBBON",   PType::Bool,   0.f,    1.f,    0.f,    0.f,  nullptr,                        "",     17 },

    { "swarm",    "Swarm",          "SWARM",    PType::Float,  0.f,    1.f,    0.f,    0.f,  nullptr,                        "",      1 },
    { "swRange",  "Swarm Range",    "SWARM",    PType::Float,  0.1f,   48.f,   12.f,   7.f,  nullptr,                        "st",   20 },
    { "swMode",   "Swarm Mode",     "SWARM",    PType::Choice, 0.f,    3.f,    0.f,    0.f,  "Center|Up|Down|Scatter",       "",     -1 },
    { "swCurve",  "Spread Curve",   "SWARM",    PType::Float,  -1.f,   1.f,    0.f,    0.f,  nullptr,                        "",     -1 },
    { "swLag",    "Swarm Lag",      "SWARM",    PType::Float,  0.f,    8.f,    0.3f,   1.f,  nullptr,                        "s",    21 },
    { "oscs",     "Oscillators",    "SWARM",    PType::Int,    1.f,    8.f,    8.f,    0.f,  nullptr,                        "",     -1 },
    { "drift",    "Analog Drift",   "SWARM",    PType::Float,  0.f,    1.f,    0.25f,  0.f,  nullptr,                        "",     18 },
    { "atSwarm",  "Pressure>Swarm", "SWARM",    PType::Float,  0.f,    1.f,    0.5f,   0.f,  nullptr,                        "",     -1 },

    { "wave1",    "Wave 1",         "OSC",      PType::Choice, 0.f,    6.f,    0.f,    0.f,  kWaves,                         "",     -1 },
    { "wave2",    "Wave 2",         "OSC",      PType::Choice, 0.f,    6.f,    1.f,    0.f,  kWaves,                         "",     -1 },
    { "waveMix",  "Wave Mix",       "OSC",      PType::Float,  0.f,    1.f,    0.f,    0.f,  nullptr,                        "",     23 },
    { "altWaves", "Split Waves",    "OSC",      PType::Bool,   0.f,    1.f,    0.f,    0.f,  nullptr,                        "",     -1 },
    { "shape",    "Shape",          "OSC",      PType::Float,  0.f,    1.f,    0.5f,   0.f,  nullptr,                        "",     22 },
    { "shapeLfo", "LFO>Shape",      "OSC",      PType::Float,  0.f,    1.f,    0.f,    0.f,  nullptr,                        "",     -1 },
    { "sub",      "Sub Osc",        "OSC",      PType::Float,  0.f,    1.f,    0.f,    0.f,  nullptr,                        "",     -1 },
    { "noise",    "Noise",          "OSC",      PType::Float,  0.f,    1.f,    0.f,    0.f,  nullptr,                        "",     -1 },

    { "route",    "Routing",        "FILTER",   PType::Choice, 0.f,    2.f,    0.f,    0.f,  "Split|Parallel|Series",        "",     -1 },
    { "drive",    "Drive",          "FILTER",   PType::Float,  0.f,    24.f,   6.f,    0.f,  nullptr,                        "dB",   -1 },
    { "keyTrack", "Key Track",      "FILTER",   PType::Float,  0.f,    1.f,    0.5f,   0.f,  nullptr,                        "",     -1 },
    { "velFilt",  "Vel>Filter",     "FILTER",   PType::Float,  0.f,    1.f,    0.2f,   0.f,  nullptr,                        "",     -1 },

    { "cutA",     "Cutoff A",       "FILTER A", PType::Float,  20.f,   18000.f,1200.f, 900.f,nullptr,                        "Hz",   74 },
    { "resA",     "Resonance A",    "FILTER A", PType::Float,  0.f,    1.f,    0.3f,   0.f,  nullptr,                        "",     71 },
    { "envA",     "Env Amt A",      "FILTER A", PType::Float,  -1.f,   1.f,    0.3f,   0.f,  nullptr,                        "",     -1 },
    { "cutB",     "Cutoff B",       "FILTER B", PType::Float,  20.f,   18000.f,1800.f, 900.f,nullptr,                        "Hz",   75 },
    { "resB",     "Resonance B",    "FILTER B", PType::Float,  0.f,    1.f,    0.25f,  0.f,  nullptr,                        "",     76 },
    { "envB",     "Env Amt B",      "FILTER B", PType::Float,  -1.f,   1.f,    0.2f,   0.f,  nullptr,                        "",     -1 },

    { "atkA",     "Attack",         "ENV A",    PType::Float,  0.001f, 10.f,   0.01f,  0.5f, nullptr,                        "s",    -1 },
    { "decA",     "Decay",          "ENV A",    PType::Float,  0.005f, 10.f,   0.6f,   0.8f, nullptr,                        "s",    -1 },
    { "susA",     "Sustain",        "ENV A",    PType::Float,  0.f,    1.f,    0.3f,   0.f,  nullptr,                        "",     -1 },
    { "relA",     "Release",        "ENV A",    PType::Float,  0.005f, 15.f,   1.0f,   1.f,  nullptr,                        "s",    -1 },
    { "atkB",     "Attack",         "ENV B",    PType::Float,  0.001f, 10.f,   0.05f,  0.5f, nullptr,                        "s",    -1 },
    { "decB",     "Decay",          "ENV B",    PType::Float,  0.005f, 10.f,   1.0f,   0.8f, nullptr,                        "s",    -1 },
    { "susB",     "Sustain",        "ENV B",    PType::Float,  0.f,    1.f,    0.5f,   0.f,  nullptr,                        "",     -1 },
    { "relB",     "Release",        "ENV B",    PType::Float,  0.005f, 15.f,   1.5f,   1.f,  nullptr,                        "s",    -1 },

    { "atk",      "Attack",         "AMP",      PType::Float,  0.001f, 10.f,   0.02f,  0.5f, nullptr,                        "s",    73 },
    { "dec",      "Decay",          "AMP",      PType::Float,  0.005f, 10.f,   0.5f,   0.8f, nullptr,                        "s",    -1 },
    { "sus",      "Sustain",        "AMP",      PType::Float,  0.f,    1.f,    1.f,    0.f,  nullptr,                        "",     -1 },
    { "rel",      "Release",        "AMP",      PType::Float,  0.005f, 15.f,   1.2f,   1.f,  nullptr,                        "s",    72 },
    { "drone",    "Drone",          "AMP",      PType::Bool,   0.f,    1.f,    0.f,    0.f,  nullptr,                        "",     -1 },
    { "velAmp",   "Vel>Amp",        "AMP",      PType::Float,  0.f,    1.f,    0.3f,   0.f,  nullptr,                        "",     -1 },

    { "lfoRate",  "Rate",           "LFO",      PType::Float,  0.01f,  20.f,   0.3f,   2.f,  nullptr,                        "Hz",   -1 },
    { "lfoShape", "Shape",          "LFO",      PType::Choice, 0.f,    4.f,    0.f,    0.f,  "Sine|Triangle|Square|S&H|Drift","",     -1 },
    { "lfoPitch", "> Pitch",        "LFO",      PType::Float,  0.f,    12.f,   0.f,    1.f,  nullptr,                        "st",   -1 },
    { "lfoSwarm", "> Swarm",        "LFO",      PType::Float,  0.f,    1.f,    0.f,    0.f,  nullptr,                        "",     -1 },
    { "lfoCut",   "> Cutoff",       "LFO",      PType::Float,  0.f,    4.f,    0.f,    0.f,  nullptr,                        "oct",  -1 },

    { "width",    "Width",          "OUTPUT",   PType::Float,  0.f,    1.f,    0.6f,   0.f,  nullptr,                        "",     -1 },
    { "revMix",   "Reverb",         "OUTPUT",   PType::Float,  0.f,    1.f,    0.25f,  0.f,  nullptr,                        "",     19 },
    { "revSize",  "Size",           "OUTPUT",   PType::Float,  0.f,    1.f,    0.6f,   0.f,  nullptr,                        "",     -1 },
    { "revDamp",  "Damping",        "OUTPUT",   PType::Float,  0.f,    1.f,    0.5f,   0.f,  nullptr,                        "",     -1 },
    { "volume",   "Volume",         "OUTPUT",   PType::Float,  -48.f,  6.f,    -6.f,   0.f,  nullptr,                        "dB",    7 },
};

inline int findParam (const char* id)
{
    for (int i = 0; i < NumParams; ++i)
    {
        const char* a = kParams[i].id; const char* b = id;
        while (*a && *a == *b) { ++a; ++b; }
        if (*a == 0 && *b == 0) return i;
    }
    return -1;
}

inline void defaultValues (float* v)
{
    for (int i = 0; i < NumParams; ++i) v[i] = kParams[i].def;
}
} // namespace swarm
