#include "ExportQueues.h"

AudioAnalyzeRMS* ExportQueues::getChannelRms(ExportChannels channel)
{
    return &channelRMS[channel];
}

AudioRecordQueue* ExportQueues::getQueue(ExportChannels channel, mtAudioExport::enStem stem)
{
    return &queue[channel][stem];
}
