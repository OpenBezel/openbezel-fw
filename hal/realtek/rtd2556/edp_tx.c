/* RTD2556 eDP transmitter TX1: AUX channel, PHY0 bring-up, link training,
 * video stream.  Sequence:
 *  - boot:      Group-A power domain + 3900 handshake, AUX PHY + HPD setup
 *  - clock:     8006 GDI_CLK_DIVIDER bit4 = M2PLL for "DP Tx Aux"
 *  - panel on:  panel VCC -> PHY0 (edp_phy0_on)
 *  - link:      SET_POWER, caps -> config -> TPS1/CR -> TPS2/EQ -> finish
 */
#include "edp_tx.h"
#include "delay.h"
#include "timebase.h"
#include "board.h"
#include "rtd2556_regs.h"


#define AUX_CMD_NATIVE_WRITE 0x8
#define AUX_CMD_NATIVE_READ 0x9

/* Fixed link for the intouch156b panel */
#define LINK_RATE PANEL_EDP_LINK_RATE   /* board.h */
#define LANE_COUNT PANEL_EDP_LANES
#if PANEL_EDP_LANES != 2 || PANEL_BPC != 6
#error "the PHY/MAC setup and the stream block below are for 2 lanes at 6 bpc"
#endif
/* link symbol clock per lane, kHz (DPCD link rate code x 27 MHz) */
#define LINK_SYM_KHZ ((u32)PANEL_EDP_LINK_RATE * 27000UL)
#define LANE_SET_INITIAL 0x01     /* swing level 1, pre-emphasis 0 (panel's request) */
#define EDP_LANE_NUM DP_MAC_CTRL_LANE_NUM_2_LANE       /* LANE_COUNT */
#define MSA_COLORBIT MSA_MISC0_COLORBIT_6_BIT          /* PANEL_BPC */
#define EDP_NVID 0x8000UL                              /* N: M / N = pixel clock / link symbol clock */
#define MVID_DEFAULT 0x004769UL                        /* M before the first measurement */
#define HDELAY 64                                      /* HSYNC to BE delay, link clocks */
#define SEC_END_CNT 71                                 /* arbiter secondary-data end count */

/* AUX_RETRY_2: all five retry-condition enables ([7:5], [4], [3]) */
#define AUX_RETRY_2_ALL_EN (AUX_RETRY_2_RETRY_EN | AUX_RETRY_2_RETRY_DEFER_EN | \
                            AUX_RETRY_2_RETRY_TIMEOUT_EN | AUX_RETRY_2_RETRY_NACK_EN | \
                            AUX_RETRY_2_RETRY_ERROR_EN)
/* AUX_IRQ_EVENT: every transaction flag ([5:0], w1c) */
#define AUX_IRQ_EVENT_ALL (AUX_IRQ_EVENT_AUXDONE_IRQ | AUX_IRQ_EVENT_RXERROR_IRQ | \
                           AUX_IRQ_EVENT_READFAIL_IRQ | AUX_IRQ_EVENT_NACK_IRQ | \
                           AUX_IRQ_EVENT_RETRY_IRQ | AUX_IRQ_EVENT_TIMEOUT_IRQ)
/* AUX_DIG_PHY9: the four PHY error flags ([3:0], w1c) */
#define AUX_DIG_PHY9_ERRORS (AUX_DIG_PHY9_MAN_ERROR | AUX_DIG_PHY9_STOP_ERROR | \
                             AUX_DIG_PHY9_SYNCEND_ERROR | AUX_DIG_PHY9_DATA_8X_ERROR)
/* HPD_IRQ: every hot-plug event flag ([7:1], w1c) */
#define HPD_IRQ_ALL (HPD_IRQ_IHPD_INT | HPD_IRQ_SHPD_INT | HPD_IRQ_LHPD_INT | HPD_IRQ_UHPD_INT | \
                     HPD_IRQ_UNHPD_INT | HPD_IRQ_RHPD_INT | HPD_IRQ_FHPD_INT)
/* PG_FIFO_CTRL: the FIFO status flags ([7:1], w1c); [0] reserved */
#define PG_FIFO_FLAGS (PG_FIFO_CTRL_LFIFO_FULL | PG_FIFO_CTRL_LFIFO_UDF | PG_FIFO_CTRL_LFIFO_UDF_UNI | \
                       PG_FIFO_CTRL_SFIFO_FUL | PG_FIFO_CTRL_SFIFO_OVE | PG_FIFO_CTRL_SFIFO_OVE_UNI | \
                       PG_FIFO_CTRL_LFIFO_MAXWL_CLEAR)

/* PHY0 statics written in more than one place */
#define PN_SWAP_ALL DPTX_PN_SWAP1_DPTX_PN_SWAP                 /* P/N swapped on all four lanes */
#define LANE_SWAP_32 (FV(DPTX_LANE_SWAP3_2_LANE3_SOURCE, 1) | FV(DPTX_LANE_SWAP3_2_LANE2_SOURCE, 0))
#define LANE_SWAP_10 (FV(DPTX_LANE_SWAP1_0_LANE1_SOURCE, 3) | FV(DPTX_LANE_SWAP1_0_LANE0_SOURCE, 2))
/* pre-driver 2x on all four lanes, port B lane current 2x = 3 */
#define DOUBLE_HALF_CURRENT (DPTX_DOUBLE_HALF_CURRENT_REG_TXA0_IB2X_CML | DPTX_DOUBLE_HALF_CURRENT_REG_TXA1_IB2X_CML | \
                             DPTX_DOUBLE_HALF_CURRENT_REG_TXB3_IB2X_CML | DPTX_DOUBLE_HALF_CURRENT_REG_TXB4_IB2X_CML | \
                             FV(DPTX_DOUBLE_HALF_CURRENT_REG_TXB_IB2X_DP, 3))
#define SWING_CURRENT_B (FV(DPTX_SWING_CURRENT_2_REG_TXB3_ISEL_COMBO, 2) | FV(DPTX_SWING_CURRENT_2_REG_TXB4_ISEL_COMBO, 2))
#define TXBIST_TRAINING (PHY0_TXBIST_00_H2_REG_BIST_REPEAT | PHY0_TXBIST_00_H2_REG_BIST_TP_SEL)
#define LVDS_POWER (P55_LVDS_COMMON_AB_CTRL0_REG_AB_IBGEN_POWL | P55_LVDS_COMMON_AB_CTRL0_REG_TXA_POWL | \
                    P55_LVDS_COMMON_AB_CTRL0_REG_TXB_POWL)
#define Z0_COMBO_LANES (DPTX_Z0_TX_1_REG_TXA0_RO_COMBO | DPTX_Z0_TX_1_REG_TXA1_RO_COMBO | \
                        DPTX_Z0_TX_1_REG_TXB3_RO_COMBO | DPTX_Z0_TX_1_REG_TXB4_RO_COMBO)

struct edp_status edp_stat;

/* ---------------------------------------------------------------------- */
/* AUX channel                                                            */
/* ---------------------------------------------------------------------- */

static void aux_begin(void)
{
    SR_CLR(HPD_IRQ_EN, HPD_IRQ_EN_SHPD_INT_EN);                    /* mask shpd irq */
    SR_WRF(AUXTX_TRAN_CTRL, (AUXTX_TRAN_CTRL_TX_ADDRONLY | AUXTX_TRAN_CTRL_AUX_FW_HANDLE),
           AUXTX_TRAN_CTRL_AUX_FW_HANDLE);                        /* fw handle */
    SR_CLR(AUX_RETRY_2, AUX_RETRY_2_RETRY_EN);
    SR_WRF(AUX_RETRY_2, AUX_RETRY_2_ALL_EN,
           (AUX_RETRY_2_RETRY_EN | AUX_RETRY_2_RETRY_TIMEOUT_EN)); /* retry on timeout */
    SR(AUX_IRQ_EVENT) = AUX_IRQ_EVENT_ALL;                         /* clear flags */
    SR_SET(AUX_FIFO_CTRL, (AUX_FIFO_CTRL_I2C_FIFO_RST | AUX_FIFO_CTRL_NA_FIFO_RST)); /* reset FIFOs */
}

static void aux_end(void)
{
    SR_CLR(AUX_RETRY_2, AUX_RETRY_2_ALL_EN);
    SR(AUX_IRQ_EVENT) = AUX_IRQ_EVENT_ALL;
    SR_SET(AUX_FIFO_CTRL, (AUX_FIFO_CTRL_I2C_FIFO_RST | AUX_FIFO_CTRL_NA_FIFO_RST));
    SR_SET(HPD_IRQ_EN, HPD_IRQ_EN_SHPD_INT_EN);
}

/* Start the transaction and wait for auxdone, sampling the PHY/MAC state
 * machines meanwhile (diagnostics).  AUX_OK when the sink ACKed. */
static u8 aux_go(void)
{
    u16 t;
    u8 v;

    SR(AUX_DIG_PHY7) = AUX_DIG_PHY7_AUX_PHY_INT;                   /* w1c */
    SR(AUX_DIG_PHY9) = AUX_DIG_PHY9_ERRORS;                        /* w1c */
    edp_stat.mac_fsm_max = edp_stat.phy_fsm_max = 0;
    SR_CLR(AUXTX_TRAN_CTRL, AUXTX_TRAN_CTRL_TX_START);
    SR_SET(AUXTX_TRAN_CTRL, AUXTX_TRAN_CTRL_TX_START);
    for (t = 0; t < 20000; t++) {            /* >= ~5 ms at PLL speed (8 hw retries x 400 us) */
        v = SR(AUX_DEBUG) & AUX_DEBUG_FSM_STATE;
        if (v > edp_stat.phy_fsm_max) edp_stat.phy_fsm_max = v;
        v = SR(AUX_FSM_STATUS) & AUX_FSM_STATUS_AUXCH_FSM;
        if (v > edp_stat.mac_fsm_max) edp_stat.mac_fsm_max = v;
        if (SR(AUX_IRQ_EVENT) & AUX_IRQ_EVENT_AUXDONE_IRQ)
            break;
    }
    edp_stat.phy_flags = (SR(AUX_DIG_PHY7) & AUX_DIG_PHY7_AUX_PHY_INT) |
                         (SR(AUX_DIG_PHY9) & AUX_DIG_PHY9_ERRORS);
    if (!(SR(AUX_IRQ_EVENT) & AUX_IRQ_EVENT_AUXDONE_IRQ) ||
        (SR(AUX_IRQ_EVENT) & AUX_IRQ_EVENT_NACK_IRQ) ||
        (SR(AUX_REPLY_CMD) & FV(AUX_REPLY_CMD_AUX_RX_CMD, 3))) {   /* AUX_RX_CMD[1:0] != 0: not an ACK */
        edp_stat.aux_dbg[0] = SR(AUX_IRQ_EVENT);
        edp_stat.aux_dbg[1] = SR(AUX_FSM_STATUS);
        edp_stat.aux_dbg[2] = SR(AUX_TX_FIFO_STATUS);
        edp_stat.aux_dbg[3] = SR(AUXTX_TRAN_CTRL);
        return (SR(AUX_IRQ_EVENT) & AUX_IRQ_EVENT_AUXDONE_IRQ) ? AUX_ERR_NACK : AUX_ERR_TIMEOUT;
    }
    return AUX_OK;
}

static u8 aux_xfer(u8 cmd, u8 addr_hi, u16 addr, u8 len, u8 *buf)
{
    u8 r, i, n, attempt;

    for (attempt = 0; attempt < 4; attempt++) {
        aux_begin();
        SR(AUXTX_REQ_CMD) = (cmd << AUXTX_REQ_CMD_TX_CMD_SHIFT) | (addr_hi & AUXTX_REQ_CMD_TX_ADDR);
        SR(AUXTX_REQ_ADDR_M) = HI(addr);
        SR(AUXTX_REQ_ADDR_L) = LO(addr);
        SR(AUXTX_REQ_LEN) = len - 1;
        if (cmd == AUX_CMD_NATIVE_WRITE)
            for (i = 0; i < len; i++)
                SR(AUXTX_REQ_DATA) = buf[i];
        r = aux_go();
        if (r == AUX_OK && cmd == AUX_CMD_NATIVE_READ) {
            n = SR(AUX_FIFO_WR_PTR) & AUX_FIFO_WR_PTR_WR_PTR;
            for (i = 0; i < len; i++)
                buf[i] = (i < n) ? SR(AUX_REPLY_DATA) : 0xFF;
            if (n < len)
                r = AUX_ERR_SHORT;
        }
        aux_end();
        if (r == AUX_OK)
            return r;
        delay_us(600);
    }
    edp_stat.aux_err = r;
    return r;
}

u8 edp_aux_read(u8 addr_hi, u16 addr, u8 len, u8 *buf)
{
    return aux_xfer(AUX_CMD_NATIVE_READ, addr_hi, addr, len, buf);
}

u8 edp_aux_write(u8 addr_hi, u16 addr, u8 len, const u8 *buf)
{
    return aux_xfer(AUX_CMD_NATIVE_WRITE, addr_hi, addr, len, (u8 *)buf);
}

static u8 dpcd_write1(u16 addr, u8 val)
{
    return edp_aux_write(0, addr, 1, &val);
}

/* ---------------------------------------------------------------------- */
/* Bring-up                                                               */
/* ---------------------------------------------------------------------- */

/* Boot-time enables:
 *  - 8006 GDI_CLK_DIVIDER = 1D: bit4 clocks "DP Tx Aux" (and GDI/CBUS/...)
 *    from M2PLL/13 instead of the crystal.  On the crystal the AUX 1 MHz
 *    Manchester clock is off-frequency and the panel ignores every request
 *    (the RX then decodes silence as ACK + 16 zero bytes).  It survives soft
 *    reset, so only a cold boot shows whether it is set.
 *  - Group-A power domain: isolation off (0009[1], 00F0[4]) and the 3900
 *    request/acknowledge handshake (write 1, poll until it reads 1).
 *  - TX1 AUX PHY, timeout, aux_en; HPD timers + detection on;
 *    SFIFO tx_en; arbiter. */
void edp_tx_init(void)
{
    u8 i;

    SR(GDI_CLK_DIVIDER) = GDI_CLK_DIVIDER_GDI_CLK_SEL | FV(GDI_CLK_DIVIDER_GDI_CLK_DIV, 13);    /* M2PLL / 13 */
    SR(XTAL_CTRL1) = XTAL_CTRL1_REG_G2B | XTAL_CTRL1_REG_G1B;
    SR_CLR(TOP_POWER_CTRL, TOP_POWER_CTRL_TOP_PWRCUT_ISO_CTRL);
    SR_CLR(VCCKOFF_CONTROL0, VCCKOFF_CONTROL0_REG_POR_VCCKOFF_TOP);
    for (i = 0; i < 30; i++) {
        SR(LVDS_DISP_TYPE) = LVDS_DISP_TYPE_DISP_TYPE_LVDS;      /* 3900 request/ack handshake */
        if (SR(LVDS_DISP_TYPE) == LVDS_DISP_TYPE_DISP_TYPE_LVDS)
            break;
        delay_ms(1);
    }
    edp_stat.groupa_polls = (i < 30) ? i : 0xFF;
    delay_ms(10);

    SR_CLR(AUX_TX_CTRL, AUX_TX_CTRL_AUX_EN);
    SR(AUX_1) = (SR(AUX_1) & (u8)~AUX_1_REG_AUX_VLDO) | AUX_1_REG_AUX_VLDO_1_05V | FV(AUX_1_REG_AUX_RX_ADJR_P, 8);
    SR_CLR(AUX_2, (AUX_2_REG_AUX_EN_SINGLEEND | AUX_2_REG_EN_AUX_RX_BIGZ0_P));
    SR_WRF(AUX_6, (AUX_6_REG_EN_AUX_RX_BIGZ0_N | AUX_6_REG_AUX_RX_ADJR_N), FV(AUX_6_REG_AUX_RX_ADJR_N, 8));
    SR(DIG_TX_03) = FV(DIG_TX_03_REG_AUX_TX_ADJR_P, 8) | FV(DIG_TX_03_REG_AUX_TX_ADJR_N, 8);
    SR(AUX_TIMEOUT) = AUX_TIMEOUT_AUX_TIMEOUT_EN | FV(AUX_TIMEOUT_AUX_TIMEOUT_NUM, 21);
    SR_SET(AUX_DIG_PHY8, AUX_DIG_PHY8_EHANDLE_SYNCEND_DIS);
    SR_SET(AUX_TX_CTRL, AUX_TX_CTRL_AUX_EN);
    SR_WRF(HPD_TIMER1, HPD_TIMER1_HPD_T10, FV(HPD_TIMER1_HPD_T10, 13));   /* HPD timers */
    SR(HPD_TIMER3) = 0x2C; SR(HPD_TIMER6) = 0xC5; SR(HPD_TIMER7) = 0xBD;
    SR_CLR(HPD_CTRL, HPD_CTRL_HPD_EN);
    SR_SET(HPD_CTRL, HPD_CTRL_HPD_EN);                           /* HPD detection on */
    SR(HPD_IRQ) = HPD_IRQ_ALL;                                   /* w1c all */
    SR(HPD_IRQ_EN) = 0x00;
    SR_SET(DPTX_SFIFO_CTRL0, DPTX_SFIFO_CTRL0_TX_EN);
    SR_SET(ARBITER_CTRL, ARBITER_CTRL_ARBITER_EN);
}

u8 edp_hpd(void)
{
    return (SR(HPD_CTRL) & HPD_CTRL_HPD_DEB) ? 1 : 0;
}

/* Give the panel a real VCC power-cycle edge: it only (re)trains after its
 * VCC drops, and soft resets never drop panel VCC.  Park PHY0 first
 * (power-down: PHY_RST held, analog off) so nothing drives an
 * unpowered panel. */
void edp_panel_power_cycle(void)
{
    SR_CLR(PHY_RST, PHY_RST_VBY1_DPHY_RST_N);
    SR(P55_LVDS_COMMON_AB_CTRL0) = 0x00;
    BOARD_PANEL_VCC = 0;
    delay_ms(300);                            /* the panel needs >= 250 ms */
    BOARD_PANEL_VCC = 1;
    delay_ms(100);
}

/* PHY0 power-on */
void edp_phy0_on(void)
{
    /* statics not covered by the sequences below */
    SR(P55_LVDS_COMMON_AB_CTRL3) = FV(P55_LVDS_COMMON_AB_CTRL3_REG_CK1X_PI_FREQ, 2) | P55_LVDS_COMMON_AB_CTRL3_REG_CKPIXEL_INV
                                   | P55_LVDS_COMMON_AB_CTRL3_REG_CMU_BIG_KVCO;
    SR(P55_LVDS_COMMON_AB_CTRL13) = FV(P55_LVDS_COMMON_AB_CTRL13_REG_CMU_SEL_R1, 7) | P55_LVDS_COMMON_AB_CTRL13_REG_CMU_SEL_CP
                                    | FV(P55_LVDS_COMMON_AB_CTRL13_REG_CMU_SEL_PUMP_I, 14);
    SR(DPTX_DOUBLE_HALF_CURRENT) = DOUBLE_HALF_CURRENT; SR(DPTX_SWING_CURRENT_2) = SWING_CURRENT_B;
    SR(DPTX_CMU0) = FV(DPTX_CMU0_REG_CMU_PI_I_SEL, 3); SR(DPTX_PN_SWAP1) = PN_SWAP_ALL;
    SR(DPTX_LANE_SWAP3_2) = LANE_SWAP_32; SR(DPTX_LANE_SWAP1_0) = LANE_SWAP_10;
    SR(DPTX_CMU5) = DPTX_CMU5_EN_SSC;
    SR(DPTX_CMU14) = 0x97; SR(DPTX_CMU13) = 0xA4;                         /* F/N code, split over CMU12-14 */
    SR(DPTX_CMU12) = DPTX_CMU12_N_F_AUTO_DB | FV(DPTX_CMU12_N_CODE, 5);   /* N_F_AUTO_DB latches the code */
    SR(DPTX_CMU16) = 0x01; SR(DPTX_CMU17) = 0xC0; SR(DPTX_CMU19) = 0x14;  /* SSC TBASE, STEP_IN */
    SR_WRF(LVDS_DISP_TYPE, LVDS_DISP_TYPE_DISP_TYPE, FV(LVDS_DISP_TYPE_DISP_TYPE, 4));
    SR_SET(PHY0_TXBIST_00_L1, (PHY0_TXBIST_00_L1_REG_SCRAMBLE_DEFAULT | PHY0_TXBIST_00_L1_REG_X1X2_SEL));
    /* eDP, D2A clock on the positive edge */
    SR_WRF(PHY0_TXBIST_01_H2,
           PHY0_TXBIST_01_H2_REG_SCRAMBLE_MODE | PHY0_TXBIST_01_H2_REG_VBY1_EDP_MODE | PHY0_TXBIST_01_H2_REG_D2A_CLKINV | 0x01,
           PHY0_TXBIST_01_H2_REG_SCRAMBLE_MODE | PHY0_TXBIST_01_H2_REG_VBY1_EDP_MODE | 0x01);
    SR_WRF(P55_LVDS_COMMON_AB_CTRL1, P55_LVDS_COMMON_AB_CTRL1_REG_AB_VCMSEL, FV(P55_LVDS_COMMON_AB_CTRL1_REG_AB_VCMSEL, 2));
    SR_WRF(P55_LVDS_COMMON_AB_CTRL2, P55_LVDS_COMMON_AB_CTRL2_REG_AB_ISEL, FV(P55_LVDS_COMMON_AB_CTRL2_REG_AB_ISEL, 15));
    SR_CLR(P55_LVDS_PORTA_CTRL6, P55_LVDS_PORTA_CTRL6_REG_TXA_PULLDN);
    SR_CLR(P55_LVDS_PORTB_CTRL6, P55_LVDS_PORTB_CTRL6_REG_TXB_PULLDN);
    SR_SET(P55_LVDS_COMMON_AB_CTRL0, LVDS_POWER);
    SR_SET(DPTX_CMU3, DPTX_CMU3_REG_DPTX_LDO_POWL);
    SR_SET(DCLK_GATED_CTRL2, DCLK_GATED_CTRL2_VBYO_CLKEN | DCLK_GATED_CTRL2_DPTX_CLKEN);
    SR_SET(P55_LVDS_COMMON_AB_CTRL0, P55_LVDS_COMMON_AB_CTRL0_REG_CMU_EN);   /* -> 0xE8 */
    SR_SET(PHY_RST, PHY_RST_VBY1_DPHY_RST_N);                       /* PHY_RST released */
    /* all lanes; [7:4] reserved here, part of reg_lane_enable[7:0] */
    SR(PHY0_TXBIST_02_H1) = 0xF0 | PHY0_TXBIST_02_H1_REG_LANE_ENABLE;
    SR_SET(DPTX_Z0_TX_1, Z0_COMBO_LANES);
    SR(DPTX_Z0_TX_2) = FV(DPTX_Z0_TX_2_REG_TXA0_RO_COMBO, 8) | FV(DPTX_Z0_TX_2_REG_TXA1_RO_COMBO, 8);
    SR(DPTX_Z0_TX_3) = FV(DPTX_Z0_TX_3_REG_TXB3_RO_COMBO, 8) | FV(DPTX_Z0_TX_3_REG_TXB4_RO_COMBO, 8);
    SR_WRF(PHY0_TXBIST_00_H2, (PHY0_TXBIST_00_H2_REG_DELAY_SEL0 | PHY0_TXBIST_00_H2_REG_DELAY_SEL1),
           FV(PHY0_TXBIST_00_H2_REG_DELAY_SEL1, 2));
    SR_WRF(PHY0_TXBIST_00_H1, (PHY0_TXBIST_00_H1_REG_DELAY_SEL2 | PHY0_TXBIST_00_H1_REG_DELAY_SEL3),
           FV(PHY0_TXBIST_00_H1_REG_DELAY_SEL2, 4) | FV(PHY0_TXBIST_00_H1_REG_DELAY_SEL3, 6));
    delay_ms(10);
    SR(DPTX_CMU3) = DPTX_CMU3_REG_DPTX_LDO_POWL | DPTX_CMU3_REG_DPTX_LDO_VSEL_2_0V | DPTX_CMU3_REG_CMU_MODE;
    SR(PHY0_TXBIST_00_H2) = PHY0_TXBIST_00_H2_REG_BIST_TP_SEL | FV(PHY0_TXBIST_00_H2_REG_DELAY_SEL1, 2);
    SR(PHY0_TXBIST_00_L1) = (PHY0_TXBIST_00_L1_REG_PRBS_SEL | PHY0_TXBIST_00_L1_REG_X1X2_SEL);
    edp_stat.pll_stat = SR(DPTX_CMU4);
}

/* Drive levels (page 55) for swing level 1 / pre-emphasis 0 on both lanes.
 * Not adaptive: the intouch panel requests level 1 and trains with it. */
static void phy0_drive_golden(void)
{
    SR(DPTX_PREEM_TXA0) = 0x00; SR(DPTX_PREEM_TXA1) = 0x00;
    SR(DPTX_PREEM_TXB3) = 0x00; SR(DPTX_PREEM_TXB4) = 0x00;
    SR(DPTX_PRE_DRIVER_COMPENSATION) = FV(DPTX_PRE_DRIVER_COMPENSATION_REG_TXA0_BOOST, 1) |
        FV(DPTX_PRE_DRIVER_COMPENSATION_REG_TXA1_BOOST, 1) | FV(DPTX_PRE_DRIVER_COMPENSATION_REG_TXB3_BOOST, 1) |
        FV(DPTX_PRE_DRIVER_COMPENSATION_REG_TXB4_BOOST, 1);
    SR(DPTX_CKDELAY) = FV(DPTX_CKDELAY_REG_TXA0_CKSYN_DLY, 1) | FV(DPTX_CKDELAY_REG_TXA1_CKSYN_DLY, 1) |
        FV(DPTX_CKDELAY_REG_TXB3_CKSYN_DLY, 1) | FV(DPTX_CKDELAY_REG_TXB4_CKSYN_DLY, 1);
    SR(DPTX_DOUBLE_HALF_CURRENT) = DOUBLE_HALF_CURRENT;
    SR(DPTX_SWING_CURRENT_1) = FV(DPTX_SWING_CURRENT_1_REG_TXA0_ISEL_COMBO, 10) |
        FV(DPTX_SWING_CURRENT_1_REG_TXA1_ISEL_COMBO, 10);
    SR(DPTX_SWING_CURRENT_2) = SWING_CURRENT_B;
}

u8 edp_read_caps(void)
{
    return edp_aux_read(0, DPCD_REV, 16, edp_stat.caps);
}

u8 edp_read_link_status(void)
{
    return edp_aux_read(0, DPCD_LANE0_1_STATUS, 6, edp_stat.lane);
}

static u8 lanes_cr_done(void)
{
    return (edp_stat.lane[0] & 0x11) == 0x11;
}

static u8 lanes_eq_done(void)
{
    return (edp_stat.lane[0] & 0x77) == 0x77 && (edp_stat.lane[2] & 0x01);
}

/* Echo the sink's swing/pre-emphasis request into TRAINING_LANEx_SET. */
static u8 write_lane_settings(u8 lane_set)
{
    u8 set[2];
    set[0] = set[1] = lane_set;
    return edp_aux_write(0, DPCD_TRAINING_LANE0_SET, 2, set);
}

static u8 lane_set_from_request(void)
{
    u8 req = edp_stat.lane[4];             /* DPCD 0x206: lane0 [1:0] swing, [3:2] pre */
    return (req & 0x03) | (((req >> 2) & 0x03) << 3);
}

/* Full link bring-up on TX1: wake + caps, config, TPS1/CR, TPS2/EQ, finish.
 * Caller has run edp_tx_init(), powered the panel and edp_phy0_on(). */
__xdata u16 pll_wait;           /* polls of the CMU lock at the last training (debug) */

u8 edp_link_train(void)
{
    u8 v = 0, i, lane_set = LANE_SET_INITIAL, tps[5];

    edp_stat.cr_tries = edp_stat.eq_tries = 0;
    edp_stat.aux_err = 0;
    for (i = 0; i < 6; i++)
        edp_stat.lane[i] = 0;
    /* Wait for the PHY0 CMU to lock - polled, not sampled once after the caller's
     * fixed settle: that settle is an uncalibrated busy loop whose real length
     * depends on clock state and code layout, and after a soft reset (end of
     * every flash) it can be shorter than the lock time. */
    for (pll_wait = 0; pll_wait < 6000; pll_wait++) {
        edp_stat.pll_stat = SR(DPTX_CMU4);
        if (edp_stat.pll_stat & DPTX_CMU4_PLL_WD_CKRDY)
            break;
        delay_ms(1);
    }
    if (!(edp_stat.pll_stat & DPTX_CMU4_PLL_WD_CKRDY))
        return EDP_LT_ERR_PLL;

    /* --- wake the sink, read caps ----------------------------------------- */
    dpcd_write1(DPCD_SINK_CONTROL, 0x01);
    delay_ms(10);
    edp_aux_read(0, DPCD_SINK_CONTROL, 1, &v);
    if (edp_read_caps())
        return EDP_LT_ERR_AUX_CAPS;
    if (edp_stat.caps[0] == 0x00 || edp_stat.caps[0] == 0xFF)
        return EDP_LT_ERR_SILENT;

    /* --- link configuration ---------------------------------------------- */
    if (dpcd_write1(DPCD_LINK_BW_SET, LINK_RATE))
        return EDP_LT_ERR_AUX_CFG;
    dpcd_write1(DPCD_LINK_COUNT_SET, LANE_COUNT);
    dpcd_write1(DPCD_EDP_CON_SET, 0x02);       /* framing change */
    SR(DPTX_PN_SWAP1) = PN_SWAP_ALL; SR(DPTX_LANE_SWAP3_2) = LANE_SWAP_32; SR(DPTX_LANE_SWAP1_0) = LANE_SWAP_10;
    SR(DPTX_SFIFO_LANE_SWAP1) = DPTX_SFIFO_LANE_SWAP1_LANE0_SOURCE_LANE0 | DPTX_SFIFO_LANE_SWAP1_LANE1_SOURCE_LANE1 |
                                DPTX_SFIFO_LANE_SWAP1_LANE2_SOURCE_LANE2 | DPTX_SFIFO_LANE_SWAP1_LANE3_SOURCE_LANE3;
    SR_WRF(PHY0_TXBIST_00_L1, PHY0_TXBIST_00_L1_REG_TXBIST_MODE, FV(PHY0_TXBIST_00_L1_REG_TXBIST_MODE, 1));
    SR_SET(PHY0_TXBIST_01_H2, PHY0_TXBIST_01_H2_REG_SCRAMBLE_DIS);
    SR_CLR(PHY0_TXBIST_01_H2, PHY0_TXBIST_01_H2_REG_ENHANCE_MODE);
    phy0_drive_golden();
    SR_WRF(DP_MAC_CTRL, (DP_MAC_CTRL_ENHANCE_MD | DP_MAC_CTRL_LANE_NUM), DP_MAC_CTRL_ENHANCE_MD | EDP_LANE_NUM);
    SR_WRF(DP_PHY_CTRL, DP_PHY_CTRL_V2ANALOG, FV(DP_PHY_CTRL_V2ANALOG, 3));    /* lanes 0,1 valid */
    SR_PULSE_LO(DPTX_SFIFO_CTRL0, DPTX_SFIFO_CTRL0_TX_EN);        /* tx_en pulse */
    dpcd_write1(DPCD_DOWNSPREAD_CTRL, 0x10);                   /* SSC */
    SR_WRF(DPTX_CMU5, (DPTX_CMU5_EN_SSC | DPTX_CMU5_ORDER | DPTX_CMU5_SEL_MODE), DPTX_CMU5_EN_SSC);

    /* --- TPS1 / clock recovery ------------------------------------------- */
    SR_SET(PHY0_TXBIST_00_H2, TXBIST_TRAINING);
    SR_SET(PHY0_TXBIST_00_L1, PHY0_TXBIST_00_L1_REG_TP_TYPE_SEL);
    tps[0] = 0x21; tps[1] = tps[2] = lane_set; tps[3] = tps[4] = 0;
    if (edp_aux_write(0, DPCD_TRAINING_PATTERN_SET, 5, tps))
        return EDP_LT_ERR_AUX_CFG;
    for (i = 0; i < 8; i++) {
        edp_stat.cr_tries++;
        delay_us(750);
        if (edp_read_link_status())
            return EDP_LT_ERR_CR;
        if (lanes_cr_done())
            break;
        lane_set = lane_set_from_request();
        write_lane_settings(lane_set);
    }
    if (!lanes_cr_done())
        return EDP_LT_ERR_CR;

    /* --- TPS2 / channel equalization ------------------------------------- */
    SR_SET(PHY0_TXBIST_00_H2, TXBIST_TRAINING);
    SR_SET(PHY0_TXBIST_01_H2, PHY0_TXBIST_01_H2_REG_SCRAMBLE_DIS);
    SR_CLR(PHY0_TXBIST_00_L1, PHY0_TXBIST_00_L1_REG_TP_TYPE_SEL);
    tps[0] = 0x22; tps[1] = tps[2] = lane_set;
    if (edp_aux_write(0, DPCD_TRAINING_PATTERN_SET, 5, tps))
        return EDP_LT_ERR_AUX_CFG;
    for (i = 0; i < 8; i++) {
        edp_stat.eq_tries++;
        delay_us(750);
        if (edp_read_link_status())
            return EDP_LT_ERR_EQ;
        if (lanes_eq_done())
            break;
        if (!lanes_cr_done())
            return EDP_LT_ERR_EQ;
        lane_set = lane_set_from_request();
        write_lane_settings(lane_set);
    }
    if (!lanes_eq_done())
        return EDP_LT_ERR_EQ;

    /* --- training off, ASSR, scrambling on ------------------------------- */
    dpcd_write1(DPCD_TRAINING_PATTERN_SET, 0x00);
    SR_CLR(PHY0_TXBIST_00_L1, PHY0_TXBIST_00_L1_REG_SCRAMBLE_DEFAULT);
    dpcd_write1(DPCD_EDP_CON_SET, 0x03);       /* ASSR + framing change */
    SR_CLR(PHY0_TXBIST_01_H2, PHY0_TXBIST_01_H2_REG_SCRAMBLE_DIS);
    SR_SET(DPTX_PHY_CTRL, DPTX_PHY_CTRL_SR_INSERT_EN);
    SR_CLR(PHY0_TXBIST_00_L1, PHY0_TXBIST_00_L1_REG_TXBIST_MODE);
    SR_CLR(PHY0_TXBIST_00_H2, PHY0_TXBIST_00_H2_REG_BIST_REPEAT);
    delay_ms(8);
    edp_read_link_status();
    return EDP_LT_OK;
}

/* ---------------------------------------------------------------------- */
/* Video stream                                                           */
/* ---------------------------------------------------------------------- */

/* Video data per line per lane, in bytes: active pixels x bpp / (8 x lanes) */
#define V_DATA_PER_LINE ((u16)((u32)PANEL_WIDTH * 3 * PANEL_BPC / (8 * PANEL_EDP_LANES)))
/* Transfer unit: 32 symbols; the valid data per TU in 1/8 symbol, rounded up -
 * 32 x pixel clock x bpp / (8 x lanes x link symbol clock) */
#define TU_SIZE_SYMBOLS 32
#define TU_DATA_X8 ((u8)(((u32)TU_SIZE_SYMBOLS * 8 * PANEL_PCLK_KHZ * 3 * PANEL_BPC \
                         + 8UL * PANEL_EDP_LANES * LINK_SYM_KHZ - 1) / (8UL * PANEL_EDP_LANES * LINK_SYM_KHZ)))

/* page 9C stream statics A0-D9:
 * the MSA timing is the panel's (board.h), the TU / data-per-line fields follow from it.
 * edp_video_on() skips the reset / control ones and writes them itself. */
#define OFF(reg) [(reg) - DP_MAC_CTRL]
static const __code u8 p9c_stream[DPTX_TOP_CTL - DP_MAC_CTRL + 1] = {
    OFF(DP_MAC_CTRL) = DP_MAC_CTRL_ENHANCE_MD | EDP_LANE_NUM,
    OFF(DP_DEBUG_CTRL) = FV(DP_DEBUG_CTRL_DPTX_DEBUG_SEL, 15),
    /* M / N: N fixed, M default placeholder (~150.6 MHz) until edp_mvid_update() measures it */
    OFF(MN_M_VID_H) = BYTE2(MVID_DEFAULT), OFF(MN_M_VID_M) = HI(MVID_DEFAULT), OFF(MN_M_VID_L) = LO(MVID_DEFAULT),
    OFF(MN_N_VID_H) = BYTE2(EDP_NVID), OFF(MN_N_VID_M) = HI(EDP_NVID), OFF(MN_N_VID_L) = LO(EDP_NVID),
    OFF(NVID_ASYNC_M) = HI(EDP_NVID), OFF(NVID_ASYNC_L) = LO(EDP_NVID),
    OFF(MSA_MISC0) = MSA_COLORBIT | MSA_MISC0_COMPONENT_FORMAT_RGB,
    /* MSA timing; sync polarity bits (HSP / VSP) 0 = positive */
    OFF(MN_STRM_ATTR_HTT_M) = HI(PANEL_HTOTAL), OFF(MN_STRM_ATTR_HTT_L) = LO(PANEL_HTOTAL),
    OFF(MN_STRM_ATTR_HST_M) = HI(PANEL_HSTART), OFF(MN_STRM_ATTR_HST_L) = LO(PANEL_HSTART),
    OFF(MN_STRM_ATTR_HWD_M) = HI(PANEL_WIDTH), OFF(MN_STRM_ATTR_HWD_L) = LO(PANEL_WIDTH),
    OFF(MN_STRM_ATTR_HSW_M) = HI(PANEL_HSYNC), OFF(MN_STRM_ATTR_HSW_L) = LO(PANEL_HSYNC),
    OFF(MN_STRM_ATTR_VTTE_M) = HI(PANEL_VTOTAL), OFF(MN_STRM_ATTR_VTTE_L) = LO(PANEL_VTOTAL),
    OFF(MN_STRM_ATTR_VST_M) = HI(PANEL_VSTART), OFF(MN_STRM_ATTR_VST_L) = LO(PANEL_VSTART),
    OFF(MN_STRM_ATTR_VHT_M) = HI(PANEL_HEIGHT), OFF(MN_STRM_ATTR_VHT_L) = LO(PANEL_HEIGHT),
    OFF(MN_STRM_ATTR_VSW_M) = HI(PANEL_VSYNC), OFF(MN_STRM_ATTR_VSW_L) = LO(PANEL_VSYNC),
    OFF(VBID) = VBID_NOVIDEO_FLAG | VBID_AUDIOMUTE_FLAG,
    OFF(ARBITER_CTRL) = ARBITER_CTRL_ARBITER_EN | ARBITER_CTRL_VACTIVE_MD,
    OFF(V_DATA_PER_LINE0) = HI(V_DATA_PER_LINE), OFF(V_DATA_PER_LINE1) = LO(V_DATA_PER_LINE),
    OFF(TU_SIZE) = TU_SIZE_SYMBOLS, OFF(TU_DATA_SIZE0) = TU_DATA_X8 >> 3, OFF(TU_DATA_SIZE1) = TU_DATA_X8 & 7,
    OFF(HDELAY0) = HI(HDELAY), OFF(HDELAY1) = LO(HDELAY),
    OFF(AUTO_HDELAY0) = 0x03, OFF(AUTO_HDELAY1) = 0x8A,
    OFF(ARBITER_SEC_END_CNT_HB) = HI(SEC_END_CNT), OFF(ARBITER_SEC_END_CNT_LB) = LO(SEC_END_CNT),
    OFF(ARBITER_DEBUG) = ARBITER_DEBUG_DEN_LINE_END_OCCUR | ARBITER_DEBUG_V_ACTIVE_OCCUR
                         | ARBITER_DEBUG_SEC_TX_END_NV_MISS | ARBITER_DEBUG_VSYNC_DE,
    OFF(DPTX_CTSFIFO_CTRL) = DPTX_CTSFIFO_CTRL_DPTX_FORCE_CTSFIFO_RSTN,   /* CTS FIFO on */
};
#undef OFF       /* D6 arbiter debug, D7 CTS FIFO, D8 rsv, D9 TOP_CTL */

/* Measure the pixel clock against the link clock (MVID auto), latch M/N, and
 * resend the MSA.  The sink regenerates the pixel clock from M/N, and M is
 * measured once and then held FIXED - so any change to DCLK after this must be
 * followed by another call, or the panel's recovered clock is off by the same
 * amount (display_dclk_set changes DCLK by up to ~5% per input mode).
 * Blocks ~170 ms nominal. */
void edp_mvid_update(void)
{
    SR_SET(MN_VID_AUTO_EN_1, MN_VID_AUTO_EN_1_MN_VID_AUTO_EN); delay_ms(50);
    SR_CLR(MN_VID_AUTO_EN_1, MN_VID_AUTO_EN_1_MN_VID_AUTO_EN);   /* MVID auto on/off */
    SR_SET(MN_VID_AUTO_EN_1, MN_VID_AUTO_EN_1_MN_VID_AUTO_EN); delay_ms(120);
    edp_stat.mvid[0] = SR(MVID_AUTO_H); edp_stat.mvid[1] = SR(MVID_AUTO_M); edp_stat.mvid[2] = SR(MVID_AUTO_L);
    SR_CLR(MN_VID_AUTO_EN_1, MN_VID_AUTO_EN_1_MN_VID_AUTO_EN);
    SR(MN_M_VID_H) = edp_stat.mvid[0]; SR(MN_M_VID_M) = edp_stat.mvid[1]; SR(MN_M_VID_L) = edp_stat.mvid[2];
    SR(MN_N_VID_H) = BYTE2(EDP_NVID); SR(MN_N_VID_M) = HI(EDP_NVID); SR(MN_N_VID_L) = LO(EDP_NVID);
    SR_CLR(MSA_CTRL, MSA_CTRL_MSA_DB_VBLANK);
    SR_WRF(MN_VID_AUTO_EN_1, (MN_VID_AUTO_EN_1_MN_VID_AUTO_EN | MN_VID_AUTO_EN_1_MN_VID_DB),
           MN_VID_AUTO_EN_1_MN_VID_DB);                   /* latch M/N */
    SR(DPTX_TOP_RSV6_TX1) &= 0xFE;                        /* bit 0, stream-on; idempotent */
    SR_PULSE_HI(DP_RESET_CTRL, DP_RESET_CTRL_MSA_RESET);  /* MSA reset pulse */
    SR_WRF(MSA_CTRL, (MSA_CTRL_MSA_DB | MSA_CTRL_MSA_DB_VBLANK), MSA_CTRL_MSA_DB);
}

/* Turn the main-link video stream on: MSA statics (panel timing),
 * N = 0x8000, M = MVID auto measurement (pixel clock vs link clock),
 * reset pulses, TU sizes, then pattern select = video (switch at vsync).
 * Caller: link trained (edp_link_train == OK) and display_freerun_init(). */
void edp_video_on(void)
{
    u8 i;

    SR(DPTX_TOP_CTL) &= 0xF3;
    SR_PULSE_LO(DPTX_CTSFIFO_CTRL, DPTX_CTSFIFO_CTRL_DPTX_FORCE_CTSFIFO_RSTN);  /* CTSFIFO pulse */
    for (i = 0; i < sizeof(p9c_stream); i++) {
        u16 a = DP_MAC_CTRL + i;
        if (a == DP_RESET_CTRL || a == PG_FIFO_CTRL || a == MN_VID_AUTO_EN_1 ||
            a == MSA_CTRL || a == LFIFO_WL_SET)
            continue;                                     /* pulse/control regs below */
        SR(a) = p9c_stream[i];
    }
    edp_mvid_update();
    SR_CLR(LFIFO_WL_SET, LFIFO_WL_SET_WL_MID_REGEN);      /* LFIFO */
    SR_PULSE_HI(DP_RESET_CTRL, DP_RESET_CTRL_PG_RESET);   /* PG reset pulse */
    SR(TU_SIZE) = TU_SIZE_SYMBOLS; SR(TU_DATA_SIZE0) = TU_DATA_X8 >> 3; SR(TU_DATA_SIZE1) = TU_DATA_X8 & 7;
    SR(V_DATA_PER_LINE0) = HI(V_DATA_PER_LINE); SR(V_DATA_PER_LINE1) = LO(V_DATA_PER_LINE);
    SR_SET(LFIFO_WL_SET, LFIFO_WL_SET_WL_MID_REGEN);
    SR(ARBITER_SEC_END_CNT_HB) = HI(SEC_END_CNT); SR(ARBITER_SEC_END_CNT_LB) = LO(SEC_END_CNT);
    SR(PG_FIFO_CTRL) = PG_FIFO_FLAGS | 0x01;              /* w1c all FIFO flags */
    SR_WRF(DPTX_ML_PAT_SEL, DPTX_ML_PAT_SEL_TX_ML_PAT_SEL, DPTX_ML_PAT_SEL_TX_ML_PAT_SEL_VIDEO);
    SR_WRF(DPTX_ML_PAT_SEL, (DPTX_ML_PAT_SEL_TX_ML_PAT_SEL_EN | DPTX_ML_PAT_SEL_PAT_SEL_DBUF),
           DPTX_ML_PAT_SEL_TX_ML_PAT_SEL_EN);             /* switch at vsync */
}
