#pragma once
// Presets inspirés du caractère des sons les plus célèbres des synthés polyphoniques
// analogiques des années 80 (cuivres, cordes, nappes, basses, leads, arpèges).
// Ce sont des recréations originales, pas des copies de sons d'usine.
#include "Params.h"
#include <utility>
#include <vector>

namespace jv
{
struct PresetDef
{
    const char* name;
    std::vector<std::pair<int, float>> v; // (paramètre, valeur réelle) ; le reste = valeurs par défaut
};

inline const std::vector<PresetDef>& presets()
{
    // Rappel : ondes 0 Tri 1 Saw 2 Pulse 3 Square | pieds 0 16' 1 8' 2 4' 3 2' | pente 0 12 dB 1 24 dB
    // Arp mode 0 Up 1 Down 2 UpDown 3 Random 4 Played | rate 0 1/4 1 1/8 2 1/8T 3 1/16 4 1/16T 5 1/32
    static const std::vector<PresetDef> list = {
        { "Init Saw", {} },

        { "Brass Stab (Jump)",
          { { P_O1WAVE, 1 }, { P_O2WAVE, 1 }, { P_O2FINE, 7 }, { P_MIX, 0.5f }, { P_CUTOFF, 1100 }, { P_RES, 0.12f },
            { P_ENVAMT, 0.55f }, { P_KEYFOL, 0.6f }, { P_FA, 0.09f }, { P_FD, 0.45f }, { P_FS, 0.4f }, { P_FR, 0.25f },
            { P_AA, 0.025f }, { P_AD, 0.4f }, { P_AS, 0.85f }, { P_AR, 0.18f }, { P_CHORUS, 1 }, { P_VELSENS, 0.4f } } },

        { "Soft Brass Swell",
          { { P_O1WAVE, 1 }, { P_O2WAVE, 1 }, { P_O2FINE, 5 }, { P_CUTOFF, 800 }, { P_ENVAMT, 0.45f }, { P_FA, 0.22f },
            { P_FD, 0.6f }, { P_FS, 0.5f }, { P_FR, 0.4f }, { P_AA, 0.12f }, { P_AS, 0.9f }, { P_AR, 0.35f }, { P_CHORUS, 1 },
            { P_LFOPITCH, 0.08f }, { P_LFORATE, 5.2f }, { P_LFODELAY, 0.8f } } },

        { "Strings Ensemble",
          { { P_O1WAVE, 1 }, { P_O2WAVE, 1 }, { P_O2FINE, 11 }, { P_HPF, 180 }, { P_CUTOFF, 5200 }, { P_RES, 0.05f },
            { P_ENVAMT, 0.1f }, { P_AA, 0.55f }, { P_AD, 0.5f }, { P_AS, 1.f }, { P_AR, 0.9f }, { P_FA, 0.6f },
            { P_FS, 1.f }, { P_CHORUS, 2 }, { P_SPREAD, 0.9f }, { P_LFOPITCH, 0.06f }, { P_LFORATE, 5.8f } } },

        { "Warm Pad",
          { { P_O1WAVE, 1 }, { P_O2WAVE, 2 }, { P_PW, 0.4f }, { P_PWM, 0.35f }, { P_LFORATE, 0.4f }, { P_O2FINE, 6 },
            { P_CUTOFF, 2400 }, { P_RES, 0.12f }, { P_ENVAMT, 0.2f }, { P_AA, 1.2f }, { P_AD, 1.f }, { P_AS, 1.f },
            { P_AR, 1.6f }, { P_FA, 1.4f }, { P_FS, 0.9f }, { P_CHORUS, 2 }, { P_SPREAD, 0.9f } } },

        { "PWM Pad",
          { { P_O1WAVE, 2 }, { P_O2WAVE, 2 }, { P_O2FINE, 8 }, { P_PW, 0.5f }, { P_PWM, 0.7f }, { P_LFORATE, 0.6f },
            { P_CUTOFF, 3200 }, { P_RES, 0.1f }, { P_ENVAMT, 0.15f }, { P_AA, 0.8f }, { P_AS, 1.f }, { P_AR, 1.2f },
            { P_FA, 0.9f }, { P_FS, 0.9f }, { P_CHORUS, 2 }, { P_SPREAD, 1.f } } },

        { "Sync Lead",
          { { P_POLY, 2 }, { P_GLIDE, 0.08f }, { P_O1WAVE, 1 }, { P_O2WAVE, 1 }, { P_SYNC, 1 }, { P_O2SEMI, 7 }, { P_MIX, 0.7f },
            { P_CUTOFF, 5500 }, { P_RES, 0.2f }, { P_ENVAMT, 0.25f }, { P_FD, 0.8f }, { P_FS, 0.6f }, { P_AA, 0.005f },
            { P_AS, 0.9f }, { P_AR, 0.2f }, { P_LFOPITCH, 0.15f }, { P_LFORATE, 5.5f }, { P_LFODELAY, 0.6f }, { P_CHORUS, 1 } } },

        { "Fat Unison Lead",
          { { P_POLY, 1 }, { P_UNIDET, 0.35f }, { P_GLIDE, 0.05f }, { P_O1WAVE, 1 }, { P_O2WAVE, 1 }, { P_O2FINE, 4 },
            { P_CUTOFF, 3500 }, { P_RES, 0.25f }, { P_ENVAMT, 0.3f }, { P_FD, 0.6f }, { P_FS, 0.55f }, { P_AA, 0.01f },
            { P_AS, 0.9f }, { P_AR, 0.25f }, { P_LFOPITCH, 0.1f }, { P_LFODELAY, 0.7f }, { P_CHORUS, 1 }, { P_SPREAD, 0.8f } } },

        { "Sub Bass",
          { { P_POLY, 2 }, { P_O1WAVE, 3 }, { P_O1RANGE, 0 }, { P_O2WAVE, 1 }, { P_O2RANGE, 0 }, { P_MIX, 0.4f },
            { P_CUTOFF, 600 }, { P_RES, 0.3f }, { P_ENVAMT, 0.4f }, { P_KEYFOL, 0.3f }, { P_FA, 0.001f }, { P_FD, 0.25f },
            { P_FS, 0.2f }, { P_FR, 0.1f }, { P_AA, 0.003f }, { P_AS, 1.f }, { P_AR, 0.1f } } },

        { "Resonant Pluck Bass",
          { { P_POLY, 2 }, { P_O1WAVE, 1 }, { P_O1RANGE, 0 }, { P_O2WAVE, 2 }, { P_O2RANGE, 1 }, { P_PW, 0.35f },
            { P_MIX, 0.4f }, { P_CUTOFF, 300 }, { P_RES, 0.5f }, { P_ENVAMT, 0.75f }, { P_KEYFOL, 0.4f }, { P_FA, 0.001f },
            { P_FD, 0.18f }, { P_FS, 0.f }, { P_FR, 0.12f }, { P_AA, 0.002f }, { P_AD, 0.35f }, { P_AS, 0.7f }, { P_AR, 0.1f } } },

        { "Poly Pluck",
          { { P_O1WAVE, 1 }, { P_O2WAVE, 2 }, { P_O2FINE, 5 }, { P_CUTOFF, 900 }, { P_RES, 0.2f }, { P_ENVAMT, 0.6f },
            { P_FA, 0.001f }, { P_FD, 0.25f }, { P_FS, 0.f }, { P_FR, 0.2f }, { P_AA, 0.002f }, { P_AD, 0.4f },
            { P_AS, 0.f }, { P_AR, 0.3f }, { P_CHORUS, 1 } } },

        { "Glass Bell",
          { { P_O1WAVE, 0 }, { P_O2WAVE, 0 }, { P_O2RANGE, 3 }, { P_O2FINE, 3 }, { P_MIX, 0.45f }, { P_XMOD, 0.35f },
            { P_CUTOFF, 12000 }, { P_RES, 0.05f }, { P_ENVAMT, 0.f }, { P_AA, 0.002f }, { P_AD, 1.8f }, { P_AS, 0.f },
            { P_AR, 1.6f }, { P_CHORUS, 2 }, { P_SPREAD, 0.8f } } },

        { "Hungry Arp (16th)",
          { { P_ARPON, 1 }, { P_ARPMODE, 0 }, { P_ARPRATE, 3 }, { P_ARPOCT, 2 }, { P_ARPGATE, 0.5f },
            { P_O1WAVE, 1 }, { P_O2WAVE, 2 }, { P_PW, 0.4f }, { P_O2FINE, 6 }, { P_CUTOFF, 1400 }, { P_RES, 0.25f },
            { P_ENVAMT, 0.5f }, { P_FD, 0.22f }, { P_FS, 0.1f }, { P_FR, 0.2f }, { P_AA, 0.002f }, { P_AD, 0.3f },
            { P_AS, 0.6f }, { P_AR, 0.2f }, { P_CHORUS, 1 } } },

        { "Arp Pad Up/Down",
          { { P_ARPON, 1 }, { P_ARPMODE, 2 }, { P_ARPRATE, 1 }, { P_ARPOCT, 2 }, { P_ARPGATE, 0.8f },
            { P_O1WAVE, 1 }, { P_O2WAVE, 1 }, { P_O2FINE, 9 }, { P_CUTOFF, 2800 }, { P_ENVAMT, 0.2f },
            { P_AA, 0.05f }, { P_AS, 0.9f }, { P_AR, 0.7f }, { P_CHORUS, 2 }, { P_SPREAD, 0.9f } } },

        { "Arp Sci-Fi Random",
          { { P_ARPON, 1 }, { P_ARPMODE, 3 }, { P_ARPRATE, 4 }, { P_ARPOCT, 3 }, { P_ARPGATE, 0.4f },
            { P_O1WAVE, 0 }, { P_O2WAVE, 2 }, { P_O2RANGE, 2 }, { P_MIX, 0.4f }, { P_XMOD, 0.2f }, { P_CUTOFF, 3000 },
            { P_RES, 0.4f }, { P_ENVAMT, 0.4f }, { P_FD, 0.3f }, { P_FS, 0.2f }, { P_AD, 0.35f }, { P_AS, 0.4f },
            { P_AR, 0.5f }, { P_CHORUS, 2 } } },

        { "Arp Bass Sequence",
          { { P_ARPON, 1 }, { P_ARPMODE, 4 }, { P_ARPRATE, 3 }, { P_ARPOCT, 1 }, { P_ARPGATE, 0.55f },
            { P_O1WAVE, 1 }, { P_O1RANGE, 0 }, { P_O2WAVE, 2 }, { P_O2RANGE, 1 }, { P_MIX, 0.4f }, { P_CUTOFF, 350 },
            { P_RES, 0.45f }, { P_ENVAMT, 0.7f }, { P_FA, 0.001f }, { P_FD, 0.16f }, { P_FS, 0.f }, { P_AD, 0.25f },
            { P_AS, 0.5f }, { P_AR, 0.1f } } },

        { "Choir Voices",
          { { P_O1WAVE, 2 }, { P_PW, 0.3f }, { P_O2WAVE, 1 }, { P_MIX, 0.4f }, { P_HPF, 300 }, { P_CUTOFF, 1800 },
            { P_RES, 0.35f }, { P_ENVAMT, 0.1f }, { P_AA, 0.4f }, { P_AS, 1.f }, { P_AR, 0.8f }, { P_FA, 0.5f },
            { P_FS, 0.9f }, { P_CHORUS, 2 }, { P_LFOPITCH, 0.05f }, { P_LFORATE, 5.f }, { P_LFODELAY, 1.f } } },

        { "Electric Piano",
          { { P_O1WAVE, 0 }, { P_O2WAVE, 2 }, { P_O2RANGE, 2 }, { P_PW, 0.45f }, { P_O2FINE, 3 }, { P_MIX, 0.3f },
            { P_CUTOFF, 3000 }, { P_RES, 0.05f }, { P_ENVAMT, 0.3f }, { P_FD, 0.8f }, { P_FS, 0.2f }, { P_AA, 0.002f },
            { P_AD, 1.4f }, { P_AS, 0.15f }, { P_AR, 0.4f }, { P_VELSENS, 0.85f }, { P_CHORUS, 1 } } },

        { "Organ",
          { { P_O1WAVE, 3 }, { P_O2WAVE, 3 }, { P_O2RANGE, 2 }, { P_MIX, 0.5f }, { P_CUTOFF, 9000 }, { P_ENVAMT, 0.f },
            { P_AA, 0.004f }, { P_AS, 1.f }, { P_AR, 0.06f }, { P_VELSENS, 0.f }, { P_CHORUS, 2 } } },

        { "Sweep FX",
          { { P_NOISE, 0.4f }, { P_O1WAVE, 1 }, { P_O2WAVE, 1 }, { P_O2FINE, 15 }, { P_CUTOFF, 1500 }, { P_RES, 0.6f },
            { P_ENVAMT, 0.f }, { P_LFOWAVE, 0 }, { P_LFORATE, 0.15f }, { P_LFOFILT, 0.7f }, { P_AA, 2.f }, { P_AS, 1.f },
            { P_AR, 2.5f }, { P_CHORUS, 2 }, { P_SPREAD, 1.f } } },

        { "Wind Noise",
          { { P_NOISE, 0.9f }, { P_MIX, 0.f }, { P_O1WAVE, 0 }, { P_HPF, 150 }, { P_CUTOFF, 900 }, { P_RES, 0.5f },
            { P_ENVAMT, 0.2f }, { P_KEYFOL, 0.8f }, { P_LFOWAVE, 3 }, { P_LFORATE, 0.8f }, { P_LFOFILT, 0.25f },
            { P_FA, 1.5f }, { P_FS, 1.f }, { P_AA, 1.2f }, { P_AS, 1.f }, { P_AR, 2.f }, { P_SPREAD, 1.f } } },
    };
    return list;
}
} // namespace jv
