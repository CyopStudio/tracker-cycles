#ifndef AUDIOENGINE_EXPORTQUEUES_H_
#define AUDIOENGINE_EXPORTQUEUES_H_

#include "Audio.h"
#include "mtAudioExport.h"

class ExportQueues
{
  public:
    enum ExportChannels
    {
        kLeft,
        kRight,
        kChannelsCount
    };

    AudioAnalyzeRMS* getChannelRms(ExportChannels channel);
    AudioRecordQueue* getQueue(ExportChannels channel, mtAudioExport::enStem stem);

  private:
    AudioAnalyzeRMS channelRMS[ExportChannels::kChannelsCount];
    AudioRecordQueue queue[ExportChannels::kChannelsCount][mtAudioExport::stem_totalCount];
};

#endif /* AUDIOENGINE_EXPORTQUEUES_H_ */
