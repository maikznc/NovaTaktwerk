#pragma once

#include "TrackInfo.h"

#include <array>
#include <cstddef>

using std::size_t;

/*
    All sound parameters of NOVA Taktwerk, as plain tables (no JUCE dependency).

    Every track has four pages with 8 knobs each, like on the original hardware:
        SYNTH   Level + 7 parameters that depend on the voice (kick, snare, hats …)
        SAMPLE  the sample layer
        FILTER  multimode filter with its own ADSR envelope
        AMP     amp envelope, overdrive, FX sends, pan and volume

    Plus a global FX / master section (distortion, compressor, reverb, delay).
*/

namespace takt
{

enum class Unit { percent, bipolarPercent, semitones, cents, seconds, hertz, bits, decimation, ratio, decibels, pan, count, choice };

struct ParamDef
{
    const char* id;
    const char* name;
    const char* tooltip;
    float min, max, def;
    float centre = 0.0f;              // value at the knob's middle position (0 = linear knob)
    float step = 0.0f;                // 0 = continuous
    Unit unit = Unit::percent;
    const char* choices = nullptr;    // "A|B|C" for Unit::choice
    bool infiniteAtMax = false;       // shows "Inf" at the maximum
};

inline constexpr int knobsPerPage = 8;
inline constexpr int numPages = 4;

enum class Page { synth, sample, filter, amp };

inline constexpr std::array<const char*, numPages> pageNames { "SYNTH", "SAMPLE", "FILTER", "AMP" };
inline constexpr std::array<const char*, numPages> pageIds   { "syn", "smp", "flt", "amp" };

using PageValues = std::array<float, knobsPerPage>;
using PageDefs   = std::array<ParamDef, knobsPerPage>;

/** All values of one track, in real units (seconds, Hz, semitones …). */
struct TrackParams
{
    std::array<PageValues, numPages> pages {};

    PageValues&       operator[] (Page p)       noexcept { return pages[(size_t) p]; }
    const PageValues& operator[] (Page p) const noexcept { return pages[(size_t) p]; }
};

//==============================================================================
// Knob indices per page

namespace syn   { enum : int { level = 0, tune = 1, decay = 2 }; }               // same on every voice
namespace tonal { enum : int { hold = 3, sweep = 4, sweepTime = 5, snap = 6, wave = 7 }; }   // kick + toms
namespace snr   { enum : int { detune = 3, snap = 4, noiseDecay = 5, noiseTone = 6, sweep = 7 }; }
namespace rim   { enum : int { balance = 3, interval = 4, snap = 5, sweep = 6, cut = 7 }; }
namespace clp   { enum : int { tone = 3, claps = 4, rate = 5, snap = 6, body = 7 }; }
namespace metal { enum : int { tone = 3, cut = 4, color = 5, noise = 6, snap = 7 }; }
namespace bell  { enum : int { interval = 3, tone = 4, color = 5, snap = 6, hold = 7 }; }

namespace smp { enum : int { level, tune, fine, bits, decim, start, end, loop }; }
namespace flt { enum : int { attack, decay, sustain, release, freq, reso, type, env }; }
namespace amp { enum : int { attack, hold, decay, drive, delay, reverb, pan, volume }; }

enum class FilterType { lp24, lp12, bp, hp12, hp24, bs, pk };

//==============================================================================
namespace defs
{
    inline ParamDef level()
    {
        return { "lev", "Level", "Lautstärke der Synth-Stimme", 0.0f, 1.0f, 1.0f };
    }

    inline ParamDef tune (const char* tip = "Tonhöhe in Halbtönen")
    {
        return { "tune", "Tune", tip, -24.0f, 24.0f, 0.0f, 0.0f, 0.01f, Unit::semitones };
    }

    inline ParamDef decay (float mn, float mx, float d, float centre, const char* tip = "Ausklingzeit")
    {
        return { "dec", "Decay", tip, mn, mx, d, centre, 0.0f, Unit::seconds };
    }

    inline ParamDef pct (const char* id, const char* name, const char* tip, float d)
    {
        return { id, name, tip, 0.0f, 1.0f, d };
    }

    inline PageDefs tonal (bool isKick)
    {
        return {{
            level(),
            tune (isKick ? "Grundton der Kick in Halbtönen (0 = 50 Hz)" : "Tonhöhe in Halbtönen"),
            decay (0.03f, 4.0f, isKick ? 0.45f : 0.45f, 0.5f, "Ausklingzeit nach dem Hold"),
            { "hold", "Hold", "Hält die volle Lautstärke, bevor das Decay startet – mehr Druck und Länge",
              0.0f, 0.5f, isKick ? 0.05f : 0.0f, 0.08f, 0.0f, Unit::seconds },
            { "swd", "Sweep", "Wie weit die Tonhöhe beim Anschlag von oben herunterfällt (Halbtöne)",
              0.0f, 60.0f, isKick ? 40.0f : 10.0f, 0.0f, 0.1f, Unit::semitones },
            { "swt", "Sw.Time", "Wie schnell der Pitch-Sweep fällt – kurz = knackig, lang = zappy",
              0.003f, 0.3f, isKick ? 0.04f : 0.12f, 0.05f, 0.0f, Unit::seconds },
            pct ("snap", "Snap", "Lautstärke des Anschlag-Clicks", isKick ? 0.55f : 0.2f),
            pct ("wave", "Wave", "Wellenform: 0 = reiner Sinus, hoch = übersteuert bis eckig", isKick ? 0.35f : 0.1f),
        }};
    }

    inline PageDefs snare()
    {
        return {{
            level(),
            tune(),
            decay (0.02f, 1.5f, 0.16f, 0.2f, "Ausklingzeit des Tonanteils"),
            { "det", "Detune", "Abstand des zweiten Oszillators in Halbtönen", 0.0f, 24.0f, 8.0f, 0.0f, 0.1f, Unit::semitones },
            pct ("snap", "Snap", "Lautstärke des Rauschanteils (Teppich)", 0.65f),
            { "ndec", "N.Decay", "Ausklingzeit des Rauschens", 0.02f, 2.0f, 0.22f, 0.3f, 0.0f, Unit::seconds },
            pct ("ntone", "N.Tone", "Helligkeit des Rauschens", 0.6f),
            { "swd", "Sweep", "Pitch-Abfall beim Anschlag in Halbtönen", 0.0f, 24.0f, 6.0f, 0.0f, 0.1f, Unit::semitones },
        }};
    }

    inline PageDefs rimshot()
    {
        return {{
            level(),
            tune(),
            decay (0.005f, 0.4f, 0.035f, 0.05f),
            pct ("bal", "Balance", "Mischung der zwei Töne", 0.5f),
            { "intv", "Interval", "Abstand des zweiten Tons in Halbtönen", 0.0f, 36.0f, 21.5f, 0.0f, 0.1f, Unit::semitones },
            pct ("snap", "Snap", "Lautstärke des Anschlag-Clicks", 0.4f),
            { "swd", "Sweep", "Pitch-Abfall beim Anschlag in Halbtönen", 0.0f, 24.0f, 2.0f, 0.0f, 0.1f, Unit::semitones },
            { "cut", "Cut", "Low-Cut: nimmt Bauch raus", 50.0f, 3000.0f, 350.0f, 400.0f, 0.0f, Unit::hertz },
        }};
    }

    inline PageDefs clap()
    {
        return {{
            level(),
            tune ("Verschiebt die Klangfarbe des Claps in Halbtönen"),
            decay (0.02f, 2.0f, 0.3f, 0.3f, "Länge der Fahne nach den Klatschern"),
            pct ("tone", "Tone", "Resonanz: hoch = schärfer, tonaler", 0.4f),
            { "claps", "Claps", "Anzahl der Klatscher", 1.0f, 8.0f, 4.0f, 0.0f, 1.0f, Unit::count },
            { "rate", "Rate", "Abstand der Klatscher", 0.002f, 0.03f, 0.009f, 0.01f, 0.0f, Unit::seconds },
            pct ("snap", "Snap", "Lautstärke der Klatscher gegenüber der Fahne", 0.6f),
            pct ("body", "Body", "Tiefer Rauschanteil für mehr Körper", 0.25f),
        }};
    }

    inline PageDefs metallic (VoiceType type)
    {
        const bool cymbal = type == VoiceType::cymbal;
        const bool closed = type == VoiceType::closedHat;

        return {{
            level(),
            tune(),
            closed ? decay (0.01f, 1.0f, 0.07f, 0.1f)
                   : (cymbal ? decay (0.1f, 8.0f, 1.8f, 1.5f) : decay (0.05f, 4.0f, 0.55f, 0.6f)),
            { "tone", "Tone", "Mittenfrequenz des Bandpasses", 1500.0f, 15000.0f, cymbal ? 5000.0f : 8000.0f, 6000.0f, 0.0f, Unit::hertz },
            { "cut", "Cut", "Low-Cut-Frequenz", 500.0f, 12000.0f, cymbal ? 3000.0f : 6500.0f, 4000.0f, 0.0f, Unit::hertz },
            pct ("color", "Color", "Resonanz: hoch = metallischer, klingelnder", cymbal ? 0.25f : 0.3f),
            pct ("noise", "Noise", "Rauschen statt Metall", cymbal ? 0.4f : 0.25f),
            pct ("snap", "Snap", "Anschlag-Transient", cymbal ? 0.2f : 0.3f),
        }};
    }

    inline PageDefs cowbell()
    {
        return {{
            level(),
            tune(),
            decay (0.02f, 2.0f, 0.3f, 0.3f),
            { "intv", "Interval", "Abstand der zwei Töne in Halbtönen", 0.0f, 24.0f, 6.8f, 0.0f, 0.1f, Unit::semitones },
            { "tone", "Tone", "Mittenfrequenz des Bandpasses", 400.0f, 6000.0f, 2100.0f, 1800.0f, 0.0f, Unit::hertz },
            pct ("color", "Color", "Resonanz des Bandpasses", 0.5f),
            pct ("snap", "Snap", "Lautstärke des kurzen Anschlags", 0.6f),
            { "hold", "Hold", "Hält die volle Lautstärke vor dem Decay", 0.0f, 0.3f, 0.0f, 0.05f, 0.0f, Unit::seconds },
        }};
    }
} // namespace defs

inline const PageDefs& synthDefs (VoiceType type)
{
    static const PageDefs kick    = defs::tonal (true);
    static const PageDefs tom     = defs::tonal (false);
    static const PageDefs snare   = defs::snare();
    static const PageDefs rimshot = defs::rimshot();
    static const PageDefs clap    = defs::clap();
    static const PageDefs closed  = defs::metallic (VoiceType::closedHat);
    static const PageDefs open    = defs::metallic (VoiceType::openHat);
    static const PageDefs cymbal  = defs::metallic (VoiceType::cymbal);
    static const PageDefs bell    = defs::cowbell();

    switch (type)
    {
        case VoiceType::kick:      return kick;
        case VoiceType::snare:     return snare;
        case VoiceType::rim:       return rimshot;
        case VoiceType::clap:      return clap;
        case VoiceType::tom:       return tom;
        case VoiceType::closedHat: return closed;
        case VoiceType::openHat:   return open;
        case VoiceType::cymbal:    return cymbal;
        case VoiceType::cowbell:   return bell;
    }
    return kick;
}

inline const PageDefs& sampleDefs()
{
    static const PageDefs d {{
        { "lev",   "Level",  "Lautstärke des Sample-Layers", 0.0f, 1.0f, 0.8f },
        { "tune",  "Tune",   "Tonhöhe des Samples in Halbtönen", -24.0f, 24.0f, 0.0f, 0.0f, 1.0f, Unit::semitones },
        { "fine",  "Fine",   "Feinstimmung in Cent", -100.0f, 100.0f, 0.0f, 0.0f, 1.0f, Unit::cents },
        { "bits",  "Bit Rd", "Bit-Reduktion: weniger Bits = dreckiger", 2.0f, 16.0f, 16.0f, 0.0f, 1.0f, Unit::bits },
        { "decim", "Decim",  "Samplerate-Reduktion: höher = mehr Aliasing und Crunch", 1.0f, 32.0f, 1.0f, 6.0f, 1.0f, Unit::decimation },
        { "start", "Start",  "Startpunkt im Sample", 0.0f, 1.0f, 0.0f },
        { "end",   "End",    "Endpunkt im Sample", 0.0f, 1.0f, 1.0f },
        { "loop",  "Loop",   "Sample zwischen Start und End wiederholen", 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, Unit::choice, "Aus|An" },
    }};
    return d;
}

inline const PageDefs& filterDefs()
{
    static const PageDefs d {{
        { "atk",  "Attack",  "Anstieg der Filter-Hüllkurve", 0.0f, 2.0f, 0.0f, 0.1f, 0.0f, Unit::seconds },
        { "dec",  "Decay",   "Abfall der Filter-Hüllkurve auf das Sustain", 0.005f, 4.0f, 0.3f, 0.3f, 0.0f, Unit::seconds },
        { "sus",  "Sustain", "Haltepegel der Filter-Hüllkurve, solange die Note gedrückt ist", 0.0f, 1.0f, 0.0f },
        { "rel",  "Release", "Ausklingen der Filter-Hüllkurve nach dem Loslassen", 0.005f, 4.0f, 0.3f, 0.3f, 0.0f, Unit::seconds },
        { "freq", "Freq",    "Filterfrequenz (wirkt auf Synth und Sample)", 20.0f, 20000.0f, 20000.0f, 1000.0f, 0.0f, Unit::hertz },
        { "res",  "Reso",    "Filterresonanz", 0.0f, 1.0f, 0.1f },
        { "type", "Type",    "Filtertyp: Low-Pass 24/12 dB, Band-Pass, High-Pass 12/24 dB, Band-Stop, Peak",
          0.0f, 6.0f, 0.0f, 0.0f, 1.0f, Unit::choice, "LP 24|LP 12|BP|HP 12|HP 24|BS|PK" },
        { "env",  "Env",     "Wie stark die Hüllkurve die Filterfrequenz bewegt (+/- bis 7 Oktaven)",
          -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, Unit::bipolarPercent },
    }};
    return d;
}

inline const PageDefs& ampDefs()
{
    static const PageDefs d {{
        { "atk",  "Attack", "Einblendzeit", 0.0f, 1.0f, 0.0f, 0.05f, 0.0f, Unit::seconds },
        { "hold", "Hold",   "Haltezeit vor dem Decay (ganz rechts = unendlich)", 0.0f, 2.0f, 2.0f, 0.25f, 0.0f, Unit::seconds, nullptr, true },
        { "dec",  "Decay",  "Ausklingzeit der Spur – schneidet lange Sounds ab (ganz rechts = unendlich)",
          0.01f, 10.0f, 10.0f, 0.6f, 0.0f, Unit::seconds, nullptr, true },
        { "od",   "Drive",  "Overdrive der Spur – Dreck, Druck, Obertöne", 0.0f, 1.0f, 0.1f },
        { "dly",  "Delay",  "Send zum Delay", 0.0f, 1.0f, 0.0f },
        { "rev",  "Reverb", "Send zum Reverb (für Rumble-Kicks)", 0.0f, 1.0f, 0.0f },
        { "pan",  "Pan",    "Stereoposition", -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, Unit::pan },
        { "vol",  "Volume", "Lautstärke der Spur", 0.0f, 1.0f, 0.8f },
    }};
    return d;
}

inline const PageDefs& pageDefs (int track, Page page)
{
    switch (page)
    {
        case Page::synth:  return synthDefs (trackInfos()[(size_t) track].type);
        case Page::sample: return sampleDefs();
        case Page::filter: return filterDefs();
        case Page::amp:    return ampDefs();
    }
    return sampleDefs();
}

/** Default value of a knob, with the per-track exceptions (overdrive amount). */
inline float defaultValue (int track, Page page, int index)
{
    if (page == Page::amp && index == amp::drive)
        return trackInfos()[(size_t) track].defaultDrive;

    return pageDefs (track, page)[(size_t) index].def;
}

inline TrackParams makeDefaultTrackParams (int track)
{
    TrackParams p;
    for (int pg = 0; pg < numPages; ++pg)
        for (int i = 0; i < knobsPerPage; ++i)
            p[(Page) pg][(size_t) i] = defaultValue (track, (Page) pg, i);
    return p;
}

//==============================================================================
// FX / master section

namespace mst
{
    enum : int
    {
        distDrive, distSym, distTone, distRevPost,
        compThreshold, compRatio, compAttack, compRelease, compMakeup, compScHpf, compMix,
        revPreDelay, revDecay, revDamp, revHpf, revLpf, revDrive, revDuck, revVolume,
        dlyTime, dlyPingPong, dlyFeedback, dlyHpf, dlyLpf, dlyVolume,
        count
    };
}

using MasterValues = std::array<float, mst::count>;

struct MasterSection { const char* title; int first; int count; };

inline const std::array<MasterSection, 4>& masterSections()
{
    // display order: row 1 = distortion + reverb, row 2 = compressor + delay
    static const std::array<MasterSection, 4> s {{
        { "DISTORTION", mst::distDrive,     4 },
        { "REVERB",     mst::revPreDelay,   8 },
        { "KOMPRESSOR", mst::compThreshold, 7 },
        { "DELAY",      mst::dlyTime,       6 },
    }};
    return s;
}

/** Delay times in beats, matching the "Time" choices. */
inline constexpr std::array<float, 11> delayBeats { 0.125f, 1.0f / 6.0f, 0.25f, 1.0f / 3.0f, 0.375f, 0.5f,
                                                    2.0f / 3.0f, 0.75f, 1.0f, 1.5f, 2.0f };

inline const std::array<ParamDef, mst::count>& masterDefs()
{
    static const std::array<ParamDef, mst::count> d {{
        { "dist_drive", "Drive",     "Analoge Verzerrung auf der ganzen Summe", 0.0f, 1.0f, 0.2f },
        { "dist_sym",   "Sym",       "Symmetrie der Verzerrung: aus der Mitte = asymmetrisch, mehr gerade Obertöne und Fuzz",
          -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, Unit::bipolarPercent },
        { "dist_tone",  "Tone",      "Helligkeit nach der Verzerrung", 0.0f, 1.0f, 0.65f },
        { "dist_rev",   "Rev Post",  "Aus = Reverb läuft durch Distortion und Kompressor (Rumble). An = Reverb kommt sauber danach",
          0.0f, 1.0f, 0.0f, 0.0f, 1.0f, Unit::choice, "Aus|An" },

        { "comp_thr",   "Threshold", "Ab hier greift der Kompressor", -40.0f, 0.0f, -10.0f, 0.0f, 0.1f, Unit::decibels },
        { "comp_ratio", "Ratio",     "Stärke der Kompression", 1.0f, 20.0f, 3.0f, 4.0f, 0.0f, Unit::ratio },
        { "comp_atk",   "Attack",    "Wie schnell der Kompressor zupackt – länger = mehr Kick-Transient bleibt",
          0.0001f, 0.1f, 0.005f, 0.01f, 0.0f, Unit::seconds },
        { "comp_rel",   "Release",   "Wie schnell er loslässt – bestimmt das Pumpen", 0.01f, 1.0f, 0.15f, 0.15f, 0.0f, Unit::seconds },
        { "comp_mup",   "Makeup",    "Aufholverstärkung", 0.0f, 24.0f, 2.0f, 0.0f, 0.1f, Unit::decibels },
        { "comp_sc",    "SC HPF",    "Low-Cut im Sidechain: höher = Bass löst weniger Kompression aus", 20.0f, 500.0f, 20.0f, 100.0f, 0.0f, Unit::hertz },
        { "comp_mix",   "Mix",       "Parallel-Kompression: Mischung aus trocken und komprimiert", 0.0f, 1.0f, 1.0f },

        { "rev_pre",    "Pre-Dly",   "Verzögerung, bevor der Hall einsetzt", 0.0f, 0.2f, 0.01f, 0.03f, 0.0f, Unit::seconds },
        { "rev_dec",    "Decay",     "Raumgröße / Halllänge", 0.0f, 1.0f, 0.6f },
        { "rev_damp",   "Damp",      "Dämpfung der Höhen im Hall", 0.0f, 1.0f, 0.5f },
        { "rev_hpf",    "HPF",       "Low-Cut vor dem Hall", 20.0f, 2000.0f, 40.0f, 200.0f, 0.0f, Unit::hertz },
        { "rev_lpf",    "LPF",       "High-Cut vor dem Hall – für Rumble ganz runter (200–400 Hz)", 200.0f, 20000.0f, 9000.0f, 2000.0f, 0.0f, Unit::hertz },
        { "rev_drive",  "Drive",     "Verzerrt nur den Hall – dreckiger Rumble, während die Kick sauber durchschlägt", 0.0f, 1.0f, 0.0f },
        { "rev_duck",   "Duck",      "Duckt den Hall, solange die trockenen Sounds spielen – so drückt sich die Kick durch den Rumble",
          0.0f, 1.0f, 0.0f },
        { "rev_vol",    "Volume",    "Lautstärke des Halls", 0.0f, 1.0f, 0.7f },

        { "dly_time",   "Time",      "Delayzeit im Songtempo", 0.0f, 10.0f, 7.0f, 0.0f, 1.0f, Unit::choice,
          "1/32|1/16T|1/16|1/8T|1/16.|1/8|1/4T|1/8.|1/4|1/4.|1/2" },
        { "dly_pp",     "Ping-Pong", "Echos springen zwischen links und rechts", 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, Unit::choice, "Aus|An" },
        { "dly_fb",     "Feedback",  "Anzahl der Wiederholungen", 0.0f, 1.0f, 0.45f },
        { "dly_hpf",    "HPF",       "Low-Cut in den Echos", 20.0f, 2000.0f, 150.0f, 200.0f, 0.0f, Unit::hertz },
        { "dly_lpf",    "LPF",       "High-Cut in den Echos", 500.0f, 20000.0f, 6000.0f, 4000.0f, 0.0f, Unit::hertz },
        { "dly_vol",    "Volume",    "Lautstärke des Delays", 0.0f, 1.0f, 0.6f },
    }};
    return d;
}

inline MasterValues makeDefaultMasterValues()
{
    MasterValues v {};
    for (size_t i = 0; i < v.size(); ++i)
        v[i] = masterDefs()[i].def;
    return v;
}

} // namespace takt
