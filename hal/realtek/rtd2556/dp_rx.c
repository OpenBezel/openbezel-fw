/* DisplayPort receiver, port D1 - see dp_rx.h. */
#include "dp_rx.h"
#include "edp_tx.h"
#include "rtd2556_regs.h"
#include "timebase.h"
#include "hdcp.h"

#include "dp_rx_int.h"

struct dp_rx_log dp_log;

/* ======================================================================
 * Link training (stage 2) - the INT0 handler.
 *
 * Everything below `ISR CONTEXT` runs inside the interrupt and calls only
 * functions that nothing else calls: SDCC's large-model functions keep
 * their parameters and locals in fixed XDATA, so a helper shared with the
 * main loop would be corrupted when the interrupt lands inside it.
 * ====================================================================== */

volatile u8 lt_state;
volatile u8 lt_lanes;    /* 1, 2 or 4 */
volatile u8 lt_rate;     /* DPCD 100 */
u8 lt_enh;               /* DPCD 101 bit 7 -> B001 enhanced framing */
u16 lt_trained_t0;       /* main loop: when LT_TRAINED was seen */
u8 pw_save[3];           /* DPCD 202-204 across SET_POWER D3 */
volatile u16 irq_burst;   /* INT0 entries since the last main-loop pass */

/* Field groups this file writes together */
#define CMU00(rx, cdr) (FV(D1_CMU_00_REG_RX_EN, rx) | FV(D1_CMU_00_REG_CDR_EN, cdr))    /* per-lane bitmasks */
/* KOFFSET_06/07: analog CDR enables + the foreground offsets of lanes 2 and 3 */
#define KOFF06(cdr) (FV(D1_KOFFSET_06_REG_EN_ANA_CDR, cdr) | FV(D1_KOFFSET_06_REG_FORE_OFF_TST_L2, 0x2E))
#define KOFF07(cdr) (FV(D1_KOFFSET_07_REG_EN_ANA_CDR, cdr) | FV(D1_KOFFSET_07_REG_FORE_OFF_TST_L3, 0x32))
#define KOFF00_AUTOK (D1_KOFFSET_00_REG_EN_BACK_OFFSET | D1_KOFFSET_00_REG_FORE_OFF_AUTOK \
                      | FV(D1_KOFFSET_00_REG_FORE_OFF_ADJR_L0, 16))
#define CDR01_BASE (D1_ANA_CDR_01_REG_XTAL_EN | D1_ANA_CDR_01_REG_XTAL_SEL | D1_ANA_CDR_01_REG_CP_ADJ_EN)
#define CDR01_MANUAL (D1_ANA_CDR_01_REG_ADP_EN_MANUAL | D1_ANA_CDR_01_REG_CP_EN_MANUAL)
/* FLD counts: bit 8 of the CDR count (ANA_CDR_04) set, of the REF count (ANA_CDR_03) clear */
#define CDR02_BASE (FV(D1_ANA_CDR_02_REG_ADP_TIME, 2) | D1_ANA_CDR_02_REG_SEL_CONT3 | D1_ANA_CDR_02_REG_CDR_DIV_CONT)
#define ACDR_BYPASS (D1_ANA_CDR_REG_ACDR_L0_EN | D1_ANA_CDR_REG_BYPASS_DATA_RDY_L0 | D1_ANA_CDR_REG_BYPASS_EQEN_RDY_L0)
#define LANES_RST_N (D1_TMDS_RESET_REG_RST_N_L0 | D1_TMDS_RESET_REG_RST_N_L1 | D1_TMDS_RESET_REG_RST_N_L2 \
                     | D1_TMDS_RESET_REG_RST_N_L3)
#define SCRAMB_VLD_ALL (D1_SCRAMBLE_CTRL_SCRAMB_VLD_LN0 | D1_SCRAMBLE_CTRL_SCRAMB_VLD_LN1 | D1_SCRAMBLE_CTRL_SCRAMB_VLD_LN2 \
                        | D1_SCRAMBLE_CTRL_SCRAMB_VLD_LN3)
#define DFE_RSTB_ALL (D1_RESET_1_DFE_ADAPT_RSTB_LANE0 | D1_RESET_1_DFE_ADAPT_RSTB_LANE1 | D1_RESET_1_DFE_ADAPT_RSTB_LANE2 \
                      | D1_RESET_1_DFE_ADAPT_RSTB_LANE3)
#define EQ_CRC_OK_ALL (D1_EQ_CRC_1_EQ_CRC_CORRECT_LN0 | D1_EQ_CRC_1_EQ_CRC_CORRECT_LN1 | D1_EQ_CRC_1_EQ_CRC_CORRECT_LN2 \
                       | D1_EQ_CRC_1_EQ_CRC_CORRECT_LN3)
/* signal detect over 1000 crystal cycles; DONE written set */
#define SIGDET_BASE (D1_DP_SIG_DET_0_DP_SIG_DET_DONE | FV(D1_DP_SIG_DET_0_DP_XTAL_CYCLE, 3))
/* L0_DFE_EN_2 (adaptation) and L0_INIT_8 (load) bits: [7] LE, [6] Vth, [5] servo, [4:0] tap4-0 */
#define DFE_LE 0x80
#define DFE_VTH 0x40
#define DFE_TAP2 0x04
#define DFE_TAP1 0x02
#define DFE_TAP0 0x01
#define AUX_IRQ_FLAGS (D1_AUX_IRQ_STATUS_WROT_INT | D1_AUX_IRQ_STATUS_TIMEOUT_INT | D1_AUX_IRQ_STATUS_AUX_RX_INT)
#define AUX_IRQ_ENS (D1_AUX_IRQ_STATUS_WROT_INT_EN | D1_AUX_IRQ_STATUS_TIMEOUT_EN | D1_AUX_IRQ_STATUS_AUX_RX_EN)
#define AUX_PHY_ERRS (D1_AUX_DIG_PHY9_MAN_ERROR | D1_AUX_DIG_PHY9_STOP_ERROR | D1_AUX_DIG_PHY9_SYNCEND_ERROR \
                      | D1_AUX_DIG_PHY9_DATA_8X_ERROR)
#define LOCK_TRA_ALL (HDMI_CTRL_17_LOCK_TRA_CH0 | HDMI_CTRL_17_LOCK_TRA_CH1 | HDMI_CTRL_17_LOCK_TRA_CH2)
#define ABC0_W1C (TMDS_ABC0_WR_AKSV_FLAG | TMDS_ABC0_RD_RI_FLAG | TMDS_ABC0_RD_BKSV_FLAG | TMDS_ABC0_HIT_I2C_SLAVE_FLAG)
#define HPD_INTS (HPD_IRQ_IHPD_INT | HPD_IRQ_SHPD_INT | HPD_IRQ_LHPD_INT | HPD_IRQ_UHPD_INT | HPD_IRQ_UNHPD_INT \
                  | HPD_IRQ_RHPD_INT | HPD_IRQ_FHPD_INT)

/* ---------------------------- ISR CONTEXT ----------------------------- */

/* busy-waits: i_units(1) ~9 us, i_short(1) ~10.5 us on the PLL clock
 * (delay.c's loop, ~1.5 us per iteration).
 * This must remain separate from delay_us because it runs in the ISR context. */
void i_wait(u16 n)
{
    while (n--) {
        __asm__("nop");
        __asm__("nop");
        __asm__("nop");
        __asm__("nop");
    }
}

static void i_dpcd_addr(u16 addr)
{
    SR(D1_DPCD_ADDR_PORT_H) = 0;
    SR(D1_DPCD_ADDR_PORT_M) = HI(addr);
    SR(D1_DPCD_ADDR_PORT_L) = LO(addr);
}
static u8 i_dpcd_r(u16 addr)
{
    i_dpcd_addr(addr);
    return SR(D1_DPCD_DATA_PORT);
}
static void i_dpcd_w(u16 addr, u8 v)
{
    i_dpcd_addr(addr);
    SR(D1_DPCD_DATA_PORT) = v;
}

static void i_lanes4(u8 off, u8 v)
{
    u8 l;
    for (l = 0; l < 4; l++)
        SR(PHY_LANE(l, off)) = v;
}

/* B001, enhanced framing always on while training */
static u8 i_b001(void)
{
    return D1_PHY_DIG_RESET_CTRL_XCLK_POWON_EN | D1_PHY_DIG_RESET_CTRL_ENHANCE_MODE
           | (lt_lanes == 1 ? D1_PHY_DIG_RESET_CTRL_LANE_NUM_1_LANE
              : lt_lanes == 2 ? D1_PHY_DIG_RESET_CTRL_LANE_NUM_2_LANE : D1_PHY_DIG_RESET_CTRL_LANE_NUM_4_LANE);
}

/* clear the symbol error counts */
static void i_sym_clear(void)
{
    u8 i;
    SR(D1_BIST_PATTERN_SEL) = 0x00;
    for (i = 0; i < 8; i++)
        i_dpcd_w(DPCD_SYMBOL_ERROR_COUNT_LANE0_L + i, 0);
}

/* symbol error counts valid (active lanes) + BIST arm */
static void i_sym_arm(void)
{
    u8 l;
    for (l = 0; l < lt_lanes; l++)
        i_dpcd_w(DPCD_SYMBOL_ERROR_COUNT_LANE0_H + 2 * l,
                 i_dpcd_r(DPCD_SYMBOL_ERROR_COUNT_LANE0_H + 2 * l) | DPCD_SYMBOL_ERROR_COUNT_LANE0_H_LANE0_ERR_COUNT_VALID);
    SR(D1_BIST_PATTERN_SEL) = FV(D1_BIST_PATTERN_SEL_RXBIST_METHOD, 2);   /* count 8b10b + disparity errors */
}

/* signal-detect / lane frequency check (B050-B054), up to 31 short waits */
static void i_sigdet(u8 base)
{
    u8 n;
    SR(D1_DP_SIG_DET_1) = 0x00;
    SR(D1_DP_SIG_DET_2) = 0x00;
    SR(D1_DP_SIG_DET_0) = base;
    SR(D1_DP_SIG_DET_0) = base | D1_DP_SIG_DET_0_DP_SIG_DET_EN;
    for (n = 0; n < 31 && !(SR(D1_DP_SIG_DET_0) & D1_DP_SIG_DET_0_DP_SIG_DET_DONE); n++)
        i_short(1);
    SR(D1_DP_SIG_DET_0) = base;
}

/* DFE init, RBR/HBR values */
#define GAIN_2_INIT (FV(D1_GAIN_2_TAP2_GAIN, 1) | FV(D1_GAIN_2_TAP3_GAIN, 1))
#define GAIN_3_INIT (FV(D1_GAIN_3_TAP4_GAIN, 1) | FV(D1_GAIN_3_SERVO_GAIN, 2))
#define RESET_1_INIT FV(D1_RESET_1_TAP0_THRESHOLD, 10)
#define DFE_EN_1_INIT (D1_L0_DFE_EN_1_TIMER_CTRL_EN_LANE0 | FV(D1_L0_DFE_EN_1_REDUCE_ADAPT_GAIN_LANE0, 2) \
                       | FV(D1_L0_DFE_EN_1_RL_THRESHOLD_LANE0, 1))
static const __code struct reg_op dfe_init[] = {
    { D1_MODE_TIMER, 0, FV(D1_MODE_TIMER_ADAPT_MODE, 3) | D1_MODE_TIMER_EDGE_DET_MODE | D1_MODE_TIMER_TRANSITION_ONLY
                        | FV(D1_MODE_TIMER_TAP1_DELAY, 2) },
    { D1_TIMER, 0, FV(D1_TIMER_SERVO_DELAY, 1) },
    { D1_GAIN_1, 0, FV(D1_GAIN_1_TAP0_GAIN, 1) | FV(D1_GAIN_1_TAP1_GAIN, 1) },
    { D1_GAIN_2, 0, GAIN_2_INIT },
    { D1_GAIN_3, 0, GAIN_3_INIT },
    { D1_GAIN_4, 0, FV(D1_GAIN_4_LE_GAIN1, 2) | FV(D1_GAIN_4_LE_GAIN2, 6) },
    { D1_GAIN_PREC, 0, FV(D1_GAIN_PREC_LE_GAIN_PREC, 6) },
    { D1_LIMIT_1, 0, FV(D1_LIMIT_1_VTH_THRESHOLD, 1) | FV(D1_LIMIT_1_TAP0_MAX, 21) },
    { D1_LIMIT_2, 0, FV(D1_LIMIT_2_TAP1_MIN, 51) },
    { D1_LIMIT_3, 0, FV(D1_LIMIT_3_TAP2_MAX, 6) | FV(D1_LIMIT_3_TAP2_MIN, 1) },
    { D1_LIMIT_4, 0, FV(D1_LIMIT_4_TAP2_MIN, 2) },
    { D1_LIMIT_5, 0, FV(D1_LIMIT_5_TAP3_MIN, 3) },
    { D1_LIMIT_6, 0, FV(D1_LIMIT_6_TAP4_MIN, 3) },
    { D1_RESET_1, 0, RESET_1_INIT | DFE_RSTB_ALL },
    { D1_LIMIT_10, 0, FV(D1_LIMIT_10_VTH_MAX, 6) | FV(D1_LIMIT_10_VTH_MIN, 2) },
    { D1_LOOP_DIV_1, 0, FV(D1_LOOP_DIV_1_SERVO_DIVISOR, 40) | FV(D1_LOOP_DIV_1_VTH_DIVISOR, 3) },
    { D1_LOOP_DIV_2, 0, FV(D1_LOOP_DIV_2_VTH_DIVISOR, 3) | FV(D1_LOOP_DIV_2_TAP_DIVISOR, 10) },
    { D1_GAIN_2, 0, GAIN_2_INIT }, { D1_GAIN_3, 0, GAIN_3_INIT }, { D1_GRAY_DEC_2, 0, 0x00 },
};
static void i_dfe_init(void)
{
    const __code struct reg_op *op = dfe_init;
    u8 n;
    for (n = sizeof(dfe_init) / sizeof(dfe_init[0]); n; n--, op++)
        SR(op->addr) = op->val;
    i_lanes4(LN(D1_L0_LIMIT_INIT), FV(D1_L0_LIMIT_INIT_LE_MIN_LANE0, 4));
    i_lanes4(LN(D1_L0_INIT_1), D1_L0_INIT_1_LE_MIN_LANE0 | D1_L0_INIT_1_LE_INIT_LANE0 | FV(D1_L0_INIT_1_SERVO_INIT_LANE0, 16));
    i_lanes4(LN(D1_L0_INIT_2), 0x12);          /* tap0 */
    i_lanes4(LN(D1_L0_INIT_3), 0x0F);          /* tap1 */
    i_lanes4(LN(D1_L0_INIT_4), 0x00);          /* tap2 */
    i_lanes4(LN(D1_L0_INIT_7), 0x04);          /* Vth */
    i_lanes4(LN(D1_L0_INIT_8), 0xFF);          /* load every initial value */
    i_lanes4(LN(D1_L0_INIT_8), 0x00);
    SR(D1_RESET_1) = RESET_1_INIT;      /* adaptation reset pulse */
    SR(D1_RESET_1) = RESET_1_INIT | DFE_RSTB_ALL;
    i_lanes4(LN(D1_L0_DFE_EN_2), 0x00);
}

/* clock-recovery PHY setup */
static void i_cr_phy(void)
{
    u8 l, keep;

    SR(D1_LANE_MUX) = 0xE4;             /* identity */
    SR(D1_DUMMY3) = 0x00;
    SR(D1_DECODE_10B8B_ERROR) = 0x00;   /* PN swap off */
    SR(D1_CMU_01) = D1_CMU_01_REG_CMU_CKSEL;   /* CMU off */
    SR(D1_CMU_00) = 0x00;
    keep = SR(D1_PHY_DIG_RESET_CTRL) & D1_PHY_DIG_RESET_CTRL_XCLK_SEL;
    SR(D1_PHY_DIG_RESET_CTRL) = keep | i_b001();
    /* receiver / CDR enables, active lanes only */
    if (lt_lanes == 4) {
        SR(D1_CMU_00) = CMU00(0xF, 0); SR(D1_CMU_00) = CMU00(0xF, 1); SR(D1_CMU_00) = CMU00(0xF, 3);
        SR(D1_CMU_00) = CMU00(0xF, 7); SR(D1_CMU_00) = CMU00(0xF, 0xF);
        SR(D1_KOFFSET_06) = KOFF06(3); SR(D1_KOFFSET_07) = KOFF07(3);
    } else if (lt_lanes == 2) {
        SR(D1_CMU_00) = CMU00(1, 0); SR(D1_CMU_00) = CMU00(1, 1); SR(D1_KOFFSET_07) = KOFF07(3);
        SR(D1_CMU_00) = CMU00(3, 1); SR(D1_CMU_00) = CMU00(3, 3); SR(D1_KOFFSET_07) = KOFF07(3);
    } else {
        SR(D1_CMU_00) = CMU00(1, 0); SR(D1_CMU_00) = CMU00(1, 1);
        SR(D1_KOFFSET_06) = KOFF06(0); SR(D1_KOFFSET_07) = KOFF07(1);
    }
    SR(D1_PHY_DIG_RESET_CTRL) = keep | i_b001();
    SR(D1_ST_01) = D1_ST_01_REG_RATE_SEL_300M_150M | D1_ST_01_REG_RESERVED;   /* RESERVED: [4] en_chargeshare_op */
    SR(D1_CMU_07) = D1_CMU_07_REG_TAP_EN | D1_CMU_07_REG_SEL_DIV_1_5G_3G;     /* RBR/HBR */
    SR(D1_ANA_CDR_01) = CDR01_MANUAL | CDR01_BASE;
    SR(D1_ANA_CDR_02) = CDR02_BASE | D1_ANA_CDR_02_REG_RSTB_FSM;
    SR(D1_ANA_CDR_03) = 0x18;
    SR(D1_ANA_CDR_04) = lt_rate == RATE_RBR ? 0x2B : 0xF3;
    SR(D1_SAMPLE_EDGE) = D1_SAMPLE_EDGE_COMMA_DIS;
    /* scrambling off while training */
    SR(D1_SCRAMBLE_CTRL) = D1_SCRAMBLE_CTRL_ENHANCE_VOTE_EN | D1_SCRAMBLE_CTRL_SCRAMB_DIS | SCRAMB_VLD_ALL;
    SR(D1_PHY_DIG_RESET_CTRL) = keep | i_b001();
    SR(D1_ANA_CDR) = ACDR_BYPASS;
    SR(D1_TMDS_RESET) = 0x00;           /* lane reset pulse */
    SR(D1_TMDS_RESET) = LANES_RST_N;
    SR(D1_ANA_CDR_06) = FV(D1_ANA_CDR_06_REG_SEL_CDR_RES, 4) | D1_ANA_CDR_06_REG_CDR_CP_IN_16_5U;
    SR(D1_ANA_CDR_14) = 0x52;
    SR(D1_CMU_07) = D1_CMU_07_REG_TAP_EN | D1_CMU_07_REG_SEL_DIV_1_5G_3G;
    i_lanes4(LN(D1_L0_DFE_EN_1), DFE_EN_1_INIT);
    i_dfe_init();
    SR(D1_CMU_03) = D1_CMU_03_REG_ENHANCE_IBN_BIAS_NORMAL;
    SR(D1_CMU_09) = lt_rate == RATE_RBR ? FV(D1_CMU_09_REG_LE_RLSEL, 3)
                                        : (FV(D1_CMU_09_REG_LE_RLSEL, 1) | FV(D1_CMU_09_REG_LE_ISEL, 2));
    SR(D1_KOFFSET_00) = KOFF00_AUTOK;   /* foreground offset calibration, auto: on, restart */
    SR(D1_KOFFSET_00) = KOFF00_AUTOK | D1_KOFFSET_00_REG_EN_FORE_OFFSET;
    SR(D1_KOFFSET_00) = KOFF00_AUTOK;
    SR(D1_KOFFSET_00) = KOFF00_AUTOK | D1_KOFFSET_00_REG_EN_FORE_OFFSET;
    i_units(0x15);
    SR(D1_CMU_01) = D1_CMU_01_REG_CMU_EN | D1_CMU_01_REG_CMU_CKSEL;
    SR(D1_ANA_CDR_01) = D1_ANA_CDR_01_REG_AUTO_MODE | CDR01_MANUAL | CDR01_BASE;
    SR(D1_ANA_CDR_02) = CDR02_BASE;     /* FLD reset */
    SR(D1_ANA_CDR) = ACDR_BYPASS | D1_ANA_CDR_REG_EN_EQEN_MANUAL_L0;
    i_lanes4(LN(D1_L0_DFE_EN_2), DFE_VTH | DFE_TAP0);  /* adapt on */
    SR(D1_ANA_CDR_02) = CDR02_BASE | D1_ANA_CDR_02_REG_RSTB_FSM;
    i_units(0x15);
    SR(D1_COEF_3) = D1_COEF_3_RECORD_LIMIT_EN;  /* record_limit pulse */
    i_units(0x15);
    SR(D1_COEF_3) = 0x00;
    SR(D1_ANA_CDR) = ACDR_BYPASS;
    SR(D1_ANA_CDR_01) = D1_ANA_CDR_01_REG_AUTO_MODE | CDR01_MANUAL | D1_ANA_CDR_01_REG_ADP_EQ_OFF | CDR01_BASE;
    i_lanes4(LN(D1_L0_DFE_EN_2), 0x00);        /* adapt off */
    /* tap0 read back per lane -> INIT_2, then load */
    SR(D1_COEF_3) = 0x00;
    for (l = 0; l < 4; l++) {
        SR(D1_COEF_3) = l << D1_COEF_3_LANE_SEL_SHIFT;
        SR(PHY_LANE(l, LN(D1_L0_INIT_2))) = SR(D1_COEF_4);
    }
    i_lanes4(LN(D1_L0_INIT_8), DFE_TAP0);
    i_lanes4(LN(D1_L0_INIT_8), 0x00);
    SR(D1_ANA_CDR) = ACDR_BYPASS | D1_ANA_CDR_REG_EN_DATA_MANUAL_L0;
    SR(D1_PHY_DIG_RESET_CTRL) = i_b001();       /* xclk_sel off */
    SR(D1_IMPEDANCE_01) = D1_IMPEDANCE_01_REG_EN_SIGDET_LANE | D1_IMPEDANCE_01_REG_Z0_MANUAL
                          | FV(D1_IMPEDANCE_01_REG_Z0_CM_ADJR, 6);
    SR(D1_DP_SIG_DET_0) = D1_DP_SIG_DET_0_DP_SIG_DET_EN | SIGDET_BASE;
    i_sigdet(SIGDET_BASE);
}

/* drive request byte for lane l: swing [1:0], pre-emphasis [3:2] */
static u8 req[4];
static u8 drv[4];

/* adjust requests (DPCD 206/207) from req[] */
static void i_reply(void)
{
    i_dpcd_w(DPCD_ADJUST_REQUEST_LANE0_1, req[0] | (req[1] << 4));
    i_dpcd_w(DPCD_ADJUST_REQUEST_LANE2_3, req[2] | (req[3] << 4));
}

/* the per-lane status nibbles for DPCD 202/203 */
static void i_status(const u8 *nib, u8 align)
{
    i_dpcd_w(DPCD_LANE0_1_STATUS, nib[0] | (nib[1] << 4));
    i_dpcd_w(DPCD_LANE2_3_STATUS, nib[2] | (nib[3] << 4));
    i_dpcd_w(DPCD_LANE_ALIGN_STATUS_UPDATED, align ? 0x81 : 0x80);
}

static void i_read_drive(void)
{
    u8 l;
    for (l = 0; l < 4; l++)
        drv[l] = l < lt_lanes ? i_dpcd_r(DPCD_TRAINING_LANE0_SET + l) : 0;
}

/* Clock-recovery handler.  A lane is CR_DONE when the
 * hardware detected TP1 on it and its swing is high enough: >= 1 when the
 * DFE read-back is >= 4, else >= 2 (RBR/HBR).  A failing lane asks for one
 * more swing step; pre-emphasis is left alone. */
static u8 i_cr(void)
{
    u8 l, tp1, nib[4], ok = 1;

    i_sym_clear();
    i_cr_phy();
    i_sym_arm();
    i_dpcd_w(DPCD_LANE0_1_STATUS, 0);
    i_dpcd_w(DPCD_LANE2_3_STATUS, 0);
    i_dpcd_w(DPCD_LANE_ALIGN_STATUS_UPDATED, 0x80);
    i_read_drive();
    tp1 = SR(D1_EQ_CRC_3) >> 4;         /* TP1_DETECT_LN0..3 */
    dp_log.tp1 = tp1;
    for (l = 0; l < 4; l++) {
        u8 sw = drv[l] & 3, pre = (drv[l] >> 3) & 3, coef, min;
        nib[l] = 0;
        req[l] = 0;
        if (l >= lt_lanes)
            continue;
        SR(D1_COEF_3) = l << D1_COEF_3_LANE_SEL_SHIFT;
        SR(D1_COEF_1) = 0x00;
        coef = SR(D1_COEF_2) & 0x0F;
        min = coef >= 4 ? 1 : 2;
        if ((tp1 >> l) & 1 && sw >= min) {
            nib[l] = 0x01;
        } else {
            ok = 0;
            if (sw < 3)
                sw++;
        }
        req[l] = sw | (pre << 2);
    }
    i_status(nib, 0);
    i_reply();
    return ok;
}

/* equalization PHY setup, RBR/HBR */
static void i_eq_phy(void)
{
    SR(D1_PHY_DIG_RESET_CTRL) = i_b001();
    SR(D1_CMU_01) = D1_CMU_01_REG_CMU_CKSEL;
    SR(D1_SAMPLE_EDGE) = 0x00;
    SR(D1_SCRAMBLE_CTRL) = D1_SCRAMBLE_CTRL_ENHANCE_VOTE_EN | D1_SCRAMBLE_CTRL_SCRAMB_DIS | SCRAMB_VLD_ALL;
    SR(D1_PHY_DIG_RESET_CTRL) = i_b001();
    SR(D1_ANA_CDR) = ACDR_BYPASS;
    SR(D1_TMDS_RESET) = 0x00;
    SR(D1_TMDS_RESET) = LANES_RST_N;
    SR(D1_CMU_03) = D1_CMU_03_REG_ENHANCE_IBN_BIAS_NORMAL;
    i_lanes4(LN(D1_L0_DFE_EN_1), DFE_EN_1_INIT);
    i_dfe_init();
    SR(D1_CMU_01) = D1_CMU_01_REG_CMU_EN | D1_CMU_01_REG_CMU_CKSEL;
    SR(D1_ANA_CDR_01) = D1_ANA_CDR_01_REG_AUTO_MODE | CDR01_MANUAL | CDR01_BASE;
    SR(D1_ANA_CDR_02) = CDR02_BASE;
    SR(D1_ANA_CDR) = ACDR_BYPASS | D1_ANA_CDR_REG_EN_EQEN_MANUAL_L0;
    i_lanes4(LN(D1_L0_DFE_EN_2), DFE_LE | DFE_VTH | DFE_TAP2 | DFE_TAP1 | DFE_TAP0);
    i_units(0x4B);
    SR(D1_ANA_CDR_02) = CDR02_BASE | D1_ANA_CDR_02_REG_RSTB_FSM;
    i_lanes4(LN(D1_L0_DFE_EN_2), 0x00);
    SR(D1_ANA_CDR_01) = D1_ANA_CDR_01_REG_AUTO_MODE | CDR01_BASE;
    /* VCO band (manual codes, lanes 0-3), two settings, each followed by a signal-detect check */
    SR(D1_ANA_CDR_07) = 0x57; SR(D1_ANA_CDR_08) = 0x55;
    SR(D1_ANA_CDR_09) = 0x56; SR(D1_ANA_CDR_10) = D1_ANA_CDR_10_REG_EN_WIDE_TEMP | 0x56;
    SR(D1_ANA_CDR_01) = D1_ANA_CDR_01_REG_ADP_EN_MANUAL | CDR01_BASE;
    i_short(5);
    i_sigdet(SIGDET_BASE | D1_DP_SIG_DET_0_LN_CK_SEL);
    SR(D1_ANA_CDR_07) = 0x53; SR(D1_ANA_CDR_08) = 0x51;
    SR(D1_ANA_CDR_09) = 0x52; SR(D1_ANA_CDR_10) = D1_ANA_CDR_10_REG_EN_WIDE_TEMP | 0x52;
    i_short(5);
    i_sigdet(SIGDET_BASE | D1_DP_SIG_DET_0_LN_CK_SEL);
    SR(D1_ANA_CDR_06) = FV(D1_ANA_CDR_06_REG_SEL_CDR_RES, 4) | FV(D1_ANA_CDR_06_REG_CDR_CP_IN, 10);
    i_short(5);
    SR(D1_ANA_CDR_01) = CDR01_MANUAL | CDR01_BASE;
    i_short(10);
    SR(D1_ANA_CDR_06) = FV(D1_ANA_CDR_06_REG_SEL_CDR_RES, 4) | D1_ANA_CDR_06_REG_CDR_CP_IN_16_5U;
    i_short(8);
    SR(D1_LIMIT_10) = FV(D1_LIMIT_10_VTH_MAX, 6);
    i_lanes4(LN(D1_L0_DFE_EN_2), DFE_VTH | DFE_TAP0);
    SR(D1_ANA_CDR) = ACDR_BYPASS | D1_ANA_CDR_REG_EN_DATA_MANUAL_L0 | D1_ANA_CDR_REG_EN_EQEN_MANUAL_L0;
}

/* Equalization handler.  A lane passes when its
 * EQ CRC matched in either window; RBR/HBR accept any drive level.  Failing
 * lanes keep CR (nibble 1) and ask for one more pre-emphasis step. */
static u8 i_eq(void)
{
    u8 l, r, nib[4], ok = 1;

    i_sym_clear();
    i_eq_phy();
    i_sym_arm();
    i_read_drive();
    i_dpcd_w(DPCD_LANE_ALIGN_STATUS_UPDATED, 0x80);
    /* two windows (EQ_CRC_SEL 1 then 0); the match flags are W1C */
    SR(D1_EQ_CRC_1) = D1_EQ_CRC_1_EQ_CRC_EN | D1_EQ_CRC_1_EQ_CRC_SEL | EQ_CRC_OK_ALL;
    i_short(5);
    r = SR(D1_EQ_CRC_1) & EQ_CRC_OK_ALL;
    SR(D1_EQ_CRC_1) = D1_EQ_CRC_1_EQ_CRC_SEL | EQ_CRC_OK_ALL;
    SR(D1_EQ_CRC_1) = D1_EQ_CRC_1_EQ_CRC_EN | EQ_CRC_OK_ALL;
    i_short(5);
    r |= SR(D1_EQ_CRC_1) & EQ_CRC_OK_ALL;
    SR(D1_EQ_CRC_1) = EQ_CRC_OK_ALL;
    dp_log.eqcrc = r;
    for (l = 0; l < 4; l++) {
        u8 sw = drv[l] & 3, pre = (drv[l] >> 3) & 3;
        nib[l] = 0;
        req[l] = 0;
        if (l >= lt_lanes)
            continue;
        if ((r >> l) & 1) {
            nib[l] = 0x07;              /* CR, EQ, symbol lock */
        } else {
            nib[l] = 0x01;
            ok = 0;
            if (pre < 3)
                pre++;
        }
        req[l] = sw | (pre << 2);
    }
    i_status(nib, ok);
    i_reply();
    SR(D1_PHY_DIG_RESET_CTRL) = i_b001() & (u8)~D1_PHY_DIG_RESET_CTRL_LANE_NUM;   /* lane_num 0 until training ends */
    return ok;
}

/* training end */
static void i_lt_end(u8 p102)
{
    u8 b001 = (i_b001() & ~D1_PHY_DIG_RESET_CTRL_ENHANCE_MODE) | lt_enh;

    i_dpcd_w(DPCD_SINK_CONTROL, (i_dpcd_r(DPCD_SINK_CONTROL) & 0xF8) | 0x01);
    SR(D1_PHY_DIG_RESET_CTRL) = b001;
    SR_CLR(D1_DESKEW_PHY, D1_DESKEW_PHY_DESKEW_2LCLK_EN | D1_DESKEW_PHY_INTRA_DESKEW_EN);   /* intra-lane deskew pulse */
    SR_SET(D1_DESKEW_PHY, D1_DESKEW_PHY_INTRA_DESKEW_EN);
    SR(D1_PHY_DIG_RESET_CTRL) = b001;
    SR(D1_SCRAMBLE_CTRL) = (SR(D1_SCRAMBLE_CTRL) & (u8)~D1_SCRAMBLE_CTRL_SCRAMB_DIS)
                           | (p102 & DPCD_TRAINING_PATTERN_SET_SCRAMBLING_DISABLE);
    i_dpcd_w(DPCD_SINK_STATUS, 0x01);
}

/* foreign INT0 sources: our code polls their STATUS bits, so the handler
 * only drops their interrupt ENABLE (writing 0 to the W1C status bits). */
static u8 i_foreign(void)
{
    u8 v, hit = 0;

    v = SR(HDMI_ERROR_TH);
    if ((v & (HDMI_ERROR_TH_FREQ_STABLE_IRQ_EN | HDMI_ERROR_TH_FREQ_STABLE)) ==
        (HDMI_ERROR_TH_FREQ_STABLE_IRQ_EN | HDMI_ERROR_TH_FREQ_STABLE)) {
        SR(HDMI_ERROR_TH) = v & (u8)~(HDMI_ERROR_TH_FREQ_STABLE_IRQ_EN | HDMI_ERROR_TH_FREQ_STABLE);
        hit = 1;
    }
    v = SR(HDMI_FREQDET_CTRL);
    if ((v & (HDMI_FREQDET_CTRL_IRQ_FREQ_UNSTABLE | HDMI_FREQDET_CTRL_FREQ_UNSTABLE)) ==
        (HDMI_FREQDET_CTRL_IRQ_FREQ_UNSTABLE | HDMI_FREQDET_CTRL_FREQ_UNSTABLE)) {
        SR(HDMI_FREQDET_CTRL) = v & (u8)~(HDMI_FREQDET_CTRL_FREQ_UNSTABLE | HDMI_FREQDET_CTRL_IRQ_FREQ_UNSTABLE);
        hit = 1;
    }
    /* HDMI character-error flags: acknowledged on every pass
     * (W1C LOCK_TRA_CH0-2).  Left set after an HDMI event they hold
     * INT0 for good (an interrupt storm); IE0 drops when this is written. */
    v = SR(HDMI_CTRL_17);
    if (v & (HDMI_CTRL_17_CED_UPDATE_CH0 | LOCK_TRA_ALL)) {
        SR(HDMI_CTRL_17) = v | LOCK_TRA_ALL;
        hit = 1;
    }
    /* HDMI HDCP: the source wrote Aksv - acknowledge and mark the keys ready;
     * the hardware computes Ri */
    v = SR(TMDS_ABC0);
    if ((v & TMDS_ABC0_WR_AKSV_FLAG) && (SR(TMDS_ABC1) & TMDS_ABC1_IRQ_AKSV_EN)) {
        SR(TMDS_ABC0) = (v & (u8)~ABC0_W1C) | TMDS_ABC0_WR_AKSV_FLAG;
        SR_SET(TMDS_RPS, TMDS_RPS_HDCP_KEY_RDY);
        hit = 1;
    }
    v = SR(HPD_IRQ) & SR(HPD_IRQ_EN) & HPD_INTS;
    if (v) {
        SR(HPD_IRQ_EN) &= ~v;           /* eDP TX hot-plug events */
        hit = 1;
    }
    return hit;
}

void dp_aux_isr(void) __interrupt(0)
{
    u8 f, h, m, l, p102, pat, v;

    if (dp_log.isr != 0xFF)
        dp_log.isr++;
    f = SR(D1_AUX_DPCD_IRQ) & SR(D1_AUX_DPCD_IRQ_EN);
    if (!f) {
        /* Every AUX request sets BADC.0, and it reaches INT0 (through the ADI
         * group, 6F10) whatever its enable bit says: claim and count it. */
        v = SR(D1_AUX_IRQ_STATUS);
        if (v & AUX_IRQ_FLAGS) {
            SR(D1_AUX_IRQ_STATUS) = v;  /* W1C the flags, enables written back as they were */
            if (dp_log.aux_rx != 0xFF)
                dp_log.aux_rx++;
            dp_log.last_cmd = SR(D1_AUX_RX_CMD);
            dp_log.last_addr_m = SR(D1_AUX_RX_ADDR_M);
            dp_log.last_addr_l = SR(D1_AUX_RX_ADDR_L);
            return;
        }
        /* AUX wake flags: AUX-PHY signal toggle (BA77.6),
         * Rx-state (BA7A.0) and PHY error (BA79[3:0]) flags, all W1C */
        v = SR(D1_AUX_DIG_PHY7) & D1_AUX_DIG_PHY7_AUX_PHY_INT;
        h = SR(D1_AUX_DIG_PHYA) & D1_AUX_DIG_PHYA_AUX_RXST_INT;
        m = SR(D1_AUX_DIG_PHY9) & AUX_PHY_ERRS;
        if (v | h | m) {
            if (v)
                SR(D1_AUX_DIG_PHY7) = (SR(D1_AUX_DIG_PHY7) & D1_AUX_DIG_PHY7_AUX_PHY_INT_EN) | v;
            if (h)
                SR(D1_AUX_DIG_PHYA) = (SR(D1_AUX_DIG_PHYA) & D1_AUX_DIG_PHYA_AUX_RXST_INT_EN) | h;
            if (m)
                SR(D1_AUX_DIG_PHY9) = (SR(D1_AUX_DIG_PHY9) & (u8)~AUX_PHY_ERRS) | m;
            if (dp_log.wake != 0xFF)
                dp_log.wake++;
            return;
        }
        if (i_foreign()) {
            if (dp_log.foreign != 0xFF)
                dp_log.foreign++;
        } else {
            /* D1 flags outside our enables (W1C).  Only those: an enabled one
             * that arrived since f was read is the next pass's work - clearing
             * it here would lose it (e.g. the source's training-end write). */
            v = SR(D1_AUX_DPCD_IRQ) & ~SR(D1_AUX_DPCD_IRQ_EN);
            if (v)
                SR(D1_AUX_DPCD_IRQ) = v;
            SR(D1_AUX_IRQ_STATUS) = (SR(D1_AUX_IRQ_STATUS) & AUX_IRQ_ENS) | AUX_IRQ_FLAGS;   /* W1C */
            /* the HDMI block's interrupt: acked (Hidx60 bit 7) at the
             * end of every INT0 pass.  The main loop uses the index port too. */
            v = SR(HDMI_AP);
            SR(HDMI_AP) = HDMI_INTCR;
            SR(HDMI_DP) = HDMI_INTCR_PENDING;
            SR(HDMI_AP) = v;
            if (dp_log.stray != 0xFF)
                dp_log.stray++;
            if (++irq_burst >= 50000u)
                EX0 = 0;                /* a storm (no main-loop pass in between): stop */
        }
        return;
    }
    SR(D1_AUX_MODE_SET) &= (u8)~D1_AUX_MODE_SET_DP_AUX_AUTO_EN;    /* source waits */
    h = SR(D1_DPCD_ADDR_PORT_H);
    m = SR(D1_DPCD_ADDR_PORT_M);
    l = SR(D1_DPCD_ADDR_PORT_L);
    SR(D1_DPCD_CTRL) &= (u8)~D1_DPCD_CTRL_DPCD_ACCESS_PORT_AUTO_INC;

    if (f & (D1_AUX_DPCD_IRQ_WR102_INT | D1_AUX_DPCD_IRQ_WR100_101_INT | D1_AUX_DPCD_IRQ_WR103_108_INT)) {
        p102 = i_dpcd_r(DPCD_TRAINING_PATTERN_SET);
        lt_rate = i_dpcd_r(DPCD_LINK_BW_SET);
        v = i_dpcd_r(DPCD_LINK_COUNT_SET);
        lt_enh = v & 0x80 ? D1_PHY_DIG_RESET_CTRL_ENHANCE_MODE : 0;
        v &= 0x1F;
        lt_lanes = v >= 4 ? 4 : v == 2 ? 2 : 1;
        pat = p102 & DPCD_TRAINING_PATTERN_SET_TRAINING_PATTERN_SET;
        if (dp_log.lt_writes != 0xFF)
            dp_log.lt_writes++;
        dp_log.rate = lt_rate;
        dp_log.lanes = v;
        dp_log.pattern = p102;
        if (pat == 1) {
            if (lt_state != LT_CR_OK || (f & D1_AUX_DPCD_IRQ_WR100_101_INT)) {
                lt_state = i_cr() ? LT_CR_OK : LT_CR_RETRY;
                dp_log.cr_rounds++;
            }
        } else if (pat) {
            if (lt_state == LT_CR_OK || lt_state == LT_EQ_RETRY) {
                lt_state = i_eq() ? LT_EQ_OK : LT_EQ_RETRY;
                dp_log.eq_rounds++;
            }
        } else if (lt_state == LT_EQ_OK) {
            i_lt_end(p102);
            lt_state = LT_TRAINED;
            dp_log.lt_gen++;            /* a (re)trained link: the stream must be redone */
        } else if (lt_state == LT_CR_RETRY || lt_state == LT_EQ_RETRY) {
            lt_state = LT_FAILED;
        }
        dp_log.lt_state = lt_state;
    }
    if (f & D1_AUX_DPCD_IRQ_WR600_INT) {
        v = i_dpcd_r(DPCD_SINK_CONTROL) & 7;
        dp_log.power = v;
        if (v == 2 || v == 5) {                         /* D3: report the link lost */
            pw_save[0] = i_dpcd_r(DPCD_LANE0_1_STATUS);
            pw_save[1] = i_dpcd_r(DPCD_LANE2_3_STATUS);
            pw_save[2] = i_dpcd_r(DPCD_LANE_ALIGN_STATUS_UPDATED);
            i_dpcd_w(DPCD_LANE0_1_STATUS, 0);
            i_dpcd_w(DPCD_LANE2_3_STATUS, 0);
            i_dpcd_w(DPCD_LANE_ALIGN_STATUS_UPDATED, pw_save[2] & 0xFE);
        } else if (v == 1) {
            i_dpcd_w(DPCD_LANE0_1_STATUS, pw_save[0]);
            i_dpcd_w(DPCD_LANE2_3_STATUS, pw_save[1]);
            i_dpcd_w(DPCD_LANE_ALIGN_STATUS_UPDATED, pw_save[2]);
        }
    }
    SR(D1_AUX_DPCD_IRQ) = f;            /* W1C */
    dp_log.dpcd_irq |= f;
    SR(D1_DPCD_ADDR_PORT_H) = h;
    SR(D1_DPCD_ADDR_PORT_M) = m;
    SR(D1_DPCD_ADDR_PORT_L) = l;
    SR(D1_AUX_MODE_SET) &= (u8)~D1_AUX_MODE_SET_DP_AUX_FORCE_DEFER_EN;
    SR(D1_AUX_MODE_SET) |= D1_AUX_MODE_SET_DP_AUX_AUTO_EN;
}

/* ------------------------- end of ISR CONTEXT ------------------------- */

/* Common: main.c drops HPD at boot, before anything banked has run. */
void dp_hpd(u8 on)
{
    P1_4 = on ? 1 : 0;
}
