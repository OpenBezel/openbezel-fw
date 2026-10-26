/* Audio output: the RTD2556's audio DAC (page 06) driving the headphone /
 * line output and, through the board's amplifier (BOARD_AUDIO_MUTE), the
 * speaker connector.  The source is the HDMI receiver's audio (hdmi_rx.c
 * hdmi_audio_start(), which owns the receiver side and its audio PLL) or the
 * DP receiver's (dp_audio_start()), selected by audio_route().
 *
 * The order here is a de-pop order: power with the outputs muted, settle,
 * then unmute. */
#ifndef RTD2556_AUDIO_H
#define RTD2556_AUDIO_H

#include "ob_types.h"

void audio_init(void);                  /* DAC + outputs up, source = HDMI (D3); stays muted */
/* volume 0..100 (0 = silent); mute overrides.  DAC digital gain, 0.75 dB
 * steps, 0x57 = 0 dB at volume 100. */
void audio_set(u8 volume, u8 mute);
/* De-pop: audio_hold() mutes the outputs (call before restarting the
 * receiver's audio); audio_tick() unmutes, to the audio_set() state, once
 * the receiver has reported an audio rate for UNMUTE_SETTLE_MS.
 * audio_init() starts held.  Volume/mute changes while held are remembered. */
void audio_route(u8 dp);                /* audio source: 0 HDMI (D3), 1 DP (D1, USB-C) */
void audio_hold(void);
void audio_tick(u16 t0ovf);

#endif
