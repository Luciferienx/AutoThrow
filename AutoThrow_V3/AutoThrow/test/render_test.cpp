// Test hors-DAW du moteur : génère une "voix" synthétique (phrases rappées + trous),
// la passe dans AutoThrow et vérifie que la reverb est bien baissée pendant la voix
// et ouverte dans les trous. Écrit aussi des WAV pour écouter.
#include "../Source/AutoThrowEngine.h"
#include <cstdio>
#include <cstring>
#include <random>
#include <string>

using namespace autothrow;

static void writeWav (const std::string& path, const std::vector<float>& L, const std::vector<float>& R, int fs)
{
    FILE* f = std::fopen (path.c_str(), "wb");
    const uint32_t n = (uint32_t) L.size(), dataBytes = n * 4, riff = 36 + dataBytes, fmtLen = 16, byteRate = (uint32_t) fs * 4, sr = (uint32_t) fs;
    const uint16_t pcm = 1, ch = 2, align = 4, bits = 16;
    std::fwrite ("RIFF", 1, 4, f); std::fwrite (&riff, 4, 1, f); std::fwrite ("WAVEfmt ", 1, 8, f);
    std::fwrite (&fmtLen, 4, 1, f); std::fwrite (&pcm, 2, 1, f); std::fwrite (&ch, 2, 1, f);
    std::fwrite (&sr, 4, 1, f); std::fwrite (&byteRate, 4, 1, f); std::fwrite (&align, 2, 1, f); std::fwrite (&bits, 2, 1, f);
    std::fwrite ("data", 1, 4, f); std::fwrite (&dataBytes, 4, 1, f);
    for (uint32_t i = 0; i < n; ++i)
        for (float s : { L[i], R[i] })
        {
            const int16_t v = (int16_t) std::lround (std::clamp (s, -1.0f, 1.0f) * 32767.0f);
            std::fwrite (&v, 2, 1, f);
        }
    std::fclose (f);
}

struct Biquad // passe-bande pour les formants
{
    float b0 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void set (float fc, float q, float fs)
    {
        const float w = 2 * kPi * fc / fs, al = std::sin (w) / (2 * q), a0 = 1 + al;
        b0 = al / a0; b2 = -al / a0; a1 = -2 * std::cos (w) / a0; a2 = (1 - al) / a0;
    }
    float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = -a1 * y + z2;           // b1 = 0
        z2 = b2 * x - a2 * y;
        return y;
    }
};

int main (int argc, char** argv)
{
    const int fs = 48000;
    const std::string outDir = argc > 1 ? argv[1] : ".";
    std::mt19937 rng (7);
    std::uniform_real_distribution<float> uni (-1.0f, 1.0f);

    // --- Génère 3 phrases de ~2,2 s séparées par 1,4 s de silence, puis 4 s de fin.
    std::vector<float> voice;
    std::vector<int> isVoice; // 1 = dans une phrase (pour les mesures)
    const float vowels[5][3] = { { 730, 1090, 2440 }, { 270, 2290, 3010 }, { 530, 1840, 2480 }, { 300, 870, 2240 }, { 570, 840, 2410 } };
    float phase = 0.0f;
    for (int phrase = 0; phrase < 3; ++phrase)
    {
        for (int syl = 0; syl < 14; ++syl)
        {
            const int sylLen = (int) (0.12f * fs), gapLen = (int) (0.035f * fs);
            Biquad f1, f2, f3;
            const auto& v = vowels[(syl + phrase) % 5];
            f1.set (v[0], 8, fs); f2.set (v[1], 10, fs); f3.set (v[2], 12, fs);
            const float pitch = 125.0f + 15.0f * std::sin ((float) syl * 0.9f);
            for (int i = 0; i < sylLen; ++i)
            {
                phase += pitch / fs; if (phase >= 1) phase -= 1;
                const float src = (2 * phase - 1) + 0.05f * uni (rng);
                const float t = (float) i / sylLen;
                const float envA = std::min (1.0f, t * 12.0f) * std::min (1.0f, (1.0f - t) * 6.0f);
                voice.push_back (0.9f * envA * (f1.process (src) + 0.6f * f2.process (src) + 0.3f * f3.process (src)));
                isVoice.push_back (1);
            }
            for (int i = 0; i < gapLen; ++i) { voice.push_back (0.0005f * uni (rng)); isVoice.push_back (1); }
        }
        const int silence = (int) ((phrase == 2 ? 4.0f : 1.4f) * fs);
        for (int i = 0; i < silence; ++i) { voice.push_back (0.0005f * uni (rng)); isVoice.push_back (0); }
    }
    const size_t n = voice.size();

    // Normalise la "voix" à -6 dBFS crête
    float pk = 0; for (float s : voice) pk = std::max (pk, std::fabs (s));
    for (float& s : voice) s *= 0.5f / pk;

    auto run = [&] (Params p, std::vector<float>& L, std::vector<float>& R, std::vector<float>* throwTrace)
    {
        Engine e; e.prepare (fs); e.setParams (p);
        L = voice; R = voice;
        const int block = 256;
        for (size_t i = 0; i < n; i += block)
        {
            const int len = (int) std::min<size_t> (block, n - i);
            e.process (L.data() + i, R.data() + i, len);
            if (throwTrace) for (int k = 0; k < len; ++k) throwTrace->push_back (e.getThrowLevel());
        }
    };

    // --- Test 1 : wet seul, avec ducking vs sans ducking.
    // Pendant la voix on doit être ~-20 dB plus bas (DUCK 90 %), dans les trous quasi identique.
    Params p; p.wetOnly = true; p.mix = 1.0f;
    Params pNoDuck = p; pNoDuck.duck = 0.0f;
    std::vector<float> L, R, L0, R0, trace;
    run (p, L, R, &trace);
    run (pNoDuck, L0, R0, nullptr);
    double eV = 0, eV0 = 0, eG = 0, eG0 = 0, eDry = 0; float peak = 0; bool finite = true;
    const size_t settle = (size_t) (0.3f * fs);
    for (size_t i = settle; i < n; ++i)
    {
        if (! std::isfinite (L[i]) || ! std::isfinite (R[i])) finite = false;
        peak = std::max (peak, std::max (std::fabs (L[i]), std::fabs (R[i])));
        const bool inVoice = isVoice[i] && isVoice[i - settle];
        const bool inGap   = ! isVoice[i] && ! isVoice[i - settle];
        if (inVoice) { eV += L[i] * L[i]; eV0 += L0[i] * L0[i]; eDry += voice[i] * voice[i]; }
        if (inGap)   { eG += L[i] * L[i]; eG0 += L0[i] * L0[i]; }
    }
    std::printf ("[duck]  wet pendant la voix : %+.1f dB vs sans duck | dans les trous : %+.1f dB vs sans duck\n",
                 10 * std::log10 (eV / eV0), 10 * std::log10 (eG / eG0));
    std::printf ("[level] reverb (sans duck) vs voix sèche, pendant les phrases : %+.1f dB\n", 10 * std::log10 (eV0 / eDry));
    std::printf ("[sécu]  crête wet : %.3f (%.1f dBFS) | valeurs finies : %s\n", peak, 20 * std::log10 (peak), finite ? "oui" : "NON");

    // --- Test 2 : la détection suit bien les phrases (throw fermé pendant les phrases)
    size_t wrong = 0, checked = 0;
    for (size_t i = (size_t) (0.05f * fs); i < n; ++i)
        if (isVoice[i] && isVoice[i - (size_t) (0.05f * fs)]) { ++checked; if (trace[i] > 0.5f) ++wrong; }
    std::printf ("[detect] échantillons de phrase avec throw ouvert par erreur : %.2f %%\n", 100.0 * wrong / checked);

    // --- Test 3 : freeze + shimmer max pendant 60 s -> doit rester stable
    {
        Params pf; pf.wetOnly = true; pf.mix = 1.0f; pf.shimmer = 1.0f; pf.decaySec = 30.0f;
        Engine e; e.prepare (fs); e.setParams (pf);
        const size_t upTo = n - (size_t) (3.8f * fs); // 0,2 s après la dernière syllabe
        std::vector<float> a (voice.begin(), voice.begin() + (long) upTo), b (a);
        e.process (a.data(), b.data(), (int) upTo);
        pf.freeze = true; e.setParams (pf);
        std::vector<float> zl ((size_t) fs * 60, 0.0f), zr ((size_t) fs * 60, 0.0f);
        e.process (zl.data(), zr.data(), (int) zl.size());
        float pkStart = 0, pkEnd = 0;
        for (size_t i = 0; i < (size_t) fs * 5; ++i) pkStart = std::max (pkStart, std::fabs (zl[i]));
        for (size_t i = zl.size() - (size_t) fs * 5; i < zl.size(); ++i) pkEnd = std::max (pkEnd, std::fabs (zl[i]));
        std::printf ("[freeze] crête début : %.3f | crête après 60 s : %.3f | %s\n", pkStart, pkEnd,
                     (std::isfinite (pkEnd) && pkEnd < 4.0f * pkStart + 0.01f) ? "stable" : "INSTABLE");
    }

    // --- Test 4 : chaque type est stable, et changer de type en plein son ne clique pas
    {
        const char* names[4] = { "ROOM", "CHAMBER", "PLATE", "HALL" };
        for (int t = 0; t < 4; ++t)
        {
            Params pt; pt.wetOnly = true; pt.mix = 1.0f; pt.duck = 0.0f; pt.type = t; pt.shimmer = 0.5f;
            Engine e; e.prepare (fs); e.setParams (pt);
            std::vector<float> a (voice), b (voice);
            e.process (a.data(), b.data(), (int) n);
            float pk = 0; bool fin = true;
            for (size_t i = 0; i < n; ++i) { pk = std::max (pk, std::fabs (a[i])); fin = fin && std::isfinite (a[i]); }
            // énergie 1 s après la fin de la dernière phrase vs juste après
            const size_t endV = n - (size_t) (4.0f * fs);
            double e0 = 0, e1 = 0;
            for (size_t i = endV; i < endV + (size_t) (0.2f * fs); ++i) e0 += a[i] * a[i];
            for (size_t i = endV + (size_t) fs; i < endV + (size_t) (1.2f * fs); ++i) e1 += a[i] * a[i];
            std::printf ("[type]  %-8s crête %.3f | décroissance sur 1 s : %6.1f dB | %s\n", names[t], pk,
                         10 * std::log10 (e1 / (e0 + 1e-20) + 1e-20), fin ? "ok" : "NaN !");
        }

        // Changement de type toutes les 0,5 s pendant la voix : on cherche le plus gros saut d'un échantillon au suivant
        auto maxJump = [&] (bool switching)
        {
            Params ps; ps.wetOnly = true; ps.mix = 1.0f; ps.duck = 0.0f; ps.shimmer = 0.0f;
            Engine e; e.prepare (fs); e.setParams (ps);
            std::vector<float> a (voice), b (voice);
            const int block = 256; float prev = 0, jump = 0; int t = 3;
            for (size_t i = 0; i < n; i += block)
            {
                if (switching && (i / block) % 94 == 0) { t = (t + 1) % 4; ps.type = t; e.setParams (ps); }
                const int len = (int) std::min<size_t> (block, n - i);
                e.process (a.data() + i, b.data() + i, len);
                for (int k = 0; k < len; ++k) { jump = std::max (jump, std::fabs (a[i + k] - prev)); prev = a[i + k]; }
            }
            return jump;
        };
        std::printf ("[click] plus gros saut sans changement : %.4f | avec changements de type : %.4f\n", maxJump (false), maxJump (true));
    }

    // --- Démos audio
    writeWav (outDir + "/demo_1_dry.wav", voice, voice, fs);
    Params demo; demo.mix = 0.6f; demo.shimmer = 0.35f; demo.decaySec = 9.0f;
    run (demo, L, R, nullptr);
    writeWav (outDir + "/demo_2_autothrow.wav", L, R, fs);
    Params ref = demo; ref.duck = 0.0f; // même reverb, sans ducking : pour comparer
    run (ref, L, R, nullptr);
    writeWav (outDir + "/demo_3_reverb_sans_duck.wav", L, R, fs);
    const char* fileNames[4] = { "room", "chamber", "plate", "hall" };
    for (int t = 0; t < 4; ++t)
    {
        Params pt = demo; pt.type = t; pt.shimmer = 0.0f;
        if (t == 0) pt.decaySec = 1.2f;
        if (t == 1) pt.decaySec = 2.5f;
        if (t == 2) pt.decaySec = 3.0f;
        run (pt, L, R, nullptr);
        writeWav (outDir + "/type_" + std::to_string (t + 1) + "_" + fileNames[t] + ".wav", L, R, fs);
    }
    return 0;
}
