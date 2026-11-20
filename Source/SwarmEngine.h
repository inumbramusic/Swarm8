// Swarm8 — DSP engine (pure C++, no JUCE dependency so it can be tested offline).
//
// Signal flow (modelled on the Dewtron Swarmatron concept):
//   8 band-limited sawtooth oscillators, all following ONE pitch (keys or ribbon),
//   fanned apart by the SWARM control -> two resonant 4-pole ladder filters
//   (each with its own envelope) -> VCA -> stereo -> FDN reverb.
#pragma once

#include "Params.h"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
#include <algorithm>

namespace swarm
{
static constexpr float kPi = 3.14159265358979f;

struct Rng
{
    uint32_t s = 0x9E3779B9u;
    float next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (s & 0xFFFFFF) / 8388608.0f - 1.0f; } // -1..1
};

inline float onePoleCoef (float seconds, float sr)
{
    if (seconds <= 0.0001f) return 1.0f;
    return 1.0f - std::exp (-1.0f / (seconds * sr));
}

//==============================================================================
struct Envelope
{
    enum Stage { Idle, Attack, Decay, Sustain, Release } stage = Idle;
    float v = 0, a = 0.01f, d = 0.5f, s = 1, r = 1, sr = 44100;

    void set (float A, float D, float S, float R) { a = A; d = D; s = S; r = R; }
    void gateOn()  { stage = Attack; }
    void gateOff() { if (stage != Idle) stage = Release; }
    void reset()   { stage = Idle; v = 0; }
    bool idle() const { return stage == Idle; }

    float tick()
    {
        switch (stage)
        {
            case Attack:
                // analog-style: aim past 1.0 and cut off when we get there
                v += (1.35f - v) * (1.0f - std::exp (-1.5f / (std::max (a, 0.0005f) * sr)));
                if (v >= 1.0f) { v = 1.0f; stage = Decay; }
                break;
            case Decay:
                v += (s - v) * (1.0f - std::exp (-4.6f / (std::max (d, 0.001f) * sr)));
                if (std::fabs (v - s) < 1.0e-4f) { v = s; stage = Sustain; }
                break;
            case Sustain: v += (s - v) * 0.001f; break; // follow sustain knob smoothly
            case Release:
                v += (0.0f - v) * (1.0f - std::exp (-4.6f / (std::max (r, 0.001f) * sr)));
                if (v < 1.0e-5f) { v = 0; stage = Idle; }
                break;
            case Idle: break;
        }
        return v;
    }
};

//==============================================================================
// Zero-delay-feedback 4-pole ladder with tanh saturation at the input/feedback junction.
struct Ladder
{
    float s[4] = { 0, 0, 0, 0 };
    float sr = 44100;

    void reset() { std::memset (s, 0, sizeof (s)); }

    float process (float x, float fc, float res, float drive)
    {
        fc = std::clamp (fc, 10.0f, sr * 0.42f);
        const float g  = std::tan (kPi * fc / sr);
        const float G  = g / (1.0f + g);
        const float G2 = G * G, G3 = G2 * G, G4 = G3 * G;
        const float k  = res * 4.15f;          // > 4 self-oscillates
        const float om = 1.0f - G;

        const float sigma = G3 * om * s[0] + G2 * om * s[1] + G * om * s[2] + om * s[3];
        const float xin   = x * drive;
        const float y4est = (G4 * xin + sigma) / (1.0f + k * G4);
        float in = std::tanh (xin - k * y4est);

        for (int i = 0; i < 4; ++i)
        {
            const float v = (in - s[i]) * G;
            const float y = v + s[i];
            s[i] = y + v;
            in = y;
        }
        // passband loss compensation for resonance, and keep level sane as drive rises
        return in * (1.0f + 0.6f * k) / std::sqrt (drive);
    }
};

//==============================================================================
struct Lfo
{
    float phase = 0, sr = 44100, sh = 0, shSmooth = 0, out = 0;
    Rng rng;

    float tick (float rate, int shape)
    {
        phase += rate / sr;
        bool wrapped = false;
        if (phase >= 1.0f) { phase -= 1.0f; wrapped = true; }
        if (wrapped) sh = rng.next();

        switch (shape)
        {
            case 0:  out = std::sin (2.0f * kPi * phase); break;
            case 1:  out = phase < 0.5f ? 4.0f * phase - 1.0f : 3.0f - 4.0f * phase; break;
            case 2:  out = phase < 0.5f ? 1.0f : -1.0f; break;
            case 3:  out = sh; break;
            default: shSmooth += (sh - shSmooth) * onePoleCoef (0.6f / std::max (rate, 0.01f), sr); out = shSmooth; break;
        }
        return out;
    }
};

//==============================================================================
// 8-line feedback delay network reverb with gentle modulation. Big, dark, smooth.
struct Reverb
{
    static constexpr int N = 8;
    std::vector<float> buf[N];
    int   w[N] {}, mask[N] {};
    float lp[N] {}, g[N] {}, len[N] {}, modPh[N] {}, modRate[N] {};
    float sr = 44100, damp = 0.5f, hpL = 0, hpR = 0;

    void prepare (float sampleRate)
    {
        sr = sampleRate;
        static const float base[N] = { 1153, 1327, 1559, 1801, 2027, 2311, 2593, 2879 };
        for (int i = 0; i < N; ++i)
        {
            int need = (int) (base[i] * 2.2f * sr / 44100.0f) + 64, sz = 1;
            while (sz < need) sz <<= 1;
            buf[i].assign ((size_t) sz, 0.0f);
            mask[i] = sz - 1; w[i] = 0; lp[i] = 0;
            modPh[i] = (float) i / N;
            modRate[i] = 0.11f + 0.07f * (float) i;
        }
        setParams (0.6f, 0.5f);
    }

    void setParams (float size, float dampAmt)
    {
        static const float base[N] = { 1153, 1327, 1559, 1801, 2027, 2311, 2593, 2879 };
        const float scale = (0.55f + 1.45f * size) * sr / 44100.0f;
        const float rt60  = 0.7f + size * size * 13.0f;
        for (int i = 0; i < N; ++i)
        {
            len[i] = base[i] * scale;
            g[i]   = std::pow (10.0f, -3.0f * len[i] / (sr * rt60));
        }
        damp = 0.08f + 0.85f * dampAmt;
    }

    void process (float inL, float inR, float& outL, float& outR)
    {
        // light high-pass on the send so the tail doesn't get muddy
        hpL += (inL - hpL) * 0.01f; hpR += (inR - hpR) * 0.01f;
        inL -= hpL; inR -= hpR;

        float o[N], sum = 0;
        for (int i = 0; i < N; ++i)
        {
            modPh[i] += modRate[i] / sr; if (modPh[i] >= 1.0f) modPh[i] -= 1.0f;
            const float d   = len[i] + 6.0f * std::sin (2.0f * kPi * modPh[i]);
            float rp        = (float) w[i] - d;
            while (rp < 0) rp += (float) (mask[i] + 1);
            const int   i0  = (int) rp;
            const float fr  = rp - (float) i0;
            const float a   = buf[i][(size_t) (i0 & mask[i])], b = buf[i][(size_t) ((i0 + 1) & mask[i])];
            float x = a + (b - a) * fr;
            lp[i] += (x - lp[i]) * (1.0f - damp);
            o[i] = lp[i] * g[i];
            sum += o[i];
        }
        const float h = sum * (2.0f / N);
        outL = outR = 0;
        for (int i = 0; i < N; ++i)
        {
            const float in = (i & 1) ? inR : inL;
            buf[i][(size_t) w[i]] = (o[i] - h) + in * 0.35f;
            w[i] = (w[i] + 1) & mask[i];
            if (i & 1) outR += o[i]; else outL += o[i];
        }
        outL *= 0.55f; outR *= 0.55f;
    }
};

//==============================================================================
class SwarmEngine
{
public:
    static constexpr int kOscs = 8;
    static constexpr int kVoices = 8;
    enum Mode { Mono = 0, Poly = 1, Chord = 2 };

    //==========================================================================
    struct Voice
    {
        float phase[kOscs] {}, syncPhase[kOscs] {}, oscGain[kOscs] {};
        float driftCur[kOscs] {}, driftTgt[kOscs] {}, driftStatic[kOscs] {};
        int   driftCount[kOscs] {};
        // chord morph: per-oscillator pitch path
        float oscPitch[kOscs] {}, mStart[kOscs] {}, mTarget[kOscs] {}, mDur[kOscs] {};
        float mElapsed = 0; bool morphing = false;

        float subPhase = 0;
        Ladder fA, fB;
        Envelope envA, envB, amp;
        float pitchSm = 48, noteBase = 48, target = 48, velocity = 1;
        int   note = -1;                 // MIDI note, -2 = ribbon voice
        bool  gate = false, sustained = false, isRibbon = false;
        uint32_t stamp = 0;
        Rng   rng;

        void prepare (float sr, uint32_t seed)
        {
            fA.sr = fB.sr = sr; fA.reset(); fB.reset();
            envA.sr = envB.sr = amp.sr = sr;
            envA.reset(); envB.reset(); amp.reset();
            rng.s = seed;
            for (int i = 0; i < kOscs; ++i)
            {
                phase[i] = 0.5f + 0.5f * rng.next();
                driftStatic[i] = rng.next();
                driftCur[i] = driftTgt[i] = 0; driftCount[i] = 0;
                oscGain[i] = 0; syncPhase[i] = 0; oscPitch[i] = 48;
            }
            subPhase = 0; gate = false; sustained = false; isRibbon = false; note = -1; morphing = false;
        }
        bool idle() const { return amp.idle(); }
        void gateOnAll()  { envA.gateOn(); envB.gateOn(); amp.gateOn(); }
        void gateOffAll() { envA.gateOff(); envB.gateOff(); amp.gateOff(); }
    };

    // values shared by every voice for one sample
    struct Shared
    {
        int nOsc, route, wave1, wave2;
        bool altWaves, chordMode;
        float pos[kOscs];
        float spread, lfoPitch, driftAmt, driftC, smC, glideC, ribbonGlideC;
        float waveMix, shapeNow, subLvl, noiseLvl, norm;
        float cutALog, cutBLog, envAAmt, envBAmt, velFiltAmt, keyTrack, lfoCut;
        float resA, resB, drive, width, velAmpAmt, chordOffset;
        int   morphCurve;
    };

    //==========================================================================
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        for (int v = 0; v < kVoices; ++v) voices[v].prepare (sr, 0x1234567u + 7919u * (uint32_t) v);
        lfo.sr = sr;
        rev.prepare (sr);
        static const float sc[kOscs] = { 0.0f, 0.62f, -0.41f, 0.17f, -0.88f, 0.93f, -0.23f, 0.45f };
        std::copy (sc, sc + kOscs, scatter);
        static const float st[kOscs] = { 0.0f, 0.71f, 0.33f, 0.92f, 0.15f, 0.58f, 0.84f, 0.42f };
        std::copy (st, st + kOscs, staggerU);
        defaultValues (p);
        cutASm = std::log2 (p[pCutA]); cutBSm = std::log2 (p[pCutB]);
        volSm = 0; swarmSm = 0; chordOffsetSm = 0;
        numNotes = 0; prevGate = false; prevAmpGate = false; retrigger = false; pitchInit = false;
        chordSize = 0; chordActive = false; chordChanged = false;
        ribbonVoice = -1; prevRibGate = false; sustainDown = false;
        lastMode = -1;
    }

    void setParams (const float* v) { std::memcpy (p, v, sizeof (p)); }
    const float* params() const { return p; }
    void setTempo (double bpm) { if (bpm > 1.0) tempo = (float) bpm; }

    int mode() const { return std::clamp ((int) std::lround (p[pVoiceMode]), 0, 2); }

    // --- MIDI ----------------------------------------------------------------
    void noteOn (int note, float vel)
    {
        checkModeChange();
        const int m = mode();
        if (m == Poly) { polyNoteOn (note, vel); return; }

        const int heldBefore = numNotes;
        for (int i = 0; i < numNotes; ++i) if (notes[i] == note) { removeNote (i); break; }
        if (numNotes < 32) notes[numNotes++] = note;
        velocity = vel;

        if (m == Mono)
        {
            if (p[pLegato] < 0.5f || numNotes == 1) retrigger = true;
        }
        else // Chord: notes pressed together form the chord
        {
            if (heldBefore == 0) { chordSize = 0; if (p[pLegato] < 0.5f) retrigger = true; }
            setChordFromHeld();
        }
    }

    void noteOff (int note)
    {
        const int m = mode();
        if (m == Poly)
        {
            for (auto& v : voices)
                if (v.gate && v.note == note && ! v.isRibbon)
                {
                    if (sustainDown) v.sustained = true;
                    else { v.gate = false; v.gateOffAll(); }
                }
            return;
        }
        for (int i = 0; i < numNotes; ++i) if (notes[i] == note) { removeNote (i); break; }
        if (m == Chord && p[pChordLatch] < 0.5f && numNotes > 0) setChordFromHeld();
    }

    void allNotesOff()
    {
        numNotes = 0; chordActive = false;
        for (auto& v : voices) if (! v.isRibbon && (v.gate || v.sustained)) { v.gate = false; v.sustained = false; v.gateOffAll(); }
    }

    // stop everything that could be held: keys, pedal, chord latch, ribbon voice
    void panic()
    {
        numNotes = 0; chordActive = false; chordChanged = false; sustainDown = false; retrigger = false;
        for (auto& v : voices) { v.gate = false; v.sustained = false; v.isRibbon = false; v.gateOffAll(); }
        ribbonVoice = -1; prevRibGate = false;
        prevGate = false; prevAmpGate = false;
    }

    void sustain (bool down)
    {
        sustainDown = down;
        if (! down)
            for (auto& v : voices)
                if (v.sustained) { v.sustained = false; v.gate = false; v.gateOffAll(); }
    }

    void pitchBend (float b) { bend = b; }   // -1..1
    void pressure (float pr) { press = pr; } // 0..1

    bool  isSilent() const
    {
        for (auto& v : voices) if (! v.idle()) return false;
        return true;
    }
    float currentPitch() const { return voices[0].pitchSm; }
    float currentSwarm() const { return swarmOut; }
    int   activeVoices() const { int n = 0; for (auto& v : voices) if (! v.idle()) ++n; return n; }

    //==========================================================================
    void process (float* L, float* R, int n)
    {
        checkModeChange();
        const int m = mode();

        for (auto& v : voices)
        {
            v.envA.set (p[pAtkA], p[pDecA], p[pSusA], p[pRelA]);
            v.envB.set (p[pAtkB], p[pDecB], p[pSusB], p[pRelB]);
            v.amp .set (p[pAtk],  p[pDec],  p[pSus],  p[pRel]);
        }
        rev.setParams (p[pRevSize], p[pRevDamp]);

        const float offset = p[pTune] + p[pFine] * 0.01f + bend * p[pBend];
        const bool  ribGate = p[pRibGate] > 0.5f;
        const float ribPitch = p[pRibBase] + p[pRibbon] * p[pRibRange] * 12.0f;
        bool fromRibbon = false;

        if (m == Mono)        blockMono (offset, ribGate, ribPitch, fromRibbon);
        else if (m == Chord)  blockChord (offset, ribGate);
        else                  blockPoly (offset, ribGate, ribPitch);

        // ---- shared per-block values --------------------------------------
        Shared s {};
        s.nOsc   = std::clamp ((int) std::lround (p[pOscs]), 1, kOscs);
        s.route  = (int) std::lround (p[pRoute]);
        s.wave1  = (int) std::lround (p[pWave1]);
        s.wave2  = (int) std::lround (p[pWave2]);
        s.altWaves = p[pAltWaves] > 0.5f;
        s.chordMode = m == Chord;
        s.morphCurve = (int) std::lround (p[pMorphCurve]);
        s.waveMix = p[pWaveMix];
        s.subLvl = p[pSub]; s.noiseLvl = p[pNoise];
        s.driftAmt = p[pDrift];
        s.glideC = onePoleCoef (fromRibbon ? 0.006f : p[pGlide], sr);
        s.ribbonGlideC = onePoleCoef (0.006f, sr);
        s.smC    = onePoleCoef (0.01f, sr);
        s.driftC = onePoleCoef (1.2f, sr);
        s.envAAmt = p[pEnvA]; s.envBAmt = p[pEnvB];
        s.velFiltAmt = p[pVelFilt]; s.keyTrack = p[pKeyTrack];
        s.resA = p[pResA]; s.resB = p[pResB];
        s.drive = std::pow (10.0f, p[pDrive] / 20.0f);
        s.width = p[pWidth]; s.velAmpAmt = p[pVelAmp];

        const int   swMode = (int) std::lround (p[pSwMode]);
        const int   lshape = (int) std::lround (p[pLfoShape]);
        const float curveE = std::exp2 (p[pSwCurve] * 2.0f);
        for (int i = 0; i < kOscs; ++i)
        {
            const float t = s.nOsc > 1 ? (float) i / (float) (s.nOsc - 1) : 0.0f;
            float ps;
            switch (swMode)
            {
                case 0: {
                    const int half = s.nOsc / 2, k = (i + 1) / 2;
                    const float mm = half > 0 ? (float) k / (float) half : 0.0f;
                    ps = (i == 0) ? 0.0f : ((i & 1) ? mm : -mm);
                    break;
                }
                case 1:  ps = t;  break;
                case 2:  ps = -t; break;
                default: ps = scatter[i]; break;
            }
            s.pos[i] = (ps < 0 ? -1.0f : 1.0f) * std::pow (std::fabs (ps), curveE);
        }
        s.norm = s.route == 0 ? 0.75f / std::sqrt (std::max (1.0f, s.nOsc * 0.5f))
                              : 0.55f / std::sqrt ((float) s.nOsc);

        const float volT  = std::pow (10.0f, p[pVolume] / 20.0f) * (m == Poly ? 0.5f : 1.0f);
        const float mix   = p[pRevMix];
        const float dryG  = std::cos (mix * kPi * 0.5f), wetG = std::sin (mix * kPi * 0.5f);
        const float swTgt = std::clamp (p[pSwarm] + press * p[pAtSwarm], 0.0f, 1.0f);
        const float lagC  = onePoleCoef (std::max (p[pSwLag], 0.003f), sr);
        const float chordOffC = onePoleCoef (ribGate ? 0.006f : std::max (p[pGlide], 0.01f), sr);

        // ---- sample loop ----------------------------------------------------
        for (int i = 0; i < n; ++i)
        {
            const float l = lfo.tick (p[pLfoRate], lshape);
            s.shapeNow = std::clamp (p[pShape] + l * p[pShapeLfo] * 0.5f, 0.0f, 1.0f);
            swarmSm += (swTgt - swarmSm) * lagC;
            const float sw = std::clamp (swarmSm + l * p[pLfoSwarm], 0.0f, 1.0f);
            swarmOut = sw;
            s.spread   = sw * p[pSwRange];
            s.lfoPitch = l * p[pLfoPitch];
            s.lfoCut   = l * p[pLfoCut];
            cutASm += (std::log2 (p[pCutA]) - cutASm) * s.smC;
            cutBSm += (std::log2 (p[pCutB]) - cutBSm) * s.smC;
            s.cutALog = cutASm; s.cutBLog = cutBSm;
            chordOffsetSm += (chordOffsetTarget - chordOffsetSm) * chordOffC;
            s.chordOffset = chordOffsetSm;
            volSm += (volT - volSm) * s.smC;

            float yl = 0, yr = 0;
            for (auto& v : voices)
                if (! v.idle()) renderVoice (v, s, yl, yr);

            yl *= volSm; yr *= volSm;
            float rl, rr;
            rev.process (yl, yr, rl, rr);
            L[i] = softClip (yl * dryG + rl * wetG);
            R[i] = softClip (yr * dryG + rr * wetG);
        }
    }

private:
    //==========================================================================
    void checkModeChange()
    {
        const int m = mode();
        if (m == lastMode) return;
        if (lastMode == -1) { lastMode = m; return; }
        for (auto& v : voices) { v.gate = false; v.sustained = false; v.isRibbon = false; v.gateOffAll(); }
        numNotes = 0; prevGate = false; prevAmpGate = false; chordActive = false; chordSize = 0;
        ribbonVoice = -1; prevRibGate = false;
        lastMode = m;
    }

    // ---------------- MONO (classic) ----------------
    void blockMono (float offset, bool ribGate, float ribPitch, bool& fromRibbon)
    {
        Voice& v = voices[0];
        const bool gate    = numNotes > 0 || ribGate;
        const bool ampGate = gate || p[pDrone] > 0.5f;

        float target;
        if (ribGate)            { target = ribPitch; fromRibbon = true; }
        else if (numNotes > 0)  target = (float) notes[numNotes - 1];
        else                    target = lastTarget;
        lastTarget = target;
        v.target = target + offset;
        v.velocity = velocity;

        if ((gate && ! prevGate) || (gate && retrigger))
        {
            if (v.amp.idle() || ! pitchInit) { v.pitchSm = v.target; pitchInit = true; }
            v.gateOnAll();
        }
        retrigger = false;
        if (! gate && prevGate) { v.envA.gateOff(); v.envB.gateOff(); }
        if (ampGate && v.amp.idle()) v.amp.gateOn();
        if (ampGate && ! prevAmpGate && v.amp.stage == Envelope::Release) v.amp.gateOn();
        if (! ampGate && prevAmpGate) v.amp.gateOff();
        prevGate = gate; prevAmpGate = ampGate;
    }

    // ---------------- CHORD MORPH ----------------
    void setChordFromHeld()
    {
        if (numNotes == 0) return;
        chordSize = std::min (numNotes, 16);
        for (int i = 0; i < chordSize; ++i) chord[i] = notes[i];
        std::sort (chord, chord + chordSize);
        chordChanged = true;
        chordActive = true;
    }

    float morphSeconds() const
    {
        static const float beats[8] = { 0.0f, 0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f, 16.0f };
        const int sync = std::clamp ((int) std::lround (p[pMorphSync]), 0, 7);
        if (sync == 0) return p[pMorphTime];
        return beats[sync] * 60.0f / std::max (tempo, 20.0f);
    }

    void blockChord (float offset, bool ribGate)
    {
        Voice& v = voices[0];
        const bool latch   = p[pChordLatch] > 0.5f;
        if (! latch && numNotes == 0) chordActive = false;
        const bool gate    = numNotes > 0 || ribGate || (latch && chordActive);
        const bool ampGate = gate || p[pDrone] > 0.5f;

        chordOffsetTarget = offset + (ribGate ? (p[pRibbon] - 0.5f) * p[pRibRange] * 12.0f : 0.0f);
        v.velocity = velocity;

        const bool fromSilence = v.amp.idle();
        if (chordChanged && chordSize > 0)
        {
            const int spreadOct = std::clamp ((int) std::lround (p[pChordSpread]), 0, 2);
            const float T = morphSeconds();
            for (int i = 0; i < kOscs; ++i)
            {
                const int k = i % chordSize;
                const int oct = spreadOct > 0 ? ((i / chordSize) % (spreadOct + 1)) : 0;
                const float tgt = (float) chord[k] + 12.0f * (float) oct;
                if (fromSilence || ! pitchInit || T < 0.002f)
                {
                    v.oscPitch[i] = tgt; v.mStart[i] = v.mTarget[i] = tgt; v.mDur[i] = 0.001f;
                }
                else
                {
                    v.mStart[i]  = v.oscPitch[i];
                    v.mTarget[i] = tgt;
                    v.mDur[i]    = std::max (0.002f, T * (1.0f - p[pStagger] * 0.85f * staggerU[i]));
                }
            }
            v.mElapsed = 0;
            v.morphing = ! (fromSilence || ! pitchInit || T < 0.002f);
            pitchInit = true;
            chordChanged = false;
            if (fromSilence) chordOffsetSm = chordOffsetTarget;
        }

        if ((gate && ! prevGate) || (gate && retrigger)) v.gateOnAll();
        retrigger = false;
        if (! gate && prevGate) { v.envA.gateOff(); v.envB.gateOff(); }
        if (ampGate && v.amp.idle()) v.amp.gateOn();
        if (ampGate && ! prevAmpGate && v.amp.stage == Envelope::Release) v.amp.gateOn();
        if (! ampGate && prevAmpGate) v.amp.gateOff();
        prevGate = gate; prevAmpGate = ampGate;
    }

    // ---------------- POLY ----------------
    int maxVoices() const { return std::clamp ((int) std::lround (p[pVoices]), 1, kVoices); }

    int allocateVoice (int note)
    {
        const int maxV = maxVoices();
        for (int i = 0; i < maxV; ++i)                       // same note again
            if ((voices[i].gate || voices[i].sustained) && voices[i].note == note && ! voices[i].isRibbon) return i;
        for (int i = 0; i < maxV; ++i)                       // free voice
            if (voices[i].idle()) return i;
        int best = -1; uint32_t oldest = 0xFFFFFFFFu;
        for (int i = 0; i < maxV; ++i)                       // oldest released
            if (! voices[i].gate && ! voices[i].isRibbon && voices[i].stamp < oldest) { oldest = voices[i].stamp; best = i; }
        if (best >= 0) return best;
        for (int i = 0; i < maxV; ++i)                       // oldest held
            if (voices[i].stamp < oldest) { oldest = voices[i].stamp; best = i; }
        return best;
    }

    void startVoice (int idx, float base, float vel, int note, bool ribbon)
    {
        Voice& v = voices[idx];
        const bool wasIdle = v.idle();
        v.note = note; v.noteBase = base; v.velocity = vel;
        v.gate = true; v.sustained = false; v.isRibbon = ribbon;
        v.stamp = ++stampCounter;
        v.target = base + polyOffset;
        if (wasIdle) v.pitchSm = v.target;
        v.gateOnAll();
    }

    void polyNoteOn (int note, float vel)
    {
        const int idx = allocateVoice (note);
        if (idx >= 0)
        {
            if (idx == ribbonVoice) ribbonVoice = -1;
            startVoice (idx, (float) note, vel, note, false);
        }
    }

    void blockPoly (float offset, bool ribGate, float ribPitch)
    {
        polyOffset = offset;
        // ribbon plays its own voice
        if (ribGate && ! prevRibGate)
        {
            const int idx = allocateVoice (-2);
            if (idx >= 0) { startVoice (idx, ribPitch, 0.8f, -2, true); ribbonVoice = idx; }
        }
        if (ribbonVoice >= 0)
        {
            Voice& rv = voices[ribbonVoice];
            if (ribGate) rv.noteBase = ribPitch;
            if (! ribGate && prevRibGate) { rv.gate = false; rv.gateOffAll(); rv.isRibbon = false; ribbonVoice = -1; }
        }
        prevRibGate = ribGate;

        const int maxV = maxVoices();
        for (int i = 0; i < kVoices; ++i)
        {
            Voice& v = voices[i];
            v.target = v.noteBase + offset;
            if (i >= maxV && v.gate) { v.gate = false; v.sustained = false; v.gateOffAll(); }
        }
    }

    //==========================================================================
    static float curveFn (int c, float x)
    {
        switch (c)
        {
            case 0:  return x;
            case 1:  return x * x * (3.0f - 2.0f * x);
            case 2:  return x * x * x;
            default: { const float y = 1.0f - x; return 1.0f - y * y * y; }
        }
    }

    void renderVoice (Voice& v, const Shared& s, float& outL, float& outR)
    {
        float base;
        if (s.chordMode)
        {
            if (v.morphing)
            {
                v.mElapsed += 1.0f / sr;
                bool done = true;
                for (int i = 0; i < kOscs; ++i)
                {
                    const float x = std::min (1.0f, v.mElapsed / v.mDur[i]);
                    if (x < 1.0f) done = false;
                    v.oscPitch[i] = v.mStart[i] + (v.mTarget[i] - v.mStart[i]) * curveFn (s.morphCurve, x);
                }
                if (done) v.morphing = false;
            }
            float sum = 0; for (int i = 0; i < s.nOsc; ++i) sum += v.oscPitch[i];
            v.pitchSm = sum / (float) s.nOsc + s.chordOffset;
            base = s.chordOffset + s.lfoPitch;
        }
        else
        {
            v.pitchSm += (v.target - v.pitchSm) * (v.isRibbon ? s.ribbonGlideC : s.glideC);
            base = v.pitchSm + s.lfoPitch;
        }

        float a = 0, b = 0;
        for (int i = 0; i < kOscs; ++i)
        {
            v.oscGain[i] += ((i < s.nOsc ? 1.0f : 0.0f) - v.oscGain[i]) * s.smC;
            if (i >= s.nOsc && v.oscGain[i] < 1.0e-4f) continue;

            if (--v.driftCount[i] <= 0)
            {
                v.driftTgt[i]   = v.rng.next();
                v.driftCount[i] = (int) (sr * (0.4f + 0.8f * (v.rng.next() * 0.5f + 0.5f)));
            }
            v.driftCur[i] += (v.driftTgt[i] - v.driftCur[i]) * s.driftC;
            const float driftSt = s.driftAmt * (0.18f * v.driftCur[i] + 0.05f * v.driftStatic[i]);

            const float semis = (s.chordMode ? v.oscPitch[i] : 0.0f) + base + s.pos[i] * s.spread + driftSt;
            float f = 440.0f * std::exp2 ((semis - 69.0f) / 12.0f);
            f = std::min (f, sr * 0.45f);
            const float dt = f / sr;

            float ph = v.phase[i] + dt;
            const bool wrapped = ph >= 1.0f;
            if (wrapped) ph -= 1.0f;
            v.phase[i] = ph;

            float o;
            float& sp = v.syncPhase[i];
            if (s.altWaves)       o = renderWave ((i & 1) ? s.wave2 : s.wave1, sp, ph, dt, wrapped, s.shapeNow);
            else if (s.waveMix <= 0.0f || s.wave1 == s.wave2)
                                  o = renderWave (s.wave1, sp, ph, dt, wrapped, s.shapeNow);
            else if (s.waveMix >= 1.0f)
                                  o = renderWave (s.wave2, sp, ph, dt, wrapped, s.shapeNow);
            else                  o = renderWave (s.wave1, sp, ph, dt, wrapped, s.shapeNow) * (1.0f - s.waveMix)
                                    + renderWave (s.wave2, sp, ph, dt, wrapped, s.shapeNow) * s.waveMix;

            o *= v.oscGain[i];
            if (s.route == 0) { if (i & 1) b += o; else a += o; }
            else a += o;
        }
        a *= s.norm; b *= s.norm;

        if (s.subLvl > 0.0f)
        {
            const float rootPitch = s.chordMode ? v.oscPitch[0] + s.chordOffset : v.pitchSm;
            const float sdt = std::min (440.0f * std::exp2 ((rootPitch + s.lfoPitch - 12.0f - 69.0f) / 12.0f), sr * 0.45f) / sr;
            v.subPhase += sdt; if (v.subPhase >= 1.0f) v.subPhase -= 1.0f;
            float sq = v.subPhase < 0.5f ? 1.0f : -1.0f;
            sq += polyBlep (v.subPhase, sdt);
            float t2 = v.subPhase + 0.5f; if (t2 >= 1.0f) t2 -= 1.0f;
            sq -= polyBlep (t2, sdt);
            const float sv = sq * s.subLvl * 0.35f;
            a += sv; b += sv;
        }
        if (s.noiseLvl > 0.0f) { a += v.rng.next() * s.noiseLvl * 0.25f; b += v.rng.next() * s.noiseLvl * 0.25f; }
        if (s.route != 0) b = a;

        const float ea = v.envA.tick(), eb = v.envB.tick(), am = v.amp.tick();
        const float velF = 1.0f - s.velFiltAmt + s.velFiltAmt * v.velocity;
        const float kt   = s.keyTrack * (v.pitchSm - 60.0f) / 12.0f;
        const float fcA  = std::exp2 (s.cutALog + s.envAAmt * 6.0f * ea * velF + kt + s.lfoCut);
        const float fcB  = std::exp2 (s.cutBLog + s.envBAmt * 6.0f * eb * velF + kt + s.lfoCut);

        float yl, yr;
        if (s.route == 2)
        {
            const float y = v.fB.process (v.fA.process (a, fcA, s.resA, s.drive), fcB, s.resB, 1.0f);
            yl = yr = y;
        }
        else
        {
            const float ya = v.fA.process (a, fcA, s.resA, s.drive);
            const float yb = v.fB.process (b, fcB, s.resB, s.drive);
            yl = ya * (1.0f + s.width) * 0.5f + yb * (1.0f - s.width) * 0.5f;
            yr = yb * (1.0f + s.width) * 0.5f + ya * (1.0f - s.width) * 0.5f;
        }
        const float gain = am * (1.0f - s.velAmpAmt + s.velAmpAmt * v.velocity);
        outL += yl * gain; outR += yr * gain;

        if (v.amp.idle()) { v.fA.reset(); v.fB.reset(); }
    }

    //==========================================================================
    static float polyBlep (float t, float dt)
    {
        if (t < dt)        { t /= dt;             return t + t - t * t - 1.0f; }
        if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
        return 0.0f;
    }

    // 0 Saw | 1 Pulse | 2 Triangle | 3 Sine | 4 Sync | 5 Fold | 6 FM
    static float renderWave (int w, float& syncPh, float ph, float dt, bool wrapped, float shp)
    {
        switch (w)
        {
            case 0: return 2.0f * ph - 1.0f - polyBlep (ph, dt);
            case 1:
            {
                const float pw = 0.05f + 0.9f * shp;
                float v = ph < pw ? 1.0f : -1.0f;
                v += polyBlep (ph, dt);
                float t2 = ph + 1.0f - pw; if (t2 >= 1.0f) t2 -= 1.0f;
                v -= polyBlep (t2, dt);
                return (v - (2.0f * pw - 1.0f)) * 0.7f;
            }
            case 2: return (ph < 0.5f ? 4.0f * ph - 1.0f : 3.0f - 4.0f * ph);
            case 3: return std::sin (2.0f * kPi * ph) * 0.9f;
            case 4:
            {
                const float ratio = 1.0f + shp * 7.0f;
                const float sdt = std::min (dt * ratio, 0.45f);
                float sp = syncPh;
                const float before = 2.0f * sp - 1.0f;
                float v;
                if (wrapped)
                {
                    sp = ph * ratio; sp -= std::floor (sp);
                    const float jump = before - (2.0f * sp - 1.0f);
                    v = 2.0f * sp - 1.0f - (jump * 0.5f) * polyBlep (ph, dt);
                }
                else
                {
                    sp += sdt; if (sp >= 1.0f) sp -= 1.0f;
                    v = 2.0f * sp - 1.0f - polyBlep (sp, sdt);
                }
                syncPh = sp;
                return v * 0.9f;
            }
            case 5:
            {
                const float g = 1.0f + shp * 6.0f;
                return std::sin (0.5f * kPi * g * std::sin (2.0f * kPi * ph)) * 0.85f;
            }
            default:
            {
                const float idx = shp * 4.0f;
                return std::sin (2.0f * kPi * ph + idx * std::sin (4.0f * kPi * ph)) * 0.85f;
            }
        }
    }

    static float softClip (float x)
    {
        const float a = std::fabs (x);
        if (a < 0.8f) return x;
        const float y = 0.8f + 0.2f * std::tanh ((a - 0.8f) / 0.2f);
        return x < 0 ? -y : y;
    }
    void removeNote (int idx)
    {
        for (int j = idx; j < numNotes - 1; ++j) notes[j] = notes[j + 1];
        --numNotes;
    }

    float p[NumParams] {};
    float sr = 44100;
    Voice voices[kVoices];
    float scatter[kOscs] {}, staggerU[kOscs] {};
    Lfo lfo;
    Reverb rev;

    int   notes[32] {}; int numNotes = 0;
    int   chord[16] {}; int chordSize = 0;
    bool  chordActive = false, chordChanged = false;
    float velocity = 1, bend = 0, press = 0, tempo = 120;
    bool  prevGate = false, prevAmpGate = false, retrigger = false, pitchInit = false;
    bool  prevRibGate = false, sustainDown = false;
    int   ribbonVoice = -1, lastMode = -1;
    uint32_t stampCounter = 0;
    float lastTarget = 48, swarmSm = 0, swarmOut = 0, polyOffset = 0;
    float chordOffsetSm = 0, chordOffsetTarget = 0;
    float cutASm = 10, cutBSm = 10, volSm = 0;
};
} // namespace swarm
