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

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        fA.sr = fB.sr = sr; fA.reset(); fB.reset();
        envA.sr = envB.sr = amp.sr = sr;
        envA.reset(); envB.reset(); amp.reset();
        lfo.sr = sr;
        rev.prepare (sr);
        Rng r; r.s = 0x1234567u;
        for (int i = 0; i < kOscs; ++i)
        {
            phase[i]       = 0.5f + 0.5f * r.next();
            driftStatic[i] = r.next();
            driftCur[i] = driftTgt[i] = 0;
            driftCount[i]  = 0;
            oscGain[i]     = 0;
        }
        // fixed "scatter" positions: osc 0 stays on the root
        static const float sc[kOscs] = { 0.0f, 0.62f, -0.41f, 0.17f, -0.88f, 0.93f, -0.23f, 0.45f };
        std::copy (sc, sc + kOscs, scatter);
        defaultValues (p);
        pitchSm = 48; pitchInit = false;
        cutASm = std::log2 (p[pCutA]); cutBSm = std::log2 (p[pCutB]);
        volSm = 0;
        numNotes = 0; prevGate = false; prevAmpGate = false;
    }

    void setParams (const float* v) { std::memcpy (p, v, sizeof (p)); }
    const float* params() const { return p; }

    // --- MIDI ----------------------------------------------------------------
    void noteOn (int note, float vel)
    {
        for (int i = 0; i < numNotes; ++i) if (notes[i] == note) { removeNote (i); break; }
        if (numNotes < 32) notes[numNotes++] = note;
        velocity = vel;
        if (p[pLegato] < 0.5f || numNotes == 1) retrigger = true;
    }
    void noteOff (int note)
    {
        for (int i = 0; i < numNotes; ++i) if (notes[i] == note) { removeNote (i); break; }
    }
    void allNotesOff() { numNotes = 0; }
    void pitchBend (float b) { bend = b; }   // -1..1
    void pressure (float pr) { press = pr; } // 0..1

    bool isSilent() const { return amp.idle(); }
    float currentPitch() const { return pitchSm; }
    float currentSwarm() const { return swarmOut; }

    //==========================================================================
    void process (float* L, float* R, int n)
    {
        // ---- block-rate control -------------------------------------------
        const bool ribGate = p[pRibGate] > 0.5f;
        const bool gate    = numNotes > 0 || ribGate;
        const bool drone   = p[pDrone] > 0.5f;
        const bool ampGate = gate || drone;

        envA.set (p[pAtkA], p[pDecA], p[pSusA], p[pRelA]);
        envB.set (p[pAtkB], p[pDecB], p[pSusB], p[pRelB]);
        amp .set (p[pAtk],  p[pDec],  p[pSus],  p[pRel]);
        rev.setParams (p[pRevSize], p[pRevDamp]);

        float target;
        bool  fromRibbon = false;
        if (ribGate)            { target = p[pRibBase] + p[pRibbon] * p[pRibRange] * 12.0f; fromRibbon = true; }
        else if (numNotes > 0)  target = (float) notes[numNotes - 1];
        else                    target = lastTarget;
        lastTarget = target;
        target += p[pTune] + p[pFine] * 0.01f + bend * p[pBend];

        if ((gate && ! prevGate) || (gate && retrigger))
        {
            if (amp.idle() || ! pitchInit) { pitchSm = target; pitchInit = true; }
            envA.gateOn(); envB.gateOn(); amp.gateOn();
        }
        retrigger = false;
        if (! gate && prevGate) { envA.gateOff(); envB.gateOff(); }
        if (ampGate && ! prevAmpGate && ! amp.idle() && amp.stage == Envelope::Release) amp.gateOn();
        if (ampGate && amp.idle()) amp.gateOn();
        if (! ampGate && prevAmpGate) amp.gateOff();
        prevGate = gate; prevAmpGate = ampGate;

        const float glideC = onePoleCoef (fromRibbon ? 0.006f : p[pGlide], sr);
        const float lagC   = onePoleCoef (std::max (p[pSwLag], 0.003f), sr);
        const float smC    = onePoleCoef (0.01f, sr);
        const float driftC = onePoleCoef (1.2f, sr);

        const int   nOsc   = std::clamp ((int) std::lround (p[pOscs]), 1, kOscs);
        const int   mode   = (int) std::lround (p[pSwMode]);
        const int   route  = (int) std::lround (p[pRoute]);
        const int   shape  = (int) std::lround (p[pLfoShape]);
        const float curveE = std::exp2 (p[pSwCurve] * 2.0f);   // >1 clusters near root, <1 pushes outward
        const float drive  = std::pow (10.0f, p[pDrive] / 20.0f);
        const float velF   = 1.0f - p[pVelFilt] + p[pVelFilt] * velocity;
        const float velA   = 1.0f - p[pVelAmp] + p[pVelAmp] * velocity;
        const float volT   = std::pow (10.0f, p[pVolume] / 20.0f);
        const float mix    = p[pRevMix];
        const float dryG   = std::cos (mix * kPi * 0.5f), wetG = std::sin (mix * kPi * 0.5f);
        const float w      = p[pWidth];
        const float swTgt  = std::clamp (p[pSwarm] + press * p[pAtSwarm], 0.0f, 1.0f);
        const float driftAmt = p[pDrift];
        const int   wave1    = (int) std::lround (p[pWave1]);
        const int   wave2    = (int) std::lround (p[pWave2]);
        const float waveMix  = p[pWaveMix];
        const bool  altWaves = p[pAltWaves] > 0.5f;
        const float subLvl   = p[pSub], noiseLvl = p[pNoise];

        // oscillator positions in [-1, 1] before shaping
        float pos[kOscs];
        for (int i = 0; i < kOscs; ++i)
        {
            const float t = nOsc > 1 ? (float) i / (float) (nOsc - 1) : 0.0f;
            switch (mode)
            {
                case 0: { // Center: root stays, others alternate up/down outward
                    const int half = nOsc / 2;
                    const int k = (i + 1) / 2;
                    const float m = half > 0 ? (float) k / (float) half : 0.0f;
                    pos[i] = (i == 0) ? 0.0f : ((i & 1) ? m : -m);
                    break;
                }
                case 1:  pos[i] = t;  break;
                case 2:  pos[i] = -t; break;
                default: pos[i] = scatter[i]; break;
            }
            const float a = std::fabs (pos[i]);
            pos[i] = (pos[i] < 0 ? -1.0f : 1.0f) * std::pow (a, curveE);
        }
        const float norm = route == 0 ? 0.75f / std::sqrt (std::max (1.0f, nOsc * 0.5f))
                                      : 0.55f / std::sqrt ((float) nOsc);

        // ---- sample loop ----------------------------------------------------
        for (int s = 0; s < n; ++s)
        {
            const float l = lfo.tick (p[pLfoRate], shape);
            const float shapeNow = std::clamp (p[pShape] + l * p[pShapeLfo] * 0.5f, 0.0f, 1.0f);

            pitchSm += (target - pitchSm) * glideC;
            swarmSm += (swTgt - swarmSm) * lagC;
            const float sw = std::clamp (swarmSm + l * p[pLfoSwarm], 0.0f, 1.0f);
            swarmOut = sw;
            const float spread = sw * p[pSwRange];
            const float base   = pitchSm + l * p[pLfoPitch];

            float a = 0, b = 0;
            for (int i = 0; i < kOscs; ++i)
            {
                oscGain[i] += ((i < nOsc ? 1.0f : 0.0f) - oscGain[i]) * smC;
                if (i >= nOsc && oscGain[i] < 1.0e-4f) continue;

                // analog drift: slow random wander + fixed per-oscillator detune
                if (--driftCount[i] <= 0)
                {
                    driftTgt[i]   = rng.next();
                    driftCount[i] = (int) (sr * (0.4f + 0.8f * (rng.next() * 0.5f + 0.5f)));
                }
                driftCur[i] += (driftTgt[i] - driftCur[i]) * driftC;
                const float driftSt = driftAmt * (0.18f * driftCur[i] + 0.05f * driftStatic[i]); // semitones

                const float semis = base + pos[i] * spread + driftSt;
                float f  = 440.0f * std::exp2 ((semis - 69.0f) / 12.0f);
                f = std::min (f, sr * 0.45f);
                const float dt = f / sr;

                float ph = phase[i] + dt;
                const bool wrapped = ph >= 1.0f;
                if (wrapped) ph -= 1.0f;
                phase[i] = ph;

                float v;
                if (altWaves)        v = renderWave ((i & 1) ? wave2 : wave1, i, ph, dt, wrapped, shapeNow);
                else if (waveMix <= 0.0f || wave1 == wave2)
                                     v = renderWave (wave1, i, ph, dt, wrapped, shapeNow);
                else if (waveMix >= 1.0f)
                                     v = renderWave (wave2, i, ph, dt, wrapped, shapeNow);
                else                 v = renderWave (wave1, i, ph, dt, wrapped, shapeNow) * (1.0f - waveMix)
                                       + renderWave (wave2, i, ph, dt, wrapped, shapeNow) * waveMix;

                v *= oscGain[i];
                if (route == 0) { if (i & 1) b += v; else a += v; }
                else a += v;
            }

            a *= norm; b *= norm;
            // sub oscillator (square, one octave below, not swarmed) + noise
            if (subLvl > 0.0f)
            {
                const float sdt = std::min (440.0f * std::exp2 ((base - 12.0f - 69.0f) / 12.0f), sr * 0.45f) / sr;
                subPhase += sdt; if (subPhase >= 1.0f) subPhase -= 1.0f;
                float sq = subPhase < 0.5f ? 1.0f : -1.0f;
                sq += polyBlep (subPhase, sdt);
                float t2 = subPhase + 0.5f; if (t2 >= 1.0f) t2 -= 1.0f;
                sq -= polyBlep (t2, sdt);
                const float sv = sq * subLvl * 0.35f;
                a += sv; b += sv;
            }
            if (noiseLvl > 0.0f) { a += rng.next() * noiseLvl * 0.25f; b += rng.next() * noiseLvl * 0.25f; }
            if (route != 0) b = a;

            const float ea = envA.tick(), eb = envB.tick(), am = amp.tick();
            cutASm += (std::log2 (p[pCutA]) - cutASm) * smC;
            cutBSm += (std::log2 (p[pCutB]) - cutBSm) * smC;
            const float kt   = p[pKeyTrack] * (pitchSm - 60.0f) / 12.0f;
            const float lc   = l * p[pLfoCut];
            const float fcA  = std::exp2 (cutASm + p[pEnvA] * 6.0f * ea * velF + kt + lc);
            const float fcB  = std::exp2 (cutBSm + p[pEnvB] * 6.0f * eb * velF + kt + lc);

            float yl, yr;
            if (route == 2) // series: 8-pole through A then B
            {
                const float y = fB.process (fA.process (a, fcA, p[pResA], drive), fcB, p[pResB], 1.0f);
                yl = yr = y;
            }
            else
            {
                const float ya = fA.process (a, fcA, p[pResA], drive);
                const float yb = fB.process (b, fcB, p[pResB], drive);
                yl = ya * (1.0f + w) * 0.5f + yb * (1.0f - w) * 0.5f;
                yr = yb * (1.0f + w) * 0.5f + ya * (1.0f - w) * 0.5f;
            }

            volSm += (volT - volSm) * smC;
            const float gain = am * velA * volSm;
            yl *= gain; yr *= gain;

            float rl, rr;
            rev.process (yl, yr, rl, rr);
            L[s] = softClip (yl * dryG + rl * wetG);
            R[s] = softClip (yr * dryG + rr * wetG);
        }
    }

private:
    // polyBLEP residual for a unit-normalised downward step of 2 (as in a saw reset)
    static float polyBlep (float t, float dt)
    {
        if (t < dt)        { t /= dt;          return t + t - t * t - 1.0f; }
        if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
        return 0.0f;
    }

    // 0 Saw | 1 Pulse | 2 Triangle | 3 Sine | 4 Sync | 5 Fold | 6 FM
    float renderWave (int w, int i, float ph, float dt, bool wrapped, float shp)
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
            case 4: // hard sync: hidden master resets a saw running 1x..8x faster
            {
                const float ratio = 1.0f + shp * 7.0f;
                const float sdt = std::min (dt * ratio, 0.45f);
                float sp = syncPhase[i];
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
                syncPhase[i] = sp;
                return v * 0.9f;
            }
            case 5: // sine wavefolder
            {
                const float g = 1.0f + shp * 6.0f;
                return std::sin (0.5f * kPi * g * std::sin (2.0f * kPi * ph)) * 0.85f;
            }
            default: // 2-op FM, modulator at 2x
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
    float phase[kOscs] {}, oscGain[kOscs] {}, driftCur[kOscs] {}, driftTgt[kOscs] {}, driftStatic[kOscs] {}, scatter[kOscs] {};
    int   driftCount[kOscs] {};
    float syncPhase[kOscs] {}, subPhase = 0;
    Ladder fA, fB;
    Envelope envA, envB, amp;
    Lfo lfo;
    Reverb rev;
    Rng rng;

    int   notes[32] {}; int numNotes = 0;
    float velocity = 1, bend = 0, press = 0;
    bool  prevGate = false, prevAmpGate = false, retrigger = false, pitchInit = false;
    float lastTarget = 48, pitchSm = 48, swarmSm = 0, swarmOut = 0;
    float cutASm = 10, cutBSm = 10, volSm = 0;
};
} // namespace swarm
