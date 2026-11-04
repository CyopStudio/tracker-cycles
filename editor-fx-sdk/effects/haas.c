// PTFX_NAME: Haas Effect
// PTFX_AUTHOR: Emanuele Lapponi
// PTFX_FLAGS: EFFECT, STEREO
// PTFX_SCRATCH: 8820

#include "tracker_plugin.h"

// Knobs: delay amount in ms, and which channel (L or R) gets the delay.
// The selected channel is shifted later by the delay amount; the output is
// extended by the tail and the delayed channel fades out over its final
// ~10 ms so it ends on a zero crossing (no click).
const PluginParam PLUGIN_PARAMS[2] = {
    PTFX_PARAM_INT("Delay", 10.0f, 100.0f, 10.0f, 1.0f, "ms"),
    PTFX_PARAM_LIST("Chan", 0, "Left|Right")
};

#define MAX_DELAY_SAMPLES 4410 // 100 ms @ 44.1 kHz

static const PtfxHostApi* s_host = 0;
static int16_t* s_delay = 0;    // delay line (ring buffer in scratch_mem)
static uint32_t s_write = 0;
static uint32_t s_delay_len = 441;  // 10 ms default
static uint8_t  s_chan = 0;      // 0 = Left, 1 = Right
static uint32_t s_tail_count = 0; // samples processed in the appended tail

static void update_tail(void) {
    if (s_host && s_host->set_extra_samples) {
        // Append the delay tail so the whole shifted channel fits in the output.
        s_host->set_extra_samples(0, s_delay_len);
    }
}

void plugin_init(void* scratch_mem, uint32_t scratch_size, const PtfxHostApi* host) {
    s_host = host;
    s_write = 0;
    s_tail_count = 0;
    s_delay = 0;
    if (scratch_mem && scratch_size >= MAX_DELAY_SAMPLES * sizeof(int16_t)) {
        s_delay = (int16_t*)scratch_mem;
        if (host && host->memset) {
            host->memset(s_delay, 0, MAX_DELAY_SAMPLES * sizeof(int16_t));
        }
    }
    update_tail();
}

void plugin_set_param(uint8_t index, float value) {
    if (index == 0) {
        uint32_t len = (uint32_t)(value * 44.1f + 0.5f);
        if (len < 1) len = 1;
        if (len >= MAX_DELAY_SAMPLES) len = MAX_DELAY_SAMPLES - 1;
        s_delay_len = len;
    } else if (index == 1) {
        s_chan = (value >= 0.5f) ? 1 : 0;
    }
    update_tail();
}

int32_t plugin_process_block(const int16_t** in, int16_t** out, uint32_t samples, uint8_t channels) {
    if (!out || !out[0] || samples == 0) {
        return 0;
    }
    uint8_t nch = (channels > 2) ? 2 : channels;
    // In the appended tail the host passes in == NULL; the delay line still
    // holds the remaining samples, so keep processing.
    const int16_t* src = (in && in[s_chan]) ? in[s_chan] : 0;

    if (!s_delay) {
        // Scratch too small: pass through.
        for (uint32_t i = 0; i < samples; i++) {
            for (uint8_t ch = 0; ch < nch; ch++) {
                out[ch][i] = in[ch] ? in[ch][i] : 0;
            }
        }
        return (int32_t)samples;
    }

    uint32_t dlen = s_delay_len;
    // Fade the final ~10 ms of the delayed channel to 0 (ends on zero crossing).
    uint32_t fade = (s_host && s_host->sample_rate > 0) ? s_host->sample_rate / 100 : 441;
    if (fade >= dlen) fade = (dlen > 1) ? dlen - 1 : 0;

    // In the appended tail the host passes in[0] == NULL (no source audio);
    // the delay line still holds the remaining samples, so keep draining it.
    int in_tail = (in[0] == 0);
    uint32_t t_base = s_tail_count;

    for (uint32_t i = 0; i < samples; i++) {
        uint32_t r = (s_write + MAX_DELAY_SAMPLES - dlen) % MAX_DELAY_SAMPLES;
        for (uint8_t ch = 0; ch < nch; ch++) {
            int16_t* o = out[ch];
            if (!o) continue;
            if (ch != s_chan) {
                // Other channel: unchanged (silence in the appended tail).
                const int16_t* oc = (in && in[ch]) ? in[ch] : 0;
                o[i] = oc ? oc[i] : 0;
            } else {
                int32_t v = s_delay[r]; // delayed sample (leading silence for i<dlen)
                if (in_tail) {
                    // In the appended tail: fade the remaining delayed audio to 0.
                    uint32_t t = t_base + i; // 0-based index within the tail
                    if (fade > 1 && t >= dlen - fade) {
                        v = (int32_t)((float)v * (float)(dlen - 1 - t) / (float)(fade - 1));
                    } else if (fade == 1 && t == dlen - 1) {
                        v = 0;
                    }
                }
                o[i] = (int16_t)SATURATE_16BIT(v);
            }
        }
        s_delay[s_write] = src ? src[i] : 0;
        s_write = (s_write + 1) % MAX_DELAY_SAMPLES;
    }
    if (in_tail) s_tail_count += samples;
    return (int32_t)samples;
}
