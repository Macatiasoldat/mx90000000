#pragma once
// Arpégiateur synchronisé au tempo de l'hôte (C++ pur).
#include "Engine.h"

namespace jv
{
struct ArpSettings
{
    int mode = 0;      // 0 Up, 1 Down, 2 Up/Down, 3 Random, 4 Played
    int rate = 3;      // 0 1/4, 1 1/8, 2 1/8T, 3 1/16, 4 1/16T, 5 1/32
    int octaves = 1;
    float gate = 0.5f;
    bool hold = false;
};

class Arpeggiator
{
public:
    Arpeggiator()
    {
        notes_.reserve (128);
        base_.reserve (128);
        seq_.reserve (128 * 4 + 8);
    }

    void prepare (double sampleRate) { sr_ = sampleRate; reset(); }

    void reset()
    {
        notes_.clear();
        counter_ = 0;
        sounding_ = -1;
        gateLeft_ = 0.0;
        nextStep_ = 0.0;
        holdWas_ = false;
    }

    // Consomme les événements de notes de `in` (triés par position) et produit les notes arpégées dans `out`.
    void process (const std::vector<Ev>& in, std::vector<Ev>& out, int n, const ArpSettings& s,
                  double bpm, bool hostPlaying, double ppq)
    {
        static const double beats[6] = { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };
        bpm = std::min (400.0, std::max (20.0, bpm));
        const double spb = sr_ * 60.0 / bpm;
        const double stepBeats = beats[std::min (5, std::max (0, s.rate))];
        const double stepSamples = std::max (1.0, spb * stepBeats);

        if (holdWas_ && ! s.hold)
            notes_.erase (std::remove_if (notes_.begin(), notes_.end(), [] (const NoteInfo& x) { return ! x.down; }), notes_.end());
        holdWas_ = s.hold;

        size_t e = 0;
        for (int t = 0; t < n; ++t)
        {
            while (e < in.size() && in[e].pos <= t)
            {
                const Ev& ev = in[e++];
                if (ev.type == EV_NOTE_ON)
                {
                    bool anyDown = false;
                    for (auto& x : notes_) anyDown = anyDown || x.down;
                    if (s.hold && ! anyDown) notes_.clear();
                    const bool wasEmpty = notes_.empty();
                    bool found = false;
                    for (auto& x : notes_) if (x.note == ev.note) { x.down = true; x.vel = ev.value; found = true; }
                    if (! found) notes_.push_back ({ ev.note, ev.value, true });
                    if (wasEmpty)
                    {
                        counter_ = 0;
                        nextStep_ = 0.0;
                        if (hostPlaying && ppq >= 0.0)
                        {
                            const double pos = (ppq + t / spb) / stepBeats;
                            const double frac = pos - std::floor (pos);
                            nextStep_ = frac < 1e-3 ? 0.0 : (1.0 - frac) * stepSamples;
                        }
                    }
                }
                else if (ev.type == EV_NOTE_OFF)
                {
                    for (auto& x : notes_) if (x.note == ev.note) x.down = false;
                    if (! s.hold)
                        notes_.erase (std::remove_if (notes_.begin(), notes_.end(), [] (const NoteInfo& x) { return ! x.down; }), notes_.end());
                }
                else if (ev.type == EV_ALL_OFF)
                    notes_.clear();

                if (notes_.empty() && sounding_ >= 0)
                {
                    out.push_back ({ t, EV_NOTE_OFF, sounding_, 0.f });
                    sounding_ = -1;
                }
            }

            if (sounding_ >= 0)
            {
                if (gateLeft_ <= 0.0) { out.push_back ({ t, EV_NOTE_OFF, sounding_, 0.f }); sounding_ = -1; }
                else gateLeft_ -= 1.0;
            }

            if (! notes_.empty())
            {
                if (nextStep_ <= 0.0)
                {
                    fire (t, out, s, stepSamples);
                    nextStep_ += stepSamples;
                }
                nextStep_ -= 1.0;
            }
        }
    }

private:
    struct NoteInfo { int note; float vel; bool down; };
    struct SeqItem { int note; float vel; };

    uint32_t rnd()
    {
        rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5;
        return rng_;
    }

    void fire (int t, std::vector<Ev>& out, const ArpSettings& s, double stepSamples)
    {
        base_.clear();
        for (auto& x : notes_) base_.push_back ({ x.note, x.vel });
        if (s.mode != 4)
            std::sort (base_.begin(), base_.end(), [] (const SeqItem& a, const SeqItem& b) { return a.note < b.note; });

        seq_.clear();
        const int oct = std::min (4, std::max (1, s.octaves));
        for (int o = 0; o < oct; ++o)
            for (auto& b : base_)
                if (b.note + 12 * o <= 127) seq_.push_back ({ b.note + 12 * o, b.vel });
        if (seq_.empty()) return;

        if (s.mode == 1)
            std::reverse (seq_.begin(), seq_.end());
        else if (s.mode == 2 && seq_.size() > 2)
        {
            const size_t sz = seq_.size();
            for (size_t i = sz - 2; i >= 1; --i) { seq_.push_back (seq_[i]); if (i == 1) break; }
        }

        size_t idx;
        if (s.mode == 3) idx = rnd() % seq_.size();
        else idx = (size_t) (counter_++) % seq_.size();

        const SeqItem pick = seq_[idx];
        if (sounding_ >= 0) out.push_back ({ t, EV_NOTE_OFF, sounding_, 0.f });
        out.push_back ({ t, EV_NOTE_ON, pick.note, pick.vel });
        sounding_ = pick.note;
        gateLeft_ = std::max (1.0, (double) std::min (0.98f, std::max (0.05f, s.gate)) * stepSamples);
    }

    double sr_ = 44100.0, nextStep_ = 0.0, gateLeft_ = 0.0;
    int sounding_ = -1;
    uint64_t counter_ = 0;
    bool holdWas_ = false;
    uint32_t rng_ = 88172645u;
    std::vector<NoteInfo> notes_;
    std::vector<SeqItem> base_, seq_;
};
} // namespace jv
