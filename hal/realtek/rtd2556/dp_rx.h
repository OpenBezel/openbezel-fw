/* DisplayPort receiver, port D1: the USB-C input (DP alt-mode; the LDR6023
 * PD controller and the HD3SS460 mux handle the connector on their own).
 *
 * Hot-plug is 8051 port bit P1.4, which resets HIGH: until this module runs,
 * the source sees a sink that never answers AUX ("can't mirror display").
 *
 * Stages (the debug VCP can re-run each):
 *   stage 1 = AUX PHY + DPCD capabilities + port power-on + HPD, and a log of
 *   what the source does;
 *   stage 2 = link training in the INT0 handler (dp_aux_isr) and the
 *   post-training PHY retune;
 *   stage 3 = the stream (dp_stream_start). */
#ifndef RTD2556_DP_RX_H
#define RTD2556_DP_RX_H

#include "ob_types.h"

/* What the source has done since dp_rx_start(), for the debug VCP. */
struct dp_rx_log {
    u8 started;
    u8 aux_rx;          /* AUX requests seen (saturating) - polled, so a lower bound */
    u8 last_cmd;        /* last AUX request: command, address [19:16] */
    u8 last_addr_m;     /* ... address [15:8] */
    u8 last_addr_l;     /* ... address [7:0] */
    u8 dpcd_irq;        /* OR of AUX_DPCD_IRQ flags (100/101, 102, 103-108, 201, 270, 600 writes) */
    u8 lt_writes;       /* DPCD 100-108 write events */
    u8 rate;            /* DPCD 100 LINK_BW_SET as last written */
    u8 lanes;           /* DPCD 101 LANE_COUNT_SET */
    u8 pattern;         /* DPCD 102 TRAINING_PATTERN_SET */
    u8 drive;           /* DPCD 103 lane 0 drive request */
    u8 power;           /* DPCD 600 SET_POWER */
    u8 lt_state;        /* training state (LT_*): 1/2 CR ok/retry, 5/6 EQ, 0E trained, 09 link up, 0C failed */
    u8 cr_rounds;
    u8 eq_rounds;
    u8 tp1;             /* last B014 TP1_DETECT lanes 0-3 */
    u8 eqcrc;           /* last EQ CRC match lanes 0-3 */
    u8 stray;           /* INT0 entries with no source we know (HDMI block acked) */
    u8 isr;             /* INT0 entries, all (saturating) */
    u8 foreign;         /* ... of which HDMI/eDP sources whose enable was dropped */
    u32 frame27;        /* stage 3: v-BS-to-BS in 27 MHz cycles */
    u8 vfifo;           /* stage 3: B821 after the FIFO check */
    u8 spll_m;          /* stream PLL M and offset as computed */
    u32 spll_offs;
    u16 msa_htt, msa_vtt, msa_hwd, msa_vht;     /* the stream that was set up */
    u8 stage3;          /* last stream setup result; 0x10 | mode-set result once shown */
    u8 alive;           /* last dp_stream_alive() result while shown */
    u8 losses;          /* stream losses handled */
    u8 wake;            /* INT0 entries for the AUX-PHY wake/error flags */
    u16 lclk_ref;       /* lane 0 link-clock count (27 MHz x 1000 / 2) at stream setup */
    u16 lclk;           /* ... last one seen by the loss check */
    u8 vwd;             /* FIFO over/underflows seen while shown (video watchdog events) */
    u8 lt_gen;          /* completed trainings (ISR); the state machine redoes the stream on a change */
};
extern struct dp_rx_log dp_log;

void dp_hpd(u8 on);                     /* P1.4 */
void dp_rx_start(void) __banked;                 /* stage 1: init the port, then HPD high */
u8 dp_stream_start(void) __banked;               /* D1 stream (MSA, timing generator, stream PLL, FIFO check); 0 = ok, 4 = retry */
u16 dp_pixel_count(void) __banked;               /* pixel clock as kHz/27 (HDMI detector units), 0 = none */
#define DP_ALIVE     0
#define DP_LINK_LOST 1
#define DP_NO_VIDEO  2
#define DP_CHANGED   3
u8 dp_stream_alive(void) __banked;               /* DP_* above: is the set-up stream still arriving unchanged */
u8 dp_link_clk_nominal(void) __banked;           /* idle-port unplug check: 1 locked, 0 not, 2 pending */
u8 dp_stream_present(void) __banked;             /* DP_ALIVE when a stream could be set up now */
void dp_link_reset(void) __banked;               /* after a loss: training forgotten, source retrains */
u8 dp_link_up(void) __banked;
extern __xdata u8 dp_audio_on;
void dp_audio_start(void) __banked;              /* D1 audio block up (dp_rx_start; FEh = 2 re-runs it) */
u8 dp_audio_present(void) __banked;               /* DP source sending audio (VB-ID unmuted, buffer not empty) */
void dp_irq_rearm(void) __banked;                /* debug: INT0 back on, stray count 0 */
void dp_rx_poll(void) __banked;                  /* main loop: record AUX activity */
u8 dp_dpcd_read(u8 hi, u16 addr) __banked;       /* DPCD (bits 19:16, 15:0) through the access port */
/* INT0 (scaler interrupt).  Declared here so main.c, which includes this
 * header, makes SDCC emit the vector. */
void dp_aux_isr(void) __interrupt(0);

#endif
