#pragma once
// Table unique des paramètres (C++ pur, sans JUCE) : sert au moteur, aux presets,
// à la déclaration des paramètres VST3 et à l'interface.
#include <array>

namespace jv
{
enum Kind { K_FLOAT, K_CHOICE, K_BOOL, K_INT };

struct ParamDef
{
    const char* id;
    const char* name;
    float mn, mx, def, skew;
    Kind kind;
    const char* choices; // séparés par '|'
};

//        enum      id            nom              min    max    def    skew  type      choix
#define JV_PARAMS(X)                                                                                         \
    X(MASTER,   "master",     "Master",        0.f,   1.f,   0.7f,  1.f,  K_FLOAT,  "")                      \
    X(POLY,     "polyMode",   "Mode",          0.f,   2.f,   0.f,   1.f,  K_CHOICE, "Poly|Unison|Mono")      \
    X(GLIDE,    "glide",      "Glide",         0.f,   5.f,   0.f,   0.4f, K_FLOAT,  "")                      \
    X(UNIDET,   "uniDetune",  "Unison Det.",   0.f,   1.f,   0.3f,  1.f,  K_FLOAT,  "")                      \
    X(SPREAD,   "spread",     "Stereo",        0.f,   1.f,   0.5f,  1.f,  K_FLOAT,  "")                      \
    X(BENDRANGE,"bendRange",  "Bend",          1.f,   12.f,  2.f,   1.f,  K_INT,    "")                      \
    X(VELSENS,  "velSens",    "Velocity",      0.f,   1.f,   0.5f,  1.f,  K_FLOAT,  "")                      \
    X(CHORUS,   "chorus",     "Chorus",        0.f,   2.f,   0.f,   1.f,  K_CHOICE, "Off|I|II")              \
    X(O1WAVE,   "osc1Wave",   "Onde",          0.f,   3.f,   1.f,   1.f,  K_CHOICE, "Tri|Saw|Pulse|Square")  \
    X(O1RANGE,  "osc1Range",  "Pied",          0.f,   3.f,   1.f,   1.f,  K_CHOICE, "16'|8'|4'|2'")          \
    X(O2WAVE,   "osc2Wave",   "Onde",          0.f,   3.f,   1.f,   1.f,  K_CHOICE, "Tri|Saw|Pulse|Square")  \
    X(O2RANGE,  "osc2Range",  "Pied",          0.f,   3.f,   1.f,   1.f,  K_CHOICE, "16'|8'|4'|2'")          \
    X(O2SEMI,   "osc2Semi",   "Semi",          -12.f, 12.f,  0.f,   1.f,  K_INT,    "")                      \
    X(O2FINE,   "osc2Fine",   "Fine",          -100.f,100.f, 0.f,   1.f,  K_FLOAT,  "")                      \
    X(SYNC,     "sync",       "Sync",          0.f,   1.f,   0.f,   1.f,  K_BOOL,   "")                      \
    X(XMOD,     "xmod",       "X-Mod",         0.f,   1.f,   0.f,   1.f,  K_FLOAT,  "")                      \
    X(PW,       "pw",         "PW",            0.05f, 0.95f, 0.5f,  1.f,  K_FLOAT,  "")                      \
    X(PWM,      "pwm",        "PWM LFO",       0.f,   1.f,   0.f,   1.f,  K_FLOAT,  "")                      \
    X(MIX,      "mix",        "Osc 2 Mix",     0.f,   1.f,   0.5f,  1.f,  K_FLOAT,  "")                      \
    X(NOISE,    "noise",      "Noise",         0.f,   1.f,   0.f,   1.f,  K_FLOAT,  "")                      \
    X(HPF,      "hpf",        "HPF",           20.f,  2000.f,20.f,  0.4f, K_FLOAT,  "")                      \
    X(CUTOFF,   "cutoff",     "Cutoff",        20.f,  18000.f,8000.f,0.3f,K_FLOAT,  "")                      \
    X(RES,      "res",        "Resonance",     0.f,   1.f,   0.1f,  1.f,  K_FLOAT,  "")                      \
    X(SLOPE,    "slope",      "Pente",         0.f,   1.f,   1.f,   1.f,  K_CHOICE, "12 dB|24 dB")           \
    X(ENVAMT,   "envAmt",     "Env Amt",       -1.f,  1.f,   0.3f,  1.f,  K_FLOAT,  "")                      \
    X(KEYFOL,   "keyFollow",  "Key Follow",    0.f,   1.f,   0.5f,  1.f,  K_FLOAT,  "")                      \
    X(LFOFILT,  "lfoFilt",    "LFO > VCF",     0.f,   1.f,   0.f,   1.f,  K_FLOAT,  "")                      \
    X(FA,       "fenvA",      "Attack",        0.001f,10.f,  0.01f, 0.35f,K_FLOAT,  "")                      \
    X(FD,       "fenvD",      "Decay",         0.005f,10.f,  0.5f,  0.35f,K_FLOAT,  "")                      \
    X(FS,       "fenvS",      "Sustain",       0.f,   1.f,   0.5f,  1.f,  K_FLOAT,  "")                      \
    X(FR,       "fenvR",      "Release",       0.005f,10.f,  0.3f,  0.35f,K_FLOAT,  "")                      \
    X(AA,       "aenvA",      "Attack",        0.001f,10.f,  0.005f,0.35f,K_FLOAT,  "")                      \
    X(AD,       "aenvD",      "Decay",         0.005f,10.f,  0.3f,  0.35f,K_FLOAT,  "")                      \
    X(AS,       "aenvS",      "Sustain",       0.f,   1.f,   0.8f,  1.f,  K_FLOAT,  "")                      \
    X(AR,       "aenvR",      "Release",       0.005f,10.f,  0.3f,  0.35f,K_FLOAT,  "")                      \
    X(LFOWAVE,  "lfoWave",    "Onde",          0.f,   3.f,   0.f,   1.f,  K_CHOICE, "Tri|Saw|Square|S&H")    \
    X(LFORATE,  "lfoRate",    "Rate",          0.05f, 30.f,  5.f,   0.4f, K_FLOAT,  "")                      \
    X(LFODELAY, "lfoDelay",   "Delay",         0.f,   4.f,   0.f,   0.5f, K_FLOAT,  "")                      \
    X(LFOPITCH, "lfoPitch",   "LFO > Pitch",   0.f,   1.f,   0.f,   1.f,  K_FLOAT,  "")                      \
    X(ARPON,    "arpOn",      "Arp On",        0.f,   1.f,   0.f,   1.f,  K_BOOL,   "")                      \
    X(ARPHOLD,  "arpHold",    "Hold",          0.f,   1.f,   0.f,   1.f,  K_BOOL,   "")                      \
    X(ARPMODE,  "arpMode",    "Mode",          0.f,   4.f,   0.f,   1.f,  K_CHOICE, "Up|Down|Up/Down|Random|Played") \
    X(ARPRATE,  "arpRate",    "Rate",          0.f,   5.f,   3.f,   1.f,  K_CHOICE, "1/4|1/8|1/8T|1/16|1/16T|1/32") \
    X(ARPOCT,   "arpOct",     "Octaves",       1.f,   4.f,   1.f,   1.f,  K_INT,    "")                      \
    X(ARPGATE,  "arpGate",    "Gate",          0.1f,  0.95f, 0.5f,  1.f,  K_FLOAT,  "")

enum PId
{
#define X(n, id, name, mn, mx, def, skew, kind, ch) P_##n,
    JV_PARAMS(X)
#undef X
    P_COUNT
};

inline const ParamDef kDefs[P_COUNT] = {
#define X(n, id, name, mn, mx, def, skew, kind, ch) { id, name, mn, mx, def, skew, kind, ch },
    JV_PARAMS(X)
#undef X
};

struct Params
{
    float v[P_COUNT];
    float operator[] (int i) const { return v[i]; }
    static Params defaults()
    {
        Params p;
        for (int i = 0; i < P_COUNT; ++i) p.v[i] = kDefs[i].def;
        return p;
    }
};
} // namespace jv
