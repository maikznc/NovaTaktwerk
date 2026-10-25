#pragma once

#include "ParamDefs.h"

#include <vector>

namespace takt
{

/**
    Factory kits. Every kit starts from the default settings ("Hard Techno") and changes only what is listed.
*/
struct Kit
{
    struct TrackValue  { int track; Page page; int index; float value; };
    struct MasterValue { int index; float value; };

    const char* name;
    std::vector<TrackValue> tracks;
    std::vector<MasterValue> master;
};

namespace kitTracks { enum : int { BD, SD, RS, CP, BT, LT, MT, HT, CH, OH, CY, CB }; }

inline const std::vector<Kit>& factoryKits()
{
    using namespace kitTracks;
    using P = Page;

    static const std::vector<Kit> kits {
        { "Hard Techno", {}, {} },

        { "Rumble",
          {
              { BD, P::synth, syn::tune, -2.0f },  { BD, P::synth, syn::decay, 0.35f },
              { BD, P::synth, tonal::hold, 0.03f }, { BD, P::synth, tonal::sweep, 46.0f },
              { BD, P::synth, tonal::sweepTime, 0.025f }, { BD, P::synth, tonal::snap, 0.7f },
              { BD, P::synth, tonal::wave, 0.55f },
              { BD, P::amp, amp::drive, 0.55f }, { BD, P::amp, amp::reverb, 0.7f },
          },
          {
              { mst::distDrive, 0.15f }, { mst::distRevPost, 1.0f },
              { mst::compThreshold, -14.0f }, { mst::compRatio, 4.0f }, { mst::compAttack, 0.006f },
              { mst::compRelease, 0.15f }, { mst::compMakeup, 3.0f },
              { mst::revPreDelay, 0.0f }, { mst::revDecay, 0.85f }, { mst::revDamp, 0.75f },
              { mst::revHpf, 25.0f }, { mst::revLpf, 300.0f }, { mst::revDrive, 0.75f },
              { mst::revDuck, 0.9f }, { mst::revVolume, 0.75f },
          } },

        { "Industrial",
          {
              { BD, P::synth, syn::decay, 0.35f }, { BD, P::synth, tonal::hold, 0.08f },
              { BD, P::amp, amp::hold, 0.12f }, { BD, P::amp, amp::decay, 0.18f },
              { BD, P::synth, tonal::sweep, 52.0f }, { BD, P::synth, tonal::sweepTime, 0.02f },
              { BD, P::synth, tonal::snap, 0.9f }, { BD, P::synth, tonal::wave, 0.85f },
              { BD, P::amp, amp::drive, 0.85f },
              { BD, P::filter, flt::freq, 9000.0f }, { BD, P::filter, flt::reso, 0.3f },

              { SD, P::synth, snr::snap, 0.85f }, { SD, P::synth, snr::noiseTone, 0.85f },
              { SD, P::synth, snr::noiseDecay, 0.35f }, { SD, P::amp, amp::drive, 0.6f },

              { CP, P::synth, clp::claps, 6.0f }, { CP, P::synth, clp::rate, 0.012f },
              { CP, P::synth, syn::decay, 0.45f }, { CP, P::synth, clp::tone, 0.7f },
              { CP, P::amp, amp::drive, 0.5f }, { CP, P::amp, amp::reverb, 0.3f },

              { BT, P::synth, tonal::wave, 0.6f }, { BT, P::amp, amp::drive, 0.5f },
              { LT, P::synth, tonal::wave, 0.6f }, { LT, P::amp, amp::drive, 0.5f },
              { MT, P::synth, tonal::wave, 0.6f }, { MT, P::amp, amp::drive, 0.5f },
              { HT, P::synth, tonal::wave, 0.6f }, { HT, P::amp, amp::drive, 0.5f },

              { CH, P::synth, metal::color, 0.8f }, { CH, P::synth, metal::noise, 0.5f }, { CH, P::amp, amp::drive, 0.4f },
              { OH, P::synth, metal::color, 0.7f }, { OH, P::amp, amp::drive, 0.4f },
              { CB, P::amp, amp::drive, 0.7f },
          },
          {
              { mst::distDrive, 0.4f }, { mst::distSym, 0.45f }, { mst::distTone, 0.5f },
              { mst::compThreshold, -18.0f }, { mst::compRatio, 6.0f }, { mst::compAttack, 0.003f },
              { mst::compRelease, 0.1f }, { mst::compMakeup, 5.0f },
              { mst::revDecay, 0.7f }, { mst::revLpf, 4000.0f }, { mst::revVolume, 0.6f },
          } },

        { "Clean",
          {
              { BD, P::synth, tonal::wave, 0.0f }, { BD, P::synth, tonal::sweep, 30.0f },
              { BD, P::synth, tonal::snap, 0.3f }, { BD, P::synth, tonal::hold, 0.0f },
              { BD, P::amp, amp::drive, 0.0f }, { SD, P::amp, amp::drive, 0.0f }, { RS, P::amp, amp::drive, 0.0f },
              { CP, P::amp, amp::drive, 0.0f }, { BT, P::amp, amp::drive, 0.0f }, { LT, P::amp, amp::drive, 0.0f },
              { MT, P::amp, amp::drive, 0.0f }, { HT, P::amp, amp::drive, 0.0f }, { CH, P::amp, amp::drive, 0.0f },
              { OH, P::amp, amp::drive, 0.0f }, { CY, P::amp, amp::drive, 0.0f }, { CB, P::amp, amp::drive, 0.0f },
          },
          {
              { mst::distDrive, 0.0f }, { mst::compMix, 0.0f },
          } },
    };

    return kits;
}

} // namespace takt
