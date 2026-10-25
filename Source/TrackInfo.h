#pragma once

#include <array>

namespace takt
{

enum class VoiceType
{
    kick,
    snare,
    rim,
    clap,
    tom,
    closedHat,
    openHat,
    cymbal,
    cowbell
};

/** Static description of one of the 12 drum tracks. */
struct TrackInfo
{
    const char* shortName;  // shown big on the pad, also used for parameter IDs
    const char* longName;   // shown small under the short name
    VoiceType   type;
    float       baseFreq;   // fundamental in Hz at Tune = 0 (tonal voices)
    float       defaultDrive;
    int         midiNote;   // General-MIDI drum note
};

inline constexpr int numTracks = 12;

inline const std::array<TrackInfo, numTracks>& trackInfos()
{
    static const std::array<TrackInfo, numTracks> infos {{
        { "BD", "Kick",       VoiceType::kick,      50.0f,  0.35f, 36 },
        { "SD", "Snare",      VoiceType::snare,     185.0f, 0.15f, 38 },
        { "RS", "Rimshot",    VoiceType::rim,       480.0f, 0.10f, 37 },
        { "CP", "Clap",       VoiceType::clap,      1.0f,   0.10f, 39 },
        { "BT", "Bass Tom",   VoiceType::tom,       62.0f,  0.10f, 41 },
        { "LT", "Low Tom",    VoiceType::tom,       92.0f,  0.10f, 43 },
        { "MT", "Mid Tom",    VoiceType::tom,       128.0f, 0.10f, 45 },
        { "HT", "High Tom",   VoiceType::tom,       172.0f, 0.10f, 47 },
        { "CH", "Closed Hat", VoiceType::closedHat, 1.0f,   0.05f, 42 },
        { "OH", "Open Hat",   VoiceType::openHat,   1.0f,   0.05f, 46 },
        { "CY", "Cymbal",     VoiceType::cymbal,    1.0f,   0.05f, 49 },
        { "CB", "Cowbell",    VoiceType::cowbell,   540.0f, 0.15f, 56 },
    }};
    return infos;
}

inline constexpr int closedHatTrack = 8;
inline constexpr int openHatTrack   = 9;

} // namespace takt
