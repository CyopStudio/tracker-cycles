#ifndef AUDIOENGINE_AUDIOVOICE_H_
#define AUDIOENGINE_AUDIOVOICE_H_
#include "Audio.h"
#include "sum_players.h"

class AudioVoice
{
  public:
    enum EnvelopeGeneratorType
    {
        kFilter,
        kWavetablePosition,
        kGranularLength = kWavetablePosition,
        kGranularPosition,
        kPanning,
        kFinetune,
        kEnvelopeGeneratorCount
    };

#ifdef PLATFORM_IMX
    enum VoiceChannels
    {
        kLeft,
        kRight,
        kChannelsCount
    };
#endif

    AudioVoice();
#ifndef PLATFORM_IMX
    // Tracker Cycles polyphony (docs/tidal_voices.md): kPlayers sample
    // players per track, summed (with a gain each) into the track's one
    // crusher / filter / amp / pan chain. Player 0 is getMemoryPlayer(),
    // the only one the stock sequencer uses; the others play Tidal notes.
    static constexpr int kPlayers = 3;
    AudioPlayMemory* getPlayer(int k) { return k == 0 ? &memoryPlayer : &extraPlayers[k - 1]; }
    AudioSumPlayers<kPlayers>* getPlayerSum() { return &playerSum; }
    AudioPlayMemory* getMemoryPlayer();
    AudioBitDepth* getBitDepth();
    AudioFilterStateVariable* getFilter();
    AudioAmplifier* getAmplifier();
    MonoToStereo* getMonoToStereo();
    AudioAnalyzeRMS* getRMS();
#else
    AudioPlayMemory* getMemoryPlayer(VoiceChannels channel);
    AudioBitDepth* getBitDepth(VoiceChannels channel);
    AudioFilterStateVariable* getFilter(VoiceChannels channel);
    AudioAmplifier* getAmplifier(VoiceChannels channel);
    AudioAmplifier* getPanningAmplifier(VoiceChannels channel);
    AudioAnalyzeRMS* getRMS(VoiceChannels channel);
#endif
    envelopeGenerator* getEnvelopeGenerator(EnvelopeGeneratorType type);

  private:
#ifndef PLATFORM_IMX
    // declaration order = construction order = AudioStream update order:
    // all players, then their sum, then the chain
    AudioPlayMemory memoryPlayer;
    AudioPlayMemory extraPlayers[kPlayers - 1];
    AudioSumPlayers<kPlayers> playerSum;
    AudioBitDepth bitDepth;
    AudioFilterStateVariable filter;
    AudioAmplifier amplifier;
    MonoToStereo monoToStereo;
    AudioAnalyzeRMS rms;
#else
    AudioPlayMemory memoryPlayer[VoiceChannels::kChannelsCount];
    AudioBitDepth bitDepth[VoiceChannels::kChannelsCount];
    AudioFilterStateVariable filter[VoiceChannels::kChannelsCount];
    AudioAmplifier amplifier[VoiceChannels::kChannelsCount];
    AudioAmplifier panningAmplifier[VoiceChannels::kChannelsCount];
    AudioAnalyzeRMS rms[VoiceChannels::kChannelsCount];
#endif

    envelopeGenerator envelope[kEnvelopeGeneratorCount];
    std::vector<AudioConnection*> connections;
};

#endif /* AUDIOENGINE_AUDIOVOICE_H_ */
