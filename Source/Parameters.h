#pragma once

#include "ParamDefs.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace takt::params
{

inline juce::String trackPrefix (int track)
{
    return juce::String (trackInfos()[(size_t) track].shortName).toLowerCase();
}

/** e.g. "bd_syn_swd", "sd_flt_freq", "ch_amp_vol" */
inline juce::String paramID (int track, Page page, int index)
{
    return trackPrefix (track) + "_" + pageIds[(size_t) page] + "_" + pageDefs (track, page)[(size_t) index].id;
}

inline juce::String masterParamID (int index) { return masterDefs()[(size_t) index].id; }

inline const juce::String masterVolumeID { "master_volume" };

inline juce::String utf8 (const char* text) { return juce::String::fromUTF8 (text); }

//==============================================================================
inline juce::String formatSeconds (float s)
{
    if (s < 0.1f)  return juce::String (s * 1000.0f, s < 0.01f ? 1 : 0) + " ms";
    if (s < 1.0f)  return juce::String (juce::roundToInt (s * 1000.0f)) + " ms";
    return juce::String (s, 2) + " s";
}

inline juce::String formatValue (const ParamDef& def, float v)
{
    using juce::String;

    if (def.infiniteAtMax && v >= def.max - 0.0001f)
        return "Inf";

    switch (def.unit)
    {
        case Unit::percent:        return String (juce::roundToInt (v * 100.0f)) + " %";
        case Unit::bipolarPercent: { const auto p = juce::roundToInt (v * 100.0f); return (p > 0 ? "+" : "") + String (p) + " %"; }
        case Unit::semitones:      return (def.min < 0.0f && v > 0.0f ? "+" : "") + String (v, std::abs (v) >= 10.0f ? 0 : 1) + " st";
        case Unit::cents:          return (v > 0.0f ? "+" : "") + String (juce::roundToInt (v)) + " ct";
        case Unit::seconds:        return formatSeconds (v);
        case Unit::hertz:          return v >= 1000.0f ? String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz"
                                                       : String (juce::roundToInt (v)) + " Hz";
        case Unit::bits:           return juce::roundToInt (v) >= 16 ? utf8 ("Aus") : String (juce::roundToInt (v)) + " bit";
        case Unit::decimation:     return juce::roundToInt (v) <= 1 ? utf8 ("Aus") : String (juce::roundToInt (v)) + "x";
        case Unit::ratio:          return String (v, v >= 10.0f ? 0 : 1) + ":1";
        case Unit::decibels:       return String (v, 1) + " dB";
        case Unit::pan:            { const auto a = juce::roundToInt (std::abs (v) * 100.0f); return a == 0 ? String ("C") : String (a) + (v < 0.0f ? " L" : " R"); }
        case Unit::count:          return String (juce::roundToInt (v));
        case Unit::choice:         return {};
    }
    return String (v, 2);
}

//==============================================================================
inline std::unique_ptr<juce::RangedAudioParameter> makeParameter (const juce::String& id, const juce::String& name,
                                                                  const ParamDef& def, float defaultValue)
{
    using namespace juce;

    if (def.unit == Unit::choice)
    {
        StringArray choices;
        choices.addTokens (utf8 (def.choices), "|", {});
        return std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, choices, roundToInt (defaultValue));
    }

    NormalisableRange<float> range { def.min, def.max, def.step };
    if (def.centre > def.min && def.centre < def.max)
        range.setSkewForCentre (def.centre);

    const ParamDef copy = def;
    auto attributes = AudioParameterFloatAttributes().withStringFromValueFunction (
        [copy] (float v, int) { return formatValue (copy, v); });

    return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, range, defaultValue, attributes);
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    for (int t = 0; t < numTracks; ++t)
    {
        const auto& info = trackInfos()[(size_t) t];
        auto group = std::make_unique<AudioProcessorParameterGroup> (trackPrefix (t),
                                                                     String (info.shortName) + " " + info.longName, " | ");

        for (int pg = 0; pg < numPages; ++pg)
        {
            const auto page = (Page) pg;
            const auto& defs = pageDefs (t, page);
            const auto pageName = String (pageNames[(size_t) pg]).substring (0, 1) + String (pageNames[(size_t) pg]).substring (1).toLowerCase();

            for (int i = 0; i < knobsPerPage; ++i)
            {
                const auto& def = defs[(size_t) i];
                const auto name = String (info.shortName) + " " + pageName + " " + utf8 (def.name);
                group->addChild (makeParameter (paramID (t, page, i), name, def, defaultValue (t, page, i)));
            }
        }

        layout.add (std::move (group));
    }

    auto master = std::make_unique<AudioProcessorParameterGroup> ("master", "FX und Master", " | ");
    for (int i = 0; i < mst::count; ++i)
    {
        const auto& def = masterDefs()[(size_t) i];
        String section;
        for (const auto& s : masterSections())
            if (i >= s.first && i < s.first + s.count)
                section = String (s.title).substring (0, 1) + String (s.title).substring (1).toLowerCase();

        master->addChild (makeParameter (masterParamID (i), section + " " + utf8 (def.name), def, def.def));
    }

    master->addChild (std::make_unique<AudioParameterFloat> (ParameterID { masterVolumeID, 1 }, "Master Volume",
                                                             NormalisableRange<float> { -48.0f, 6.0f, 0.1f }, 0.0f,
                                                             AudioParameterFloatAttributes().withStringFromValueFunction (
                                                                 [] (float v, int) { return String (v, 1) + " dB"; })));
    layout.add (std::move (master));
    return layout;
}

} // namespace takt::params
