#include "elapsedMillis.h"

bool elapsedMillisAudio::virtualTime = 0;
unsigned long elapsedMillisAudio::virtualMicros = 0;

bool elapsedMicrosAudio::virtualTime = 0;
unsigned long elapsedMicrosAudio::virtualMicros = 0;

Timer* elapsedMillis::timer = nullptr;
std::vector<elapsedMillis*> elapsedMillis::uninitializedObjects;
std::vector<unsigned long> elapsedMillis::startValue;

Timer* elapsedMicros::timer = nullptr;
std::vector<elapsedMicros*> elapsedMicros::uninitializedObjects;
std::vector<unsigned long> elapsedMicros::startValue;

Timer* elapsedMillisAudio::timer = nullptr;
std::vector<elapsedMillisAudio*> elapsedMillisAudio::uninitializedObjects;
std::vector<unsigned long> elapsedMillisAudio::startValue;

Timer* elapsedMicrosAudio::timer = nullptr;
std::vector<elapsedMicrosAudio*> elapsedMicrosAudio::uninitializedObjects;
std::vector<unsigned long> elapsedMicrosAudio::startValue;
