// Offline test for the drum engine and the FX section.
// Usage: NovaRenderTest

#include "../Source/DSP/DrumTrack.h"
#include "../Source/DSP/MasterFx.h"

#include <iostream>
#include <random>

using namespace takt;

namespace
{
constexpr double sampleRate = 48000.0;
int failures = 0;

void check (bool ok, const juce::String& what)
{
    std::cout << (ok ? "  [ok]   " : "  [FAIL] ") << what << "\n";
    if (! ok) ++failures;
}

int secs (double s) { return (int) (s * sampleRate); }

struct Stats
{
    float peak = 0.0f;
    double rms = 0.0;
    bool finite = true;
    int lastAudible = 0;
};

Stats analyse (const float* d, int num)
{
    Stats s;
    double sum = 0.0;
    for (int i = 0; i < num; ++i)
    {
        if (! std::isfinite (d[i])) s.finite = false;
        const auto a = std::abs (d[i]);
        s.peak = std::max (s.peak, a);
        sum += (double) d[i] * d[i];
        if (a > 0.001f) s.lastAudible = i;
    }
    s.rms = std::sqrt (sum / std::max (1, num));
    return s;
}

/** Share of the energy above a frequency (simple one-pole split) – a rough "brightness / dirt" measure. */
double energyAbove (const std::vector<float>& x, float hz)
{
    dsp::OnePoleLowPass lp;
    lp.setCutoff (hz, sampleRate);
    double hi = 0.0, all = 0.0;
    for (auto v : x)
    {
        const auto h = v - lp.process (v);
        hi += (double) h * h;
        all += (double) v * v;
    }
    return all > 0.0 ? hi / all : 0.0;
}

/** Renders one hit of a single track (left channel) and returns the samples. */
std::vector<float> renderTrack (int t, const TrackParams& p, double seconds, SampleData::Ptr sample = nullptr,
                                std::vector<float>* reverbSendOut = nullptr)
{
    DrumTrack tr;
    tr.setTrack (trackInfos()[(size_t) t]);
    tr.setParams (p);
    tr.prepare (sampleRate);
    tr.setSample (sample);

    const auto n = secs (seconds);
    std::vector<float> l ((size_t) n, 0.0f), r ((size_t) n, 0.0f), d ((size_t) n, 0.0f), v ((size_t) n, 0.0f);
    tr.trigger (1.0f);
    tr.render (l.data(), r.data(), d.data(), v.data(), n);
    if (reverbSendOut != nullptr) *reverbSendOut = v;
    return l;
}

SampleData::Ptr makeSample()
{
    SampleData::Ptr sample = new SampleData();
    sample->sourceSampleRate = 44100.0;
    sample->audio.setSize (1, 44100 / 2);
    juce::Random rng (42);
    for (int i = 0; i < sample->audio.getNumSamples(); ++i)
    {
        const auto env = std::exp (-(float) i / 6000.0f);
        sample->audio.setSample (0, i, env * (0.6f * std::sin ((float) i * 0.07f) + 0.4f * (rng.nextFloat() * 2.0f - 1.0f)));
    }
    return sample;
}
} // namespace

int main()
{
    //==========================================================================
    std::cout << "\n1) Alle Stimmen mit Default-Sound\n\n  Spur  Peak    RMS     hoerbar bis\n";
    for (int t = 0; t < numTracks; ++t)
    {
        const auto len = trackInfos()[(size_t) t].type == VoiceType::cymbal ? 3.0 : 1.2;
        const auto x = renderTrack (t, makeDefaultTrackParams (t), len);
        const auto s = analyse (x.data(), (int) x.size());
        std::cout << "  " << trackInfos()[(size_t) t].shortName << "    " << juce::String (s.peak, 3) << "   "
                  << juce::String (s.rms, 3) << "   " << juce::String ((double) s.lastAudible / sampleRate, 2) << " s\n";
        check (s.finite && s.peak > 0.05f && s.peak < 1.6f, juce::String (trackInfos()[(size_t) t].shortName) + ": Pegel ok, keine NaN");
    }

    //==========================================================================
    std::cout << "\n2) Kick: Hold, Wave, Drive, Sweep\n";
    {
        auto clean = makeDefaultTrackParams (0);
        clean[Page::synth][tonal::wave] = 0.0f;
        clean[Page::synth][tonal::hold] = 0.0f;
        clean[Page::amp][amp::drive] = 0.0f;
        const auto c = renderTrack (0, clean, 1.0);

        const auto hard = renderTrack (0, makeDefaultTrackParams (0), 1.0);

        auto brutal = makeDefaultTrackParams (0);
        brutal[Page::synth][tonal::wave] = 0.9f;
        brutal[Page::amp][amp::drive] = 0.9f;
        brutal[Page::synth][tonal::hold] = 0.12f;
        const auto b = renderTrack (0, brutal, 1.0);

        const auto rc = analyse (c.data(), (int) c.size()).rms;
        const auto rh = analyse (hard.data(), (int) hard.size()).rms;
        const auto rb = analyse (b.data(), (int) b.size()).rms;
        // harmonics of the body (after the sweep): energy above 120 Hz on a 50 Hz kick = distortion
        auto body = [] (const std::vector<float>& x) { return std::vector<float> (x.begin() + secs (0.1), x.begin() + secs (0.3)); };
        const auto hc = energyAbove (body (c), 120.0f), hh = energyAbove (body (hard), 120.0f), hb = energyAbove (body (b), 120.0f);

        std::cout << "  RMS:          clean " << juce::String (rc, 3) << "   default " << juce::String (rh, 3) << "   brutal " << juce::String (rb, 3) << "\n";
        std::cout << "  Obertoene im Body: clean " << juce::String (hc * 100, 1) << " %   default " << juce::String (hh * 100, 1)
                  << " %   brutal " << juce::String (hb * 100, 1) << " %\n";
        check (rh > rc * 1.2, "Default-Kick hat mehr Druck (RMS) als die cleane");
        check (hb > hh && hh > hc, "Mehr Wave/Drive = mehr Obertoene (Dreck)");

        auto noHold = makeDefaultTrackParams (0);
        noHold[Page::synth][tonal::hold] = 0.0f;
        auto longHold = makeDefaultTrackParams (0);
        longHold[Page::synth][tonal::hold] = 0.3f;
        const auto a1 = renderTrack (0, noHold, 2.0), a2 = renderTrack (0, longHold, 2.0);
        check (analyse (a2.data(), (int) a2.size()).lastAudible > analyse (a1.data(), (int) a1.size()).lastAudible + secs (0.2),
               "Hold verlaengert die Kick");
    }

    //==========================================================================
    std::cout << "\n3) Filter, Filter-Huellkurve, Amp-Huellkurve\n";
    {
        for (int type = 0; type < 7; ++type)
        {
            auto p = makeDefaultTrackParams (1);
            p[Page::filter][flt::type] = (float) type;
            p[Page::filter][flt::freq] = 1200.0f;
            p[Page::filter][flt::reso] = 0.9f;
            const auto x = renderTrack (1, p, 0.8);
            const auto s = analyse (x.data(), (int) x.size());
            check (s.finite && s.peak < 3.0f, juce::String ("Filtertyp ") + juce::String (type) + " stabil (Peak " + juce::String (s.peak, 2) + ")");
        }

        auto closed = makeDefaultTrackParams (8);
        closed[Page::filter][flt::freq] = 300.0f;
        auto swept = closed;
        swept[Page::filter][flt::env] = 1.0f;
        swept[Page::filter][flt::decay] = 0.2f;
        const auto c = renderTrack (8, closed, 0.5), e = renderTrack (8, swept, 0.5);
        check (analyse (e.data(), (int) e.size()).rms > analyse (c.data(), (int) c.size()).rms * 2.0,
               "Filter-Env oeffnet das geschlossene Filter");

        auto cut = makeDefaultTrackParams (9);
        cut[Page::amp][amp::hold] = 0.05f;
        cut[Page::amp][amp::decay] = 0.05f;
        const auto full = renderTrack (9, makeDefaultTrackParams (9), 2.0), gated = renderTrack (9, cut, 2.0);
        check (analyse (gated.data(), (int) gated.size()).lastAudible < secs (0.15)
               && analyse (full.data(), (int) full.size()).lastAudible > secs (0.3),
               "Amp Hold/Decay schneidet die Open Hat ab");

        auto slow = makeDefaultTrackParams (0);
        slow[Page::amp][amp::attack] = 0.2f;
        const auto att = renderTrack (0, slow, 0.5);
        check (analyse (att.data(), secs (0.01)).peak < 0.1f, "Amp Attack blendet ein");
    }

    //==========================================================================
    std::cout << "\n4) Sample-Layer\n";
    {
        const auto sample = makeSample();
        auto p = makeDefaultTrackParams (2);
        p[Page::synth][syn::level] = 0.0f;
        p[Page::amp][amp::drive] = 0.0f;

        const auto full = renderTrack (2, p, 1.0, sample);
        const auto fs = analyse (full.data(), (int) full.size());
        check (fs.peak > 0.1f, "Sample spielt");

        auto shortP = p; shortP[Page::sample][smp::end] = 0.2f;
        const auto s1 = renderTrack (2, shortP, 1.0, sample);
        check (analyse (s1.data(), (int) s1.size()).lastAudible < fs.lastAudible / 2, "End verkuerzt das Sample");

        auto rev = p; rev[Page::sample][smp::start] = 1.0f; rev[Page::sample][smp::end] = 0.0f;
        const auto r = renderTrack (2, rev, 1.0, sample);
        check (analyse (r.data(), secs (0.05)).peak < analyse (r.data() + secs (0.3), secs (0.05)).peak,
               "End vor Start spielt rueckwaerts (leise -> laut)");

        auto loop = p; loop[Page::sample][smp::start] = 0.0f; loop[Page::sample][smp::end] = 0.05f; loop[Page::sample][smp::loop] = 1.0f;
        const auto l = renderTrack (2, loop, 1.0, sample);
        check (analyse (l.data() + secs (0.8), secs (0.1)).peak > 0.05f, "Loop haelt das Sample am Leben");

        auto crush = p; crush[Page::sample][smp::bits] = 4.0f; crush[Page::sample][smp::decim] = 8.0f;
        const auto cr = renderTrack (2, crush, 1.0, sample);
        double diff = 0.0, sig = 0.0;
        for (size_t i = 0; i < cr.size(); ++i) { diff += (double) (cr[i] - full[i]) * (cr[i] - full[i]); sig += (double) full[i] * full[i]; }
        check (diff > sig * 0.05, "Bit Rd / Decim veraendern das Sample deutlich (Fehleranteil " + juce::String (100.0 * diff / sig, 1) + " %)");
    }

    //==========================================================================
    std::cout << "\n5) FX: Delay, Reverb, Distortion, Kompressor\n";
    {
        auto runFx = [] (MasterValues v, std::vector<float> dry, std::vector<float> dlySend, std::vector<float> revSend, double bpm)
        {
            MasterFx fx;
            fx.prepare (sampleRate);
            fx.setParameters (v, bpm);
            std::vector<float> l = dry, r = dry;
            for (size_t pos = 0; pos < l.size(); pos += MasterFx::maxBlockSize)
            {
                const auto n = (int) std::min<size_t> (MasterFx::maxBlockSize, l.size() - pos);
                fx.process (l.data() + pos, r.data() + pos, dlySend.data() + pos, revSend.data() + pos, n);
            }
            return std::make_pair (l, r);
        };

        const auto n = (size_t) secs (2.0);
        std::vector<float> silence (n, 0.0f), click (n, 0.0f);
        for (int i = 0; i < 48; ++i) click[(size_t) i] = 0.8f;

        auto v = makeDefaultMasterValues();
        v[mst::distDrive] = 0.0f; v[mst::compMix] = 0.0f;
        v[mst::dlyTime] = 5.0f;       // 1/8
        v[mst::dlyPingPong] = 0.0f;
        const auto [dl, dr] = runFx (v, silence, click, silence, 120.0);
        const auto echoAt = secs (0.25);
        check (analyse (dl.data() + echoAt - 200, 2000).peak > 0.1f && analyse (dl.data() + 2000, echoAt - 2400).peak < 0.01f,
               "Delay 1/8 bei 120 BPM: Echo nach 250 ms");

        const auto [rl, rr] = runFx (v, silence, silence, click, 120.0);
        check (analyse (rl.data() + secs (0.5), secs (0.3)).rms > 0.001, "Reverb erzeugt eine Fahne");

        // Duck: a loud dry signal pushes the reverb down
        std::vector<float> loudDry (n, 0.0f);
        for (size_t i = (size_t) secs (0.4); i < (size_t) secs (0.9); ++i) loudDry[i] = 0.5f * std::sin ((float) i * 0.02f);
        auto ducked = v; ducked[mst::revDuck] = 1.0f;
        const auto [ul, ur] = runFx (v, loudDry, silence, click, 120.0);
        const auto [kl, kr] = runFx (ducked, loudDry, silence, click, 120.0);
        std::vector<float> wetU (n), wetK (n);
        for (size_t i = 0; i < n; ++i) { wetU[i] = ul[i] - loudDry[i]; wetK[i] = kl[i] - loudDry[i]; }
        check (analyse (wetK.data() + secs (0.5), secs (0.3)).rms < analyse (wetU.data() + secs (0.5), secs (0.3)).rms * 0.2,
               "Duck drueckt den Hall unter lauten Sounds weg");

        std::vector<float> sine (n);
        for (size_t i = 0; i < n; ++i) sine[i] = 0.5f * std::sin ((float) i * juce::MathConstants<float>::twoPi * 55.0f / (float) sampleRate);

        auto dist = v; dist[mst::distDrive] = 0.7f;
        const auto [dsl, dsr] = runFx (dist, sine, silence, silence, 120.0);
        check (energyAbove (dsl, 1000.0f) > energyAbove (sine, 1000.0f) * 3.0,
               "Distortion erzeugt Obertoene (" + juce::String (energyAbove (sine, 1000.0f) * 100, 2) + " % -> "
               + juce::String (energyAbove (dsl, 1000.0f) * 100, 2) + " % ueber 1 kHz)");

        auto comp = v; comp[mst::compMix] = 1.0f; comp[mst::compThreshold] = -20.0f; comp[mst::compRatio] = 10.0f; comp[mst::compMakeup] = 0.0f;
        const auto [cl, cr] = runFx (comp, sine, silence, silence, 120.0);
        check (analyse (cl.data() + secs (1.0), secs (0.5)).rms < analyse (sine.data() + secs (1.0), secs (0.5)).rms * 0.5,
               "Kompressor regelt runter");
    }

    //==========================================================================
    std::cout << "\n6) Choke und Stresstest\n";
    {
        DrumTrack oh, ch;
        oh.setTrack (trackInfos()[openHatTrack]); oh.setParams (makeDefaultTrackParams (openHatTrack)); oh.prepare (sampleRate);
        std::vector<float> l ((size_t) secs (1.0)), r (l.size()), d (l.size()), v (l.size());
        oh.trigger (1.0f);
        oh.render (l.data(), r.data(), d.data(), v.data(), secs (0.2));
        oh.choke();
        oh.render (l.data() + secs (0.2), r.data() + secs (0.2), d.data() + secs (0.2), v.data() + secs (0.2), secs (0.8));
        check (analyse (l.data() + secs (0.25), secs (0.3)).peak < 0.001f, "Choke stoppt die Open Hat");

        std::array<DrumTrack, numTracks> tracks;
        for (int t = 0; t < numTracks; ++t)
        {
            tracks[(size_t) t].setTrack (trackInfos()[(size_t) t]);
            tracks[(size_t) t].setParams (makeDefaultTrackParams (t));
            tracks[(size_t) t].prepare (sampleRate);
        }

        MasterFx fx;
        fx.prepare (sampleRate);
        std::mt19937 gen (7);
        std::uniform_real_distribution<float> u (0.0f, 1.0f);
        const auto sample = makeSample();
        std::array<float, 256> L {}, R {}, D {}, V {};
        bool finite = true;
        float peak = 0.0f;

        for (int block = 0; block < (int) (30.0 * sampleRate / 256); ++block)
        {
            L.fill (0.0f); R.fill (0.0f); D.fill (0.0f); V.fill (0.0f);
            for (int t = 0; t < numTracks; ++t)
            {
                if (u (gen) < 0.04f)
                {
                    TrackParams p;
                    for (int pg = 0; pg < numPages; ++pg)
                        for (int i = 0; i < knobsPerPage; ++i)
                        {
                            const auto& def = pageDefs (t, (Page) pg)[(size_t) i];
                            p[(Page) pg][(size_t) i] = def.min + u (gen) * (def.max - def.min);
                        }
                    p[Page::filter][flt::type] = std::floor (p[Page::filter][flt::type]);
                    tracks[(size_t) t].setParams (p);
                    if (u (gen) < 0.3f) tracks[(size_t) t].setSample (u (gen) < 0.5f ? sample : nullptr);
                }
                if (u (gen) < 0.06f) tracks[(size_t) t].trigger (u (gen));
                if (u (gen) < 0.03f) tracks[(size_t) t].release();
            }

            if (u (gen) < 0.02f)
            {
                MasterValues mv {};
                for (size_t i = 0; i < mv.size(); ++i)
                    mv[i] = masterDefs()[i].min + u (gen) * (masterDefs()[i].max - masterDefs()[i].min);
                mv[mst::dlyTime] = std::floor (mv[mst::dlyTime]);
                fx.setParameters (mv, 80.0 + u (gen) * 100.0);
            }

            for (auto& tr : tracks)
                tr.render (L.data(), R.data(), D.data(), V.data(), 256);
            fx.process (L.data(), R.data(), D.data(), V.data(), 256);

            const auto s = analyse (L.data(), 256);
            finite = finite && s.finite;
            peak = std::max (peak, s.peak);
        }
        check (finite, "30 s Zufall auf allen Parametern + FX: keine NaN/Inf");
        check (peak < 40.0f, "Summenpegel begrenzt (Peak vor Master-Clip " + juce::String (peak, 1) + ")");
    }

    std::cout << "\n" << (failures == 0 ? "Alle Checks bestanden." : juce::String (failures) + " Check(s) fehlgeschlagen.") << "\n";
    return failures == 0 ? 0 : 1;
}
