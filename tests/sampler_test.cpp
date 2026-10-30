// Test hors-ligne du sampler : justesse, modes de lecture, choke, séquenceur, démo WAV.
// Les sons de test sont synthétisés ici (originaux), aucun son externe.
#include "../Source/Sequencer.h"
#include <cstdio>
#include <functional>

using namespace sp;

static int failures = 0;
#define CHECK(cond, msg) do { if (! (cond)) { ++failures; printf ("  ECHEC: %s\n", msg); } } while (0)

static SamplePtr makeSine (double freq, double secs, double sr = 44100.0, const char* name = "sine")
{
    auto s = std::make_shared<Sample>();
    s->sr = sr; s->frames = (int) (secs * sr); s->name = name;
    s->L.resize ((size_t) s->frames);
    for (int i = 0; i < s->frames; ++i) s->L[(size_t) i] = 0.8f * (float) std::sin (2.0 * 3.14159265358979 * freq * i / sr);
    return s;
}

static SamplePtr makeKick (double sr = 44100.0)
{
    auto s = std::make_shared<Sample>();
    s->sr = sr; s->frames = (int) (0.45 * sr); s->name = "kick";
    s->L.resize ((size_t) s->frames);
    double ph = 0;
    for (int i = 0; i < s->frames; ++i)
    {
        const double t = i / sr, f = 45.0 + 110.0 * std::exp (-t * 28.0);
        ph += 2.0 * 3.14159265358979 * f / sr;
        s->L[(size_t) i] = (float) (std::sin (ph) * std::exp (-t * 7.0) * 0.95);
    }
    return s;
}

static SamplePtr makeNoise (double secs, double decay, bool highpass, const char* name, double sr = 44100.0)
{
    auto s = std::make_shared<Sample>();
    s->sr = sr; s->frames = (int) (secs * sr); s->name = name;
    s->L.resize ((size_t) s->frames);
    uint32_t r = 12345; float prev = 0;
    for (int i = 0; i < s->frames; ++i)
    {
        r ^= r << 13; r ^= r >> 17; r ^= r << 5;
        float n = ((r & 0xFFFF) / 32768.f - 1.f);
        float y = highpass ? n - prev : n; prev = n;
        s->L[(size_t) i] = y * (float) std::exp (-(i / sr) * decay) * 0.5f;
    }
    return s;
}

struct Rig
{
    Engine eng;
    Sequencer seq;
    Params P = Params::defaults();
    std::vector<float> L, R;
    float peak = 0; bool bad = false;

    Rig() { eng.prepare (44100.0); }

    // events : (échantillon absolu, Ev)
    void run (double seconds, std::vector<std::pair<long, Ev>> evs, bool useSeq = false, double bpm = 120.0, bool hostPlaying = false, double ppq0 = 0.0)
    {
        const int block = 512;
        const long total = (long) (seconds * 44100.0);
        std::vector<float> bl (block), br (block);
        std::vector<Ev> seqEv, in, merged;
        for (long start = 0; start < total; start += block)
        {
            in.clear(); seqEv.clear(); merged.clear();
            for (auto& [t, e] : evs)
                if (t >= start && t < start + block) { Ev x = e; x.pos = (int) (t - start); in.push_back (x); }
            if (useSeq)
            {
                double ppq = hostPlaying ? ppq0 + (double) start / (44100.0 * 60.0 / bpm) : -1.0;
                seq.process (seqEv, block, 44100.0, P, bpm, hostPlaying, ppq);
            }
            // fusion triée
            size_t a = 0, b = 0;
            while (a < in.size() || b < seqEv.size())
            {
                if (b >= seqEv.size() || (a < in.size() && in[a].pos <= seqEv[b].pos)) merged.push_back (in[a++]);
                else merged.push_back (seqEv[b++]);
            }
            eng.setParams (P);
            int cur = 0;
            for (auto& e : merged)
            {
                if (e.pos > cur) { eng.render (bl.data() + cur, br.data() + cur, e.pos - cur); cur = e.pos; }
                eng.handle (e);
            }
            if (cur < block) eng.render (bl.data() + cur, br.data() + cur, block - cur);
            for (int i = 0; i < block; ++i)
            {
                if (! std::isfinite (bl[(size_t) i]) || ! std::isfinite (br[(size_t) i])) bad = true;
                peak = std::max ({ peak, std::fabs (bl[(size_t) i]), std::fabs (br[(size_t) i]) });
                L.push_back (bl[(size_t) i]); R.push_back (br[(size_t) i]);
            }
        }
    }
};

static double freqOf (const std::vector<float>& x, long a, long b)
{
    int zc = 0;
    for (long i = a; i + 1 < b; ++i) if (x[(size_t) i] <= 0 && x[(size_t) i + 1] > 0) ++zc;
    return zc / ((double) (b - a) / 44100.0);
}
static float rmsOf (const std::vector<float>& x, long a, long b)
{
    double s = 0; for (long i = a; i < b && i < (long) x.size(); ++i) s += x[(size_t) i] * x[(size_t) i];
    return (float) std::sqrt (s / std::max<long> (1, b - a));
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
    // 1) Justesse en mode pad : sinus 440 Hz -> 440 Hz
    {
        Rig r; r.P.pad[0][PK_MODE] = 1; r.eng.setSample (0, makeSine (440, 2.0));
        r.run (1.0, { { 0, { 0, EV_PAD_ON, 0, 1.f } } });
        const double f = freqOf (r.L, 4410, 40000);
        printf ("Pad 440 Hz : %.1f Hz\n", f);
        CHECK (std::fabs (f - 440.0) < 3.0, "justesse pad");
    }
    // 2) Mode clavier : racine 60, touche 72 -> 880 Hz ; touche 48 -> 220 Hz
    {
        Rig r; r.P.pad[0][PK_MODE] = 1; r.P.g[G_KBMODE] = 1.f; r.P.g[G_ROOT] = 60.f;
        r.eng.setSample (0, makeSine (440, 3.0)); r.eng.setKeyboardPad (0);
        r.run (1.0, { { 0, { 0, EV_KEY_ON, 72, 1.f } } });
        const double f = freqOf (r.L, 4410, 40000);
        Rig r2; r2.P.pad[0][PK_MODE] = 1; r2.eng.setSample (0, makeSine (440, 3.0)); r2.eng.setKeyboardPad (0);
        r2.run (1.0, { { 0, { 0, EV_KEY_ON, 48, 1.f } } });
        const double f2 = freqOf (r2.L, 4410, 40000);
        printf ("Clavier +12 : %.1f Hz | -12 : %.1f Hz\n", f, f2);
        CHECK (std::fabs (f - 880.0) < 6.0, "octave haut");
        CHECK (std::fabs (f2 - 220.0) < 3.0, "octave bas");
    }
    // 3) One-shot ignore le relâchement ; Gate s'arrête ; Boucle tient
    {
        Rig a; a.P.pad[0][PK_MODE] = 0; a.eng.setSample (0, makeSine (300, 0.5));
        a.run (0.8, { { 0, { 0, EV_PAD_ON, 0, 1.f } }, { 4410, { 0, EV_PAD_OFF, 0, 0.f } } });
        Rig b; b.P.pad[0][PK_MODE] = 1; b.eng.setSample (0, makeSine (300, 0.5));
        b.run (0.8, { { 0, { 0, EV_PAD_ON, 0, 1.f } }, { 4410, { 0, EV_PAD_OFF, 0, 0.f } } });
        Rig c; c.P.pad[0][PK_MODE] = 2; c.eng.setSample (0, makeSine (300, 0.1));
        c.run (1.5, { { 0, { 0, EV_PAD_ON, 0, 1.f } }, { 44100, { 0, EV_PAD_OFF, 0, 0.f } } });
        printf ("One-shot rms@0.3s=%.3f | Gate rms@0.3s=%.4f | Boucle rms@0.8s=%.3f rms@1.4s=%.4f\n",
                rmsOf (a.L, 13000, 14000), rmsOf (b.L, 13000, 14000), rmsOf (c.L, 35000, 36000), rmsOf (c.L, 61000, 62000));
        CHECK (rmsOf (a.L, 13000, 14000) > 0.3f, "one-shot doit continuer");
        CHECK (rmsOf (b.L, 13000, 14000) < 0.01f, "gate doit se taire");
        CHECK (rmsOf (c.L, 35000, 36000) > 0.3f, "boucle doit tenir");
        CHECK (rmsOf (c.L, 61000, 62000) < 0.01f, "boucle doit s'arrêter après relâchement");
        CHECK (! a.bad && ! b.bad && ! c.bad, "NaN");
    }
    // 4) Choke : le charley ouvert (pad 1) est coupé par le charley fermé (pad 2), même groupe
    {
        Rig r; r.P.pad[1][PK_CHOKE] = 1; r.P.pad[2][PK_CHOKE] = 1;
        r.eng.setSample (1, makeSine (500, 2.0)); r.eng.setSample (2, makeSine (900, 0.05));
        r.run (1.0, { { 0, { 0, EV_PAD_ON, 1, 1.f } }, { 22050, { 0, EV_PAD_ON, 2, 1.f } } });
        printf ("Choke rms avant=%.3f apres=%.4f\n", rmsOf (r.L, 20000, 22000), rmsOf (r.L, 30000, 32000));
        CHECK (rmsOf (r.L, 20000, 22000) > 0.3f, "pad 1 sonne");
        CHECK (rmsOf (r.L, 30000, 32000) < 0.02f, "pad 1 coupé");
    }
    // 5) Reverse + début/fin : rampe montante lue à l'envers
    {
        auto s = std::make_shared<Sample>(); s->sr = 44100; s->frames = 44100; s->name = "ramp";
        s->L.resize (44100); for (int i = 0; i < 44100; ++i) s->L[(size_t) i] = (float) i / 44100.f;
        Rig r; r.P.pad[0][PK_MODE] = 1; r.P.pad[0][PK_REV] = 1.f; r.P.pad[0][PK_VOL] = 1.f; r.P.pad[0][PK_ATK] = 0.001f;
        r.eng.setSample (0, s);
        r.run (0.5, { { 0, { 0, EV_PAD_ON, 0, 1.f } } });
        const float early = r.L[2000], late = r.L[20000];
        printf ("Reverse : debut=%.3f plus tard=%.3f (doit décroître)\n", early, late);
        CHECK (early > late && early > 0.5f, "reverse");
    }
    // 6) Séquenceur libre : kick sur pas 0,4,8,12 à 120 BPM, 1/16 -> 0, 22050, 44100...
    {
        Rig r; r.P.g[G_SEQFREE] = 1.f; r.P.g[G_SEQRATE] = 1.f; r.P.pad[0][PK_MODE] = 1;
        r.eng.setSample (0, makeKick());
        for (int s : { 0, 4, 8, 12 }) r.seq.set (0, 0, s, 1);
        std::vector<Ev> evs; std::vector<long> ons;
        {
            Sequencer q; for (int s : { 0, 4, 8, 12 }) q.set (0, 0, s, 1);
            long off = 0;
            for (int b = 0; b < 8 * 43; ++b)
            {
                evs.clear(); q.process (evs, 512, 44100.0, r.P, 120.0, false, -1.0);
                for (auto& e : evs) if (e.type == EV_PAD_ON) ons.push_back (off + e.pos);
                off += 512;
            }
        }
        printf ("Seq libre :"); for (size_t i = 0; i < 6 && i < ons.size(); ++i) printf (" %ld", ons[i]); printf ("\n");
        CHECK (ons.size() >= 4 && ons[0] == 0 && std::labs (ons[1] - 22050) <= 1 && std::labs (ons[2] - 44100) <= 1, "positions du séquenceur");
        // swing : un pas impair décalé
        Sequencer q2; q2.set (0, 0, 1, 1); Params P2 = r.P; P2.g[G_SWING] = 0.5f;
        std::vector<Ev> e2; long first = -1, off = 0;
        for (int b = 0; b < 40 && first < 0; ++b) { e2.clear(); q2.process (e2, 512, 44100.0, P2, 120.0, false, -1.0); for (auto& e : e2) if (e.type == EV_PAD_ON && first < 0) first = off + e.pos; off += 512; }
        printf ("Swing 50%% : pas 1 à %ld (attendu ~%ld)\n", first, (long) (5512.5 + 0.5 * 5512.5));
        CHECK (std::labs (first - (long) (5512.5 * 1.5)) <= 2, "swing");
    }
    // 7) Séquenceur synchronisé au transport : ppq 1.0 => pas 4 (1/16) déclenché pile
    {
        Sequencer q; q.set (0, 3, 4, 2); Params P = Params::defaults(); P.g[G_SEQON] = 1.f;
        std::vector<Ev> e; long found = -1;
        // bloc commençant à ppq=0.95 ; le pas 4 est à ppq=1.0 : 0.05 noire = 1102.5 samples @120
        e.clear(); q.process (e, 2048, 44100.0, P, 120.0, true, 0.95);
        for (auto& x : e) if (x.type == EV_PAD_ON) found = x.pos;
        printf ("Sync transport : pas à %ld (attendu ~1102)\n", found);
        CHECK (std::labs (found - 1102) <= 2, "sync ppq");
    }

    // 8) Démo : kit synthétisé (original) + basse, séquenceur libre à 96 BPM avec swing, delay et reverb absente
    {
        Rig r; r.P.g[G_SEQFREE] = 1.f; r.P.g[G_SEQRATE] = 1.f; r.P.g[G_SWING] = 0.22f; r.P.g[G_DLYMIX] = 0.18f; r.P.g[G_DLYTIME] = 312.f; r.P.g[G_DLYFB] = 0.4f;
        r.P.pad[0][PK_MODE] = 1; r.P.pad[1][PK_MODE] = 1; r.P.pad[2][PK_MODE] = 1; r.P.pad[3][PK_MODE] = 1;
        r.P.pad[2][PK_CHOKE] = 1; r.P.pad[3][PK_CHOKE] = 1;
        r.P.pad[3][PK_PAN] = 0.4f; r.P.pad[2][PK_PAN] = -0.3f; r.P.pad[4][PK_PAN] = 0.1f;
        r.eng.setSample (0, makeKick());
        r.eng.setSample (1, makeNoise (0.25, 18.0, false, "snare"));
        r.eng.setSample (2, makeNoise (0.06, 60.0, true, "hat-closed"));
        r.eng.setSample (3, makeNoise (0.4, 9.0, true, "hat-open"));
        r.eng.setSample (4, makeSine (55, 0.6, 44100.0, "bass"));
        r.P.pad[4][PK_MODE] = 1; r.P.pad[4][PK_REL] = 0.08f; r.P.pad[4][PK_TUNE] = 0.f; r.P.pad[4][PK_CUT] = 900.f;
        for (int s : { 0, 6, 8, 11 }) r.seq.set (0, 0, s, s == 0 ? 2 : 1);
        for (int s : { 4, 12 }) r.seq.set (0, 1, s, 2);
        for (int s = 0; s < 16; s += 2) r.seq.set (0, 2, s, (s % 4 == 0) ? 1 : 2);
        r.seq.set (0, 3, 14, 1); r.seq.set (0, 2, 14, 0);
        for (int s : { 0, 3, 8, 10 }) r.seq.set (0, 4, s, 1);
        r.run (10.0, {}, true, 96.0);
        // étage de sortie du plugin : master + limiteur doux (tanh)
        float pk = 0;
        for (size_t i = 0; i < r.L.size(); ++i)
        {
            r.L[i] = std::tanh (r.L[i] * r.P.g[G_MASTER]); r.R[i] = std::tanh (r.R[i] * r.P.g[G_MASTER]);
            pk = std::max ({ pk, std::fabs (r.L[i]), std::fabs (r.R[i]) });
        }
        printf ("\nDemo : peak brut=%.3f, apres master+tanh=%.3f %s\n", r.peak, pk, r.bad ? "NaN!" : "");
        CHECK (! r.bad && pk < 1.0f && pk > 0.2f, "niveau démo");
        writeWav ("/home/claude/Jovian8/demo_sampler.wav", r.L, r.R);
    }

    printf ("\n%s (%d echec(s))\n", failures ? "TESTS EN ECHEC" : "TOUS LES TESTS OK", failures);
    return failures ? 1 : 0;
}
