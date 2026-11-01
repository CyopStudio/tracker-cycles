/* Audio Library for Teensy 3.X
 * Copyright (c) 2014, Paul Stoffregen, paul@pjrc.com
 *
 * Development of this audio library was funded by PJRC.COM, LLC by sales of
 * Teensy and Audio Adaptor boards.  Please support PJRC's efforts to develop
 * open source software by purchasing Teensy or other PJRC products.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice, development funding notice, and this permission
 * notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef Audio_h_
#define Audio_h_

#include "interruptsControl.h"
// When changing multiple audio object settings that must update at
// the same time, these functions allow the audio library interrupt
// to be disabled.  For example, you may wish to begin playing a note
// in response to reading an analog sensor.  If you have "velocity"
// information, you might start the sample playing and also adjust
// the gain of a mixer channel.  Use AudioNoInterrupts() first, then
// make both changes to the 2 separate objects.  Then allow the audio
// library to update with AudioInterrupts().  Both changes will happen
// at the same time, because AudioNoInterrupts() prevents any updates
// while you make changes.
//
#define AudioNoInterrupts() DISABLE_AUDIO_INTERRUPTS()
#define AudioInterrupts()   ENABLE_AUDIO_INTERRUPTS()

// include all the library headers, so a sketch can use a single
// #include <Audio.h> to get the whole library
//
#include "Input.h"
#include "Output.h"
#include "UsbOutput.h"
#include "amplifier.h"
#include "analyze_rms.h"
#include "effect_bitDepth.h"
#include "effect_dynamics.h"
#include "effect_limiter.h"
#include "effect_polyverb.h"
#include "effect_shortDelay.h"
#include "filter_variable.h"
#include "mixer.h"
#include "monoToStereo.h"
#include "mux.h"
#include "play_memory.h"
#include "play_sd_wav.h"
#include "record_queue.h"
#include "stereoToMono.h"
#include "synth_PolySynth.h"
#include "synth_waveform.h"
#endif
