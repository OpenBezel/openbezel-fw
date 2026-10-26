/* RTD2556 scaler page 0x7A */
#ifndef RTD2556_REGS_PAGE_7A_H
#define RTD2556_REGS_PAGE_7A_H

/* --- D1 GDI PHY Power On Region (Page 7A) --- */
#define D1_SDM_03        0x7A0E
#define   D1_SDM_03_REG_BYPASS_CLK_RDY                 0x02  /* [1] Bypass clk rdy signal from cmu */
#define D1_WD            0x7A0F
#define   D1_WD_REG_PLL_WD_EN                          0x01  /* [0] Enable wdog */
#define D1_EQEN_TIMER    0x7A10
#define   D1_EQEN_TIMER_REG_BYPASS_EQEN_RDY            0x02  /* [1] 0:auto mode */
#define D1_DATA_TIMER    0x7A11
#define   D1_DATA_TIMER_REG_BYPASS_DATA_RDY            0x02  /* [1] Bypass data rdy signal from cdr */
#define D1_ANA_CDR       0x7A12
#define   D1_ANA_CDR_REG_ACDR_L0_EN                    0x10  /* [4] Enable eqen_rdy from lane's WD or central WD */
#define   D1_ANA_CDR_REG_EN_DATA_MANUAL_L0             0x08  /* [3] Enable when bypass_data_rdy_l0 = 1 */
#define   D1_ANA_CDR_REG_BYPASS_DATA_RDY_L0            0x04  /* [2] Bypass data rdy signal from adp_en */
#define   D1_ANA_CDR_REG_EN_EQEN_MANUAL_L0             0x02  /* [1] Enable when bypass_eqen_rdy_l0 = 1 */
#define   D1_ANA_CDR_REG_BYPASS_EQEN_RDY_L0            0x01  /* [0] 0:auto mode */
#define D1_BANDGAP_03    0x7A17
#define   D1_BANDGAP_03_REG_P_CHANNEL_EN               0x80  /* [7] enable p channel */
#define   D1_BANDGAP_03_REG_N_CHANNEL_EN               0x40  /* [6] enable n channel */
#define   D1_BANDGAP_03_REG_RXVCOM_SEL                 0x30  /* [5:4] rx amplifier input common mode voltage: */
#define   D1_BANDGAP_03_REG_RXVCOM_SEL_SHIFT           4
#define   D1_BANDGAP_03_REG_EN_ENHANCE_BIAS            0x08  /* [3] enhance clock lane buffer current */
#define   D1_BANDGAP_03_REG_PSM                        0x04  /* [2] 1=CDR in power saving mode,only output ref clock */
#define   D1_BANDGAP_03_REG_MHL_MODE                   0x02  /* [1] 1: mhl mode no matter dp_mode=1 or 0; */
#define D1_IMPEDANCE_00  0x7A18
#define   D1_IMPEDANCE_00_REG_Z0_ADJR                  0x0F  /* [3:0] manual code of 50ohm when z0_manual=1 */
#define D1_IMPEDANCE_01  0x7A19
#define   D1_IMPEDANCE_01_REG_EN_SIGDET_LANE           0xF0  /* [7:4] sigdet enable of lane0~lane3 */
#define   D1_IMPEDANCE_01_REG_Z0_MANUAL                0x08  /* [3] enable manual mode of 50ohm */
#define   D1_IMPEDANCE_01_REG_Z0_CM_ADJR               0x07  /* [2:0] manual code of cm resistance */
#define   D1_IMPEDANCE_01_REG_Z0_CM_ADJR_SHIFT         0
#define D1_CMU_00        0x7A1A
#define   D1_CMU_00_REG_RX_EN                          0xF0  /* [7:4] lane3~0 power control of common mode set & 50 ohm autok */
#define   D1_CMU_00_REG_RX_EN_SHIFT                    4
#define   D1_CMU_00_REG_CDR_EN                         0x0F  /* [3:0] lane3~0 cdr enable(data trans enable */
#define   D1_CMU_00_REG_CDR_EN_SHIFT                   0
#define D1_CMU_01        0x7A1B
#define   D1_CMU_01_REG_CMU_EN                         0x80  /* [7] PLL enable */
#define   D1_CMU_01_REG_CMU_CKSEL                      0x20  /* [5] CMU's input reference clock coming from: */
#define   D1_CMU_01_REG_EN_MHL_OP                      0x10  /* [4] enable of amplifier used for mhl mode */
#define   D1_CMU_01_REG_HST_BUF_SEL                    0x04  /* [2] select hysteresis amplifier in clock lane */
#define D1_CMU_02        0x7A1C
#define   D1_CMU_02_REG_EN_VTEST_HIZ                   0x10  /* [4] enable vtest hiz mode */
#define D1_CMU_03        0x7A1D
#define   D1_CMU_03_REG_ENHANCE_SA_BIAS                0x0C  /* [3:2] enhance sumamp bias */
#define   D1_CMU_03_REG_ENHANCE_SA_BIAS_NORMAL         0x00  /* 00 */
#define   D1_CMU_03_REG_ENHANCE_IBN_BIAS_NORMAL        0x01  /* 01 */
#define D1_CMU_07        0x7A21
#define   D1_CMU_07_REG_TAP_EN                         0xF8  /* [7:3] DFE TAP enable */
#define   D1_CMU_07_REG_SEL_DIV_1_5G_3G                0x01  /* 001 */
#define D1_CMU_08        0x7A22
#define   D1_CMU_08_REG_SEL_DFE_DELAY                  0x80  /* [7] increase dfe feedback delay */
#define   D1_CMU_08_REG_LE_LOWGAIN_HIGHBOOST           0x0E  /* [3:1] LEQ stage3~stage1 high boost enable,only bit2 usable,bit1,bit0 rese... */
#define D1_CMU_09        0x7A23
#define   D1_CMU_09_REG_LE_RLSEL                       0xE0  /* [7:5] LEQ RLSEL */
#define   D1_CMU_09_REG_LE_RLSEL_SHIFT                 5
#define   D1_CMU_09_REG_LE_ISEL                        0x1C  /* [4:2] LEQ current SEL */
#define   D1_CMU_09_REG_LE_ISEL_SHIFT                  2
#define D1_KOFFSET_00    0x7A24
#define   D1_KOFFSET_00_REG_EN_BACK_OFFSET             0x80  /* [7] Enable background offset calibration DAC: */
#define   D1_KOFFSET_00_REG_EN_FORE_OFFSET             0x40  /* [6] EQ foreground offset calibration enable: */
#define   D1_KOFFSET_00_REG_FORE_OFF_AUTOK             0x20  /* [5] EQ foreground offset calibration using auto mode: */
#define   D1_KOFFSET_00_REG_FORE_OFF_ADJR_L0           0x1F  /* [4:0] code of fore offset when autok=0 */
#define   D1_KOFFSET_00_REG_FORE_OFF_ADJR_L0_SHIFT     0
#define D1_KOFFSET_02    0x7A26
#define   D1_KOFFSET_02_REG_OFFCAL_RANGE               0xC0  /* [7:6] offset calibration current range */
#define   D1_KOFFSET_02_REG_OFFCAL_RANGE_SHIFT         6
#define D1_KOFFSET_03    0x7A27
#define   D1_KOFFSET_03_REG_HYBRID                     0x20  /* [5] Hybrid CDR mode enable */
#define D1_KOFFSET_06    0x7A2A
#define   D1_KOFFSET_06_REG_EN_ANA_CDR                 0xC0  /* [7:6] enable of analog cdr */
#define   D1_KOFFSET_06_REG_EN_ANA_CDR_SHIFT           6
#define   D1_KOFFSET_06_REG_FORE_OFF_TST_L2            0x3F  /* [5:0] Foreground calibration offset value of lane2 */
#define   D1_KOFFSET_06_REG_FORE_OFF_TST_L2_SHIFT      0
#define D1_KOFFSET_07    0x7A2B
#define   D1_KOFFSET_07_REG_EN_ANA_CDR                 0xC0  /* [7:6] enable of analog cdr <0>:lane0, <1>:lane1, <2>:lane2. <3>lane3 */
#define   D1_KOFFSET_07_REG_EN_ANA_CDR_SHIFT           6
#define   D1_KOFFSET_07_REG_FORE_OFF_TST_L3            0x3F  /* [5:0] Foreground calibration offset value of lane3 */
#define   D1_KOFFSET_07_REG_FORE_OFF_TST_L3_SHIFT      0
#define D1_ANA_CDR_01    0x7A2C
#define   D1_ANA_CDR_01_REG_AUTO_MODE                  0x80  /* [7] FLD auto mode */
#define   D1_ANA_CDR_01_REG_ADP_EN_MANUAL              0x40  /* [6] Adaptive equzlier manual mode enable(active @ REG_AUTO_MODE=0) */
#define   D1_ANA_CDR_01_REG_CP_EN_MANUAL               0x20  /* [5] Enable only at FLD manual mode(REG_AUTO_MODE=0): */
#define   D1_ANA_CDR_01_REG_ADP_EQ_OFF                 0x10  /* [4] 1:DFE equalizer adaption disable */
#define   D1_ANA_CDR_01_REG_XTAL_EN                    0x08  /* [3] 1:FLD using xtal as reference clock */
#define   D1_ANA_CDR_01_REG_XTAL_SEL                   0x04  /* [2] FLD xtal divider select /8 */
#define   D1_ANA_CDR_01_REG_CP_ADJ_EN                  0x01  /* [0] 1:enable adjust charge pump current when tracking */
#define D1_ANA_CDR_02    0x7A2D
#define   D1_ANA_CDR_02_REG_ADP_TIME                   0xE0  /* [7:5] adaptive equalizer turn on time delay after CDR close loop */
#define   D1_ANA_CDR_02_REG_ADP_TIME_SHIFT             5
#define   D1_ANA_CDR_02_REG_SEL_CONT3                  0x10  /* [4] 1:BPD UP/DN pulse div3   0:Bypass */
#define   D1_ANA_CDR_02_REG_RSTB_FSM                   0x04  /* [2] reset FLD(low active) */
#define   D1_ANA_CDR_02_REG_CDR_DIV_CONT               0x01  /* [0] FLD VCO output/5 counter number,@3G HDMI,=500 */
#define D1_ANA_CDR_03    0x7A2E
#define D1_ANA_CDR_04    0x7A2F
#define D1_ANA_CDR_05    0x7A30
#define   D1_ANA_CDR_05_REG_CDR_KVCO                   0x0C  /* [3:2] analog CDR multiband VCO KVCO: */
#define   D1_ANA_CDR_05_REG_CDR_KVCO_SHIFT             2
#define   D1_ANA_CDR_05_REG_VCM_TUNE                   0x03  /* [1:0] selection of analog CDR's multiband VCO's Vc common mode */
#define   D1_ANA_CDR_05_REG_VCM_TUNE_SHIFT             0
#define D1_ANA_CDR_06    0x7A31
#define   D1_ANA_CDR_06_REG_SEL_CDR_RES                0x70  /* [6:4] selection of Analog CDR's loop resistor: */
#define   D1_ANA_CDR_06_REG_SEL_CDR_RES_SHIFT          4
#define   D1_ANA_CDR_06_REG_CDR_CP_IN                  0x0F  /* [3:0] Analog CDR 's charge pump current selection: */
#define   D1_ANA_CDR_06_REG_CDR_CP_IN_SHIFT            0
#define   D1_ANA_CDR_06_REG_CDR_CP_IN_16_5U            0x05  /* 0101 */
#define D1_ANA_CDR_07    0x7A32
#define D1_ANA_CDR_08    0x7A33
#define D1_ANA_CDR_09    0x7A34
#define D1_ANA_CDR_10    0x7A35
#define   D1_ANA_CDR_10_REG_EN_WIDE_TEMP               0x80  /* [7] en_wide_temp */
#define D1_ANA_CDR_14    0x7A39
#define D1_TMDS_RESET    0x7A3A
#define   D1_TMDS_RESET_REG_RST_N_L3                   0x80  /* [7] Reset N of digital */
#define   D1_TMDS_RESET_REG_RST_N_L2                   0x40  /* [6] Reset N of digital */
#define   D1_TMDS_RESET_REG_RST_N_L1                   0x20  /* [5] Reset N of digital */
#define   D1_TMDS_RESET_REG_RST_N_L0                   0x10  /* [4] Reset N of digital */
#define D1_ST_01         0x7A44
#define   D1_ST_01_REG_RATE_SEL_300M_150M              0x20  /* 001 */
#define   D1_ST_01_REG_RESERVED                        0x10  /* [4] reg_en_chargeshare_op */

/* --- D1 GDI DFE (Page 7A) --- */
#define D1_L0_DFE_EN_1   0x7AA0
#define   D1_L0_DFE_EN_1_TIMER_CTRL_EN_LANE0           0x40  /* [6] Enable the timer to control adaptation-loop */
#define   D1_L0_DFE_EN_1_REDUCE_ADAPT_GAIN_LANE0       0x30  /* [5:4] Reduce the all adaptation-loop's gain after settling */
#define   D1_L0_DFE_EN_1_REDUCE_ADAPT_GAIN_LANE0_SHIFT 4
#define   D1_L0_DFE_EN_1_RL_THRESHOLD_LANE0            0x03  /* [1:0] The threshold of run-length detection */
#define   D1_L0_DFE_EN_1_RL_THRESHOLD_LANE0_SHIFT      0
#define D1_L0_DFE_EN_2   0x7AA1
#define D1_L0_LIMIT_INIT 0x7AA2
#define   D1_L0_LIMIT_INIT_LE_MIN_LANE0                0xF0  /* [7:4] Minimum of LE (-0 ~ -31) */
#define   D1_L0_LIMIT_INIT_LE_MIN_LANE0_SHIFT          4
#define D1_L0_INIT_1     0x7AA3
#define   D1_L0_INIT_1_LE_MIN_LANE0                    0x80  /* [7] Minimum of LE (-0 ~ -31) */
#define   D1_L0_INIT_1_LE_INIT_LANE0                   0x40  /* [6] Initial value of LE coefficient, effective only on mode 0/2 (-0~-31) */
#define   D1_L0_INIT_1_SERVO_INIT_LANE0                0x1F  /* [4:0] Initial value of servo-loop coefficient (bit[7:0] : +0~+31) */
#define   D1_L0_INIT_1_SERVO_INIT_LANE0_SHIFT          0
#define D1_L0_INIT_2     0x7AA4
#define D1_L0_INIT_3     0x7AA5
#define D1_L0_INIT_4     0x7AA6
#define D1_L0_INIT_7     0x7AA9
#define D1_L0_INIT_8     0x7AAA
#define D1_L0_GDI_DE     0x7AAF
#define   D1_L0_GDI_DE_DE_BRANK_EN_LANE0               0x01  /* [0] Enable brank function of gdi_de */
#define D1_L1_DFE_EN_1   0x7AB0
#define D1_L1_GDI_DE     0x7ABF
#define   D1_L1_GDI_DE_DE_BRANK_EN_LANE1               0x01  /* [0] Enable brank function of gdi_de */
#define D1_L2_GDI_DE     0x7ACF
#define   D1_L2_GDI_DE_DE_BRANK_EN_LANE2               0x01  /* [0] Enable brank function of gdi_de */
#define D1_L3_GDI_DE     0x7ADF
#define   D1_L3_GDI_DE_DE_BRANK_EN_LANE3               0x01  /* [0] Enable brank function of gdi_de */
#define D1_MODE_TIMER    0x7AE0
#define   D1_MODE_TIMER_ADAPT_MODE                     0xC0  /* [7:6] 00 : Edge-based adaptation only with LE loop. */
#define   D1_MODE_TIMER_ADAPT_MODE_SHIFT               6
#define   D1_MODE_TIMER_EDGE_DET_MODE                  0x20  /* [5] Edge lead or lag data */
#define   D1_MODE_TIMER_TRANSITION_ONLY                0x10  /* [4] Only transition bit is effective at Tap2~4 */
#define   D1_MODE_TIMER_TAP1_DELAY                     0x07  /* [2:0] Delay time of tap1+LE loop after turning on tap0 */
#define   D1_MODE_TIMER_TAP1_DELAY_SHIFT               0
#define D1_TIMER         0x7AE1
#define   D1_TIMER_SERVO_DELAY                         0x03  /* [1:0] Delay time of servo-loop after turning on tap0 */
#define   D1_TIMER_SERVO_DELAY_SHIFT                   0
#define D1_GAIN_1        0x7AE2
#define   D1_GAIN_1_TAP0_GAIN                          0xE0  /* [7:5] Filter's gain of Tap0 */
#define   D1_GAIN_1_TAP0_GAIN_SHIFT                    5
#define   D1_GAIN_1_TAP1_GAIN                          0x1C  /* [4:2] Filter's gain of Tap1 */
#define   D1_GAIN_1_TAP1_GAIN_SHIFT                    2
#define D1_GAIN_2        0x7AE3
#define   D1_GAIN_2_TAP2_GAIN                          0xE0  /* [7:5] Filter's gain of Tap2 */
#define   D1_GAIN_2_TAP2_GAIN_SHIFT                    5
#define   D1_GAIN_2_TAP3_GAIN                          0x1C  /* [4:2] Filter's gain of Tap3 */
#define   D1_GAIN_2_TAP3_GAIN_SHIFT                    2
#define D1_GAIN_3        0x7AE4
#define   D1_GAIN_3_TAP4_GAIN                          0xE0  /* [7:5] Filter's gain of Tap4 */
#define   D1_GAIN_3_TAP4_GAIN_SHIFT                    5
#define   D1_GAIN_3_SERVO_GAIN                         0x1C  /* [4:2] Filter's gain of servo-loop */
#define   D1_GAIN_3_SERVO_GAIN_SHIFT                   2
#define D1_GAIN_4        0x7AE5
#define   D1_GAIN_4_LE_GAIN1                           0xE0  /* [7:5] Filter's gain of LE1, effective only on mode 0/2 */
#define   D1_GAIN_4_LE_GAIN1_SHIFT                     5
#define   D1_GAIN_4_LE_GAIN2                           0x1C  /* [4:2] Filter's gain of LE2, effective only on mode 2 */
#define   D1_GAIN_4_LE_GAIN2_SHIFT                     2
#define D1_LIMIT_1       0x7AE6
#define   D1_LIMIT_1_VTH_THRESHOLD                     0xE0  /* [7:5] Increase Vth's coefficient when Vth's coefficient less than the vth... */
#define   D1_LIMIT_1_VTH_THRESHOLD_SHIFT               5
#define   D1_LIMIT_1_TAP0_MAX                          0x1F  /* [4:0] Maximum of Tap0 (+0.5 ~ +31.5) */
#define   D1_LIMIT_1_TAP0_MAX_SHIFT                    0
#define D1_LIMIT_2       0x7AE7
#define   D1_LIMIT_2_TAP1_MIN                          0x3F  /* [5:0] Minimum of Tap1 (-0.5 ~ -63.5) */
#define   D1_LIMIT_2_TAP1_MIN_SHIFT                    0
#define D1_LIMIT_3       0x7AE8
#define   D1_LIMIT_3_TAP2_MAX                          0xF8  /* [7:3] Maximum of Tap2 (+0.5 ~ +31.5) */
#define   D1_LIMIT_3_TAP2_MAX_SHIFT                    3
#define   D1_LIMIT_3_TAP2_MIN                          0x07  /* [2:0] Minimum of Tap2 (-0.5 ~ -31.5) */
#define   D1_LIMIT_3_TAP2_MIN_SHIFT                    0
#define D1_LIMIT_4       0x7AE9
#define   D1_LIMIT_4_TAP2_MIN                          0xC0  /* [7:6] Minimum of Tap2 (-0.5 ~ -31.5) */
#define   D1_LIMIT_4_TAP2_MIN_SHIFT                    6
#define D1_LIMIT_5       0x7AEA
#define   D1_LIMIT_5_TAP3_MIN                          0xF8  /* [7:3] Minimum of Tap3 (-0.5 ~ -31.5) */
#define   D1_LIMIT_5_TAP3_MIN_SHIFT                    3
#define D1_LIMIT_6       0x7AEB
#define   D1_LIMIT_6_TAP4_MIN                          0x1F  /* [4:0] Minimum of Tap4 (-0.5 ~ -31.5) */
#define   D1_LIMIT_6_TAP4_MIN_SHIFT                    0
#define D1_LOOP_DIV_1    0x7AEC
#define   D1_LOOP_DIV_1_SERVO_DIVISOR                  0xFC  /* [7:2] Servo-loop's divider of symbol clock (2~63) */
#define   D1_LOOP_DIV_1_SERVO_DIVISOR_SHIFT            2
#define   D1_LOOP_DIV_1_VTH_DIVISOR                    0x03  /* [1:0] Vth-adjust's divider of tap clock (1~15) */
#define   D1_LOOP_DIV_1_VTH_DIVISOR_SHIFT              0
#define D1_LOOP_DIV_2    0x7AED
#define   D1_LOOP_DIV_2_VTH_DIVISOR                    0xC0  /* [7:6] Vth-adjust's divider of tap clock (1~15) */
#define   D1_LOOP_DIV_2_VTH_DIVISOR_SHIFT              6
#define   D1_LOOP_DIV_2_TAP_DIVISOR                    0x3F  /* [5:0] Tap-loop's divider of symbol clock (2~63) */
#define   D1_LOOP_DIV_2_TAP_DIVISOR_SHIFT              0
#define D1_RESET_1       0x7AEF
#define   D1_RESET_1_TAP0_THRESHOLD                    0xF0  /* [7:4] Increase Vth's coefficient when Vth's coefficient less than the vth... */
#define   D1_RESET_1_TAP0_THRESHOLD_SHIFT              4
#define   D1_RESET_1_DFE_ADAPT_RSTB_LANE3              0x08  /* [3] Reset adaptation loop of lane3 (0 : reset, 1 : normal) */
#define   D1_RESET_1_DFE_ADAPT_RSTB_LANE2              0x04  /* [2] Reset adaptation loop of lane2 (0 : reset, 1 : normal) */
#define   D1_RESET_1_DFE_ADAPT_RSTB_LANE1              0x02  /* [1] Reset adaptation loop of lane1 (0 : reset, 1 : normal) */
#define   D1_RESET_1_DFE_ADAPT_RSTB_LANE0              0x01  /* [0] Reset adaptation loop of lane0 (0 : reset, 1 : normal) */
#define D1_COEF_1        0x7AF0
#define D1_COEF_2        0x7AF1
#define D1_COEF_3        0x7AF2
#define   D1_COEF_3_LANE_SEL_SHIFT                     6
#define   D1_COEF_3_RECORD_LIMIT_EN                    0x01  /* [0] Enable to record max/min value of Tap0/Tap1/LE */
#define D1_COEF_4        0x7AF3
#define D1_GRAY_DEC_2    0x7AF8
#define D1_LIMIT_10      0x7AF9
#define   D1_LIMIT_10_VTH_MAX                          0xF0  /* [7:4] Maximum of Vth coefficient (0 ~ 15) */
#define   D1_LIMIT_10_VTH_MAX_SHIFT                    4
#define   D1_LIMIT_10_VTH_MIN                          0x07  /* [2:0] Minimum of Vth coefficient (0 ~ 7) */
#define   D1_LIMIT_10_VTH_MIN_SHIFT                    0
#define D1_GAIN_PREC     0x7AFA
#define   D1_GAIN_PREC_LE_GAIN_PREC                    0x07  /* [2:0] Filter's precursor gain of LE, effective only on mode 2 */
#define   D1_GAIN_PREC_LE_GAIN_PREC_SHIFT              0

#endif /* RTD2556_REGS_PAGE_7A_H */
