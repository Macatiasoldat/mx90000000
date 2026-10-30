#pragma once
// Moteur de sampler 16 pads (C++ pur, sans JUCE) : lecture avec interpolation cubique,
// début/fin, boucle, reverse, hauteur, enveloppe attack/release, filtre, choke, mode clavier,
// puis drive + delay stéréo.
#include "SamplerParams.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace sp
{
constexpr float kPi = 3.14159265358979323846f;
constexpr int kMaxVoices = 32;

struct Sample
{
    std::vector<float> L, R; // R vide = mono
    double sr = 44100.0;
    int frames = 0;
    std::string name;
    std::vector<float> peaks; // enveloppe pour l'affichage (0..1)
};
using SamplePtr = std::shared_ptr<const Sample>;

enum EvType { EV_PAD_ON, EV_PAD_OFF, EV_KEY_ON, EV_KEY_OFF, EV_ALL_OFF };
struct Ev
{
    int pos;
    EvType type;
    int note; // numéro de pad (PAD_*) ou note MIDI (KEY_*)
    float value; // vélocité 0..1
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

class Engine
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        dlySize = (int) (sr * 1.2f) + 8;
        dlyL.assign ((size_t) dlySize, 0.f);
        dlyR.assign ((size_t) dlySize, 0.f);
        reset();
    }

    void reset()
    {
        for (auto& v : voices) v = Voice();
        std::fill (dlyL.begin(), dlyL.end(), 0.f);
        std::fill (dlyR.begin(), dlyR.end(), 0.f);
        dlyPos = 0;
        dlyTimeCur = 0.375f * sr;
        ageCounter = 0;
    }

    // Remplace l'échantillon d'un pad ; renvoie l'ancien pour qu'il soit libéré hors du thread audio.
    SamplePtr setSample (int pad, SamplePtr s)
    {
        for (auto& v : voices)
            if (v.active && v.pad == pad) v.active = false;
        SamplePtr old = std::move (pads[(size_t) pad]);
        pads[(size_t) pad] = std::move (s);
        return old;
    }

    void setParams (const Params& p) { P = p; }
    void setKeyboardPad (int p) { kbPad = std::min (kPads - 1, std::max (0, p)); }

    void handle (const Ev& e)
    {
        switch (e.type)
        {
            case EV_PAD_ON:
                if (e.note >= 0 && e.note < kPads) trigger (e.note, e.value, 0.f, -1);
                break;
            case EV_PAD_OFF:
                for (auto& v : voices)
                    if (v.active && v.pad == e.note && v.key < 0) release (v, false);
                break;
            case EV_KEY_ON:
                trigger (kbPad, e.value, (float) (e.note - (int) P.g[G_ROOT]), e.note);
                break;
            case EV_KEY_OFF:
                for (auto& v : voices)
                    if (v.active && v.key == e.note) release (v, false);
                break;
            case EV_ALL_OFF:
                for (auto& v : voices)
                    if (v.active) release (v, true);
                break;
        }
    }

    int activeVoices() const
    {
        int n = 0;
        for (auto& v : voices) n += v.active ? 1 : 0;
        return n;
    }

    void render (float* L, float* R, int n)
    {
        std::fill (L, L + n, 0.f);
        std::fill (R, R + n, 0.f);
        for (auto& v : voices)
            if (v.active) renderVoice (v, L, R, n);
        applyFx (L, R, n);
    }

private:
    struct Voice
    {
        bool active = false;
        int pad = 0, key = -1, stage = 0; // 0 attack, 1 tenue, 2 release, 3 release rapide
        double pos = 0.0;
        float keySemis = 0.f, vel = 1.f, env = 0.f;
        uint32_t age = 0;
        const Sample* smp = nullptr;
        Svf fl, fr;
    };

    static float herm (const float* d, int N, int i, float t)
    {
        auto at = [&] (int k) { return d[std::min (N - 1, std::max (0, k))]; };
        const float y0 = at (i - 1), y1 = at (i), y2 = at (i + 1), y3 = at (i + 2);
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + y1;
    }

    void release (Voice& v, bool force)
    {
        if (force) { v.stage = 3; return; }
        if ((int) P.pad[v.pad][PK_MODE] == 0) return; // one-shot : joue jusqu'au bout
        if (v.stage < 2) v.stage = 2;
    }

    void trigger (int pad, float vel, float semis, int key)
    {
        if (pad < 0 || pad >= kPads || ! pads[(size_t) pad] || vel <= 0.f) return;

        const int cg = (int) P.pad[pad][PK_CHOKE];
        for (auto& v : voices)
        {
            if (! v.active) continue;
            const bool chokeSame = cg > 0 && (int) P.pad[v.pad][PK_CHOKE] == cg;
            const bool monoPad = key < 0 && v.pad == pad && v.key < 0; // un pad = une voix
            if (chokeSame || monoPad) v.stage = 3;
        }

        Voice* tgt = nullptr;
        for (auto& v : voices) if (! v.active) { tgt = &v; break; }
        if (! tgt)
        {
            uint32_t best = UINT32_MAX;
            for (auto& v : voices) if (v.age < best) { best = v.age; tgt = &v; }
        }

        const Sample* s = pads[(size_t) pad].get();
        const float N = (float) s->frames;
        const float startF = P.pad[pad][PK_START] * N;
        const float endF = std::max (startF + 32.f, P.pad[pad][PK_END] * N);
        const bool rev = P.pad[pad][PK_REV] > 0.5f;

        *tgt = Voice();
        tgt->active = true;
        tgt->pad = pad;
        tgt->key = key;
        tgt->keySemis = semis;
        tgt->vel = vel;
        tgt->smp = s;
        tgt->age = ++ageCounter;
        tgt->pos = rev ? (double) std::min (endF, N - 1.f) : (double) startF;
    }

    void renderVoice (Voice& v, float* L, float* R, int n)
    {
        const Sample& s = *v.smp;
        const float* pp = P.pad[v.pad];
        const int N = s.frames;
        const float* dL = s.L.data();
        const float* dR = s.R.empty() ? s.L.data() : s.R.data();

        const double startF = std::min ((double) pp[PK_START] * N, (double) (N - 32));
        const double endF = std::min ((double) N, std::max (startF + 32.0, (double) pp[PK_END] * N));
        const bool rev = pp[PK_REV] > 0.5f;
        const int mode = (int) pp[PK_MODE];
        const double ratio = (double) (s.sr / sr) * std::exp2 ((double) (pp[PK_TUNE] + v.keySemis) / 12.0);
        const float atkInc = 1.f / (std::max (pp[PK_ATK], 0.001f) * sr);
        const float relCoef = std::exp (-4.6f / (std::max (pp[PK_REL], 0.005f) * sr));
        const float fastCoef = std::exp (-4.6f / (0.008f * sr));
        const float ang = (pp[PK_PAN] * 0.5f + 0.5f) * kPi * 0.5f;
        const float gl = std::cos (ang) * 1.41421f, gr = std::sin (ang) * 1.41421f;
        const float gain = pp[PK_VOL] * v.vel;
        const bool useFilt = pp[PK_CUT] < 19000.f;
        const float fg = std::tan (kPi * std::min (pp[PK_CUT], 0.45f * sr) / sr);
        const float fR = 1.f - 0.97f * pp[PK_RES];

        for (int i = 0; i < n; ++i)
        {
            if (! rev)
            {
                if (v.pos >= endF)
                {
                    if (mode == 2) { v.pos -= (endF - startF); if (v.pos < startF) v.pos = startF; }
                    else { v.active = false; break; }
                }
            }
            else if (v.pos <= startF)
            {
                if (mode == 2) { v.pos += (endF - startF); if (v.pos > endF) v.pos = endF; }
                else { v.active = false; break; }
            }

            const int i0 = (int) std::floor (v.pos);
            const float t = (float) (v.pos - i0);
            const float sl = herm (dL, N, i0, t), srr = herm (dR, N, i0, t);

            float fade = 1.f;
            if (mode != 2)
            {
                const double rem = rev ? v.pos - startF : endF - v.pos;
                if (rem < 64.0) fade = (float) (rem / 64.0);
            }

            switch (v.stage)
            {
                case 0: v.env += atkInc; if (v.env >= 1.f) { v.env = 1.f; v.stage = 1; } break;
                case 2: v.env *= relCoef; if (v.env < 1e-4f) v.active = false; break;
                case 3: v.env *= fastCoef; if (v.env < 1e-4f) v.active = false; break;
                default: break;
            }
            if (! v.active) break;

            const float a = v.env * fade * gain;
            float l = sl * a, r = srr * a;
            if (useFilt) { l = v.fl.lp (l, fg, fR); r = v.fr.lp (r, fg, fR); }
            L[i] += l * gl;
            R[i] += r * gr;

            v.pos += rev ? -ratio : ratio;
        }
    }

    void applyFx (float* L, float* R, int n)
    {
        const float drive = P.g[G_DRIVE];
        const float k = 1.f + drive * 5.f, kn = 1.f / std::tanh (k);
        const float fb = P.g[G_DLYFB], mix = P.g[G_DLYMIX];
        const float target = std::min ((float) (dlySize - 4), std::max (2.f, P.g[G_DLYTIME] * 0.001f * sr));
        for (int i = 0; i < n; ++i)
        {
            float l = L[i], r = R[i];
            if (drive > 0.001f) { l = std::tanh (l * k) * kn; r = std::tanh (r * k) * kn; }

            dlyTimeCur += (target - dlyTimeCur) * 0.0008f;
            float rp = (float) dlyPos - dlyTimeCur;
            if (rp < 0.f) rp += (float) dlySize;
            const int i0 = (int) rp;
            const float fr = rp - (float) i0;
            const int i1 = (i0 + 1) % dlySize;
            const float wl = dlyL[(size_t) i0] * (1.f - fr) + dlyL[(size_t) i1] * fr;
            const float wr = dlyR[(size_t) i0] * (1.f - fr) + dlyR[(size_t) i1] * fr;
            dlyL[(size_t) dlyPos] = l + fb * wr; // ping-pong
            dlyR[(size_t) dlyPos] = r + fb * wl;
            dlyPos = (dlyPos + 1) % dlySize;

            L[i] = l + mix * wl;
            R[i] = r + mix * wr;
        }
    }

    Params P = Params::defaults();
    float sr = 44100.f;
    SamplePtr pads[kPads];
    Voice voices[kMaxVoices];
    int kbPad = 0;
    uint32_t ageCounter = 0;
    std::vector<float> dlyL, dlyR;
    int dlySize = 1, dlyPos = 0;
    float dlyTimeCur = 16537.f;
};
} // namespace sp
