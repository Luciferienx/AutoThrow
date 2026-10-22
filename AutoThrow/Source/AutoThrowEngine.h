// AutoThrowEngine.h
// Moteur DSP d'AutoThrow : pur C++, aucune dépendance à JUCE.
//
// Principe :
//   1. Un détecteur suit le niveau de la voix (sidechain interne).
//   2. Tant que la voix joue, la reverb est "duckée" (baissée) pour garder la voix propre.
//   3. Dès que la voix s'arrête plus longtemps que GAP, la reverb s'ouvre en BLOOM : c'est le throw.
//   4. La reverb est un FDN 8 lignes modulé, avec shimmer (octave +12) dans la boucle et un mode FREEZE.

#pragma once

#include <array>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>

namespace autothrow
{

constexpr float kPi = 3.14159265358979323846f;

// ---------------------------------------------------------------------------
// Briques de base
// ---------------------------------------------------------------------------

struct OnePoleLP
{
    float z = 0.0f, a = 0.0f;
    void setCutoff (float hz, float fs) { a = std::exp (-2.0f * kPi * hz / fs); }
    void setBypass() { a = 0.0f; }
    float process (float x) { z = x * (1.0f - a) + z * a; return z; }
    void reset() { z = 0.0f; }
};

struct OnePoleHP
{
    OnePoleLP lp;
    void setCutoff (float hz, float fs) { lp.setCutoff (hz, fs); }
    float process (float x) { return x - lp.process (x); }
    void reset() { lp.reset(); }
};

// HPF 12 dB/oct (deux 1-pôle en série) : suffisant pour nettoyer le bas d'une reverb.
struct HP12
{
    OnePoleHP a, b;
    void setCutoff (float hz, float fs) { a.setCutoff (hz, fs); b.setCutoff (hz, fs); }
    float process (float x) { return b.process (a.process (x)); }
    void reset() { a.reset(); b.reset(); }
};

struct DelayLine
{
    std::vector<float> buf;
    int w = 0, mask = 0;

    void prepare (int maxSamples)
    {
        int n = 1;
        while (n < maxSamples + 4) n <<= 1;
        buf.assign ((size_t) n, 0.0f);
        mask = n - 1;
        w = 0;
    }
    void reset() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
    void push (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }

    // Lecture "d" échantillons dans le passé (d >= 1), interpolation linéaire.
    float read (float d) const
    {
        float rp = (float) w - d;
        while (rp < 0.0f) rp += (float) buf.size();
        const int i0 = (int) rp;
        const float f = rp - (float) i0;
        const float s0 = buf[(size_t) (i0 & mask)];
        const float s1 = buf[(size_t) ((i0 + 1) & mask)];
        return s0 + f * (s1 - s0);
    }
    float readInt (int d) const { return buf[(size_t) ((w - d) & mask)]; }
};

struct Allpass
{
    DelayLine d;
    int len = 1;
    float g = 0.7f;
    void prepare (float ms, float fs, float gain)
    {
        len = std::max (1, (int) std::round (ms * 0.001f * fs));
        d.prepare (len + 1);
        g = gain;
    }
    float process (float x)
    {
        const float v = d.readInt (len);
        const float u = x + g * v;
        d.push (u);
        return v - g * u;
    }
    void reset() { d.reset(); }
};

// Pitch shifter à deux têtes de lecture croisées. ratio 2 = +1 octave.
struct PitchShifter
{
    DelayLine d;
    float window = 2048.0f, phase = 0.0f, ratio = 2.0f;

    void prepare (float fs, float startPhase)
    {
        window = 0.06f * fs;
        d.prepare ((int) window + 8);
        phase = startPhase;
    }
    float process (float x)
    {
        d.push (x);
        phase += (ratio - 1.0f) / window;
        if (phase >= 1.0f) phase -= 1.0f;
        float p2 = phase + 0.5f;
        if (p2 >= 1.0f) p2 -= 1.0f;
        // Fenêtres sin² : la somme des deux gains vaut toujours 1, donc le shifter
        // ne peut jamais amplifier -> la boucle de la reverb reste stable.
        const float a = std::sin (kPi * phase), b = std::sin (kPi * p2);
        return d.read (1.0f + (1.0f - phase) * window) * a * a
             + d.read (1.0f + (1.0f - p2)    * window) * b * b;
    }
    void reset() { d.reset(); }
};

// ---------------------------------------------------------------------------
// Reverb FDN 8 lignes
// ---------------------------------------------------------------------------

class BigVerb
{
public:
    static constexpr int N = 8;
    static constexpr int kShimLines = 2;

    void prepare (float sampleRate)
    {
        fs = sampleRate;
        // Longueurs "grande salle", toutes premières entre elles -> pas de résonance métallique.
        static const float ms[N]   = { 43.7f, 51.3f, 59.9f, 67.1f, 74.3f, 83.9f, 92.3f, 101.7f };
        static const float rate[N] = { 0.11f, 0.17f, 0.23f, 0.29f, 0.37f, 0.43f, 0.53f, 0.61f };
        modDepth = 0.0006f * fs; // ~0.6 ms de modulation : chorus doux dans la queue

        for (int i = 0; i < N; ++i)
        {
            len[i] = ms[i] * 0.001f * fs;
            lines[i].prepare ((int) (len[i] + modDepth + 4.0f));
            lfoInc[i] = 2.0f * kPi * rate[i] / fs;
            lfoPhase[i] = (float) i * 0.7f;
            shifters[i].prepare (fs, (float) i / N);
        }

        static const float apL[4] = { 4.77f, 3.59f, 12.73f, 9.30f };
        static const float apR[4] = { 5.03f, 3.83f, 13.41f, 8.87f };
        for (int k = 0; k < 4; ++k)
        {
            diffL[k].prepare (apL[k], fs, 0.68f);
            diffR[k].prepare (apR[k], fs, 0.68f);
        }
        inHP_L.setCutoff (180.0f, fs);
        inHP_R.setCutoff (180.0f, fs);
        reset();
        setParams (8.0f, 0.5f, 0.0f, false);
    }

    void reset()
    {
        for (auto& l : lines) l.reset();
        for (auto& d : damp) d.reset();
        for (auto& s : shifters) s.reset();
        for (auto& a : diffL) a.reset();
        for (auto& a : diffR) a.reset();
        inHP_L.reset(); inHP_R.reset();
        inGain = 1.0f;
    }

    // decaySec : T60 en secondes. tone 0..1 (sombre -> brillant). shimmer 0..1.
    void setParams (float decaySec, float tone, float shimmerAmt, bool freezeOn)
    {
        freeze = freezeOn;
        shimmer = std::clamp (shimmerAmt, 0.0f, 1.0f);
        const float cutoff = 1500.0f * std::pow (14000.0f / 1500.0f, std::clamp (tone, 0.0f, 1.0f));
        // Le shimmer "mange" un peu de queue : on rallonge le decay pour compenser.
        // fbGain reste < 1, donc la stabilité est garantie quoi qu'il arrive.
        const float t60 = std::max (0.3f, decaySec) * (1.0f + 1.5f * shimmer);
        for (int i = 0; i < N; ++i)
        {
            fbGain[i] = freeze ? 1.0f : std::min (0.9995f, std::pow (10.0f, -3.0f * len[i] / (t60 * fs)));
            if (freeze) damp[i].setBypass();
            else        damp[i].setCutoff (std::min (cutoff, 0.45f * fs), fs);
        }
    }

    void process (float inL, float inR, float& outL, float& outR)
    {
        // En freeze on coupe l'entrée en douceur : la queue tourne à l'infini.
        const float target = freeze ? 0.0f : 1.0f;
        inGain += (target - inGain) * 0.002f;

        float l = inHP_L.process (inL) * inGain;
        float r = inHP_R.process (inR) * inGain;
        for (int k = 0; k < 4; ++k) { l = diffL[k].process (l); r = diffR[k].process (r); }

        float y[N];
        for (int i = 0; i < N; ++i)
        {
            lfoPhase[i] += lfoInc[i];
            if (lfoPhase[i] > 2.0f * kPi) lfoPhase[i] -= 2.0f * kPi;
            // En FREEZE : lecture entière, sans modulation ni interpolation -> aucune perte, la queue tient.
            y[i] = freeze ? lines[i].readInt ((int) len[i])
                          : damp[i].process (lines[i].read (len[i] + modDepth * (0.5f + 0.5f * std::sin (lfoPhase[i]))));
        }

        // Sorties stéréo décorrélées
        outL = 0.5f * (y[0] - y[2] + y[4] - y[6]);
        outR = 0.5f * (y[1] - y[3] + y[5] - y[7]);

        // Matrice de Hadamard (mélange "énergie conservée")
        float x[N];
        std::copy (y, y + N, x);
        for (int h = 1; h < N; h *= 2)
            for (int i = 0; i < N; i += h * 2)
                for (int j = i; j < i + h; ++j)
                {
                    const float a = x[j], b = x[j + h];
                    x[j] = a + b;
                    x[j + h] = a - b;
                }
        const float norm = 0.35355339f; // 1/sqrt(8)

        for (int i = 0; i < N; ++i)
        {
            float fb = x[i] * norm;
            // Shimmer : deux lignes repassent par un pitch +12 dans la boucle.
            // Crossfade linéaire (gain <= 1) : jamais d'emballement.
            // En FREEZE on le coupe, sinon il viderait petit à petit la queue gelée.
            if (i < kShimLines && shimmer > 0.0f && ! freeze)
            {
                const float up = shifters[i].process (fb);
                fb += shimmer * (up - fb);
            }
            const float in = (i & 1) ? r : l;
            float v = softClip (fb * fbGain[i] + in * 0.5f);
            if (! std::isfinite (v)) v = 0.0f; // sécurité absolue
            lines[i].push (v);
        }
    }

private:
    // Saturation douce dans la boucle : invisible à niveau normal, empêche tout emballement en FREEZE + SHIMMER.
    static float softClip (float x)
    {
        const float t = 0.9f;
        const float a = std::fabs (x);
        if (a <= t) return x;
        const float over = (a - t) / (1.0f - t);
        return std::copysign (t + (1.0f - t) * std::tanh (over), x);
    }

    float fs = 48000.0f, modDepth = 0.0f, shimmer = 0.0f, inGain = 1.0f;
    bool freeze = false;
    DelayLine lines[N];
    OnePoleLP damp[N];
    PitchShifter shifters[N];
    float len[N] {}, fbGain[N] {}, lfoInc[N] {}, lfoPhase[N] {};
    Allpass diffL[4], diffR[4];
    HP12 inHP_L, inHP_R;
};

// ---------------------------------------------------------------------------
// Moteur complet : détection + ducking/throw + reverb
// ---------------------------------------------------------------------------

struct Params
{
    float decaySec   = 8.0f;   // 1 .. 30 s
    float shimmer    = 0.25f;  // 0 .. 1
    float tone       = 0.5f;   // 0 .. 1
    float mix        = 0.5f;   // 0 .. 1 (niveau du wet)
    float sensDb     = -35.0f; // seuil de détection de la voix
    float gapMs      = 180.0f; // silence minimum avant que le throw parte
    float duck       = 0.9f;   // 0 .. 1 : combien la reverb est baissée quand la voix joue
    float bloomMs    = 250.0f; // vitesse d'ouverture du throw
    bool  freeze     = false;
    bool  wetOnly    = false;  // pour utiliser le plugin sur un bus en send
};

class Engine
{
public:
    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        verb.prepare (fs);
        scHP.setCutoff (150.0f, fs);
        envAtt = std::exp (-1.0f / (0.002f * fs));
        envRel = std::exp (-1.0f / (0.060f * fs));
        duckCoef = std::exp (-1.0f / (0.012f * fs)); // la reverb s'efface vite quand la voix revient
        outHP_L.setCutoff (200.0f, fs);
        outHP_R.setCutoff (200.0f, fs);
        reset();
        setParams (p);
    }

    void reset()
    {
        verb.reset();
        scHP.reset(); outHP_L.reset(); outHP_R.reset();
        env = 0.0f; holdCounter = 0; voiceActive = false;
        wetGain = 1.0f - p.duck; mixSm = p.mix;
    }

    void setParams (const Params& np)
    {
        p = np;
        verb.setParams (p.decaySec, p.tone, p.shimmer, p.freeze);
        holdSamples = (int) (p.gapMs * 0.001f * fs);
        bloomCoef = std::exp (-1.0f / (std::max (0.005f, p.bloomMs * 0.001f) * fs));
        threshLin = std::pow (10.0f, p.sensDb / 20.0f);
    }

    // Traitement en place, stéréo. Pour du mono : passer le même buffer en L et R.
    void process (float* L, float* R, int numSamples)
    {
        for (int n = 0; n < numSamples; ++n)
        {
            const float dryL = L[n], dryR = R[n];

            // 1) Détection de la voix
            const float sc = std::fabs (scHP.process (0.5f * (dryL + dryR)));
            env = sc > env ? envAtt * env + (1.0f - envAtt) * sc
                           : envRel * env + (1.0f - envRel) * sc;

            if (env > threshLin) { voiceActive = true; holdCounter = holdSamples; }
            else if (holdCounter > 0) --holdCounter;
            else voiceActive = false;

            // 2) Gain du throw : baissé pendant la voix, ouvert dans les trous
            const float target = voiceActive ? (1.0f - p.duck) : 1.0f;
            const float c = voiceActive ? duckCoef : bloomCoef;
            wetGain = target + (wetGain - target) * c;
            mixSm += (p.mix - mixSm) * 0.001f;

            // 3) Reverb (toujours alimentée : la queue contient la fin de phrase)
            float wl, wr;
            verb.process (dryL, dryR, wl, wr);
            wl = outHP_L.process (wl) * kWetTrim;
            wr = outHP_R.process (wr) * kWetTrim;

            const float g = wetGain * mixSm;
            const float dryGain = p.wetOnly ? 0.0f : 1.0f;
            L[n] = dryL * dryGain + wl * g;
            R[n] = dryR * dryGain + wr * g;
        }
        meterThrow = wetGain;
        meterEnv = env;
    }

    // Pour l'interface
    bool  isVoiceActive() const { return voiceActive; }
    float getThrowLevel() const { return meterThrow; }
    float getInputEnv()   const { return meterEnv; }

private:
    static constexpr float kWetTrim = 1.0f;
    float fs = 48000.0f;
    Params p;
    BigVerb verb;
    OnePoleHP scHP;
    HP12 outHP_L, outHP_R;
    float env = 0.0f, envAtt = 0.0f, envRel = 0.0f, threshLin = 0.0f;
    float wetGain = 0.0f, duckCoef = 0.0f, bloomCoef = 0.0f, mixSm = 0.0f;
    float meterThrow = 0.0f, meterEnv = 0.0f;
    int holdSamples = 0, holdCounter = 0;
    bool voiceActive = false;
};

} // namespace autothrow
