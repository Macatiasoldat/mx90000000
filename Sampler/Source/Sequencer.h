#pragma once
// Séquenceur de pas : 8 patterns × 16 pads × 16 pas, synchronisé au transport de l'hôte
// (position en noires) ou en marche libre. C++ pur.
#include "SamplerEngine.h"
#include <array>
#include <atomic>

namespace sp
{
class Sequencer
{
public:
    static constexpr int kPatterns = 8, kSteps = 16;

    Sequencer()
    {
        for (auto& c : cells) c.store (0);
        pending_.reserve (2048);
    }

    // 0 = vide, 1 = normal, 2 = accent
    uint8_t get (int pat, int pad, int step) const { return cells[idx (pat, pad, step)].load (std::memory_order_relaxed); }
    void set (int pat, int pad, int step, uint8_t v) { cells[idx (pat, pad, step)].store (v, std::memory_order_relaxed); }
    void clearPattern (int pat) { for (int p = 0; p < kPads; ++p) for (int s = 0; s < kSteps; ++s) set (pat, p, s, 0); }

    std::atomic<int> displayStep { -1 };

    void reset() { running_ = false; mode_ = 0; pending_.clear(); displayStep.store (-1); }

    void process (std::vector<Ev>& out, int n, double sr, const Params& P, double bpm, bool hostPlaying, double ppq)
    {
        static const double beats[4] = { 0.5, 0.25, 1.0 / 6.0, 0.125 };
        const double stepBeats = beats[std::min (3, std::max (0, (int) P.g[G_SEQRATE]))];
        bpm = std::min (400.0, std::max (20.0, bpm));
        const double spb = sr * 60.0 / bpm;
        const double stepSamples = std::max (1.0, spb * stepBeats);

        const bool synced = P.g[G_SEQON] > 0.5f && hostPlaying && ppq >= 0.0;
        const bool freeRun = P.g[G_SEQFREE] > 0.5f;
        const int mode = synced ? 1 : (freeRun ? 2 : 0);

        if (mode == 0)
        {
            if (running_) stopAll (out);
            running_ = false; mode_ = 0;
            displayStep.store (-1);
            return;
        }

        if (mode == 2 && mode_ != 2) freePos_ = 0.0;
        const double s0 = mode == 1 ? ppq / stepBeats : freePos_;
        bool resync = ! running_ || mode != mode_;
        if (mode == 1 && ! resync && std::fabs (s0 - expected_) > 0.25) resync = true;
        if (resync)
        {
            stopAll (out);
            lastK_ = (long) std::ceil (s0 - 1e-6) - 1;
        }
        running_ = true;
        mode_ = mode;

        const int len = std::min (kSteps, std::max (1, (int) P.g[G_SEQLEN]));
        const int pat = std::min (kPatterns - 1, std::max (0, (int) P.g[G_PATTERN]));
        const double span = n / stepSamples;
        const double swing = P.g[G_SWING];
        const double gate = P.g[G_SEQGATE];

        for (long k = lastK_ + 1; (double) k < s0 + span - 1e-9; ++k)
        {
            double t = ((double) k - s0) * stepSamples;
            if (t < 0.0) t = 0.0;
            if (((k % 2) + 2) % 2 == 1) t += swing * stepSamples;
            const int step = (int) (((k % len) + len) % len);
            displayStep.store (step);
            for (int p = 0; p < kPads; ++p)
            {
                const uint8_t v = get (pat, p, step);
                if (v == 0) continue;
                pending_.push_back ({ t, { 0, EV_PAD_ON, p, v >= 2 ? 1.f : 0.72f } });
                pending_.push_back ({ t + gate * stepSamples, { 0, EV_PAD_OFF, p, 0.f } });
            }
            lastK_ = k;
        }

        // sortie des événements tombant dans ce bloc, report des autres
        size_t w = 0;
        for (size_t i = 0; i < pending_.size(); ++i)
        {
            Pending e = pending_[i];
            if (e.t < (double) n) { e.ev.pos = std::max (0, (int) e.t); out.push_back (e.ev); }
            else { e.t -= (double) n; pending_[w++] = e; }
        }
        pending_.resize (w);

        // tri par position (insertion, stable, sans allocation)
        for (size_t i = 1; i < out.size(); ++i)
        {
            Ev x = out[i];
            size_t j = i;
            while (j > 0 && out[j - 1].pos > x.pos) { out[j] = out[j - 1]; --j; }
            out[j] = x;
        }

        expected_ = s0 + span;
        if (mode == 2) freePos_ = s0 + span;
    }

private:
    struct Pending { double t; Ev ev; };

    static size_t idx (int pat, int pad, int step) { return (size_t) ((pat * kPads + pad) * kSteps + step); }

    void stopAll (std::vector<Ev>& out)
    {
        pending_.clear();
        for (int p = 0; p < kPads; ++p) out.push_back ({ 0, EV_PAD_OFF, p, 0.f });
    }

    std::array<std::atomic<uint8_t>, (size_t) (kPatterns * kPads * kSteps)> cells;
    std::vector<Pending> pending_;
    bool running_ = false;
    int mode_ = 0;
    long lastK_ = -1;
    double expected_ = 0.0, freePos_ = 0.0;
};
} // namespace sp
