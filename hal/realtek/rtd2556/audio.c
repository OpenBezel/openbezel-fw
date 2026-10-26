/* Audio output - see audio.h. */
#include "audio.h"
#include "board.h"
#include "delay.h"
#include "hdmi_rx.h"
#include "dp_rx.h"
#include "rtd2556_regs.h"
#include "timebase.h"

#define DA_GAIN_0DB 0x57            /* DA_GAIN_L/R: 0 dB; each step is 0.75 dB */
#define VOL_SPAN_STEPS 64           /* 48 dB */
#define DAC_SETTLE_MS 150           /* VCM / de-pop settle before unmuting */
#define MUX_SRC_D3 3                /* ADI source select: D3 = the HDMI port */
/* De-pop: unmute only once the receiver has reported an audio rate
 * continuously for this long.  The rate appears with the picture, seconds
 * after hdmi_audio_start(); unmuting the instant it appears pops (the
 * receiver's audio is still settling), while mute/unmute with steady audio
 * is silent. */
#define UNMUTE_SETTLE_MS 300

static u8 dac_up;
static u8 held;                     /* outputs kept muted until audio_tick() releases them */
static u16 held_t0;                /* first t0ovf with audio, valid when have_rate */
static u8 have_rate;
static u8 want_vol, want_mute = 1;

static void outputs(u8 on)
{
    /* AOUT / HPOUT: normal mode, mute bits per channel (C0 = both muted) */
    SR(REG_AOUT_CTRL) = on ? REG_AOUT_CTRL_REG_AOUT_NORM
                           : (REG_AOUT_CTRL_REG_MUTE_AOUT_L | REG_AOUT_CTRL_REG_MUTE_AOUT_R | REG_AOUT_CTRL_REG_AOUT_NORM);
    SR(REG_HPOUT_CTRL) = on ? REG_HPOUT_CTRL_REG_HP_NORM
                            : (REG_HPOUT_CTRL_REG_MUTE_HPOUT_L | REG_HPOUT_CTRL_REG_MUTE_HPOUT_R | REG_HPOUT_CTRL_REG_HP_NORM);
    /* DAC digital mute (and keep the sigma-delta modulator out of reset) */
    SR(DA_MUTE) = on ? DA_MUTE_DAMOD_RST_N
                     : (DA_MUTE_DA_MUTE_L | DA_MUTE_DA_MUTE_R | DA_MUTE_DAMOD_RST_N);
    BOARD_AUDIO_MUTE = on ? 0 : 1;
}

static __xdata u8 route_dp;

/* ADI audio source: the HDMI port (D3) or the DP port (D1, USB-C):
 * 6F00/01/02 = 33/03/03 on HDMI, 11/01/01 on DP. */
void audio_route(u8 dp)
{
    u8 src = dp ? 1 : MUX_SRC_D3;

    route_dp = dp;
    SR(MUX_DA_DVC_IN) = src;
    SR(MUX_DIG_OUT) = (src << 4) | src;
    SR(MUX_DP_I2S_IN) = src;
}

static u8 audio_in(void)
{
    return route_dp ? dp_audio_present() : hdmi_audio_rate();
}

void audio_init(void)
{
    outputs(0);
    /* source: the DAC (through the DVC) and the digital outputs take D3 */
    audio_route(0);
    /* analog power: every DAC block on, de-pop on */
    SR(REG_DAC_POWER0) = 0xFF;
    SR(REG_DAC_POWER1) = REG_DAC_POWER1_REG_POW_DEPOP | REG_DAC_POWER1_REG_POW_DEPOP_CK | REG_DAC_POWER1_REG_POW_DEPOP_CORE
                       | REG_DAC_POWER1_REG_POW_DEPOP_OP | FV(REG_DAC_POWER1_REG_VCM_SEL, 1);
    SR(REG_ANA_RESERVE0) = 0x08;
    SR(DA_CLK_CTRL) = DA_CLK_CTRL_DEPOP_CLK_EN | FV(DA_CLK_CTRL_DEPOP_CLK_DIV, 2);   /* DA_CLK_RATE 0: MCLK = 256 fs */
    SR(DA_GAIN_L) = DA_GAIN_0DB;
    SR(DA_GAIN_R) = DA_GAIN_0DB;
    delay_ms(DAC_SETTLE_MS);
    dac_up = 1;
    audio_hold();                   /* unmuted by audio_tick() once the receiver has audio */
}

static void apply(void)
{
    u8 g, volume = want_vol;

    if (!dac_up || held)
        return;
    if (want_mute || volume == 0) {
        outputs(0);
        return;
    }
    if (volume > 100)
        volume = 100;
    /* linear in dB over a 48 dB span: 100 -> 0 dB, 50 -> -24 dB, 1 -> -47.5 dB
     * (64 gain steps of 0.75 dB; the full 0x57 range would make 50% -32 dB) */
    g = (u8)(DA_GAIN_0DB - ((u16)(100 - volume) * VOL_SPAN_STEPS) / 100);
    SR(DA_GAIN_R) = g;
    SR(DA_GAIN_L) = (u8)(g | DA_GAIN_L_DA_GAIN_DB);  /* DB: "set after adjusting DAC gain" */
    outputs(1);
}

void audio_set(u8 volume, u8 mute)
{
    want_vol = volume;
    want_mute = mute;
    apply();
}

void audio_hold(void)
{
    outputs(0);
    held = 1;
    have_rate = 0;
}

void audio_tick(u16 t0ovf)
{
    if (!held) {
        /* Audio gone (mode change, source loss, host stopped playing): mute
         * NOW.  A host mode change reaches mode_set() only seconds later,
         * and the receiver's audio restarting under unmuted outputs pops. */
        if (dac_up && !audio_in())
            audio_hold();
        return;
    }
    if (!audio_in()) {
        have_rate = 0;              /* no audio (yet): stay muted, no hiss */
        return;
    }
    if (!have_rate) {
        have_rate = 1;
        held_t0 = t0ovf;
        return;
    }
    if ((u16)(t0ovf - held_t0) < tb_ms(UNMUTE_SETTLE_MS))
        return;
    held = 0;
    apply();
}
