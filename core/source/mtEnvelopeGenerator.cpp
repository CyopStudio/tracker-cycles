#include "mtEnvelopeGenerator.h"
#include "Platform.h"
#include "mtWavetableEnvelopes.h"
#include "util_math.h"
#include <stdint.h>

const envelopeGenerator::strEnv envelopeGenerator::s_defaultEnvelope = {
    .amount = 1.0f,
    .delay = 0,
    .attack = 0,
    .hold = 0,
    .decay = 0,
    .sustain = 1.0f,
    .release = 0,
    .loop = 0,
    .enable = 0
};

envelopeGenerator::envelopeGenerator() : envelope(const_cast<strEnv*>(&s_defaultEnvelope))
{
}

void envelopeGenerator::begin(Timer* timer)
{
    envelopeGenerator::timer = timer;
}

void envelopeGenerator::init(strEnv* _env)
{
    envelope = _env;
    decay = 0.0f;
    release = 0.0f;
}

void envelopeGenerator::start()
{
    // Serial.println("envelope.start()");
    keyPressed = 1;
    calc();
}

void envelopeGenerator::stop()
{
    keyPressed = 0;
    if (!envelope) return;
    if (envelope->loop == 1 || envelope->loop == 3) // LFOs
    {
        envTemp.phase = phase_nothing;
        envTemp.killOutput = 0;
    }
}

void envelopeGenerator::kill()
{
    // gdy jest noteOff i od razu noteOn maszyna stanów nie nadąża przejsc do release -
    // gdy jest zerowy release bieże 1.0 bo taki jest stan sustaina a powinna 0.0 bo nuta powinna
    // się skonczyc w nieskonczenie krotkim czasie gdy release = 0
    if (!envelope)
    {
        envTemp.killOutput = 0.0f;
        envTemp.phase = phase_nothing;
        envTemp.timer = 0;
        keyPressed = 0;
        return;
    }
    if ((envTemp.phase == phase_sustain) && (envelope->release == 0))
        envTemp.killOutput = 0.0f;
    else
        envTemp.killOutput = (envelope->loop == 1 || envelope->loop == 3) ? 0.0f : envTemp.output;
    envTemp.phase = phase_nothing;
    envTemp.timer = 0;
    keyPressed = 0;
}

void envelopeGenerator::killToZero()
{
    envTemp.output = 0.0f;
    envTemp.killOutput = 0.0f;
    envTemp.phase = phase_nothing;
    envTemp.timer = 0;
    keyPressed = 0;
}

void envelopeGenerator::calc()
{
    if (!envelope) return;
    if (envelope->loop == 2) // WT Env (One-shot or sustained wavetable envelope)
    {
        if (envTemp.phase == phase_nothing)
        {
            if (!keyPressed)
            {
                envTemp.tempOutput = 0.0f;
                envTemp.output = 0.0f;
                return;
            }
            envTemp.phase = phase_attack;
            envTemp.timer = 0;
        }

        amount = envelope->amount;
        float dur = (decay > 0.0f) ? decay : ((float)envelope->decay * timeMul);
        if (dur < 1.0f) dur = 1000.0f * timeMul;
        float rel = (release > 0.0f) ? release : ((float)envelope->release * timeMul);
        if (rel < 50.0f) rel = 50.0f * timeMul;

        if (envTemp.phase == phase_attack)
        {
            if (!keyPressed && wtSust > 0)
            {
                wtReleaseStartPhase = (dur > 0.0f) ? ((float)envTemp.timer / dur) : 1.0f;
                if (wtReleaseStartPhase > 1.0f) wtReleaseStartPhase = 1.0f;
                envTemp.phase = phase_release;
                envTemp.timer = 0;
            }
            else
            {
                float p = (dur > 0.0f) ? ((float)envTemp.timer / dur) : 1.0f;
                if (wtSust > 0)
                {
                    float pSust = (float)wtSust * 0.01f;
                    if (p >= pSust)
                    {
                        p = pSust;
                        envTemp.phase = phase_sustain;
                    }
                }
                else if (p >= 1.0f)
                {
                    p = 1.0f;
                    envTemp.phase = phase_nothing;
                }
                float y = evalWavetableEnvelopeFloat(p, wtShape, wtBend);
                envTemp.tempOutput = y * amount;
                envTemp.output = envTemp.tempOutput;
                return;
            }
        }

        if (envTemp.phase == phase_sustain)
        {
            if (!keyPressed)
            {
                wtReleaseStartPhase = (float)wtSust * 0.01f;
                envTemp.phase = phase_release;
                envTemp.timer = 0;
            }
            else
            {
                float y = evalWavetableEnvelopeFloat((float)wtSust * 0.01f, wtShape, wtBend);
                envTemp.tempOutput = y * amount;
                envTemp.output = envTemp.tempOutput;
                return;
            }
        }

        if (envTemp.phase == phase_release)
        {
            float relP = (rel > 0.0f) ? ((float)envTemp.timer / rel) : 1.0f;
            if (relP >= 1.0f)
            {
                envTemp.phase = phase_nothing;
                envTemp.tempOutput = 0.0f;
            }
            else
            {
                float p = wtReleaseStartPhase + relP * (1.0f - wtReleaseStartPhase);
                float y = evalWavetableEnvelopeFloat(p, wtShape, wtBend) * (1.0f - relP);
                envTemp.tempOutput = y * amount;
            }
            envTemp.output = envTemp.tempOutput;
            return;
        }

        envTemp.tempOutput = 0.0f;
        envTemp.output = 0.0f;
        return;
    }
    else if (envelope->loop == 3) // WT LFO (Looped wavetable LFO)
    {
        amount = envelope->amount;
        attack = (float)envelope->attack * timeMul;
        if (attack < 1000.0f) attack = 1000.0f;

        float p = 0.0f;
        if (isSync)
        {
            uint32_t ticksOnPeriod = (6912 / 12) * syncRate;
            if (ticksOnPeriod > 0)
            {
                uint32_t stepShift = (startStep) * (6912 / 12);
                p = (float)((currentSeqTicks + stepShift) % ticksOnPeriod) / (float)ticksOnPeriod;
            }
        }
        else
        {
            p = (attack > 0.0f) ? ((float)envTemp.timer / attack) : 0.0f;
            if (p >= 1.0f)
            {
                envTemp.timer = (uint32_t)attack > 0 ? (envTemp.timer % (uint32_t)attack) : 0;
                p = (attack > 0.0f) ? ((float)envTemp.timer / attack) : 0.0f;
            }
        }

        float y = evalWavetableEnvelopeFloat(p, wtShape, wtBend);
        envTemp.tempOutput = y * amount;
        envTemp.output = (2.0f * y - 1.0f) * amount;
        return;
    }

    // values
    amount = envelope->amount;   // float -1 - 1
    sustain = envelope->sustain; // float 0-1

    // time-dependent values
    delay = (float)envelope->delay * timeMul;
    attack = (float)envelope->attack * timeMul;
    hold = (float)envelope->hold * timeMul;
    decay = (float)envelope->decay * timeMul;
    release = (float)envelope->release * timeMul;

    _loop = envelope->loop;

    calcSyncSeq();

    if (envTemp.phase == phase_nothing)
    {
        if (keyPressed)
        {
            envTemp.phase = phase_predelay;
        }
        else
        {
            envTemp.timer = 0;
        }
    }

    if (envTemp.phase == phase_predelay)
    {
        if (keyPressed)
        {
            // etap trwa
            if (envTemp.timer < delay)
            {
                envTemp.phase = phase_predelay;
            }
            // etap skończony
            else
            {
                envTemp.timer = 0;
                envTemp.phase++;
            }
        }
        else
        {
            envTemp.timer = 0;
            envTemp.phase = 0;
        }
    }
    //	ATTACK
    if (envTemp.phase == phase_attack)
    {
        if (keyPressed)
        {
            // inkrementujemy timer
            // envTemp.timer += calcInt;

            // etap trwa
            if (envTemp.timer < attack)
            {
                envTemp.phase = phase_attack;
            }
            // etap skończony
            else
            {
                if (isRandom && !isSync)
                {
                    utilMathRandom::randomSeed(timer->micros());
                    randomValue = utilMathRandom::random(0, envelope->amount * 1000) / 1000.0;
                }
                envTemp.timer = 0;
                envTemp.phase++;
            }
        }
        // jesli puszczony przechodzi od razu do release
        else
        {
            envTemp.timer = 0;
            envTemp.phase = phase_release;
        }
    }
    //	HOLD
    if (envTemp.phase == phase_hold)
    {
        // wciśnięty, hold krótki
        if (keyPressed)
        {
            // inkrementujemy timer
            // envTemp.timer += calcInt;

            // etap trwa
            if (envTemp.timer <= hold)
            {
                envTemp.phase = phase_hold;
            }
            // etap skończony
            else
            {
                envTemp.timer = 0;
                if (isRandom)
                {
                    if (!isSync)
                    {
                        utilMathRandom::randomSeed(timer->micros());
                        randomValue = utilMathRandom::random(0, envelope->amount * 1000) / 1000.0;
                    }
                }
                else
                {
                    envTemp.phase++;
                }
            }
        }
        else
        {
            envTemp.timer = 0;
            envTemp.phase = phase_release;
        }
    }
    // DECAY
    if (envTemp.phase == phase_decay)
    {
        if (keyPressed)
        {
            // etap trwa
            if (envTemp.timer <= decay)
            {
                envTemp.phase = phase_decay;
            }
            // etap skończony
            else
            {
                envTemp.timer = 0;
                envTemp.phase++;
            }
        }

        // jesli puszczony przechodzi od razu do release
        else
        {
            envTemp.timer = 0;
            envTemp.phase = phase_release;
        }
    }

    // SUSTAIN
    if (envTemp.phase == phase_sustain)
    {
        // wciśnięty
        if (keyPressed && !_loop)
        {
            envTemp.timer = 1;
            envTemp.phase = phase_sustain;
        }

        else if (keyPressed && _loop)
        {
            envTemp.timer = 0;
            envTemp.phase = phase_release;
        }
        // puszczony, przechodzimy do release
        else
        {
            envTemp.timer = 0;
            envTemp.phase = phase_release;
        }
    }

    // RELEASE
    if (envTemp.phase == phase_release)
    {
        // etap trwa
        if (envTemp.timer <= release)
        {
            envTemp.phase = phase_release;
        }

        else if (_loop)
        {
            envTemp.timer = 0;
            envTemp.phase = phase_predelay;
        }
        // etap skończony
        else
        {
            envTemp.timer = 0;
            envTemp.phase = phase_nothing;
        }
    }

    // #ifdef ADSR_DEBUG
    // 	Serial.print(envTemp.timer);
    // #endif

    float tempOutput = 0;

    switch (envTemp.phase)
    {
        case phase_nothing:

            envTemp.tempOutput = 0;
            envTemp.maxOutput = envTemp.tempOutput;

            break;
        case phase_predelay:

            // #ifdef ADSR_DEBUG
            // 		Serial.print(" predelay");
            // #endif

            envTemp.tempOutput = envTemp.killOutput;

            envTemp.maxOutput = envTemp.tempOutput;

            break;
        case phase_attack:

            // #ifdef ADSR_DEBUG
            // 		Serial.print(" attack");
            // #endif

            // tempOutput = (envTemp.timer / attack) * amount ;
            if (attack > 0)
                tempOutput =
                  envTemp.killOutput + ((envTemp.timer / attack) * (amount - envTemp.killOutput));
            else
                tempOutput = amount; // przy attack - 0 powstawał nan i loop dzialal nieprawidlowo -
                                     // andrzej2000

            envTemp.tempOutput =
              isRandom ? randomValue : clamp<float>(tempOutput, envTemp.killOutput, MAX_OUTPUT);
            envTemp.maxOutput = envTemp.tempOutput;
            break;

        case phase_hold:
            // #ifdef ADSR_DEBUG
            // 		Serial.print(" hold");
            // #endif

            envTemp.tempOutput = isRandom ? randomValue : amount;
            envTemp.maxOutput = envTemp.tempOutput;
            break;

        case phase_decay:
            // #ifdef ADSR_DEBUG
            // 		Serial.print(" decay");
            // #endif

            if (decay > 0)
                tempOutput = (((1 - (envTemp.timer / decay)) * (1 - sustain) + sustain)) *
                             amount; // * (amount / MAX_ADSR_PAR);
            else
                tempOutput =
                  sustain *
                  amount; // przy decay - 0 powstawał nan i loop dzialal nieprawidlowo - andrzej2000

            // tempOutput = tempOutput * (amount > 0 ? 1 : -1);
            envTemp.tempOutput = clamp<float>(tempOutput, MIN_OUTPUT, MAX_OUTPUT);
            envTemp.maxOutput = envTemp.tempOutput;

            break;

        case phase_sustain:
            // #ifdef ADSR_DEBUG
            // 		Serial.print(" sustain");
            // #endif
            tempOutput = sustain * amount;
            envTemp.tempOutput = clamp<float>(tempOutput, MIN_OUTPUT, MAX_OUTPUT);
            envTemp.maxOutput = envTemp.tempOutput;

            break;
        case phase_release:

            // #ifdef ADSR_DEBUG
            // 		Serial.print(" release");
            // #endif

            if ((envTemp.maxOutput == 0.0f) && (sustain != 0.0f) && (amount != 0.0f))
                envTemp.maxOutput = sustain * amount; // andrzej 2000 - czasem wskakuje od razu w
                                                      // ten stan i jest 0 na envTemp.maxOutput
            else if ((_loop == 1) && (sustain == 0.0f))
                envTemp.maxOutput = 0.0f; // andrzej 2000 - czasem wpada tu z jedynką po holdzie bo
                                          // go zerowy decay nie sciaga do 0 i jest lipton

            if (release > 0)
            {
                tempOutput = (1 - (envTemp.timer / release)) * envTemp.maxOutput;
            }
            else
            {
                tempOutput = 0;
            }
            envTemp.tempOutput = clamp<float>(tempOutput, MIN_OUTPUT, MAX_OUTPUT);
            break;

        default: break;
    }

    // #ifdef ADSR_DEBUG
    // 	Serial.print(" - ");
    // 	Serial.print(envTemp.timer);
    // #endif

    // jesli modulacja targetuje w ten adsr
    // if (modTarget == (ch + 1))
    // {
    // 	envTemp.output = constrain(envTemp.tempOutput + mod, MIN_OUTPUT, MAX_OUTPUT);
    // }
    // else
    // {
    envTemp.output = _loop ? 2 * clamp<float>(envTemp.tempOutput, MIN_OUTPUT, MAX_OUTPUT) - amount
                           : clamp<float>(envTemp.tempOutput, MIN_OUTPUT, MAX_OUTPUT);
    // }

    // Serial.print("output: ");
    // Serial.println(envTemp.output, 3);

    // #ifdef ADSR_DEBUG
    // 	Serial.println();
    // #endif
}

float envelopeGenerator::fMap(float x, float in_min, float in_max, float out_min, float out_max)
{
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

float envelopeGenerator::getOut()
{
    if (!envelope || !envelope->enable)
        return 0.0;

    calc();
    return (envTemp.output);
}

float envelopeGenerator::getLastOut()
{
    // calc();
    if (!envelope || !envelope->enable)
        return 0.0;

    return (envTemp.output);
}

void envelopeGenerator::syncTrackerSeq(uint32_t val, float seqSpeed)
{
    currentSeqSpeed = seqSpeed;
    currentSeqTicks = val;
}

void envelopeGenerator::setSync(uint8_t state)
{
    isSync = state;
}
void envelopeGenerator::calcSyncSeq()
{
    if (!isSync)
        return;

    uint32_t ticksOnPeriod = (6912 / 12) * syncRate;
    if (ticksOnPeriod == 0)
        return;

    uint32_t stepShift = (startStep) * (6912 / 12);

    currentSeqTicks += stepShift;

    uint32_t currentPointInPhase = currentSeqTicks % ticksOnPeriod;

    float lfoFrequency = (currentSeqSpeed / 15.0);
    float periodTime = (1000000.0f / lfoFrequency) * syncRate;

    if (phaseNumber[0] != -1 && phaseNumber[1] != -1)
    {
        float halfPeriod = periodTime / 2;
        if (phaseNumber[0] == phase_attack)
        {
            attack = halfPeriod;
        }
        else if (phaseNumber[0] == phase_hold)
        {
            hold = halfPeriod;
        }

        if (phaseNumber[1] == phase_decay)
        {
            decay = halfPeriod;
        }
        else if (phaseNumber[1] == phase_release)
        {
            release = halfPeriod;
        }

        if (currentPointInPhase > ticksOnPeriod / 2)
        {
            envTemp.phase = phaseNumber[1];

            envTemp.timer =
              (halfPeriod) *
              (float)((float)(currentPointInPhase - ticksOnPeriod / 2.0f) / (ticksOnPeriod / 2.0f));
        }
        else
        {
            envTemp.phase = phaseNumber[0];
            envTemp.timer =
              (halfPeriod) * (float)((float)(currentPointInPhase) / (ticksOnPeriod / 2.0f));
        }
    }
    else if (phaseNumber[0] != -1)
    {
        if (phaseNumber[0] == phase_attack)
        {
            attack = periodTime;
        }
        else if (phaseNumber[0] == phase_decay)
        {
            decay = periodTime;
        }
        else if (phaseNumber[0] == phase_hold)
        {
            hold = periodTime;
        }

        envTemp.phase = phaseNumber[0];
        envTemp.timer = (periodTime) * (float)((float)currentPointInPhase / ticksOnPeriod);

        uint32_t tickSinceLastRandom = currentSeqTicks - lastRandomTick;
        if (isRandom && (tickSinceLastRandom >= ticksOnPeriod))
        {
            utilMathRandom::randomSeed(timer->micros());
            if (envelope)
                randomValue = utilMathRandom::random(0, envelope->amount * 1000) / 1000.0;

            lastRandomTick = currentSeqTicks;
        }
    }
}
void envelopeGenerator::setSyncStartStep(uint16_t n)
{
    startStep = n;
}
void envelopeGenerator::setPhaseNumbers(int8_t n1, int8_t n2)
{
    phaseNumber[0] = n1;
    phaseNumber[1] = n2;
}
void envelopeGenerator::setSyncRate(float sync)
{
    syncRate = sync;
}

void envelopeGenerator::setIsRandom(bool value)
{
    isRandom = value;
}

void envelopeGenerator::putMidiParameter(uint8_t ch, uint8_t parNo, uint8_t value)
{
    (void)ch;
    switch (parNo)
    {
    case par_predelay:
        delay = (float)value * timeMul;
        break;
    case par_attack:
        attack = (float)value * timeMul;
        if (attack < 1000.0f) attack = 1000.0f;
        break;
    case par_hold:
        hold = (float)value * timeMul;
        break;
    case par_decay:
        decay = (float)value * timeMul;
        break;
    case par_sustain:
        sustain = (float)value * 0.01f;
        break;
    case par_release:
        release = (float)value * timeMul;
        break;
    case par_amount:
        amount = (float)value;
        break;
    default:
        break;
    }
}

