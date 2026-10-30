#pragma once
// Table des paramètres du sampler (C++ pur, sans JUCE).
#include <string>

namespace sp
{
constexpr int kPads = 16;

enum Kind { K_FLOAT, K_CHOICE, K_BOOL, K_INT };

struct ParamDef
{
    const char* id;
    const char* name;
    float mn, mx, def, skew;
    Kind kind;
    const char* choices; // séparés par '|'
};

// --- paramètres globaux
#define SP_GLOBALS(X)                                                                                  \
    X(MASTER,  "master",   "Master",       0.f,   1.f,    0.8f,  1.f,  K_FLOAT,  "")                   \
    X(DRIVE,   "drive",    "Drive",        0.f,   1.f,    0.f,   1.f,  K_FLOAT,  "")                   \
    X(DLYTIME, "dlyTime",  "Delay Temps",  20.f,  1000.f, 375.f, 0.5f, K_FLOAT,  "")                   \
    X(DLYFB,   "dlyFb",    "Delay Retour", 0.f,   0.9f,   0.35f, 1.f,  K_FLOAT,  "")                   \
    X(DLYMIX,  "dlyMix",   "Delay Mix",    0.f,   1.f,    0.f,   1.f,  K_FLOAT,  "")                   \
    X(RVBSIZE, "rvbSize",  "Reverb Taille",0.f,   1.f,    0.5f,  1.f,  K_FLOAT,  "")                   \
    X(RVBMIX,  "rvbMix",   "Reverb Mix",   0.f,   1.f,    0.f,   1.f,  K_FLOAT,  "")                   \
    X(SWING,   "swing",    "Swing",        0.f,   0.6f,   0.f,   1.f,  K_FLOAT,  "")                   \
    X(SEQRATE, "seqRate",  "Pas",          0.f,   3.f,    1.f,   1.f,  K_CHOICE, "1/8|1/16|1/16T|1/32")\
    X(SEQLEN,  "seqLen",   "Longueur",     1.f,   16.f,   16.f,  1.f,  K_INT,    "")                   \
    X(SEQGATE, "seqGate",  "Gate",         0.1f,  0.95f,  0.8f,  1.f,  K_FLOAT,  "")                   \
    X(SEQON,   "seqOn",    "Suit lecture", 0.f,   1.f,    0.f,   1.f,  K_BOOL,   "")                   \
    X(SEQFREE, "seqFree",  "Seq libre",    0.f,   1.f,    0.f,   1.f,  K_BOOL,   "")                   \
    X(PATTERN, "pattern",  "Pattern",      0.f,   7.f,    0.f,   1.f,  K_CHOICE, "1|2|3|4|5|6|7|8")    \
    X(KBMODE,  "kbMode",   "Mode clavier", 0.f,   1.f,    0.f,   1.f,  K_BOOL,   "")                   \
    X(ROOT,    "rootNote", "Note racine",  0.f,   127.f,  60.f,  1.f,  K_INT,    "")

enum GId
{
#define X(n, id, name, mn, mx, def, skew, kind, ch) G_##n,
    SP_GLOBALS(X)
#undef X
    G_COUNT
};

inline const ParamDef kGlobals[G_COUNT] = {
#define X(n, id, name, mn, mx, def, skew, kind, ch) { id, name, mn, mx, def, skew, kind, ch },
    SP_GLOBALS(X)
#undef X
};

// --- paramètres propres à chaque pad
#define SP_PADKEYS(X)                                                                                   \
    X(VOL,   "vol",   "Volume",   0.f,    1.f,     0.8f,    1.f,  K_FLOAT,  "")                         \
    X(PAN,   "pan",   "Pan",      -1.f,   1.f,     0.f,     1.f,  K_FLOAT,  "")                         \
    X(TUNE,  "tune",  "Hauteur",  -24.f,  24.f,    0.f,     1.f,  K_FLOAT,  "")                         \
    X(START, "start", "Debut",    0.f,    1.f,     0.f,     1.f,  K_FLOAT,  "")                         \
    X(END,   "end",   "Fin",      0.f,    1.f,     1.f,     1.f,  K_FLOAT,  "")                         \
    X(MODE,  "mode",  "Mode",     0.f,    2.f,     0.f,     1.f,  K_CHOICE, "One-shot|Gate|Boucle")     \
    X(REV,   "rev",   "Reverse",  0.f,    1.f,     0.f,     1.f,  K_BOOL,   "")                         \
    X(ATK,   "atk",   "Attack",   0.001f, 2.f,     0.001f,  0.4f, K_FLOAT,  "")                         \
    X(REL,   "rel",   "Release",  0.005f, 5.f,     0.15f,   0.4f, K_FLOAT,  "")                         \
    X(CUT,   "cut",   "Filtre",   20.f,   20000.f, 20000.f, 0.3f, K_FLOAT,  "")                         \
    X(RES,   "res",   "Reso",     0.f,    0.95f,   0.1f,    1.f,  K_FLOAT,  "")                         \
    X(CHOKE, "choke", "Choke",    0.f,    4.f,     0.f,     1.f,  K_INT,    "")

enum PKey
{
#define X(n, id, name, mn, mx, def, skew, kind, ch) PK_##n,
    SP_PADKEYS(X)
#undef X
    PK_COUNT
};

inline const ParamDef kPadKeys[PK_COUNT] = {
#define X(n, id, name, mn, mx, def, skew, kind, ch) { id, name, mn, mx, def, skew, kind, ch },
    SP_PADKEYS(X)
#undef X
};

inline std::string padId (int pad, int key) { return "p" + std::to_string (pad + 1) + "_" + kPadKeys[key].id; }

struct Params
{
    float g[G_COUNT];
    float pad[kPads][PK_COUNT];

    static Params defaults()
    {
        Params p;
        for (int i = 0; i < G_COUNT; ++i) p.g[i] = kGlobals[i].def;
        for (int b = 0; b < kPads; ++b)
            for (int k = 0; k < PK_COUNT; ++k) p.pad[b][k] = kPadKeys[k].def;
        return p;
    }
};
} // namespace sp
