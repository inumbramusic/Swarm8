// Offline test: renders every factory preset with a short performance to a WAV file
// and reports peak / RMS / NaN checks. Build: g++ -O2 -std=c++17 render_demo.cpp -o render_demo
#include "../Source/SwarmEngine.h"
#include "../Source/Presets.h"
#include <cstdio>
#include <string>

using namespace swarm;

static void writeWav (const char* path, const std::vector<float>& L, const std::vector<float>& R, int sr)
{
    FILE* f = std::fopen (path, "wb");
    const uint32_t n = (uint32_t) L.size(), dataBytes = n * 4;
    auto u32 = [&] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
    auto u16 = [&] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    std::fwrite ("RIFF", 1, 4, f); u32 (36 + dataBytes); std::fwrite ("WAVEfmt ", 1, 8, f);
    u32 (16); u16 (1); u16 (2); u32 ((uint32_t) sr); u32 ((uint32_t) sr * 4); u16 (4); u16 (16);
    std::fwrite ("data", 1, 4, f); u32 (dataBytes);
    for (uint32_t i = 0; i < n; ++i)
    {
        int16_t a = (int16_t) std::lround (std::clamp (L[i], -1.f, 1.f) * 32767.f);
        int16_t b = (int16_t) std::lround (std::clamp (R[i], -1.f, 1.f) * 32767.f);
        std::fwrite (&a, 2, 1, f); std::fwrite (&b, 2, 1, f);
    }
    std::fclose (f);
}

int main (int argc, char** argv)
{
    const int sr = 48000, block = 64;
    const char* out = argc > 1 ? argv[1] : "demo.wav";
    std::vector<float> L, R;
    float v[NumParams];
    bool anyBad = false;

    for (auto& pr : factoryPresets())
    {
        if (std::string (pr.name) == "Init") continue;
        SwarmEngine e; e.prepare (sr);
        presetValues (pr, v);
        const std::string name = pr.name;
        const bool bass   = name == "Sub Rumble Bass";
        const bool ribbon = name == "Siren Ribbon";
        const float presetSwarm = v[pSwarm];
        const bool reverse = presetSwarm > 0.9f;
        const float len = 7.0f, tail = 2.8f;
        const int total = (int) ((len + tail) * sr);
        double peak = 0, sumSq = 0; bool bad = false;
        const size_t startIdx = L.size();
        int noteIdx = -1;
        static const int riff[] = { 33, 33, 45, 33, 36, 33, 31, 33, 33, 45, 40, 38 };

        for (int pos = 0; pos < total; pos += block)
        {
            const float t = (float) pos / sr;
            const int n = std::min (block, total - pos);
            float pv[NumParams]; std::copy (v, v + NumParams, pv);

            if (bass)
            {
                const int step = (int) (t / 0.5f);
                if (t < len && step != noteIdx) { if (noteIdx >= 0) e.noteOff (riff[noteIdx % 12]); noteIdx = step; e.noteOn (riff[step % 12], 0.9f); }
                if (t >= len && noteIdx >= 0) { e.noteOff (riff[noteIdx % 12]); noteIdx = -2; }
            }
            else if (ribbon)
            {
                pv[pRibGate] = t < len ? 1.f : 0.f;
                pv[pRibbon]  = 0.35f + 0.25f * std::sin (t * 0.9f) + 0.1f * std::sin (t * 2.3f);
            }
            else
            {
                if (pos == 0) e.noteOn (name == "Dorm Room Dread" ? 38 : 48, 0.85f);
                if (t >= len && noteIdx == -1) { e.noteOff (name == "Dorm Room Dread" ? 38 : 48); noteIdx = -2; }
                // swarm gesture: hold, sweep out, come back (reverse for presets that start wide)
                float g = 0;
                if (t > 1.0f && t < 4.0f) g = (t - 1.0f) / 3.0f;
                else if (t >= 4.0f && t < 6.5f) g = 1.0f - (t - 4.0f) / 2.5f;
                pv[pSwarm] = reverse ? 1.0f - g : std::min (1.0f, presetSwarm + g * (1.0f - presetSwarm));
            }

            e.setParams (pv);
            float bl[block], br[block];
            e.process (bl, br, n);
            for (int i = 0; i < n; ++i)
            {
                if (! std::isfinite (bl[i]) || ! std::isfinite (br[i])) bad = true;
                peak = std::max (peak, (double) std::max (std::fabs (bl[i]), std::fabs (br[i])));
                sumSq += bl[i] * bl[i] + br[i] * br[i];
                L.push_back (bl[i]); R.push_back (br[i]);
            }
        }
        const double rms = std::sqrt (sumSq / (2.0 * total));
        std::printf ("%5.1fs  %-18s peak %6.2f dBFS  rms %6.1f dBFS %s\n",
                     (double) startIdx / sr, pr.name, 20 * std::log10 (peak + 1e-9), 20 * std::log10 (rms + 1e-9), bad ? "NaN!" : "");
        anyBad |= bad;
        for (int i = 0; i < sr / 3; ++i) { L.push_back (0); R.push_back (0); }
    }
    writeWav (out, L, R, sr);
    return anyBad ? 1 : 0;
}
