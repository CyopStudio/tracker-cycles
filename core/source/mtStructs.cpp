#include "mtStructs.h"

void strMtValues::setDefaultTrackName(int trackIdx)
{
    assert(trackIdx < kMaxAllTracksInTrackers);

    if (trackIdx < kSampleTracks)
    {
        sprintf(TrackNames[trackIdx], "Track %d", trackIdx + 1);
    }
    else
    {
        sprintf(midiTrackNames[trackIdx - kSampleTracks], "Midi %d", trackIdx + 1);
    }
}
