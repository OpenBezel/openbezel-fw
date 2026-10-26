/* RTD2556 scaler page 0x7C */
#ifndef RTD2556_REGS_PAGE_7C_H
#define RTD2556_REGS_PAGE_7C_H

/* --- D3 GDI PHY Power On Region (Page 7C) --- */
#define DIG_00        0x7C00
#define   DIG_00_REG_PHY_SI_RESET_N                 0x07  /* [2:0] Digital Phy Reset_n (3 Lane) */
#define DIG_01        0x7C01
#define DIG_03        0x7C03
#define   DIG_03_REG_PHY_PN_SWAP                    0x70  /* [6:4] Swap PN to Avoid PN Mismatch (3 lane) */
#define DIG_05        0x7C05
#define   DIG_05_REG_PHY_SI_HD_DP_SEL               0x80  /* [7] 0:DP */
#define SDM_03        0x7C0E
#define   SDM_03_REG_BYPASS_CLK_RDY                 0x02  /* [1] Bypass clk rdy signal from cmu */
#define   SDM_03_REG_EN_CLKOUT_MANUAL               0x01  /* [0] Enable When bypass_clk_rdy=1 */
#define WD            0x7C0F
#define   WD_REG_PLL_WD_EN                          0x01  /* [0] Enable wdog */
#define EQEN_TIMER    0x7C10
#define   EQEN_TIMER_REG_BYPASS_EQEN_RDY            0x02  /* [1] 0:auto mode */
#define   EQEN_TIMER_REG_EN_EQEN_MANUAL             0x01  /* [0] Enable When bypass_eqen_rdy =1 */
#define DATA_TIMER    0x7C11
#define   DATA_TIMER_REG_BYPASS_DATA_RDY            0x02  /* [1] Bypass data rdy signal from cdr */
#define   DATA_TIMER_REG_EN_DATA_MANUAL             0x01  /* [0] Enable When bypass_data_rdy=1 */
#define ANA_CDR       0x7C12
#define   ANA_CDR_REG_ACDR_L0_EN                    0x10  /* [4] Enable eqen_rdy from lane's WD or central WD */
#define   ANA_CDR_REG_EN_DATA_MANUAL_L0             0x08  /* [3] Enable when bypass_data_rdy_l0 = 1 */
#define   ANA_CDR_REG_BYPASS_DATA_RDY_L0            0x04  /* [2] Bypass data rdy signal from adp_en */
#define   ANA_CDR_REG_EN_EQEN_MANUAL_L0             0x02  /* [1] Enable when bypass_eqen_rdy_l0 = 1 */
#define   ANA_CDR_REG_BYPASS_EQEN_RDY_L0            0x01  /* [0] 0:auto mode */
#define BANDGAP_03    0x7C17
#define   BANDGAP_03_REG_P_CHANNEL_EN               0x80  /* [7] enable p channel */
#define   BANDGAP_03_REG_N_CHANNEL_EN               0x40  /* [6] enable n channel */
#define   BANDGAP_03_REG_RXVCOM_SEL                 0x30  /* [5:4] rx amplifier input common mode voltage: */
#define   BANDGAP_03_REG_RXVCOM_SEL_SHIFT           4
#define   BANDGAP_03_REG_EN_ENHANCE_BIAS            0x08  /* [3] enhance clock lane buffer current */
#define   BANDGAP_03_REG_PSM                        0x04  /* [2] 1=CDR in power saving mode,only output ref clock */
#define   BANDGAP_03_REG_MHL_MODE                   0x02  /* [1] 0:determined by dp_mode */
#define   BANDGAP_03_REG_Z0_MANUAL                  0x01  /* [0] enable manual mode of 50ohm */
#define IMPEDANCE_00  0x7C18
#define   IMPEDANCE_00_REG_Z0_EN                    0xF0  /* [7:4] 0 = disable 50 Ohm on */
#define   IMPEDANCE_00_REG_Z0_ADJR                  0x0F  /* [3:0] manual code of 50ohm when z0_manual=1 */
#define IMPEDANCE_01  0x7C19
#define   IMPEDANCE_01_REG_Z0_CM_ADJR               0x07  /* [2:0] manual code of cm resistance */
#define CMU_00        0x7C1A
#define   CMU_00_REG_RX_EN                          0xF0  /* [7:4] lane3~0 power control of common mode set & 50 ohm autok */
#define   CMU_00_REG_RX_EN_SHIFT                    4
#define   CMU_00_REG_CDR_EN                         0x0F  /* [3:0] lane3~0 cdr enable(data trans enable) */
#define   CMU_00_REG_CDR_EN_SHIFT                   0
#define CMU_01        0x7C1B
#define   CMU_01_REG_CMU_EN                         0x80  /* [7] PLL enable */
#define   CMU_01_REG_CMU_REFCK_SEL                  0x60  /* [6:5] select cmu reference clk,when reg_hybrid=0 */
#define   CMU_01_REG_EN_MHL_OP                      0x10  /* [4] enable of amplifier used for mhl mode */
#define   CMU_01_REG_EN_APLL_LANE2                  0x01  /* [0] enable apll mode of lane2, used as cmu. */
#define CMU_02        0x7C1C
#define   CMU_02_REG_EN_VTEST_HIZ                   0x10  /* [4] enable vtest hiz mode */
#define CMU_03        0x7C1D
#define   CMU_03_REG_EN_SUMAMP_HIGHGAIN             0x10  /* [4] enable sumamp high dcgain */
#define CMU_04        0x7C1E
#define   CMU_04_REG_CMU_SEL_CP                     0x30  /* [5:4] sel cmu cp */
#define   CMU_04_REG_CMU_SEL_CP_2P                  0x20  /* 10 */
#define   CMU_04_REG_EN_LARGERS_FORCMU              0x08  /* [3] when enable, rs increase 8k, for apll mode */
#define   CMU_04_REG_CMU_SEL_RS                     0x07  /* [2:0] reg_cmu_sel_rs */
#define   CMU_04_REG_CMU_SEL_RS_16K                 0x06  /* 110 */
#define CMU_05        0x7C1F
#define   CMU_05_REG_CMU_SELPREDIV                  0x30  /* [5:4] cmu pre_divider set */
#define   CMU_05_REG_CMU_SELPREDIV_2                0x10  /* 01 */
#define CMU_06        0x7C20
#define CMU_07        0x7C21
#define   CMU_07_REG_TAP_EN                         0xF8  /* [7:3] dfe tap coef enable */
#define   CMU_07_REG_SEL_DIV                        0x07  /* [2:0] data rate sel */
#define   CMU_07_REG_SEL_DIV_1_5G_3G                0x01  /* 001 */
#define   CMU_07_REG_SEL_DIV_750M_1_5G              0x02  /* 010 */
#define   CMU_07_REG_SEL_DIV_375_750M               0x03  /* 011 */
#define   CMU_07_REG_SEL_DIV_187_5M_375M            0x04  /* 100 */
#define CMU_08        0x7C22
#define   CMU_08_REG_SEL_DFE_DELAY                  0x80  /* [7] increase dfe feedback delay */
#define   CMU_08_REG_LE_LOWGAIN_HIGHBOOST           0x0E  /* [3:1] LEQ stage3~stage1 high boost enable,only bit2 usable,bit1,bit0 rese... */
#define   CMU_08_REG_LE_LOWGAIN_HIGHBOOST_SHIFT     1
#define   CMU_08_REG_RESERVED                       0x01  /* [0] reg_reserved<1>: */
#define CMU_09        0x7C23
#define   CMU_09_REG_LE_RLSEL                       0xE0  /* [7:5] LEQ RLSEL */
#define   CMU_09_REG_LE_RLSEL_SHIFT                 5
#define   CMU_09_REG_LE_ISEL                        0x1C  /* [4:2] LEQ current SEL */
#define KOFFSET_00    0x7C24
#define   KOFFSET_00_REG_EN_BACK_OFFSET             0x80  /* [7] Enable background offset calibration DAC: */
#define   KOFFSET_00_REG_EN_FORE_OFFSET             0x40  /* [6] EQ foreground offset calibration enable: */
#define   KOFFSET_00_REG_FORE_OFF_AUTOK             0x20  /* [5] EQ foreground offset calibration using auto mode: */
#define KOFFSET_02    0x7C26
#define   KOFFSET_02_REG_OFFCAL_RANGE               0xC0  /* [7:6] offset calibration current range */
#define   KOFFSET_02_REG_OFFCAL_RANGE_SHIFT         6
#define KOFFSET_04    0x7C28
#define   KOFFSET_04_REG_EN_FLD_REF_ECBUSS          0x80  /* [7] select ecbus-s clock as fld reference clk */
#define   KOFFSET_04_REG_HYBRID                     0x40  /* [6] Hybrid CDR mode enable */
#define ANA_CDR_00    0x7C2A
#define   ANA_CDR_00_REG_RSTB_FSM                   0x80  /* [7] reset FLD(low active) */
#define   ANA_CDR_00_REG_EN_ANA_CDR                 0x0F  /* [3:0] enable of analog cdr */
#define ANA_CDR_01    0x7C2B
#define   ANA_CDR_01_REG_AUTO_MODE                  0x80  /* [7] FLD auto mode */
#define   ANA_CDR_01_REG_ADP_EN_MANUAL              0x40  /* [6] Adaptive equzlier manual mode enable(active @ REG_AUTO_MODE=0) */
#define   ANA_CDR_01_REG_CP_EN_MANUAL               0x20  /* [5] Enable only at FLD manual mode(REG_AUTO_MODE=0): */
#define   ANA_CDR_01_REG_ADP_EQ_OFF                 0x10  /* [4] 0:DFE equalizer adaption enable */
#define   ANA_CDR_01_REG_XTAL_EN                    0x08  /* [3] 0:FLD using reference clock as reference(DP using TP1/10 as FLD ref... */
#define   ANA_CDR_01_REG_CP_ADJ_EN                  0x01  /* [0] 1:enable adjust charge pump current when tracking */
#define ANA_CDR_02    0x7C2C
#define   ANA_CDR_02_REG_ADP_TIME                   0x70  /* [6:4] adaptive equalizer turn on time delay after CDR close loop */
#define   ANA_CDR_02_REG_ADP_TIME_SHIFT             4
#define   ANA_CDR_02_REG_VCM_TUNE                   0x0C  /* [3:2] selection of analog CDR's multiband VCO's Vc common mode */
#define   ANA_CDR_02_REG_VCM_TUNE_SHIFT             2
#define   ANA_CDR_02_REG_CDR_KVCO                   0x03  /* [1:0] analog CDR multiband VCO KVCO: */
#define   ANA_CDR_02_REG_CDR_KVCO_SHIFT             0
#define ANA_CDR_03    0x7C2D
#define   ANA_CDR_03_REG_EN_WIDE_TEMP               0x20  /* [5] en_wide_temp */
#define   ANA_CDR_03_REG_SEL_CONT3                  0x10  /* [4] 0:Bypass */
#define   ANA_CDR_03_REG_REF_DIV_CONT               0x02  /* [1] FLD reference clock output counter number,@3G HDMI,=250 */
#define   ANA_CDR_03_REG_CDR_DIV_CONT               0x01  /* [0] FLD VCO output/5 counter number,@3G HDMI,=500 */
#define ANA_CDR_04    0x7C2E
#define ANA_CDR_05    0x7C2F
#define ANA_CDR_07    0x7C31
#define   ANA_CDR_07_REG_CDR_CP_IN                  0x0F  /* [3:0] Analog CDR 's charge pump current selection: */
#define   ANA_CDR_07_REG_CDR_CP_IN_SHIFT            0
#define TMDS_RESET    0x7C38
#define   TMDS_RESET_REG_RST_N_L0                   0x40  /* [6] Reset N of digital */
#define   TMDS_RESET_REG_RST_N_L1                   0x20  /* [5] Reset N of digital */
#define   TMDS_RESET_REG_RST_N_L2                   0x10  /* [4] Reset N of digital */
#define   TMDS_RESET_REG_CDR_RST_N_L0               0x04  /* [2] Reset N of CDR (execpt CLKWR) */
#define   TMDS_RESET_REG_CDR_RST_N_L1               0x02  /* [1] Reset N of CDR (execpt CLKWR) */
#define   TMDS_RESET_REG_CDR_RST_N_L2               0x01  /* [0] Reset N of CDR (execpt CLKWR) */
#define EN_01         0x7C3E
#define   EN_01_REG_RATE_SEL                        0x70  /* [6:4] CDR(Digital) Rate Sel */
#define   EN_01_REG_RATE_SEL_300M_150M              0x10  /* 001 */
#define   EN_01_REG_RATE_SEL_150M_75M               0x20  /* 010 */
#define   EN_01_REG_RATE_SEL_75M_37_5M              0x30  /* 011 */
#define   EN_01_REG_RATE_SEL_37_5M_18_75M           0x40  /* 100 */
#define KD            0x7C4F
#define   KD_REG_KD_L0                              0x80  /* [7] differential gain of CDR filter. */
#define   KD_REG_KD_L1                              0x40  /* [6] differential gain of CDR filter. */
#define   KD_REG_KD_L2                              0x20  /* [5] differential gain of CDR filter. */
#define KI_L0         0x7C50
#define   KI_L0_REG_KI_L0                           0x07  /* [2:0] Integration Gain of Digital LPF */
#define KI_L1         0x7C51
#define   KI_L1_REG_KI_L1                           0x07  /* [2:0] Integration Gain of Digital LPF */
#define KI_L2         0x7C52
#define   KI_L2_REG_KI_L2                           0x07  /* [2:0] Integration Gain of Digital LPF */
#define KP_L0         0x7C53
#define KP_L1         0x7C54
#define KP_L2         0x7C55

/* --- D3 GDI DFE (Page 7C) --- */
#define L0_DFE_EN_1   0x7CA0
#define   L0_DFE_EN_1_TIMER_CTRL_EN_LANE0           0x40  /* [6] Enable the timer to control adaptation-loop */
#define   L0_DFE_EN_1_REDUCE_ADAPT_GAIN_LANE0       0x30  /* [5:4] Reduce the all adaptation-loop's gain after settling */
#define   L0_DFE_EN_1_REDUCE_ADAPT_GAIN_LANE0_SHIFT 4
#define   L0_DFE_EN_1_RL_DET_EN_LANE0               0x08  /* [3] Enable run-length detection */
#define   L0_DFE_EN_1_RL_DET_MODE_LANE0             0x04  /* [2] Blank out case-3 symbol */
#define L0_DFE_EN_2   0x7CA1
#define L0_LIMIT_INIT 0x7CA2
#define   L0_LIMIT_INIT_LE_MIN_LANE0                0xF0  /* [7:4] Minimum of LE (-0 ~ -31) */
#define   L0_LIMIT_INIT_LE_MIN_LANE0_SHIFT          4
#define   L0_LIMIT_INIT_LE_INIT_LANE0               0x0F  /* [3:0] Initial value of LE coefficient, effective only on mode 0/2 (-0~-31) */
#define   L0_LIMIT_INIT_LE_INIT_LANE0_SHIFT         0
#define L0_INIT_1     0x7CA3
#define   L0_INIT_1_LE_MIN_LANE0                    0x80  /* [7] Minimum of LE (-0 ~ -31) */
#define   L0_INIT_1_SERVO_INIT_LANE0                0x1F  /* [4:0] Initial value of servo-loop coefficient (bit[7:0] : +0~+31) */
#define   L0_INIT_1_SERVO_INIT_LANE0_SHIFT          0
#define L0_INIT_2     0x7CA4
#define L0_INIT_3     0x7CA5
#define L0_INIT_4     0x7CA6
#define L0_INIT_5     0x7CA7
#define L0_INIT_6     0x7CA8
#define L0_INIT_7     0x7CA9
#define L0_INIT_8     0x7CAA
#define L0_GDI_DE     0x7CAF
#define   L0_GDI_DE_DE_INVERSE_LANE0                0x02  /* [1] inverse of gdi_de to brank */
#define   L0_GDI_DE_DE_BRANK_EN_LANE0               0x01  /* [0] Enable brank function of gdi_de */
#define L1_DFE_EN_1   0x7CB0
#define L2_DFE_EN_1   0x7CC0
#define MODE_TIMER    0x7CE0
#define   MODE_TIMER_ADAPT_MODE                     0xC0  /* [7:6] 00 : Edge-based adaptation only with LE loop. */
#define   MODE_TIMER_ADAPT_MODE_SHIFT               6
#define   MODE_TIMER_EDGE_DET_MODE                  0x20  /* [5] Edge lead or lag data */
#define   MODE_TIMER_TRANSITION_ONLY                0x10  /* [4] Only transition bit is effective at Tap2~4 */
#define   MODE_TIMER_LE_AUTO_RELOAD                 0x08  /* [3] Enable auto-reload le coefficient when max-tap0 & max-le */
#define   MODE_TIMER_TAP1_DELAY                     0x07  /* [2:0] Delay time of tap1+LE loop after turning on tap0 */
#define   MODE_TIMER_TAP1_DELAY_SHIFT               0
#define TIMER         0x7CE1
#define   TIMER_SERVO_DELAY                         0x03  /* [1:0] Delay time of servo-loop after turning on tap0 */
#define   TIMER_SERVO_DELAY_SHIFT                   0
#define GAIN_1        0x7CE2
#define   GAIN_1_TAP0_GAIN                          0xE0  /* [7:5] Filter's gain of Tap0 */
#define   GAIN_1_TAP0_GAIN_SHIFT                    5
#define   GAIN_1_TAP1_GAIN                          0x1C  /* [4:2] Filter's gain of Tap1 */
#define   GAIN_1_TAP1_GAIN_SHIFT                    2
#define GAIN_2        0x7CE3
#define   GAIN_2_TAP2_GAIN                          0xE0  /* [7:5] Filter's gain of Tap2 */
#define   GAIN_2_TAP2_GAIN_SHIFT                    5
#define   GAIN_2_TAP3_GAIN                          0x1C  /* [4:2] Filter's gain of Tap3 */
#define   GAIN_2_TAP3_GAIN_SHIFT                    2
#define   GAIN_2_TAP1_TRANS                         0x02  /* [1] Transition bit is effective at Tap1&LE(LE only for loop mode 2/3) */
#define   GAIN_2_TAP0_TRANS                         0x01  /* [0] Transition bit is effective at Tap0 */
#define GAIN_3        0x7CE4
#define   GAIN_3_TAP4_GAIN                          0xE0  /* [7:5] Filter's gain of Tap4 */
#define   GAIN_3_TAP4_GAIN_SHIFT                    5
#define   GAIN_3_SERVO_GAIN                         0x1C  /* [4:2] Filter's gain of servo-loop */
#define   GAIN_3_SERVO_GAIN_SHIFT                   2
#define GAIN_4        0x7CE5
#define   GAIN_4_LE_GAIN1                           0xE0  /* [7:5] Filter's gain of LE1, effective only on mode 0/2 */
#define   GAIN_4_LE_GAIN1_SHIFT                     5
#define   GAIN_4_LE_GAIN2                           0x1C  /* [4:2] Filter's gain of LE2, effective only on mode 2 */
#define   GAIN_4_LE_GAIN2_SHIFT                     2
#define LIMIT_1       0x7CE6
#define   LIMIT_1_VTH_THRESHOLD                     0xE0  /* [7:5] Increase Vth's coefficient when Vth's coefficient less than the vth... */
#define   LIMIT_1_VTH_THRESHOLD_SHIFT               5
#define   LIMIT_1_TAP0_MAX                          0x1F  /* [4:0] Maximum of Tap0 (+0.5 ~ +31.5) */
#define   LIMIT_1_TAP0_MAX_SHIFT                    0
#define LIMIT_2       0x7CE7
#define   LIMIT_2_TAP1_MIN                          0x3F  /* [5:0] Minimum of Tap1 (-0.5 ~ -63.5) */
#define   LIMIT_2_TAP1_MIN_SHIFT                    0
#define LIMIT_3       0x7CE8
#define   LIMIT_3_TAP2_MAX                          0xF8  /* [7:3] Maximum of Tap2 (+0.5 ~ +31.5) */
#define   LIMIT_3_TAP2_MAX_SHIFT                    3
#define   LIMIT_3_TAP2_MIN                          0x07  /* [2:0] Minimum of Tap2 (-0.5 ~ -31.5) */
#define   LIMIT_3_TAP2_MIN_SHIFT                    0
#define LIMIT_4       0x7CE9
#define   LIMIT_4_TAP2_MIN                          0xC0  /* [7:6] Minimum of Tap2 (-0.5 ~ -31.5) */
#define   LIMIT_4_TAP2_MIN_SHIFT                    6
#define   LIMIT_4_TAP3_MAX                          0x1F  /* [4:0] Maximum of Tap3 (+0.5 ~ +31.5) */
#define LIMIT_5       0x7CEA
#define   LIMIT_5_TAP3_MIN                          0xF8  /* [7:3] Minimum of Tap3 (-0.5 ~ -31.5) */
#define   LIMIT_5_TAP3_MIN_SHIFT                    3
#define   LIMIT_5_TAP4_MAX                          0x07  /* [2:0] Maximum of Tap4 (+0.5 ~ +31.5) */
#define LIMIT_6       0x7CEB
#define   LIMIT_6_TAP4_MAX                          0xC0  /* [7:6] Maximum of Tap4 (+0.5 ~ +31.5) */
#define   LIMIT_6_TAP4_MIN                          0x1F  /* [4:0] Minimum of Tap4 (-0.5 ~ -31.5) */
#define   LIMIT_6_TAP4_MIN_SHIFT                    0
#define LOOP_DIV_1    0x7CEC
#define   LOOP_DIV_1_SERVO_DIVISOR                  0xFC  /* [7:2] Servo-loop's divider of symbol clock (2~63) */
#define   LOOP_DIV_1_SERVO_DIVISOR_SHIFT            2
#define   LOOP_DIV_1_VTH_DIVISOR                    0x03  /* [1:0] Vth-adjust's divider of tap clock (1~15) */
#define   LOOP_DIV_1_VTH_DIVISOR_SHIFT              0
#define LOOP_DIV_2    0x7CED
#define   LOOP_DIV_2_VTH_DIVISOR                    0xC0  /* [7:6] Vth-adjust's divider of tap clock (1~15) */
#define   LOOP_DIV_2_VTH_DIVISOR_SHIFT              6
#define   LOOP_DIV_2_TAP_DIVISOR                    0x3F  /* [5:0] Tap-loop's divider of symbol clock (2~63) */
#define   LOOP_DIV_2_TAP_DIVISOR_SHIFT              0
#define RESET_1       0x7CEF
#define   RESET_1_TAP0_THRESHOLD                    0xF0  /* [7:4] Increase Vth's coefficient when Vth's coefficient less than the vth... */
#define   RESET_1_TAP0_THRESHOLD_SHIFT              4
#define   RESET_1_DFE_ADAPT_RSTB_LANE3              0x08  /* [3] Reset adaptation loop of lane3 (0 : reset, 1 : normal) */
#define   RESET_1_DFE_ADAPT_RSTB_LANE2              0x04  /* [2] Reset adaptation loop of lane2 (0 : reset, 1 : normal) */
#define   RESET_1_DFE_ADAPT_RSTB_LANE1              0x02  /* [1] Reset adaptation loop of lane1 (0 : reset, 1 : normal) */
#define   RESET_1_DFE_ADAPT_RSTB_LANE0              0x01  /* [0] Reset adaptation loop of lane0 (0 : reset, 1 : normal) */
#define GRAY_DEC_2    0x7CF8
#define   GRAY_DEC_2_SERVO_NOTRANS                  0x20  /* [5] Transition bit is no effective in servo */
#define   GRAY_DEC_2_TAP0_NOTRANS                   0x10  /* [4] Transition bit is no effective in tap0 */
#define LIMIT_10      0x7CF9
#define   LIMIT_10_VTH_MAX                          0xF0  /* [7:4] Maximum of Vth coefficient (0 ~ 15) */
#define   LIMIT_10_VTH_MAX_SHIFT                    4
#define GAIN_PREC     0x7CFA
#define   GAIN_PREC_LE_PREC_SIGN                    0x08  /* [3] Sign bit of precursor in LE loop, effective only on mode 2. */
#define   GAIN_PREC_LE_GAIN_PREC                    0x07  /* [2:0] Filter's precursor gain of LE, effective only on mode 2 */
#define   GAIN_PREC_LE_GAIN_PREC_SHIFT              0

#endif /* RTD2556_REGS_PAGE_7C_H */
