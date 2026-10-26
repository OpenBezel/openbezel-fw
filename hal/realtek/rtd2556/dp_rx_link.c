/* DisplayPort receiver, port D1: everything outside the INT0 handler - port
 * bring-up, DPCD, EDID, the post-training retune, the stream (MSA, stream PLL,
 * FIFO) and audio.  Banked; the handler and the state it
 * shares with this file are in dp_rx.c / dp_rx_int.h. */
#include "dp_rx.h"
#include "delay.h"
#include "rtd2556_regs.h"
#include "timebase.h"
#include "hdcp.h"
#include "dp_rx_int.h"


static void run(const __code struct reg_op *op, u8 n)
{
    for (; n; n--, op++) {
        if (op->mask)
            SR(op->addr) = (SR(op->addr) & op->mask) | op->val;
        else
            SR(op->addr) = op->val;
    }
}
#define RUN(t) run(t, sizeof(t) / sizeof(t[0]))
/* ROP(addr, field mask, value): reg = (reg & ~mask) | value; a mask of 0xFF is a
 * plain write */
#define ROP(addr, mask, val) { addr, (u8)~(mask), val }

/* Field groups this file writes together */
#define DPCD_CTRL_RESETS (D1_DPCD_CTRL_AUX_RESET | D1_DPCD_CTRL_RX_MSG_RESET | D1_DPCD_CTRL_DPCD_RESET)
#define FIFO_RST_W1C (D1_AUX_FIFO_RST_IIC_SLAVE_ADD_INT | D1_AUX_FIFO_RST_WRHDCP_INT | D1_AUX_FIFO_RST_AUX_FIFO_RST)
#define RESERVE5_W1C (D1_AUX_RESERVE5_WR_AKSV_FLAG | D1_AUX_RESERVE5_WR_TYPE_FLAG | D1_AUX_RESERVE5_WR_SKE_SEND_EKS_FLAG)
#define ADDR_IRQ_W1C (D1_PBA_DUMMY_1_DPCD_ADDR0_IRQ | D1_PBA_DUMMY_1_DPCD_ADDR1_IRQ | D1_PBA_DUMMY_1_DPCD_ADDR2_IRQ \
                      | D1_PBA_DUMMY_1_DPCD_ADDR3_IRQ)
#define OUT_RGB (D1_DP_OUTPUT_CTRL_ROUT_EN | D1_DP_OUTPUT_CTRL_GOUT_EN | D1_DP_OUTPUT_CTRL_BOUT_EN)
#define RST_N_DIG (D1_TMDS_RESET_REG_RST_N_L0 | D1_TMDS_RESET_REG_RST_N_L1 | D1_TMDS_RESET_REG_RST_N_L2 \
                   | D1_TMDS_RESET_REG_RST_N_L3)     /* digital out of reset, CDRs not */
#define ADAPT_RSTB_ALL (D1_RESET_1_DFE_ADAPT_RSTB_LANE0 | D1_RESET_1_DFE_ADAPT_RSTB_LANE1 \
                        | D1_RESET_1_DFE_ADAPT_RSTB_LANE2 | D1_RESET_1_DFE_ADAPT_RSTB_LANE3)
#define SIG_DET_LN0 (D1_DP_SIG_DET_0_LN_CK_SEL | FV(D1_DP_SIG_DET_0_DP_XTAL_CYCLE, 3))   /* + SIG_DET_1/2 = 0: lane 0 */
#define AUD_FSM_CFG (D1_AUD_FSM_CTRL_0_AUD_WAIT_EMPTY_EN | D1_AUD_FSM_CTRL_0_AUD_FAST_MUTE_EN \
                     | D1_AUD_FSM_CTRL_0_LYO_CHG_MUTE_EN)
#define AUD_FSM_ON (D1_AUD_FSM_CTRL_0_AUD_EN | AUD_FSM_CFG)
#define ABUF_W1C (D1_AUD_BUFFER_CTRL_0_ABUF_EMPTY | D1_AUD_BUFFER_CTRL_0_ABUF_FULL | D1_AUD_BUFFER_CTRL_0_ABUF_UDFL \
                  | D1_AUD_BUFFER_CTRL_0_ABUF_OVFL)
#define PG1_W1C (D1_PG_CTRL_1_BE_OCR | D1_PG_CTRL_1_VFIFO_EMPTY | D1_PG_CTRL_1_VFIFO_UDFL | D1_PG_CTRL_1_VFIFO_FULL \
                 | D1_PG_CTRL_1_VFIFO_OVFL | D1_PG_CTRL_1_VFIFO_OVFL_PRI)
#define MSA_POPUP (D1_MN_STRM_ATTR_CTRL_STRM_ATTR_POPUP | D1_MN_STRM_ATTR_CTRL_MN_VIDEO_POPUP \
                   | D1_MN_STRM_ATTR_CTRL_MN_AUDIO_POPUP | D1_MN_STRM_ATTR_CTRL_BE_HIT_VBID0 \
                   | D1_MN_STRM_ATTR_CTRL_AUTO_CLR_MN_EN | D1_MN_STRM_ATTR_CTRL_STRM_ATTR_ERRC_EN)
#define PG_RGB8 (D1_PG_CTRL_0_COMPONENT_FORMAT_RGB | FV(D1_PG_CTRL_0_COLORBIT, 1))       /* RGB, 8 bpc */

/* ---- DPCD access port (BAC0 control, BAC1-3 address, BAC4 data) ---------- */
static void dpcd_addr(u8 hi, u16 addr)
{
    SR_CLR(D1_DPCD_CTRL, D1_DPCD_CTRL_DPCD_ACCESS_PORT_AUTO_INC);
    SR(D1_DPCD_ADDR_PORT_H) = hi;
    SR(D1_DPCD_ADDR_PORT_M) = HI(addr);
    SR(D1_DPCD_ADDR_PORT_L) = LO(addr);
}

static void dpcd_w(u8 hi, u16 addr, u8 v)
{
    dpcd_addr(hi, addr);
    SR(D1_DPCD_DATA_PORT) = v;
}

u8 dp_dpcd_read(u8 hi, u16 addr) __banked
{
    dpcd_addr(hi, addr);
    return SR(D1_DPCD_DATA_PORT);
}

static void dpcd_fill(u16 addr, u8 n, u8 v)
{
    for (; n; n--, addr++)
        dpcd_w(0, addr, v);
}

/* ---- AUX PHY ---------------------------------------------------------------- */
static const __code struct reg_op aux_phy[] = {
    ROP(D1_DPCD_CTRL, DPCD_CTRL_RESETS | D1_DPCD_CTRL_AUX_DATA_FIFO_SEL | D1_DPCD_CTRL_DPCD_ACCESS_PORT_AUTO_INC, 0),
    ROP(D1_AUX_DIG_PHY2, 0xFF, FV(D1_AUX_DIG_PHY2_AVE_POS, 24)),
    ROP(D1_AUX_DIG_PHY3, D1_AUX_DIG_PHY3_AUX_CLKSEL_AUTO_EN | D1_AUX_DIG_PHY3_AUX_NEW_MODE, D1_AUX_DIG_PHY3_AUX_NEW_MODE),
    ROP(D1_AUX_PHY_DIG2, D1_AUX_PHY_DIG2_FAKE_LT_EN, 0),
    ROP(D1_AUX_DIG_PHY4, D1_AUX_DIG_PHY4_PCHARGE_NUM, FV(D1_AUX_DIG_PHY4_PCHARGE_NUM, 9)),
    ROP(D1_AUX_DIG_PHY5, D1_AUX_DIG_PHY5_AVE_MAN_EN | D1_AUX_DIG_PHY5_AVE_NUM, FV(D1_AUX_DIG_PHY5_AVE_NUM, 13)),
    ROP(D1_AUX_MODE_SET, D1_AUX_MODE_SET_ERROR_RST_FSM_EN | D1_AUX_MODE_SET_DP_LINK_TRANS_EN | D1_AUX_MODE_SET_DP_AUX_EN,
        D1_AUX_MODE_SET_DP_AUX_EN),                         /* auto reply still off */
    ROP(D1_DP_IIC_SET, 0xFF, D1_DP_IIC_SET_DP_IICRD_NEW),   /* I2C-over-AUX by hardware (MCU mapping off) */
    ROP(D1_AUX_FIFO_RST, FIFO_RST_W1C | D1_AUX_FIFO_RST_SCL_RATE_CHG, D1_AUX_FIFO_RST_SCL_RATE_CHG),
    ROP(D1_AUX_STATUS, D1_AUX_STATUS_AUXNACKM | D1_AUX_STATUS_AUXNOREPLY, D1_AUX_STATUS_AUXNOREPLY),
    ROP(D1_AUX_DIG_PHY6, D1_AUX_DIG_PHY6_AUX_SDM_EN | D1_AUX_DIG_PHY6_SYNC_CNT_MODE, D1_AUX_DIG_PHY6_SYNC_CNT_MODE),
    ROP(D1_AUX_TX_TIMER, D1_AUX_TX_TIMER_AUX_TIMER_EN | D1_AUX_TX_TIMER_IIC_ACK_TIMER_EN,
        D1_AUX_TX_TIMER_AUX_TIMER_EN | D1_AUX_TX_TIMER_IIC_ACK_TIMER_EN),
    ROP(D1_AUX_TX_TIMER_2, D1_AUX_TX_TIMER_2_TIMEOUT_TARGET, FV(D1_AUX_TX_TIMER_2_TIMEOUT_TARGET, 13)),
    ROP(D1_MCUIIC, D1_MCUIIC_IIC_DEFER_CASE1 | D1_MCUIIC_IIC_DEFER_CASE2, 0),
    ROP(D1_AUX_DIG_PHY8, (u8)~D1_AUX_DIG_PHY8_AUX_PHY_TEST_SEL, D1_AUX_DIG_PHY8_EHANDLE_EN | D1_AUX_DIG_PHY8_SYNC1_WO_IDLE),
    ROP(D1_AUX_5, D1_AUX_5_REG_AUX_RSTB, 0),                /* pulse */
    ROP(D1_AUX_5, D1_AUX_5_REG_AUX_RSTB, D1_AUX_5_REG_AUX_RSTB),
    /* AUX Z0 (BA61 B3, BA62 09, BA66 13) */
    ROP(D1_AUX_1, 0xFF, D1_AUX_1_REG_AUX_VLDO_1_05V | D1_AUX_1_REG_AUX_SELVCOM | FV(D1_AUX_1_REG_AUX_RX_ADJR_P, 3)),
    ROP(D1_AUX_2, 0xFF, D1_AUX_2_REG_AUX_SEL_THRESHOLD_50MV | D1_AUX_2_REG_EN_AUX_RX_BIGZ0_P),
    ROP(D1_AUX_6, D1_AUX_6_REG_EN_AUX_RX_BIGZ0_N | D1_AUX_6_REG_AUX_RX_ADJR_N,
        D1_AUX_6_REG_EN_AUX_RX_BIGZ0_N | FV(D1_AUX_6_REG_AUX_RX_ADJR_N, 3)),
    ROP(D1_DIG_TX_03, 0xFF, FV(D1_DIG_TX_03_REG_AUX_TX_ADJR_P, 15) | FV(D1_DIG_TX_03_REG_AUX_TX_ADJR_N, 15)),
    ROP(D1_DIG_TX_04, D1_DIG_TX_04_REG_AUX_SEWINS, D1_DIG_TX_04_REG_AUX_SEWINS_50MV),
    ROP(D1_AUX_4, D1_AUX_4_REG_AUX_REV, D1_AUX_4_REG_AUX_REV_400UA),
    ROP(D1_AUX_DIG_PHY9, D1_AUX_DIG_PHY9_SINGLE_END_MODE, D1_AUX_DIG_PHY9_SINGLE_END_MODE),
    ROP(D1_AUX_DIG_PHY3, D1_AUX_DIG_PHY3_AUX_PHY_REF_CLK_DIV, 0),       /* ref clock dividers 0 ... */
    ROP(D1_AUX_PAYLOAD_CLEAR, D1_AUX_PAYLOAD_CLEAR_AUX_MAC_REF_CLK_DIV, 0),
    ROP(POWER_CTRL, POWER_CTRL_AUX_PHY_PS_CLK_SEL, 0),
    ROP(D1_AUX_DIG_PHY3, D1_AUX_DIG_PHY3_AUX_PHY_REF_CLK_DIV, FV(D1_AUX_DIG_PHY3_AUX_PHY_REF_CLK_DIV, 1)),   /* ... then 1 */
    ROP(D1_AUX_PAYLOAD_CLEAR, D1_AUX_PAYLOAD_CLEAR_AUX_MAC_REF_CLK_DIV, FV(D1_AUX_PAYLOAD_CLEAR_AUX_MAC_REF_CLK_DIV, 1)),
};

/* ---- DPCD capabilities: DP 1.1, HBR x4 ------------------------------------- */
struct dpcd_val {
    u16 addr;
    u8 val;
};
static const __code struct dpcd_val caps[] = {
    { DPCD_SINK_STATUS, 0x00 },
    { DPCD_REV, 0x11 },                    /* 1.1 */
    { DPCD_E_RESERVE, 0x00 },              /* TRAINING_AUX_RD_INTERVAL 0, no extended caps */
    { DPCD_MAX_LINK_RATE, 0x0A },          /* HBR */
    { DPCD_DOWNSTREAM_PORT_COUNT, 0x81 },  /* OUI support, 1 port */
    { DPCD_SINK_VENDOR_IEEE_OUI_L, 0x00 }, { DPCD_SINK_VENDOR_IEEE_OUI_M, 0xE0 }, { DPCD_SINK_VENDOR_IEEE_OUI_H, 0x4C },     /* sink OUI (Realtek's) */
    { DPCD_500, 0x00 }, { DPCD_501, 0xE0 }, { DPCD_502, 0x4C },     /* branch OUI */
    { DPCD_DOWNSTREAM_PORT, 0x00 },
    { DPCD_MSTM_CAP, 0x00 },               /* no MST */
    { DPCD_MAX_LINK_COUNT, 0x84 },         /* MAX_LANE_COUNT 4 | ENHANCED_FRAME_CAP */
    { DPCD_MAX_DOWNSPREAD, 0x01 },         /* 0.5% */
    { DPCD_NORP, 0x01 },
    { DPCD_MAIN_LINK_CH_CODING, 0x01 },    /* 8b/10b */
    { DPCD_RX_PORT0_CAP0, 0x02 },
    { DPCD_RX_PORT0_CAP1, 0x00 },
    { DPCD_RX_PORT1_CAP0, 0x06 },
    { DPCD_RX_PORT1_CAP1, 0x00 },
    { DPCD_SINK_COUNT, 0x01 },             /* 1; + CP_READY (0x41) only once the HDCP keys are loaded */
    { DPCD_DEVICE_IRQ_VECTOR, 0x00 },
    { DPCD_SINK_STATUS, 0x00 },
    { DPCD_TEST_SINK_MISC, 0x20 },
    { DPCD_SINK_EQUALIZER_COUNT, 0x00 },
    { DPCD_ELT_PRESET_COUNT, 0x00 },
    { DPCD_80, 0x08 },                     /* downstream port 0 caps */
};

/* ---- AUX interrupt setup, minus the sources we
 * do not service: HDCP writes (BADA wrhdcp_int_en), Aksv (BAA5) and the
 * 0x2200 address watch (BAFB) stay disabled - their flags would hold the
 * level-triggered INT0 with nobody to clear them.  The source then reads
 * 0x2200 / 0x210 from DPCD RAM like any other address. */
static const __code struct reg_op aux_irq[] = {
    ROP(D1_AUX_FIFO_RST, FIFO_RST_W1C, D1_AUX_FIFO_RST_WRHDCP_INT),          /* W1C */
    ROP(D1_AUX_FIFO_RST, FIFO_RST_W1C | D1_AUX_FIFO_RST_WRHDCP_INT_EN, 0),   /* WRHDCP_INT_EN off */
    ROP(D1_AUX_IRQ_STATUS, D1_AUX_IRQ_STATUS_WROT_INT_EN | D1_AUX_IRQ_STATUS_TIMEOUT_EN | D1_AUX_IRQ_STATUS_AUX_RX_EN, 0),
    ROP(D1_AUX_DIG_PHY7, D1_AUX_DIG_PHY7_AUX_PHY_INT_EN, 0),
    ROP(D1_AUX_DPCD_IRQ_EN, 0xFF, D1_AUX_DPCD_IRQ_EN_WR102_INT_EN | D1_AUX_DPCD_IRQ_EN_WR100_101_INT_EN
                                  | D1_AUX_DPCD_IRQ_EN_WR103_108_INT_EN | D1_AUX_DPCD_IRQ_EN_WR270_INT_EN
                                  | D1_AUX_DPCD_IRQ_EN_WR600_INT_EN),
    ROP(D1_AUX_RESERVE5, RESERVE5_W1C, D1_AUX_RESERVE5_WR_AKSV_FLAG),        /* W1C */
    ROP(D1_AUX_RESERVE5, RESERVE5_W1C | D1_AUX_RESERVE5_WR_AKSV_IRQ_EN, 0), /* WR_AKSV_IRQ_EN off */
    ROP(D1_AUX_IRQ_ADDR0_MSB, 0xFF, FV(D1_AUX_IRQ_ADDR0_MSB_DPCD_ADDR0_WILDCARD, 3)),     /* watch 0x02200 */
    ROP(D1_AUX_IRQ_ADDR0_MSB1, 0xFF, 0x22),
    ROP(D1_AUX_IRQ_ADDR0_LSB, 0xFF, 0x00),
    ROP(D1_PBA_DUMMY_1, ADDR_IRQ_W1C, D1_PBA_DUMMY_1_DPCD_ADDR0_IRQ),        /* W1C */
    ROP(D1_PBA_DUMMY_1, ADDR_IRQ_W1C | D1_PBA_DUMMY_1_DPCD_ADDR0_IRQ_EN, 0), /* DPCD_ADDR0_IRQ_EN off */
    ROP(D1_AUX_IRQ_ADDR3_MSB, 0xFF, FV(D1_AUX_IRQ_ADDR3_MSB_DPCD_ADDR3_WILDCARD, 1)),     /* watch 0x00210 */
    ROP(D1_AUX_IRQ_ADDR3_MSB1, 0xFF, 0x02),
    ROP(D1_AUX_IRQ_ADDR3_LSB, 0xFF, 0x10),
};

/* ---- Port power-on --------------------------------------------------------- */
/* shared PHY power-on, D1 part (the common 1D14/1D15 clock gating
 * is skipped, as our HDMI bring-up skips it) */
static const __code struct reg_op phy_on[] = {
    ROP(D1_CMU_02, D1_CMU_02_REG_EN_VTEST_HIZ, 0),
    ROP(D1_KOFFSET_02, D1_KOFFSET_02_REG_OFFCAL_RANGE, FV(D1_KOFFSET_02_REG_OFFCAL_RANGE, 3)),
    ROP(D1_WD, D1_WD_REG_PLL_WD_EN, 0),
    ROP(D1_ANA_CDR_02, D1_ANA_CDR_02_REG_ADP_TIME, FV(D1_ANA_CDR_02_REG_ADP_TIME, 2)),
    ROP(D1_KOFFSET_03, D1_KOFFSET_03_REG_HYBRID, 0),
    ROP(D1_BANDGAP_03, D1_BANDGAP_03_REG_EN_ENHANCE_BIAS | D1_BANDGAP_03_REG_PSM, D1_BANDGAP_03_REG_EN_ENHANCE_BIAS),
    ROP(D1_CMU_01, D1_CMU_01_REG_HST_BUF_SEL, 0),
};
/* AUX power saving off; PHY port switch-on */
static const __code struct reg_op port_on[] = {
    ROP(D1_AUX_2, D1_AUX_2_REG_EN_AUX_PSM, 0),
    ROP(D1_AUX_3, D1_AUX_3_REG_EN_AUX_PSM_BIAS, 0),
    ROP(D1_AUX_DIG_PHYA, D1_AUX_DIG_PHYA_AUX_RXST_INT_EN, 0),
    ROP(D1_STHD_CTRL_1, D1_STHD_CTRL_1_ST_LOAD_BY_ACT, 0),
    ROP(D1_BANDGAP_03, D1_BANDGAP_03_REG_RXVCOM_SEL, FV(D1_BANDGAP_03_REG_RXVCOM_SEL, 1)),
    ROP(D1_KOFFSET_00, D1_KOFFSET_00_REG_EN_BACK_OFFSET | D1_KOFFSET_00_REG_EN_FORE_OFFSET | D1_KOFFSET_00_REG_FORE_OFF_AUTOK,
        D1_KOFFSET_00_REG_EN_BACK_OFFSET),
    ROP(D1_IMPEDANCE_01, D1_IMPEDANCE_01_REG_Z0_MANUAL, D1_IMPEDANCE_01_REG_Z0_MANUAL),
    ROP(D1_IMPEDANCE_00, D1_IMPEDANCE_00_REG_Z0_ADJR, 8),
    ROP(D1_IMPEDANCE_01, D1_IMPEDANCE_01_REG_Z0_CM_ADJR, 6),
    ROP(D1_BANDGAP_03, D1_BANDGAP_03_REG_P_CHANNEL_EN | D1_BANDGAP_03_REG_N_CHANNEL_EN,
        D1_BANDGAP_03_REG_P_CHANNEL_EN | D1_BANDGAP_03_REG_N_CHANNEL_EN),
    ROP(D1_SDM_03, D1_SDM_03_REG_BYPASS_CLK_RDY, D1_SDM_03_REG_BYPASS_CLK_RDY),
    ROP(D1_EQEN_TIMER, D1_EQEN_TIMER_REG_BYPASS_EQEN_RDY, D1_EQEN_TIMER_REG_BYPASS_EQEN_RDY),
    ROP(D1_DATA_TIMER, D1_DATA_TIMER_REG_BYPASS_DATA_RDY, D1_DATA_TIMER_REG_BYPASS_DATA_RDY),
};
/* port power-on: MAC idle init + PHY idle in DP mode */
static const __code struct reg_op mac_idle[] = {
    ROP(D1_PHY_DIG_RESET_CTRL, D1_PHY_DIG_RESET_CTRL_PHY_DIG_RESET, 0),
    ROP(D1_DP_OUTPUT_CTRL, OUT_RGB, OUT_RGB),
    ROP(D1_MN_SCLKG_SDM_CTRL, D1_MN_SCLKG_SDM_CTRL_OCLK_EN, D1_MN_SCLKG_SDM_CTRL_OCLK_EN),
    ROP(D1_MN_STRM_ATTR_CTRL, D1_MN_STRM_ATTR_CTRL_STRM_ATTR_ERRC_EN, D1_MN_STRM_ATTR_CTRL_STRM_ATTR_ERRC_EN),
    ROP(D1_DP_VWD_CTRL, 0xFF, 0x00),
    ROP(D1_DP_IRQ_CTRL0, 0xFF, 0x00),
    ROP(D1_DP_GLB_STATUS, 0xFF, D1_DP_GLB_STATUS_VBID3_CHG),   /* W1C */
    ROP(D1_DP_IRQ_CTRL1, 0xFF, 0x00),
    ROP(D1_BANDGAP_03, D1_BANDGAP_03_REG_MHL_MODE, 0),
    ROP(D1_CMU_01, D1_CMU_01_REG_EN_MHL_OP | D1_CMU_01_REG_HST_BUF_SEL, 0),
    ROP(D1_CMU_08, D1_CMU_08_REG_LE_LOWGAIN_HIGHBOOST, 0),
    ROP(D1_CMU_08, D1_CMU_08_REG_SEL_DFE_DELAY, 0),
    ROP(D1_L0_GDI_DE, D1_L0_GDI_DE_DE_BRANK_EN_LANE0, 0),
    ROP(D1_L1_GDI_DE, D1_L1_GDI_DE_DE_BRANK_EN_LANE1, 0),
    ROP(D1_L2_GDI_DE, D1_L2_GDI_DE_DE_BRANK_EN_LANE2, 0),
    ROP(D1_L3_GDI_DE, D1_L3_GDI_DE_DE_BRANK_EN_LANE3, 0),
    ROP(D1_SAMPLE_EDGE, D1_SAMPLE_EDGE_SAMPLE, 0),
    ROP(D1_ANA_CDR_10, D1_ANA_CDR_10_REG_EN_WIDE_TEMP, D1_ANA_CDR_10_REG_EN_WIDE_TEMP),
    ROP(D1_ANA_CDR_05, D1_ANA_CDR_05_REG_CDR_KVCO | D1_ANA_CDR_05_REG_VCM_TUNE,
        FV(D1_ANA_CDR_05_REG_CDR_KVCO, 1) | FV(D1_ANA_CDR_05_REG_VCM_TUNE, 3)),
    ROP(D1_ANA_CDR_02, D1_ANA_CDR_02_REG_SEL_CONT3, D1_ANA_CDR_02_REG_SEL_CONT3),
    ROP(D1_CMU_03, D1_CMU_03_REG_ENHANCE_SA_BIAS, D1_CMU_03_REG_ENHANCE_SA_BIAS_NORMAL),
    ROP(D1_IMPEDANCE_01, D1_IMPEDANCE_01_REG_EN_SIGDET_LANE, D1_IMPEDANCE_01_REG_EN_SIGDET_LANE),
};

/* ---- EDID over AUX: hardware answers I2C-over-AUX from DDC1's EDID RAM
 * (XDATA 0xE280, 256 bytes).
 * Ours is the HDMI EDID with the video input byte saying DisplayPort. */
extern const __code u8 board_edid[256];
#define DP_EDID_RAM 0xE280u
#define EDID_INPUT 20                   /* video input definition */
#define EDID_INPUT_DP_8BPC 0xA5         /* digital, 8 bpc, DisplayPort */

static void dp_edid_load(void)
{
    u8 i, sum = 0;

    SR_SET(DDC1_EDID_IRQ, DDC1_EDID_IRQ_DDC1_FORCE_NACK);  /* "not ready" while loading */
    for (i = 0; i < 127; i++) {
        u8 b = i == EDID_INPUT ? EDID_INPUT_DP_8BPC : board_edid[i];
        SR(DP_EDID_RAM + i) = b;
        sum += b;
    }
    SR(DP_EDID_RAM + 127) = (u8)-sum;
    for (i = 128; i; i++)                                   /* extension block as is */
        SR(DP_EDID_RAM + i) = board_edid[i];
    /* DDC1 channel (control 2, enable, control 1), then ready and the
     * segment pointer on so block 1 can be read.
     * EDID only here: ISP enable + segment, no DDC/CI.  DDC1_DDCCI_EN stays
     * off: with the DDC/CI engine's channel auto-switch on, the source's
     * I2C-over-AUX traffic would pull DDC/CI over to DDC1 and off the HDMI
     * bus, the one the host uses for DDC/CI. */
    SR(DDC1_DDC_CONTROL_2) = DDC1_DDC_CONTROL_2_DDC1_DDCISP_EN | DDC1_DDC_CONTROL_2_DDC1_SEG_WR;
    SR(DDC1_DDC_ENABLE) = DDC1_DDC_ENABLE_DDC1_DBN_EN | DDC1_DDC_ENABLE_DDC1_DDC_EN;
    SR(DDC1_DDC_CONTROL_1) = FV(DDC1_DDC_CONTROL_1_DDC1_STOP_DBN_SEL, 3) | DDC1_DDC_CONTROL_1_DDC1_DDC2;
    SR_CLR(DDC1_EDID_IRQ, DDC1_EDID_IRQ_DDC1_FORCE_NACK);
    SR_SET(DDC1_DDC_CONTROL_2, DDC1_DDC_CONTROL_2_DDC1_DDCSEG_EN);
}



/* Post-training PHY re-tune, 30 ms after training ends.  Main loop,
 * with INT0 held off. */
static const __code struct reg_op post_lt_a[] = {
    ROP(D1_CMU_01, 0xFF, D1_CMU_01_REG_CMU_CKSEL),
    ROP(D1_ANA_CDR, 0xFF, D1_ANA_CDR_REG_ACDR_L0_EN | D1_ANA_CDR_REG_BYPASS_DATA_RDY_L0 | D1_ANA_CDR_REG_BYPASS_EQEN_RDY_L0),
    ROP(D1_TMDS_RESET, 0xFF, 0x00),
    ROP(D1_TMDS_RESET, 0xFF, RST_N_DIG),
    ROP(D1_LOOP_DIV_2, 0xFF, FV(D1_LOOP_DIV_2_VTH_DIVISOR, 3) | FV(D1_LOOP_DIV_2_TAP_DIVISOR, 10)),
    ROP(D1_LIMIT_10, 0xFF, FV(D1_LIMIT_10_VTH_MAX, 6) | FV(D1_LIMIT_10_VTH_MIN, 2)),
};

/* D1_Ln_DFE_EN_2 bits: [7] LE, [6] Vth, [5] servo, [4:0] tap4-0 adaptation */
#define DFE_ADAPT_LE 0x80
#define DFE_ADAPT_VTH 0x40
#define DFE_ADAPT_TAP2 0x04
#define DFE_ADAPT_TAP1 0x02
#define DFE_ADAPT_TAP0 0x01
/* KOFFSET_00 with the foreground offset calibration in auto mode */
#define KOFF_AUTOK (D1_KOFFSET_00_REG_EN_BACK_OFFSET | D1_KOFFSET_00_REG_FORE_OFF_AUTOK \
                    | FV(D1_KOFFSET_00_REG_FORE_OFF_ADJR_L0, 16))
/* ANA_CDR_01: adaptive EQ manual, FLD on the crystal / 8, CP adjust while tracking */
#define CDR01_BASE (D1_ANA_CDR_01_REG_ADP_EN_MANUAL | D1_ANA_CDR_01_REG_XTAL_EN | D1_ANA_CDR_01_REG_XTAL_SEL \
                    | D1_ANA_CDR_01_REG_CP_ADJ_EN)
#define CDR02_BASE (FV(D1_ANA_CDR_02_REG_ADP_TIME, 2) | D1_ANA_CDR_02_REG_SEL_CONT3 | D1_ANA_CDR_02_REG_CDR_DIV_CONT)
#define ANA_CDR_L0 (D1_ANA_CDR_REG_ACDR_L0_EN | D1_ANA_CDR_REG_BYPASS_DATA_RDY_L0 | D1_ANA_CDR_REG_EN_EQEN_MANUAL_L0 \
                    | D1_ANA_CDR_REG_BYPASS_EQEN_RDY_L0)

static void lanes4(u8 off, u8 v)
{
    u8 l;
    for (l = 0; l < 4; l++)
        SR(PHY_LANE(l, off)) = v;
}

static void post_lt(void)
{
    u8 b001 = D1_PHY_DIG_RESET_CTRL_XCLK_POWON_EN
              | (lt_lanes == 1 ? D1_PHY_DIG_RESET_CTRL_LANE_NUM_1_LANE
                 : lt_lanes == 2 ? D1_PHY_DIG_RESET_CTRL_LANE_NUM_2_LANE : D1_PHY_DIG_RESET_CTRL_LANE_NUM_4_LANE);

    b001 |= lt_enh;
    SR_CLR(D1_AUX_MODE_SET, D1_AUX_MODE_SET_DP_AUX_AUTO_EN);
    RUN(post_lt_a);
    SR(D1_SCRAMBLE_CTRL) = (SR(D1_SCRAMBLE_CTRL) & ~D1_SCRAMBLE_CTRL_SCRAMB_DIS) |
                           (dp_dpcd_read(0, DPCD_TRAINING_PATTERN_SET) & DPCD_TRAINING_PATTERN_SET_SCRAMBLING_DISABLE);
    /* LE minimum 20 (its bit 4 is in INIT_1), LE initial 16, servo initial 16 */
    lanes4(LN(D1_L0_LIMIT_INIT), FV(D1_L0_LIMIT_INIT_LE_MIN_LANE0, 4));
    lanes4(LN(D1_L0_INIT_1), D1_L0_INIT_1_LE_MIN_LANE0 | D1_L0_INIT_1_LE_INIT_LANE0 | FV(D1_L0_INIT_1_SERVO_INIT_LANE0, 16));
    SR(D1_RESET_1) = FV(D1_RESET_1_TAP0_THRESHOLD, 10);
    SR(D1_RESET_1) = FV(D1_RESET_1_TAP0_THRESHOLD, 10) | ADAPT_RSTB_ALL;
    lanes4(LN(D1_L0_DFE_EN_2), 0x00);
    SR(D1_KOFFSET_00) = KOFF_AUTOK | D1_KOFFSET_00_REG_EN_FORE_OFFSET;
    SR(D1_KOFFSET_00) = KOFF_AUTOK | D1_KOFFSET_00_REG_EN_FORE_OFFSET;
    SR(D1_KOFFSET_00) = KOFF_AUTOK;
    SR(D1_KOFFSET_00) = KOFF_AUTOK | D1_KOFFSET_00_REG_EN_FORE_OFFSET;
    i_units(0x15);
    SR(D1_CMU_01) = D1_CMU_01_REG_CMU_EN | D1_CMU_01_REG_CMU_CKSEL;
    SR(D1_ANA_CDR_01) = CDR01_BASE | D1_ANA_CDR_01_REG_AUTO_MODE | D1_ANA_CDR_01_REG_CP_EN_MANUAL;
    SR(D1_ANA_CDR_02) = CDR02_BASE;
    SR(D1_ANA_CDR) = ANA_CDR_L0;
    lanes4(LN(D1_L0_DFE_EN_2), DFE_ADAPT_LE | DFE_ADAPT_VTH | DFE_ADAPT_TAP2 | DFE_ADAPT_TAP1 | DFE_ADAPT_TAP0);
    i_units(0x4B);
    SR(D1_ANA_CDR_02) = CDR02_BASE | D1_ANA_CDR_02_REG_RSTB_FSM;
    lanes4(LN(D1_L0_DFE_EN_2), 0x00);
    SR(D1_ANA_CDR_01) = CDR01_BASE;
    SR(D1_ANA_CDR_06) = FV(D1_ANA_CDR_06_REG_SEL_CDR_RES, 4) | FV(D1_ANA_CDR_06_REG_CDR_CP_IN, 10);
    i_short(5);
    SR(D1_ANA_CDR_01) = CDR01_BASE | D1_ANA_CDR_01_REG_CP_EN_MANUAL;
    i_short(10);
    SR(D1_ANA_CDR_06) = FV(D1_ANA_CDR_06_REG_SEL_CDR_RES, 4) | D1_ANA_CDR_06_REG_CDR_CP_IN_16_5U;
    i_short(8);
    SR(D1_LIMIT_10) = FV(D1_LIMIT_10_VTH_MAX, 6);
    lanes4(LN(D1_L0_DFE_EN_2), DFE_ADAPT_VTH | DFE_ADAPT_TAP0);
    SR(D1_ANA_CDR) = ANA_CDR_L0 | D1_ANA_CDR_REG_EN_DATA_MANUAL_L0;
    SR(D1_PHY_DIG_RESET_CTRL) = b001;
    SR(D1_DP_SIG_DET_0) = SIG_DET_LN0 | D1_DP_SIG_DET_0_DP_SIG_DET_DONE;
    SR(D1_DP_SIG_DET_1) = 0x00;
    SR(D1_DP_SIG_DET_2) = 0x00;
    SR(D1_DP_SIG_DET_0) = SIG_DET_LN0 | D1_DP_SIG_DET_0_DP_SIG_DET_DONE;
    SR(D1_DP_SIG_DET_0) = SIG_DET_LN0 | D1_DP_SIG_DET_0_DP_SIG_DET_EN | D1_DP_SIG_DET_0_DP_SIG_DET_DONE;
    {
        u8 n;
        for (n = 0; n < 31 && !(SR(D1_DP_SIG_DET_0) & D1_DP_SIG_DET_0_DP_SIG_DET_DONE); n++)
            i_short(1);
    }
    SR(D1_DP_SIG_DET_0) = SIG_DET_LN0 | D1_DP_SIG_DET_0_DP_SIG_DET_DONE;
    SR_CLR(D1_DESKEW_PHY, D1_DESKEW_PHY_DESKEW_2LCLK_EN | D1_DESKEW_PHY_INTRA_DESKEW_EN);
    SR_SET(D1_DESKEW_PHY, D1_DESKEW_PHY_INTRA_DESKEW_EN);
    SR(D1_PHY_DIG_RESET_CTRL) = b001 | D1_PHY_DIG_RESET_CTRL_PHY_DIG_RESET;
    SR(D1_PHY_DIG_RESET_CTRL) = b001;
    i_units(0x25);
    SR(D1_BIST_PATTERN_SEL) = 0x00;
    SR(D1_BIST_PATTERN_SEL) = FV(D1_BIST_PATTERN_SEL_RXBIST_METHOD, 2);
    SR_SET(D1_AUX_MODE_SET, D1_AUX_MODE_SET_DP_AUX_AUTO_EN);
}

/* D1 audio block (SEC data, audio PLL, frequency detector, FIFO, DVC, I2S
 * out), brought up with the port in this order: PLL, detector/buffer, DVC.
 * The FSM is switched on by dp_stream_start() once the stream is up. */
static const __code struct reg_op dp_audio_pll_pre[] = {
    /* audio PLL: power down, M = 1A (14.318 MHz crystal), N = 0,
     * WD bits, VCO reset bit clear; then powered up in steps by dp_audio_pll() */
    ROP(D1_PB9_MN_SCLKG_PLL_PWR, D1_PB9_MN_SCLKG_PLL_PWR_REG_DPLL_PWDN, D1_PB9_MN_SCLKG_PLL_PWR_REG_DPLL_PWDN),
    ROP(D1_PB9_MN_SCLKG_DIVM, 0xFF, 0x1A),
    ROP(D1_PB9_MN_SCLKG_PLL_PWR, D1_PB9_MN_SCLKG_PLL_PWR_REG_DPLL_O, 0),
    ROP(D1_PB9_MN_SCLKG_DIVN, 0xFF, 0x00),
    ROP(D1_PB9_MN_SCLKG_PLL_WD, D1_PB9_MN_SCLKG_PLL_WD_REG_DPLL_VCOMD, FV(D1_PB9_MN_SCLKG_PLL_WD_REG_DPLL_VCOMD, 2)),
    ROP(D1_PB9_MN_SCLKG_PLL_WD, D1_PB9_MN_SCLKG_PLL_WD_REG_DPLL_WDO | D1_PB9_MN_SCLKG_PLL_WD_REG_DPLL_RESERVE,
        D1_PB9_MN_SCLKG_PLL_WD_REG_DPLL_RESERVE),
    ROP(D1_PB9_MN_SCLKG_PLL_INSEL, D1_PB9_MN_SCLKG_PLL_INSEL_REG_DPLL_VCORSTB, 0),
};

#define I2S_ALL (D1_DP_I2S_CTRL_DP_I2S_3_SWITCH | D1_DP_I2S_CTRL_DP_I2S_2_SWITCH | D1_DP_I2S_CTRL_DP_I2S_1_SWITCH \
                 | D1_DP_I2S_CTRL_DP_I2S_0_SWITCH)
#define SPDIF_ALL (D1_SPDIF_TX_0_DP_SPDIF_3_SWITCH | D1_SPDIF_TX_0_DP_SPDIF_2_SWITCH | D1_SPDIF_TX_0_DP_SPDIF_1_SWITCH \
                   | D1_SPDIF_TX_0_DP_SPDIF_0_SWITCH)
#define FREQ_DET_CFG (FV(D1_AUD_FREQUENY_DET_0_AUD_FREQ_TIME, 1) | D1_AUD_FREQUENY_DET_0_AUTO_STOP_TRK_EN)

static const __code struct reg_op dp_audio_init[] = {
    ROP(D1_AUD_FSM_CTRL_0, 0xFF, AUD_FSM_CFG),             /* FSM off while set up */
    ROP(D1_AUD_FREQUENY_DET_0, 0xFF, FREQ_DET_CFG),
    ROP(D1_AUD_FREQUENY_TH_0, 0xFF, 0x00), ROP(D1_AUD_FREQUENY_TH_1, 0xFF, 0x00),
    ROP(D1_AUD_FREQUENY_TH_2, 0xFF, 0x28), ROP(D1_AUD_FREQUENY_TH_3, 0xFF, 0x4C),
    ROP(D1_AUD_FREQUENY_TH_4, 0xFF, 0x5C), ROP(D1_AUD_FREQUENY_TH_5, 0xFF, 0x70),
    ROP(D1_MN_RS_DEC_CTRL, 0xFF, D1_MN_RS_DEC_CTRL_RS_DEC_EN | D1_MN_RS_DEC_CTRL_RS_ERR1 | D1_MN_RS_DEC_CTRL_RS_ERR2
                                 | D1_MN_RS_DEC_CTRL_RS_ERR2_ENH_MODE | D1_MN_RS_DEC_CTRL_RS_ERR2_MODE),
    ROP(D1_AUD_FREQUENY_DET_1, 0xFF, 0xA9),
    ROP(D1_AUD_PS_CTRL_0, 0xFF, D1_AUD_PS_CTRL_0_AUD_SDM_EN | D1_AUD_PS_CTRL_0_D_CODE_AUTO_EN),
    ROP(D1_AUD_BDRY_0, 0xFF, D1_AUD_BDRY_0_TR_BDRY_EN | FV(D1_AUD_BDRY_0_ABUF_BOUNDARY, 48)),
    ROP(D1_AUD_BDRY_2, 0xFF, 0x00), ROP(D1_AUD_BDRY_3, 0xFF, 0x40),
    ROP(D1_AUD_BDRY_4, 0xFF, 0x7F), ROP(D1_AUD_BDRY_5, 0xFF, 0xFF),
    ROP(D1_AUD_TRND_0, 0xFF, D1_AUD_TRND_0_TR_TRND_EN | D1_AUD_TRND_0_DP_TRND_MODE | FV(D1_AUD_TRND_0_TRN_MAX_TIME, 7)),
    ROP(D1_AUD_TRND_7, 0xFF, 0x00), ROP(D1_AUD_TRND_8, 0xFF, 0x02),
    ROP(D1_AUD_TRND_9, 0xFF, 0x7F), ROP(D1_AUD_TRND_10, 0xFF, 0xFF),
    ROP(D1_DP_AUD_CTRL, 0xFF, D1_DP_AUD_CTRL_ABUF_EN | D1_DP_AUD_CTRL_ATYPE_SRC),
    ROP(D1_DP_AUD_ID, 0xFF, 0x00),
    ROP(D1_SCODE_0, 0xFF, 0x30),
    ROP(D1_AUD_PS_CTRL_0, 0xFF, D1_AUD_PS_CTRL_0_AUD_SDM_EN | D1_AUD_PS_CTRL_0_D_CODE_AUTO_EN),
    ROP(D1_SCODE_0, 0xFF, 0x30), ROP(D1_SCODE_1, 0xFF, 0x20), ROP(D1_SCODE_2, 0xFF, 0x20),
    ROP(D1_DCODE_0, 0xFF, 0x29), ROP(D1_DCODE_1, 0xFF, 0xE0), ROP(D1_DCODE_2, 0xFF, 0xD3),
    ROP(D1_DCODE_3, 0xFF, 0x34), ROP(D1_DCODE_4, 0xFF, 0x29), ROP(D1_DCODE_5, 0xFF, 0xE0),
    ROP(D1_DVC_CTRL, 0xFF, D1_DVC_CTRL_DPAUD_FAN_EN | FV(D1_DVC_CTRL_DVC_DOWN_DB, 3) | FV(D1_DVC_CTRL_DVC_UP_DB, 3)),
    ROP(D1_DVC_VA, 0xFF, 0x01),
    ROP(D1_DVC_WINDOW_CONTROL_0, 0xFF, 0x00), ROP(D1_DVC_WINDOW_CONTROL_1, 0xFF, 0x25),
    ROP(D1_AUD_FREQUENY_DET_0, 0xFF, D1_AUD_FREQUENY_DET_0_AFREQ_DET | D1_AUD_FREQUENY_DET_0_FREQ_CHANGE_INT | FREQ_DET_CFG),
    ROP(D1_DP_I2S_CTRL, 0xFF, I2S_ALL),
    ROP(D1_SPDIF_TX_0, 0xFF, D1_SPDIF_TX_0_CS_AUTO | D1_SPDIF_TX_0_SP_ST_EN | SPDIF_ALL),
};

__xdata u8 dp_audio_on;

/* audio PLL power-up: power-up, CMP enable, latch, validate - 2 ticks apart.
 * { keep, set } as reg_op's. */
#define PLL_STEP(mask, val) { (u8)~(mask), val }
static const __code u8 pll_steps[][2] = {
    PLL_STEP(D1_PB9_MN_SCLKG_PLL_PWR_REG_DPLL_PWDN, 0),
    PLL_STEP(D1_PB9_MN_SCLKG_PLL_PWR_REG_DPLL_CMPEN, D1_PB9_MN_SCLKG_PLL_PWR_REG_DPLL_CMPEN),
    PLL_STEP(D1_PB9_MN_SCLKG_PLL_PWR_REG_DPLL_CALLCH, D1_PB9_MN_SCLKG_PLL_PWR_REG_DPLL_CALLCH),
    PLL_STEP(D1_PB9_MN_SCLKG_PLL_PWR_REG_DPLL_CALSW, D1_PB9_MN_SCLKG_PLL_PWR_REG_DPLL_CALSW),
};

void dp_audio_start(void) __banked
{
    u8 i;

    RUN(dp_audio_pll_pre);
    for (i = 0; i < sizeof(pll_steps) / sizeof(pll_steps[0]); i++) {
        SR(D1_PB9_MN_SCLKG_PLL_PWR) = (SR(D1_PB9_MN_SCLKG_PLL_PWR) & pll_steps[i][0]) | pll_steps[i][1];
        delay_ms(3);
    }
    RUN(dp_audio_init);
    dp_audio_on = 1;
    if (dp_log.stage3 == 0x10)          /* stream already shown */
        SR(D1_AUD_FSM_CTRL_0) = AUD_FSM_ON;
}

/* The source is sending audio: VB-ID's audio-mute bit clear and the sample
 * rate detector stable (not B970's "empty" flag: it is sticky, W1C).  What audio.c's de-pop gate waits for on DP (HDMI uses
 * its sample-rate detector). */
u8 dp_audio_present(void) __banked
{
    u8 b;

    if (!dp_audio_on || (SR(D1_DP_VBID) & D1_DP_VBID_AMUTE))
        return 0;
    /* buffer over/underflow: clear the flags (W1C; bit 3 "empty" is sticky too)
     * and restart the FSM */
    b = SR(D1_AUD_BUFFER_CTRL_0);
    if (b & (D1_AUD_BUFFER_CTRL_0_ABUF_UDFL | D1_AUD_BUFFER_CTRL_0_ABUF_OVFL)
            && (SR(D1_AUD_FSM_CTRL_0) & D1_AUD_FSM_CTRL_0_AUD_EN)) {
        SR_CLR(D1_AUD_FSM_CTRL_0, D1_AUD_FSM_CTRL_0_AUD_EN);
        SR(D1_AUD_BUFFER_CTRL_0) = (b & (u8)~ABUF_W1C) | ABUF_W1C;
        SR_SET(D1_AUD_FSM_CTRL_0, D1_AUD_FSM_CTRL_0_AUD_EN);
        return 0;
    }
    return (SR(D1_AUD_DEPTH_IGAIN_1) & D1_AUD_DEPTH_IGAIN_1_AUD_FREQ_STABLE) ? 1 : 0;
}

void dp_rx_start(void) __banked
{
    u8 i;

    EX0 = 0;
    dp_hpd(0);                          /* a clean unplug first: P1.4 resets high */
    for (i = 0; i < sizeof(dp_log); i++)
        ((__xdata u8 *)&dp_log)[i] = 0;
    lt_state = LT_IDLE;
    dp_edid_load();
    RUN(aux_phy);
    SR_CLR(D1_AUX_MODE_SET, D1_AUX_MODE_SET_DP_AUX_AUTO_EN);
    for (i = 0; i < sizeof(caps) / sizeof(caps[0]); i++)
        dpcd_w(0, caps[i].addr, caps[i].val);
    dpcd_fill(0x403, 9, 0);
    dpcd_fill(0x503, 9, 0);
    dpcd_fill(0x208, 4, 0);
    /* HDCP: Bcaps HDCP_CAPABLE (not a repeater) and CP_READY in
     * SINK_COUNT once hdcp_init() has loaded the keys */
    dpcd_w(BYTE2(DPCD_HDCP_BCAPS), (u16)DPCD_HDCP_BCAPS, hdcp_ready ? 0x01 : 0x00);
    if (hdcp_ready)
        dpcd_w(0, DPCD_SINK_COUNT, 0x41);
    SR_SET(D1_AUX_MODE_SET, D1_AUX_MODE_SET_DP_AUX_AUTO_EN); /* hardware answers native AUX */
    RUN(aux_irq);
    RUN(phy_on);
    delay_ms(8);
    RUN(port_on);
    RUN(mac_idle);
    dp_audio_start();                   /* with the port */
    /* before HPD assert: HDCP reset pulse, link state cleared */
    SR_PULSE_HI(D1_PHY_DIG_RESET_CTRL, D1_PHY_DIG_RESET_CTRL_HDCP_CKT_RESET);
    dpcd_w(0, DPCD_DEVICE_IRQ_VECTOR, 0);
    SR(D1_DPCD_CTRL) = 0;               /* AUX reset released */
    SR_CLR(D1_AUX_MODE_SET, D1_AUX_MODE_SET_DP_AUX_AUTO_EN);
    dpcd_w(0, DPCD_LINK_BW_SET, 0);
    dpcd_w(0, DPCD_LINK_COUNT_SET, 0);
    SR_SET(D1_AUX_MODE_SET, D1_AUX_MODE_SET_DP_AUX_AUTO_EN);
    SR_SET(D1_AUX_IRQ_STATUS, D1_AUX_IRQ_STATUS_TIMEOUT_INT | D1_AUX_IRQ_STATUS_AUX_RX_INT);   /* W1C */
    SR(D1_AUX_DPCD_IRQ) = 0xFF;         /* W1C */
    delay_ms(750);                  /* unplug long enough to be seen */
    dp_log.started = 1;
    /* stage 2: the training handler.  INT0 is the scaler's level-triggered
     * interrupt line (TCON.IT0 = 0, IRQ_CTRL0.IRQ_EN already set by HDMI). */
    SR_SET(IRQ_CTRL0, IRQ_CTRL0_IRQ_EN);
    IT0 = 0;
    EX0 = 1;
    EA = 1;
    dp_hpd(1);
}

#ifdef CONFIG_DEBUG_VCP
void dp_irq_rearm(void) __banked
{
    dp_log.stray = 0;
    EX0 = 1;
    EA = 1;
}
#endif

void dp_rx_poll(void) __banked
{
    irq_burst = 0;
    if (!dp_log.started)
        return;
    if (lt_state != LT_TRAINED) {
        lt_trained_t0 = 0;
    } else {
        u16 now = tb_now();
        if (!lt_trained_t0) {
            lt_trained_t0 = now | 1;
        } else if ((u16)(now - lt_trained_t0) >= tb_ms(30)) {
            EX0 = 0;
            if (lt_state != LT_TRAINED) {
        lt_trained_t0 = 0;
    } else {
                post_lt();
                lt_state = LT_LINK_UP;
                dp_log.lt_state = LT_LINK_UP;
            }
            lt_trained_t0 = 0;
            EX0 = 1;
        }
    }
}

/* ======================================================================
 * Stream (stage 3), in order: MSA, DPF timing
 * generator, stream clock PLL, FIFO check.
 * ====================================================================== */

static __xdata u32 w32, fvco, offs, p_hz;
static __xdata u16 htt, hwd, vtt, vht, pclk10;
static u8 meas_armed;            /* a link-clock count is running (dp_stream_alive) */
static __xdata u16 lclk_ref;     /* lane 0 link-clock count at stream setup */

/* w32 *= c, w32 /= c: one accumulator */
static void w_mul(u32 c) { w32 *= c; }
static void w_div(u32 c) { w32 /= c; }

static u16 rd16(u16 a)
{
    return ((u16)SR(a) << 8) | SR(a + 1);
}

static void wr16(u16 a, u16 v)
{
    SR(a) = HI(v);
    SR(a + 1) = LO(v);
}

/* v-BS-to-BS frame time in 27 MHz cycles -> dp_log.frame27 (0 = none) */
static void bs2bs(void)
{
    u8 n;
    dp_log.frame27 = 0;
    SR(D1_MN_MEAS_CTRL) = D1_MN_MEAS_CTRL_MEAS_DONE;       /* W1C */
    SR(D1_MN_MEAS_CTRL) = D1_MN_MEAS_CTRL_MEAS_EN;
    for (n = 0; n < 150 && !(SR(D1_MN_MEAS_CTRL) & D1_MN_MEAS_CTRL_MEAS_DONE); n++)
        delay_ms(1);
    if (n == 150)
        return;
    SR(D1_MN_MEAS_CTRL) = D1_MN_MEAS_CTRL_MEAS_EN | D1_MN_MEAS_CTRL_MEAS_POPUP;
    ((__xdata u8 *)&w32)[3] = 0;                            /* little-endian */
    ((__xdata u8 *)&w32)[2] = SR(D1_MN_MEAS_CTRL) & D1_MN_MEAS_CTRL_V_BS2BS_LENGTH;
    ((__xdata u8 *)&w32)[1] = SR(D1_MN_MEAS_VLN_M);
    ((__xdata u8 *)&w32)[0] = SR(D1_MN_MEAS_VLN_L);
    dp_log.frame27 = w32;
}

/* link rate (DPCD 100) in 0.1 MHz */
static u16 ls10(void)
{
    return lt_rate == 0x14 ? 5400 : lt_rate == RATE_RBR ? 1620 : 2700;
}

/* DPF timing: regenerated standard syncs from the MSA */
static void dpf_setup(void)
{

    SR(D1_MN_MEAS_CTRL) = D1_MN_MEAS_CTRL_MEAS_DONE | D1_MN_MEAS_CTRL_MEAS_POPUP | 0x06;
    SR(D1_DPF_CTRL_0) = 0x00;
    SR(D1_MAC_DIG_RESET_CTRL) = D1_MAC_DIG_RESET_CTRL_DPF_RESET;    /* reset pulse */
    SR(D1_MAC_DIG_RESET_CTRL) = 0x00;
    wr16(D1_MN_DPF_VHT_M, vht);
    wr16(D1_MN_DPF_HWD_M, hwd);
    wr16(D1_MN_DPF_HTT_M, htt);
    wr16(D1_MN_DPF_HSW_M, rd16(D1_MSA_HSW_0) & 0x7FFF);
    wr16(D1_MN_DPF_HST_M, htt - hwd - 8);
    SR(D1_INTERLACE_MODE_CONFIG) = D1_INTERLACE_MODE_CONFIG_FAKE_INTERLACE_DETECT;
    bs2bs();
    SR(D1_INTERLACE_MODE_CONFIG) = 0x00;    /* progressive (phones send no interlace) */
    wr16(D1_MN_DPF_VTT_M, vtt);
    wr16(D1_MN_DPF_VST_M, vtt - vht - 4);
    wr16(D1_MN_DPF_VSW_M, 3);
    /* blank-to-VS: ((4*HTT*256/pclk) * LS/256 + 8*LS/pclk + 512*LS/pclk) / 2 */
    w32 = htt;
    w_mul(1024);
    w_div(pclk10);
    w_mul(ls10());
    w32 >>= 8;
    offs = w32;
    w32 = ls10();
    w_mul(8);
    w_div(pclk10);
    offs += w32;
    w32 = ls10();
    w_mul(512);
    w_div(pclk10);
    w32 += offs;
    w32 >>= 1;
    SR(D1_OVBLK2VS_H) = BYTE2(w32); SR(D1_OVBLK2VS_M) = HI(w32); SR(D1_OVBLK2VS_L) = LO(w32);
    SR(D1_EVBLK2VS_H) = BYTE2(w32); SR(D1_EVBLK2VS_M) = HI(w32); SR(D1_EVBLK2VS_L) = LO(w32);
    SR(D1_PG_CTRL_0) = PG_RGB8;
    SR(D1_DP_RSV0) = 0x00;
    SR(D1_DPF_CTRL_0) = 0x00;
    SR(D1_DPF_CTRL_0) = D1_DPF_CTRL_0_DISP_DBUF;
    SR(D1_PG_CTRL_0) = PG_RGB8;
}

/* tracking settle: up to 10 x { PE, NLOCK flag }, one frame apart, until locked */
static void spll_settle(u8 nlock, u8 frame_ms)
{
    u8 i;
    for (i = 0; i < 10; i++) {
        SR(D1_MN_SCLKG_TRK_VS_PE) = 0x01;
        SR(D1_MN_SCLKG_TRK_VS_NLOCK) = nlock;
        delay_ms(frame_ms + frame_ms / 2);
        if (!(SR(D1_MN_SCLKG_TRK_VS_NLOCK) & D1_MN_SCLKG_TRK_VS_NLOCK_SCLKG_VS_NLOCK))
            break;
    }
}

/* stream PLL */
static __xdata u8 divs, k, m, n, frame_ms;     /* spll_setup's */
#define SCLKG_CTRL_LINK (FV(D1_MN_SCLKG_CTRL_SCLKG_PLL_IN_SEL, 1) | D1_MN_SCLKG_CTRL_SCLKG_DBUF)   /* link clock in; load */
#define SPLL_PWR_ON (D1_PB8_MN_SCLKG_PLL_PWR_REG_DPLL_CALSW | D1_PB8_MN_SCLKG_PLL_PWR_REG_DPLL_CALLCH \
                     | D1_PB8_MN_SCLKG_PLL_PWR_REG_DPLL_CMPEN | FV(D1_PB8_MN_SCLKG_PLL_PWR_REG_DPLL_O, 1))

/* Above this the "pixel clock" is a bad measurement (garbage MSA totals or a
 * short BS2BS count): 4-lane HBR2 carries at most 720 MHz at 24 bpp.  It also
 * keeps the VCO search below in range - m is a u8, and from ~1.9 GHz on it
 * would wrap and the search never end. */
#define SPLL_P_MAX 720000000UL

static u8 spll_setup(void)
{

    /* pixel clock: q = HTT*VTT*100/BS2BS, P = q * 270 kHz */
    w32 = htt;
    w_mul(vtt);
    w_mul(100);
    w_div(dp_log.frame27);
    w_mul(270000UL);
    p_hz = w32;
    w_div(100000UL);
    pclk10 = (u16)w32;
    if (p_hz < 4218750UL || p_hz > SPLL_P_MAX)
        return 3;
    /* 270 MHz / 2^n bands: K 1/2/4/8/16/32 -> DIVS 0/1/2/4/8/16 */
    if (p_hz >= 135000000UL)      k = 1;
    else if (p_hz >= 67500000UL)  k = 2;
    else if (p_hz >= 33750000UL)  k = 4;
    else if (p_hz >= 16875000UL)  k = 8;
    else if (p_hz >= 8437500UL)   k = 16;
    else                          k = 32;
    divs = k >> 1;
    w32 = p_hz;
    w_mul(2 * k);
    fvco = w32;
    /* N = Fin / 13.5 MHz - 2, Fin = LS / 2: RBR 4, HBR 8, HBR2 18.  (LS in
     * 0.1 MHz: ls10 / 270 - 2.  Careful: SDCC's int is 16 bits.) */
    n = (u8)(ls10() / 270) - 2;
    w_div(13500000UL);
    m = (u8)w32;
    m -= m / 8;
    for (;;) {                          /* while (m * 14.85 MHz < Fvco) m++ */
        w32 = m;
        w_mul(14850000UL);
        if (w32 >= fvco)
            break;
        m++;
    }
    w32 = m;
    w_mul(13500000UL);
    offs = w32;
    w32 = fvco;
    w32 -= offs;
    w_mul(32);
    w_div(m);
    w_mul(10);
    w_div(135000UL);
    w_mul(16384UL);
    w_div(1000);
    offs = w32 << 2;
    w32 = dp_log.frame27;
    w_div(27000UL);
    frame_ms = (u8)w32 + 1;

    SR(D1_MN_SCLKG_CTRL) = SCLKG_CTRL_LINK;
    SR(D1_PB8_MN_SCLKG_PLL_PWR) = D1_PB8_MN_SCLKG_PLL_PWR_REG_DPLL_PWDN | SPLL_PWR_ON;    /* off */
    SR_CLR(D1_MN_SCLKG_TRK_CTRL, D1_MN_SCLKG_TRK_CTRL_SCLKG_VS_EN);
    SR_WRF(D1_PB8_MN_SCLKG_PLL_WD, (u8)~D1_PB8_MN_SCLKG_PLL_WD_REG_DPLL_WDO,
           FV(D1_PB8_MN_SCLKG_PLL_WD_REG_DPLL_VCOMD, 2) | D1_PB8_MN_SCLKG_PLL_WD_REG_DPLL_RESERVE
           | D1_PB8_MN_SCLKG_PLL_WD_REG_DPLL_STOP);
    SR(D1_MN_SCLKG_DIVS) = divs;
    SR(D1_PB8_MN_SCLKG_PLL_PWR) = D1_PB8_MN_SCLKG_PLL_PWR_REG_DPLL_PWDN | SPLL_PWR_ON;
    SR(D1_MN_SCLKG_OFFS_L) = LO(offs);
    SR(D1_MN_SCLKG_OFFS_M) = HI(offs);
    SR(D1_MN_SCLKG_OFFS_H) = (SR(D1_MN_SCLKG_OFFS_H) & (u8)~D1_MN_SCLKG_OFFS_H_SCLKG_PLL_OFFS) | (BYTE2(offs) & 7);
    SR(D1_MN_SCLKG_SDM_CTRL) = D1_MN_SCLKG_SDM_CTRL_OCLK_EN | D1_MN_SCLKG_SDM_CTRL_HS_TRACKING_NEW_MODE;
    SR(D1_MN_SCLKG_SDM_CTRL) = D1_MN_SCLKG_SDM_CTRL_OCLK_EN | D1_MN_SCLKG_SDM_CTRL_SDM_EN
                               | D1_MN_SCLKG_SDM_CTRL_HS_TRACKING_NEW_MODE;
    SR(D1_MN_SCLKG_CTRL) = SCLKG_CTRL_LINK;
    SR(D1_PB8_MN_SCLKG_DIVM) = m - 2;
    SR(D1_PB8_MN_SCLKG_DIVN) = n;
    SR_WRF(D1_MN_SCLKG_OFFS_H, D1_MN_SCLKG_OFFS_H_SCLKG_LD_D_CODE | D1_MN_SCLKG_OFFS_H_SCLKG_PS_OLD
           | D1_MN_SCLKG_OFFS_H_SCLKG_MN_BL_EN | D1_MN_SCLKG_OFFS_H_SCLKG_MN_BT, D1_MN_SCLKG_OFFS_H_SCLKG_LD_D_CODE);
    SR(D1_MN_SCLKG_CTRL) = SCLKG_CTRL_LINK;
    /* the link-clock input itself is MN_SCLKG_CTRL's PLL_IN_SEL */
    SR(D1_PB8_MN_SCLKG_PLL_INSEL) = D1_PB8_MN_SCLKG_PLL_INSEL_REG_DPLL_VO2 | FV(D1_PB8_MN_SCLKG_PLL_INSEL_REG_DPLL_CAL, 1);
    for (k = 0; k < 4; k++) {
        SR(D1_PB8_MN_SCLKG_PLL_PWR) = SPLL_PWR_ON;      /* on */
        delay_ms(3);
    }
    /* charge pump: q = (4396*m/1340)/13; RS 3, CS 3, IP (q*20+25)/50 - 1 */
    w32 = m;
    w_mul(4396);
    w_div(1340UL * 13);                 /* (4396*m/1340)/13 (32-bit: 4396*m overflows an int) */
    k = (u8)w32;
    SR(D1_PB8_MN_SCLKG_PLL_CHP) = FV(D1_PB8_MN_SCLKG_PLL_CHP_REG_DPLL_RS, 3) | FV(D1_PB8_MN_SCLKG_PLL_CHP_REG_DPLL_CS, 3)
                                  | (u8)((((u16)k * 20 + 25) / 50) - 1);
    SR(D1_MN_SCLKG_SDM_CTRL) = D1_MN_SCLKG_SDM_CTRL_OCLK_EN | D1_MN_SCLKG_SDM_CTRL_SDM_EN
                               | D1_MN_SCLKG_SDM_CTRL_HS_TRACKING_MODE;
    SR(D1_MN_SCLKG_TRK_VS_I_M) = 0x00; SR(D1_MN_SCLKG_TRK_VS_I_L) = 0x04;
    SR(D1_MN_SCLKG_TRK_VS_P_H) = 0x00; SR(D1_MN_SCLKG_TRK_VS_P_M) = 0x7F; SR(D1_MN_SCLKG_TRK_VS_P_L) = 0xFF;
    w32 = p_hz;
    w_div(htt);
    w_div(33000UL);
    if (w32 > 15)
        w32 = 15;
    SR(D1_MN_SCLKG_TRK_MN_I_H) = ((u8)w32 << D1_MN_SCLKG_TRK_MN_I_H_SCLKG_VS_PERX_EN_SHIFT)
                                 | (SR(D1_MN_SCLKG_TRK_MN_I_H) & (u8)~D1_MN_SCLKG_TRK_MN_I_H_SCLKG_VS_PERX_EN);
    SR(D1_MN_SCLKG_TRK_VS_NLOCK) = p_hz < 50000000UL ? 0x7F : 0x3F;
    SR(D1_MN_SCLKG_TRK_CTRL) = D1_MN_SCLKG_TRK_CTRL_SCLKG_VS_EN | D1_MN_SCLKG_TRK_CTRL_VS_HS_SEL
                               | D1_MN_SCLKG_TRK_CTRL_PE_TRAIL_NEW_MODE;
    SR(D1_MN_SCLKG_CTRL) = SCLKG_CTRL_LINK;
    spll_settle(SR(D1_MN_SCLKG_TRK_VS_NLOCK) | D1_MN_SCLKG_TRK_VS_NLOCK_SCLKG_VS_NLOCK, frame_ms);
    SR(D1_MN_SCLKG_TRK_VS_I_M) = 0x00; SR(D1_MN_SCLKG_TRK_VS_I_L) = 0x02;
    w32 = offs << 1;
    w_div(31);
    w32 -= 2;
    SR(D1_MN_SCLKG_TRK_VS_P_H) = BYTE2(w32);
    SR(D1_MN_SCLKG_TRK_VS_P_M) = HI(w32);
    SR(D1_MN_SCLKG_TRK_VS_P_L) = LO(w32);
    w32 = dp_log.frame27;
    w_div(vtt);
    w32 >>= 1;                          /* half a line, 27 MHz cycles */
    wr16(D1_HS_TRACKING_NEW_MODE1, (u16)w32);
    SR_SET(D1_MN_SCLKG_SDM_TEST, D1_MN_SCLKG_SDM_TEST_MANUAL_VBID_SEL);
    SR(D1_VBID_MAN_MADE) = FV(D1_VBID_MAN_MADE_BE_START_NUM, 2) | FV(D1_VBID_MAN_MADE_BE_END_NUM, 2);
    SR_WRF(D1_MN_SCLKG_SDM_CTRL, D1_MN_SCLKG_SDM_CTRL_HS_TRACKING_NEW_MODE | D1_MN_SCLKG_SDM_CTRL_HS_TRACKING_MODE,
           D1_MN_SCLKG_SDM_CTRL_HS_TRACKING_NEW_MODE);
    SR(D1_MN_SCLKG_TRK_VS_NLOCK) = 0x1F;
    spll_settle(D1_MN_SCLKG_TRK_VS_NLOCK_SCLKG_VS_NLOCK | 0x1F, frame_ms);
    SR_SET(D1_DPF_CTRL_0, D1_DPF_CTRL_0_DISP_EN);   /* double buffered */
    for (k = 0; k < 75 && !(SR(D1_DP_VBID) & D1_DP_VBID_VBLK); k++)
        delay_ms(1);
    delay_ms(68);
    dp_log.spll_m = m - 2;
    dp_log.spll_offs = offs;
    return 0;
}

/* video FIFO check: six clean windows, no under/overflow */
static u8 vfifo_ok(void)
{
    u8 i, v;
    SR(D1_PG_CTRL_1) = PG1_W1C;
    delay_ms(3);
    for (i = 0; i < 6; i++) {
        delay_ms(10);
        v = SR(D1_PG_CTRL_1);
        dp_log.vfifo = v;
        if (v & (D1_PG_CTRL_1_VFIFO_UDFL | D1_PG_CTRL_1_VFIFO_OVFL))
            return 0;
    }
    return 1;
}

/* 0 = stream running; else 1 link not up, 2 no frame, 3 clock out of range,
 * 4 FIFO not clean (retry later) */
u8 dp_stream_start(void) __banked
{
    meas_armed = 0;
    SR(D1_DP_VWD_CTRL) = 0x00;          /* video watchdog off while (re)building the stream */
    if (lt_state != LT_LINK_UP)
        return 1;
    SR(D1_MN_STRM_ATTR_CTRL) = MSA_POPUP;
    htt = rd16(D1_MSA_HTT_0);
    hwd = rd16(D1_MSA_HWD_0);
    vtt = rd16(D1_MSA_VTTE_0);
    vht = rd16(D1_MSA_VHT_0);
    if (!htt || !vtt || !hwd || !vht)
        return 2;
    SR(D1_MN_DPF_BG_RED_M) = 0; SR(D1_MN_DPF_BG_RED_L) = 0;
    SR(D1_MN_DPF_BG_GRN_M) = 0; SR(D1_MN_DPF_BG_GRN_L) = 0;
    SR(D1_MN_DPF_BG_BLU_M) = 0; SR(D1_MN_DPF_BG_BLU_L) = 0;
    bs2bs();
    if (!dp_log.frame27)
        return 2;
    w32 = htt;
    w_mul(vtt);
    w_mul(100);
    w_div(dp_log.frame27);
    w_mul(27);
    w_div(10);
    pclk10 = (u16)w32;                  /* q * 270 kHz in 0.1 MHz */
    if (!pclk10)
        return 3;
    dpf_setup();
    if (spll_setup())
        return 3;
    if (!vfifo_ok())
        return 4;
    SR(D1_DP_GLB_STATUS) = D1_DP_GLB_STATUS_VBID3_CHG;  /* W1C; the B93B watchdog irq stays off */
    /* Video watchdog, armed after the FIFO check: on a video
     * FIFO overflow or underflow the DPF drops to its background (black)
     * by itself, within a frame - before the main-loop loss check (~350 ms)
     * - so an unplug or a source mode change shows black instead of noise. */
    SR(D1_DP_VWD_CTRL) = D1_DP_VWD_CTRL_VWD_BY_VFIFO_OVF_EN | D1_DP_VWD_CTRL_VWD_BY_VFIFO_UDF_EN;
    SR_SET(D1_DP_OUTPUT_CTRL, OUT_RGB);
    if (dp_audio_on)
        SR(D1_AUD_FSM_CTRL_0) = AUD_FSM_ON; /* after the stream */
    /* the link-clock reference for dp_stream_alive() */
    SR(D1_DP_SIG_DET_0) = SIG_DET_LN0;
    SR(D1_DP_SIG_DET_1) = 0x00;
    SR(D1_DP_SIG_DET_2) = 0x00;
    SR(D1_DP_SIG_DET_0) = SIG_DET_LN0 | D1_DP_SIG_DET_0_DP_SIG_DET_EN;
    delay_ms(3);
    lclk_ref = rd16(D1_DP_SIG_DET_3);
    dp_log.lclk_ref = lclk_ref;
    dp_log.msa_htt = htt; dp_log.msa_vtt = vtt; dp_log.msa_hwd = hwd; dp_log.msa_vht = vht;
    return 0;
}

/* Pixel clock as an HDMI-style frequency-detector count (kHz / 27): what the
 * display-clock math eats.  P / 27 kHz = q * 10. */
u16 dp_pixel_count(void) __banked
{
    if (!dp_log.frame27 || !htt)
        return 0;
    w32 = htt;
    w_mul(vtt);
    w_mul(1000UL);
    w32 += dp_log.frame27 >> 1;
    w_div(dp_log.frame27);
    return (u16)w32;
}

/* Is the stream we set up still there?  DP_ALIVE; DP_LINK_LOST when the
 * training state moved on or no blank-start (BE) symbols arrive (cable
 * out); DP_NO_VIDEO when the link is up but VB-ID says no video; DP_CHANGED
 * when the MSA totals/size differ from what was set up. */
u8 dp_stream_alive(void) __banked
{
    /* Live check first: lane 0's link clock, counted against the 27 MHz
     * reference (B050-B054, xtal cycles
     * 3: 5000 at HBR, 3000 at RBR).  Started on the last call (a tick,
     * >100 ms, ago): it must have finished, within 10 %.  Nothing latched
     * sees an unplug - link state, BE, VB-ID, the MSA and even the frame
     * time (the DPF free-runs) all hold, and the decoder simply stops
     * reporting errors. */
    u8 lost = 0;

    if (meas_armed) {
        u16 cnt = rd16(D1_DP_SIG_DET_3);
        dp_log.lclk = cnt;
        /* Against the count taken when the stream was set up, not the
         * nominal: with the cable out the CDR free-runs only ~1-1.6 % fast
         * (5044-5072 vs 4990 locked at HBR), and a spread-
         * spectrum source sits 0.25 % low.  Locked reads vary +-2. */
        if (!(SR(D1_DP_SIG_DET_0) & D1_DP_SIG_DET_0_DP_SIG_DET_DONE) ||
            cnt < lclk_ref - (lclk_ref >> 8) || cnt > lclk_ref + (lclk_ref >> 8))
            lost = 1;
    }
    /* restart every call, lost or not: the caller wants consecutive misses */
    SR(D1_DP_SIG_DET_0) = SIG_DET_LN0;
    SR(D1_DP_SIG_DET_1) = 0x00;         /* lane 0 */
    SR(D1_DP_SIG_DET_2) = 0x00;
    SR(D1_DP_SIG_DET_0) = SIG_DET_LN0 | D1_DP_SIG_DET_0_DP_SIG_DET_EN;   /* start */
    meas_armed = 1;
    if (lost)
        return DP_LINK_LOST;
    if (lt_state != LT_LINK_UP || !(SR(D1_PG_CTRL_1) & D1_PG_CTRL_1_BE_OCR))
        return DP_LINK_LOST;
    if (SR(D1_DP_VBID) & D1_DP_VBID_NO_VIDEO)
        return DP_NO_VIDEO;
    /* The video watchdog blanks on a FIFO over/underflow and stays blanked:
     * one seen means the stream must be redone (which re-arms it). */
    lost = SR(D1_PG_CTRL_1) & (D1_PG_CTRL_1_VFIFO_UDFL | D1_PG_CTRL_1_VFIFO_OVFL);
    if (lost) {
        SR(D1_PG_CTRL_1) = PG1_W1C;
        dp_log.vwd++;
        return DP_CHANGED;
    }
    SR(D1_MN_STRM_ATTR_CTRL) = MSA_POPUP;
    if (rd16(D1_MSA_HTT_0) != htt || rd16(D1_MSA_VTTE_0) != vtt ||
        rd16(D1_MSA_HWD_0) != hwd || rd16(D1_MSA_VHT_0) != vht)
        return DP_CHANGED;
    return DP_ALIVE;
}

/* Unplug check for the port while it is NOT shown (no setup reference yet):
 * lane 0's link clock against the nominal for the trained rate.  Locked it
 * reads 0-0.3 % low (4989-4992 at HBR, spread spectrum), free-running with
 * the cable out 1-1.6 % high (5044-5072).  Non-blocking, one count per call:
 * 1 locked, 0 not (or not trained), 2 no result yet. */
u8 dp_link_clk_nominal(void) __banked
{
    u8 r = 2;
    u16 cnt, exp;

    if (lt_state != LT_LINK_UP) {
        meas_armed = 0;                 /* the training handler owns B050 then */
        return 0;
    }
    if (meas_armed) {
        cnt = rd16(D1_DP_SIG_DET_3);
        exp = ls10() / 27 * 50;
        dp_log.lclk = cnt;
        r = 0;
        if (SR(D1_DP_SIG_DET_0) & D1_DP_SIG_DET_0_DP_SIG_DET_DONE)
            if (cnt > exp - exp / 100)
                if (cnt < exp + exp / 200)
                    r = 1;
    }
    SR(D1_DP_SIG_DET_0) = SIG_DET_LN0;
    SR(D1_DP_SIG_DET_1) = 0x00;
    SR(D1_DP_SIG_DET_2) = 0x00;
    SR(D1_DP_SIG_DET_0) = SIG_DET_LN0 | D1_DP_SIG_DET_0_DP_SIG_DET_EN;
    meas_armed = 1;
    return r;
}

/* Before a setup: link up, blank-start symbols arriving, VB-ID says video,
 * MSA totals present. */
u8 dp_stream_present(void) __banked
{
    if (lt_state != LT_LINK_UP || !(SR(D1_PG_CTRL_1) & D1_PG_CTRL_1_BE_OCR))
        return DP_LINK_LOST;
    if (SR(D1_DP_VBID) & D1_DP_VBID_NO_VIDEO)
        return DP_NO_VIDEO;
    SR(D1_MN_STRM_ATTR_CTRL) = MSA_POPUP;
    if (!rd16(D1_MSA_HTT_0) || !rd16(D1_MSA_VTTE_0))
        return DP_NO_VIDEO;
    return DP_ALIVE;
}

/* After a loss: forget the training, so a re-plugged source finds
 * "not trained" in DPCD 202-204 and trains through the handler again. */
void dp_link_reset(void) __banked
{
    EX0 = 0;
    lt_state = LT_IDLE;
    dp_log.lt_state = LT_IDLE;
    dpcd_w(0, DPCD_LANE0_1_STATUS, 0);
    dpcd_w(0, DPCD_LANE2_3_STATUS, 0);
    dpcd_w(0, DPCD_LANE_ALIGN_STATUS_UPDATED, 0x80);
    dpcd_w(0, DPCD_SINK_STATUS, 0);
    SR_CLR(D1_DP_OUTPUT_CTRL, OUT_RGB);
    EX0 = 1;
}

u8 dp_link_up(void) __banked
{
    return lt_state == LT_LINK_UP;
}
