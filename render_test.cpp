// Test hors-ligne du moteur : niveaux, NaN, justesse de hauteur, arpégiateur, WAV de démo.
#include "../Source/Arp.h"
#include "../Source/Presets.h"
#include <cstdio>
#include <cstring>
#include <string>

using namespace jv;

static Params applyPreset (const PresetDef& pd)
{
    Params p = Params::defaults();
    for (auto& kv : pd.v) p.v[kv.first] = kv.second;
    return p;
}

struct Result { float peak = 0, rms = 0; bool bad = false; std::vector<float> L, R; };

// Joue une séquence de notes [(note, debutSec, dureeSec)] avec le preset donné.
static Result play (const Params& p, const std::vector<std::tuple<int, double, double>>& notes, double seconds, double bpm = 120.0)
{
    const double sr = 44100.0;
    const int block = 512;
    Engine eng; eng.prepare (sr, block);
    Arpeggiator arp; arp.prepare (sr);
    ArpSettings as; as.mode = (int) p[P_ARPMODE]; as.rate = (int) p[P_ARPRATE]; as.octaves = (int) p[P_ARPOCT]; as.gate = p[P_ARPGATE]; as.hold = p[P_ARPHOLD] > 0.5f;
    const bool arpOn = p[P_ARPON] > 0.5f;

    Result r;
    const long total = (long) (seconds * sr);
    r.L.reserve (total); r.R.reserve (total);
    std::vector<float> L (block), R (block);
    std::vector<Ev> in, out, merged;
    double sumsq = 0;
    for (long start = 0; start < total; start += block)
    {
        in.clear(); out.clear();
        for (auto& [n, t0, d] : notes)
        {
            long on = (long) (t0 * sr), off = (long) ((t0 + d) * sr);
            if (on >= start && on < start + block) in.push_back ({ (int) (on - start), EV_NOTE_ON, n, 0.8f });
            if (off >= start && off < start + block) in.push_back ({ (int) (off - start), EV_NOTE_OFF, n, 0.f });
        }
        std::stable_sort (in.begin(), in.end(), [] (const Ev& a, const Ev& b) { return a.pos < b.pos; });
        const std::vector<Ev>* evs = &in;
        if (arpOn) { arp.process (in, out, block, as, bpm, false, -1.0); evs = &out; }
        eng.setParams (p);
        int cur = 0;
        for (auto& e : *evs)
        {
            if (e.pos > cur) { eng.render (L.data() + cur, R.data() + cur, e.pos - cur); cur = e.pos; }
            eng.handle (e);
        }
        if (cur < block) eng.render (L.data() + cur, R.data() + cur, block - cur);
        for (int i = 0; i < block; ++i)
        {
            if (! std::isfinite (L[i]) || ! std::isfinite (R[i])) r.bad = true;
            r.peak = std::max ({ r.peak, std::fabs (L[i]), std::fabs (R[i]) });
            sumsq += L[i] * L[i] + R[i] * R[i];
            r.L.push_back (L[i]); r.R.push_back (R[i]);
        }
    }
    r.rms = (float) std::sqrt (sumsq / (2.0 * total));
    return r;
}

static void writeWav (const char* path, const std::vector<float>& L, const std::vector<float>& R)
{
    FILE* f = fopen (path, "wb");
    const uint32_t sr = 44100, n = (uint32_t) L.size(), dataBytes = n * 4, riff = 36 + dataBytes;
    const uint16_t fmt = 1, ch = 2, bits = 16, align = 4; const uint32_t brate = sr * 4, sixteen = 16;
    fwrite ("RIFF", 1, 4, f); fwrite (&riff, 4, 1, f); fwrite ("WAVEfmt ", 1, 8, f); fwrite (&sixteen, 4, 1, f);
    fwrite (&fmt, 2, 1, f); fwrite (&ch, 2, 1, f); fwrite (&sr, 4, 1, f); fwrite (&brate, 4, 1, f);
    fwrite (&align, 2, 1, f); fwrite (&bits, 2, 1, f); fwrite ("data", 1, 4, f); fwrite (&dataBytes, 4, 1, f);
    for (uint32_t i = 0; i < n; ++i)
    {
        int16_t a = (int16_t) (std::max (-1.f, std::min (1.f, L[i])) * 32767), b = (int16_t) (std::max (-1.f, std::min (1.f, R[i])) * 32767);
        fwrite (&a, 2, 1, f); fwrite (&b, 2, 1, f);
    }
    fclose (f);
}

int main()
{
    int failures = 0;

    // 1) Justesse : La3 (69) doit donner ~440 Hz (passages par zéro, filtre grand ouvert, 1 seul osc.)
    {
        Params p = Params::defaults();
        p.v[P_MIX] = 0.f; p.v[P_CUTOFF] = 18000; p.v[P_ENVAMT] = 0; p.v[P_KEYFOL] = 0; p.v[P_O1WAVE] = 0; p.v[P_SPREAD] = 0;
        p.v[P_AA] = 0.001f; p.v[P_AS] = 1.f; p.v[P_FS] = 1.f; p.v[P_RES] = 0.f;
        auto r = play (p, { { 69, 0.0, 1.0 } }, 1.0);
        int zc = 0; for (size_t i = 22050; i + 1 < 44100; ++i) if (r.L[i] <= 0 && r.L[i + 1] > 0) ++zc;
        double f = zc / (22050.0 / 44100.0);
        printf ("Justesse La3: %.1f Hz (attendu 440)\n", f);
        if (std::fabs (f - 440.0) > 3.0) { ++failures; printf ("  ECHEC justesse\n"); }
    }

    // 2) Tous les presets : accord de 3 notes (ou notes tenues pour l'arp), pas de NaN, niveaux raisonnables
    printf ("\n%-24s %7s %7s\n", "Preset", "peak", "rms");
    const auto& list = presets();
    for (size_t i = 0; i < list.size(); ++i)
    {
        Params p = applyPreset (list[i]);
        auto r = play (p, { { 48, 0.1, 2.5 }, { 55, 0.1, 2.5 }, { 60, 0.1, 2.5 }, { 64, 0.1, 2.5 } }, 4.0);
        printf ("%-24s %7.3f %7.3f %s\n", list[i].name, r.peak, r.rms, r.bad ? "NaN!" : "");
        if (r.bad || r.peak > 1.0f || r.rms < 0.005f) { ++failures; printf ("  ECHEC niveau/NaN\n"); }
    }

    // 3) Arpégiateur : Up, 1/16 à 120 BPM => pas de 11025 samples ; notes C-E-G attendues
    {
        Arpeggiator arp; arp.prepare (44100.0);
        ArpSettings s; s.mode = 0; s.rate = 3; s.octaves = 1; s.gate = 0.5f;
        std::vector<Ev> in = { { 0, EV_NOTE_ON, 60, 1.f }, { 0, EV_NOTE_ON, 64, 1.f }, { 0, EV_NOTE_ON, 67, 1.f } }, out;
        arp.process (in, out, 44100, s, 120.0, false, -1.0);
        printf ("\nArp Up 1/16 @120 (1 s) :");
        int seq[8], k = 0; std::vector<int> ons;
        for (auto& e : out) if (e.type == EV_NOTE_ON) { ons.push_back (e.pos); seq[k++ % 8] = e.note; printf (" %d@%d", e.note, e.pos); }
        printf ("\n");
        const int expect[] = { 60, 64, 67, 60, 64, 67, 60, 64 };
        bool ok = ons.size() == 8 || ons.size() == 9;
        for (int i = 0; i < 8 && i < k; ++i) ok = ok && seq[i] == expect[i];
        if (! ok) { ++failures; printf ("  ECHEC arp\n"); }
    }

    // 4) Démos WAV
    auto demo = [&] (const char* presetName, const std::vector<std::tuple<int, double, double>>& notes, double secs, const char* file)
    {
        for (auto& pd : list) if (std::string (pd.name) == presetName)
        {
            auto r = play (applyPreset (pd), notes, secs);
            writeWav (file, r.L, r.R);
        }
    };
    std::vector<float> dL, dR;
    // Démo combinée
    struct D { const char* name; std::vector<std::tuple<int, double, double>> notes; double secs; };
    std::vector<D> ds = {
        { "Brass Stab (Jump)", { { 60, 0.0, 0.5 }, { 64, 0.0, 0.5 }, { 67, 0.0, 0.5 }, { 72, 0.0, 0.5 }, { 62, 0.9, 0.5 }, { 65, 0.9, 0.5 }, { 69, 0.9, 0.5 }, { 74, 0.9, 0.5 } }, 2.0 },
        { "Strings Ensemble", { { 57, 0.0, 2.2 }, { 60, 0.0, 2.2 }, { 64, 0.0, 2.2 } }, 3.2 },
        { "Hungry Arp (16th)", { { 57, 0.0, 3.8 }, { 60, 0.0, 3.8 }, { 64, 0.0, 3.8 } }, 4.0 },
        { "Resonant Pluck Bass", { { 36, 0.0, 0.2 }, { 36, 0.5, 0.2 }, { 39, 1.0, 0.2 }, { 41, 1.5, 0.4 } }, 2.4 },
        { "Sync Lead", { { 72, 0.0, 0.7 }, { 75, 0.8, 0.4 }, { 79, 1.3, 1.0 } }, 3.0 },
    };
    for (auto& d : ds)
        for (auto& pd : list) if (std::string (pd.name) == d.name)
        {
            auto r = play (applyPreset (pd), d.notes, d.secs);
            float pk = 0; for (float v : r.L) pk = std::max (pk, std::fabs (v));
            for (size_t i = 0; i < r.L.size(); ++i) { dL.push_back (r.L[i]); dR.push_back (r.R[i]); }
            dL.insert (dL.end(), 22050, 0.f); dR.insert (dR.end(), 22050, 0.f);
        }
    writeWav ("/home/claude/Jovian8/demo_presets.wav", dL, dR);
    (void) demo;

    printf ("\n%s (%d echec(s))\n", failures ? "TESTS EN ECHEC" : "TOUS LES TESTS OK", failures);
    return failures ? 1 : 0;
}
