#pragma once
// Moteur de synthèse polyphonique 8 voix (C++ pur, sans JUCE).
// 2 VCO (PolyBLEP) + sync + cross-mod, bruit, HPF, VCF 12 dB (SVF) / 24 dB (échelle),
// 2 enveloppes ADSR, LFO, modes Poly / Unison / Mono, glide, pédale de sustain.
#include "Params.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

namespace jv
{
constexpr float kPi = 3.14159265358979323846f;
constexpr int kVoices = 8;

enum EvType { EV_NOTE_ON, EV_NOTE_OFF, EV_ALL_OFF, EV_BEND, EV_MOD, EV_SUSTAIN };
struct Ev
{
    int pos;
    EvType type;
    int note;
    float value;
};

struct EnvSet { float a, d, s, r; };

struct Envelope
{
    enum Stage { Idle, Att, Dec, Sus, Rel };
    Stage st = Idle;
    float v = 0.f;

    float tick (const EnvSet& e, float sr)
    {
        switch (st)
        {
            case Att:
                v += 1.f / (std::max (e.a, 0.0005f) * sr);
                if (v >= 1.f) { v = 1.f; st = Dec; }
                break;
            case Dec:
            {
                const float c = std::exp (-4.6f / (std::max (e.d, 0.001f) * sr));
                v = e.s + (v - e.s) * c;
                if (std::fabs (v - e.s) < 1e-4f) { v = e.s; st = Sus; }
                break;
            }
            case Sus: v = e.s; break;
            case Rel:
                v *= std::exp (-4.6f / (std::max (e.r, 0.001f) * sr));
                if (v < 1e-4f) { v = 0.f; st = Idle; }
                break;
            default: break;
        }
        return v;
    }
};

struct Osc
{
    float phase = 0.f;
    bool wrapped = false;

    static float blep (float t, float dt)
    {
        if (t < dt) { t /= dt; return t + t - t * t - 1.f; }
        if (t > 1.f - dt) { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
        return 0.f;
    }

    // wave : 0 tri, 1 saw, 2 pulse (largeur pw), 3 carré
    float gen (int wave, float dt, float pw)
    {
        float out;
        if (wave == 0)
            out = 4.f * std::fabs (phase - 0.5f) - 1.f;
        else if (wave == 1)
            out = 2.f * phase - 1.f - blep (phase, dt);
        else
        {
            const float w = (wave == 3) ? 0.5f : pw;
            out = phase < w ? 1.f : -1.f;
            out += blep (phase, dt);
            float t2 = phase - w;
            if (t2 < 0.f) t2 += 1.f;
            out -= blep (t2, dt);
            out -= (2.f * w - 1.f); // retire la composante continue
        }
        phase += dt;
        wrapped = false;
        if (phase >= 1.f) { phase -= 1.f; wrapped = true; }
        return out;
    }
};

struct Svf
{
    float s1 = 0.f, s2 = 0.f;
    void reset() { s1 = s2 = 0.f; }
    float lp (float x, float g, float R)
    {
        const float hp = (x - (2.f * R + g) * s1 - s2) / (1.f + 2.f * R * g + g * g);
        const float bp = g * hp + s1;
        s1 = g * hp + bp;
        const float l = g * bp + s2;
        s2 = g * bp + l;
        return l;
    }
};

struct Ladder
{
    float z[4] = { 0, 0, 0, 0 };
    void reset() { z[0] = z[1] = z[2] = z[3] = 0.f; }
    float process (float x, float g, float k)
    {
        const float G = g / (1.f + g), G2 = G * G, G3 = G2 * G, G4 = G2 * G2, ig = 1.f / (1.f + g);
        const float S = (G3 * z[0] + G2 * z[1] + G * z[2] + z[3]) * ig;
        float y = (x - k * S) / (1.f + k * G4);
        for (int i = 0; i < 4; ++i)
        {
            const float v = (y - z[i]) * G;
            const float o = v + z[i];
            z[i] = o + v;
            y = o;
        }
        return y;
    }
};

class Engine
{
public:
    void prepare (double sampleRate, int maxBlock = 1024)
    {
        sr = (float) sampleRate;
        lfoBuf.assign ((size_t) std::max (16, maxBlock), 0.f);
        held.reserve (256);
        reset();
    }

    void reset()
    {
        for (auto& v : voices) v = Voice();
        held.clear();
        sustainDown = false;
        lastPitch = -1.f;
        lfoPhase = 0.f;
        bend = 0.f;
        modWheel = 0.f;
        ageCounter = 0;
    }

    void setParams (const Params& p) { P = p; }

    void handle (const Ev& e)
    {
        switch (e.type)
        {
            case EV_NOTE_ON:  noteOn (e.note, e.value); break;
            case EV_NOTE_OFF: noteOff (e.note); break;
            case EV_ALL_OFF:  allNotesOff(); break;
            case EV_BEND:     bend = e.value; break;
            case EV_MOD:      modWheel = e.value; break;
            case EV_SUSTAIN:  setSustain (e.value > 0.5f); break;
        }
    }

    void allNotesOff()
    {
        held.clear();
        for (auto& v : voices)
            if (v.gateOn) release (v);
    }

    void render (float* L, float* R, int n)
    {
        while (n > 0)
        {
            const int m = std::min (n, (int) lfoBuf.size());
            renderChunk (L, R, m);
            L += m; R += m; n -= m;
        }
    }

private:
    struct Voice
    {
        bool active = false, gateOn = false, pendingRelease = false;
        int note = 60;
        float vel = 1.f, pitch = 60.f, target = 60.f, detune = 0.f, lfoGain = 1.f, o2prev = 0.f, hpS = 0.f;
        uint32_t age = 0;
        Osc o1, o2;
        Envelope fe, ae;
        Ladder lad;
        Svf svf;
    };

    float rnd() // [0,1)
    {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        return (float) (rng & 0xFFFFFF) / 16777216.f;
    }

    void release (Voice& v)
    {
        v.gateOn = false;
        v.pendingRelease = false;
        if (v.fe.st != Envelope::Idle) v.fe.st = Envelope::Rel;
        if (v.ae.st != Envelope::Idle) v.ae.st = Envelope::Rel;
    }

    void trigger (Voice& v, int note, float vel, float detune)
    {
        const bool fresh = ! v.active || v.ae.st == Envelope::Idle;
        if (fresh)
        {
            v.lad.reset(); v.svf.reset(); v.hpS = 0.f; v.o2prev = 0.f;
            v.fe.v = 0.f; v.ae.v = 0.f;
            v.pitch = (P[P_GLIDE] > 0.001f && lastPitch >= 0.f) ? lastPitch : (float) note;
        }
        v.note = note; v.vel = vel; v.target = (float) note; v.detune = detune;
        v.gateOn = true; v.pendingRelease = false; v.active = true; v.age = ++ageCounter;
        v.fe.st = Envelope::Att; v.ae.st = Envelope::Att;
        v.lfoGain = P[P_LFODELAY] > 0.001f ? 0.f : 1.f;
    }

    void triggerMono (int note, float vel)
    {
        if ((int) P[P_POLY] == 2)
        {
            trigger (voices[0], note, vel, 0.f);
            for (int i = 1; i < kVoices; ++i)
                if (voices[i].gateOn) release (voices[i]);
        }
        else
        {
            for (int i = 0; i < kVoices; ++i)
                trigger (voices[i], note, vel, ((i - 3.5f) / 3.5f) * P[P_UNIDET] * 40.f);
        }
        lastPitch = (float) note;
    }

    void noteOn (int n, float vel)
    {
        if ((int) P[P_POLY] == 0)
        {
            Voice* tgt = nullptr;
            for (auto& v : voices) if (v.active && v.gateOn && v.note == n) { tgt = &v; break; }
            if (! tgt) for (auto& v : voices) if (! v.active) { tgt = &v; break; }
            if (! tgt)
            {
                uint32_t best = UINT32_MAX;
                for (auto& v : voices) if (! v.gateOn && v.age < best) { best = v.age; tgt = &v; }
                if (! tgt)
                    for (auto& v : voices) if (v.age < best) { best = v.age; tgt = &v; }
            }
            trigger (*tgt, n, vel, 0.f);
            lastPitch = (float) n;
        }
        else
        {
            held.erase (std::remove_if (held.begin(), held.end(), [n] (const std::pair<int, float>& h) { return h.first == n; }), held.end());
            held.push_back ({ n, vel });
            triggerMono (n, vel);
        }
    }

    void noteOff (int n)
    {
        if ((int) P[P_POLY] == 0)
        {
            for (auto& v : voices)
                if (v.active && v.gateOn && v.note == n)
                {
                    if (sustainDown) v.pendingRelease = true; else release (v);
                }
        }
        else
        {
            held.erase (std::remove_if (held.begin(), held.end(), [n] (const std::pair<int, float>& h) { return h.first == n; }), held.end());
            if (held.empty())
            {
                for (auto& v : voices)
                    if (v.gateOn) { if (sustainDown) v.pendingRelease = true; else release (v); }
            }
            else
                triggerMono (held.back().first, held.back().second);
        }
    }

    void setSustain (bool d)
    {
        sustainDown = d;
        if (! d)
            for (auto& v : voices)
                if (v.pendingRelease) release (v);
    }

    void renderChunk (float* L, float* R, int m)
    {
        // --- LFO global
        const float lfoInc = P[P_LFORATE] / sr;
        const int lw = (int) P[P_LFOWAVE];
        for (int i = 0; i < m; ++i)
        {
            lfoPhase += lfoInc;
            if (lfoPhase >= 1.f) { lfoPhase -= 1.f; shValue = rnd() * 2.f - 1.f; }
            float x;
            switch (lw)
            {
                case 0: x = 1.f - 4.f * std::fabs (lfoPhase - 0.5f); break;
                case 1: x = 1.f - 2.f * lfoPhase; break;
                case 2: x = lfoPhase < 0.5f ? 1.f : -1.f; break;
                default: x = shValue; break;
            }
            lfoBuf[(size_t) i] = x;
        }
        std::fill (L, L + m, 0.f);
        std::fill (R, R + m, 0.f);

        // --- paramètres constants sur le bloc
        const float glideT = P[P_GLIDE];
        const float glideCoef = glideT > 0.001f ? 1.f - std::exp (-1.f / (glideT * sr)) : 1.f;
        const float lfoDelay = P[P_LFODELAY];
        const float lfoGainInc = lfoDelay > 0.001f ? 1.f / (lfoDelay * sr) : 1.f;
        const float pd = std::min (1.f, std::max (0.f, P[P_LFOPITCH] + modWheel));
        const float pitchDepth = pd * pd * 4.f;
        const float bendSemis = bend * P[P_BENDRANGE];
        const EnvSet fenv { P[P_FA], P[P_FD], P[P_FS], P[P_FR] };
        const EnvSet aenv { P[P_AA], P[P_AD], P[P_AS], P[P_AR] };
        const int w1 = (int) P[P_O1WAVE], w2 = (int) P[P_O2WAVE];
        const float oct1 = (P[P_O1RANGE] - 1.f) * 12.f;
        const float oct2 = (P[P_O2RANGE] - 1.f) * 12.f + P[P_O2SEMI] + P[P_O2FINE] / 100.f;
        const bool sync = P[P_SYNC] > 0.5f;
        const float xmod = P[P_XMOD], pw = P[P_PW], pwm = P[P_PWM], mix = P[P_MIX], noiseLvl = P[P_NOISE];
        const float hg = std::tan (kPi * std::min (P[P_HPF], 0.45f * sr) / sr);
        const float hpG = hg / (1.f + hg);
        const float cutoff = P[P_CUTOFF], res = P[P_RES];
        const bool slope24 = (int) P[P_SLOPE] == 1;
        const float kRes = std::min (res, 0.98f) * 4.f;
        const float svfR = 1.f - 0.97f * res;
        const float envAmt = P[P_ENVAMT], keyFol = P[P_KEYFOL], lfoFilt = P[P_LFOFILT];
        const float velSens = P[P_VELSENS];
        const float voiceGain = (int) P[P_POLY] == 1 ? 0.30f : 0.42f;
        const float spread = P[P_SPREAD];

        for (int vi = 0; vi < kVoices; ++vi)
        {
            Voice& v = voices[(size_t) vi];
            if (! v.active) continue;

            const float panPos = 0.5f + 0.5f * spread * (((vi * 5) % 8) / 3.5f - 1.f);
            const float gl = std::cos (panPos * kPi * 0.5f) * 1.41421f;
            const float gr = std::sin (panPos * kPi * 0.5f) * 1.41421f;
            const float velGain = (1.f - velSens) + velSens * v.vel;
            const float detSemis = v.detune / 100.f;

            for (int i = 0; i < m; ++i)
            {
                v.pitch += (v.target - v.pitch) * glideCoef;
                v.lfoGain = std::min (1.f, v.lfoGain + lfoGainInc);
                const float lfo = lfoBuf[(size_t) i] * v.lfoGain;
                const float semis = v.pitch + detSemis + bendSemis + lfo * pitchDepth;

                const float dt1 = std::min (0.45f, 440.f * std::exp2 ((semis + oct1 - 69.f) / 12.f) / sr);
                const float dt2 = std::min (0.45f, 440.f * std::exp2 ((semis + oct2 - 69.f) / 12.f) / sr);
                const float pwE = std::min (0.95f, std::max (0.05f, pw + pwm * lfo * 0.45f));
                const float dt1m = xmod > 0.f ? std::min (0.45f, dt1 * std::exp2 (xmod * v.o2prev * 2.f)) : dt1;

                const float s1 = v.o1.gen (w1, dt1m, pwE);
                if (sync && v.o1.wrapped) v.o2.phase = std::min (0.999f, v.o1.phase * (dt2 / dt1m));
                const float s2 = v.o2.gen (w2, dt2, pwE);
                v.o2prev = s2;

                float sig = (s1 * (1.f - mix) + s2 * mix) * 1.4f;
                if (noiseLvl > 0.f) sig += (rnd() * 2.f - 1.f) * noiseLvl * 0.8f;

                // HPF 6 dB
                {
                    const float vv = (sig - v.hpS) * hpG;
                    const float lp = vv + v.hpS;
                    v.hpS = lp + vv;
                    sig -= lp;
                }

                const float fe = v.fe.tick (fenv, sr);
                const float ae = v.ae.tick (aenv, sr);

                float octs = envAmt * 6.f * fe + lfoFilt * lfo * 2.f + keyFol * (v.pitch - 60.f) / 12.f;
                octs = std::min (8.f, std::max (-8.f, octs));
                float fc = std::min (cutoff * std::exp2 (octs), 0.45f * sr);
                fc = std::max (fc, 20.f);
                const float g = std::tan (kPi * fc / sr);
                const float y = slope24 ? v.lad.process (sig, g, kRes) * (1.f + 0.5f * res)
                                        : v.svf.lp (sig, g, svfR);

                const float out = y * ae * velGain * voiceGain;
                L[i] += out * gl;
                R[i] += out * gr;

                if (! v.gateOn && v.ae.st == Envelope::Idle) { v.active = false; break; }
            }
        }

        const float master = P[P_MASTER];
        for (int i = 0; i < m; ++i)
        {
            L[i] = std::tanh (L[i]) * master;
            R[i] = std::tanh (R[i]) * master;
        }
    }

    Params P = Params::defaults();
    float sr = 44100.f;
    Voice voices[kVoices];
    std::vector<std::pair<int, float>> held;
    std::vector<float> lfoBuf;
    bool sustainDown = false;
    float lastPitch = -1.f, lfoPhase = 0.f, shValue = 0.f, bend = 0.f, modWheel = 0.f;
    uint32_t ageCounter = 0, rng = 2463534242u;
};
} // namespace jv
