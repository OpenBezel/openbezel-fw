/* RTD2556 scaler page 0x55 */
#ifndef RTD2556_REGS_PAGE_55_H
#define RTD2556_REGS_PAGE_55_H

/* --- TXCD Port Control for Analog LVDS (Page 55) --- */
#define P55_LVDS_COMMON_AB_CTRL0     0x5500
#define   P55_LVDS_COMMON_AB_CTRL0_REG_AB_IBGEN_POWL         0x80  /* [7] IBGEN power: */
#define   P55_LVDS_COMMON_AB_CTRL0_REG_TXA_POWL              0x40  /* [6] PORT A power */
#define   P55_LVDS_COMMON_AB_CTRL0_REG_TXB_POWL              0x20  /* [5] PORT B power */
#define   P55_LVDS_COMMON_AB_CTRL0_REG_CMU_EN                0x08  /* [3] PLL power */
#define P55_LVDS_COMMON_AB_CTRL1     0x5501
#define   P55_LVDS_COMMON_AB_CTRL1_REG_AB_VCMSEL             0x1F  /* [4:0] TX driver common-mode voltage(VCM) selections: */
#define   P55_LVDS_COMMON_AB_CTRL1_REG_AB_VCMSEL_SHIFT       0
#define P55_LVDS_COMMON_AB_CTRL2     0x5502
#define   P55_LVDS_COMMON_AB_CTRL2_REG_AB_ISEL               0x0F  /* [3:0] LVDS TX driver current selections: */
#define   P55_LVDS_COMMON_AB_CTRL2_REG_AB_ISEL_SHIFT         0
#define P55_LVDS_COMMON_AB_CTRL3     0x5503
#define   P55_LVDS_COMMON_AB_CTRL3_REG_CK1X_PI_FREQ          0x60  /* [6:5] PORT A,B phase interpolator (AB_PI) operation frequency selections: */
#define   P55_LVDS_COMMON_AB_CTRL3_REG_CK1X_PI_FREQ_SHIFT    5
#define   P55_LVDS_COMMON_AB_CTRL3_REG_CKPIXEL_INV           0x10  /* [4] CK_PIXEL clock sample data with */
#define   P55_LVDS_COMMON_AB_CTRL3_REG_CMU_BIG_KVCO          0x04  /* [2] Kvco selections: */
#define P55_LVDS_COMMON_AB_CTRL13    0x550D
#define   P55_LVDS_COMMON_AB_CTRL13_REG_CMU_SEL_R1           0xE0  /* [7:5] PLL LPF serial resistor */
#define   P55_LVDS_COMMON_AB_CTRL13_REG_CMU_SEL_R1_SHIFT     5
#define   P55_LVDS_COMMON_AB_CTRL13_REG_CMU_SEL_CP           0x10  /* [4] PLL LPF parallel capacitor */
#define   P55_LVDS_COMMON_AB_CTRL13_REG_CMU_SEL_PUMP_I       0x0F  /* [3:0] PLL charge pump current */
#define   P55_LVDS_COMMON_AB_CTRL13_REG_CMU_SEL_PUMP_I_SHIFT 0
#define P55_LVDS_PORTA_CTRL6         0x5526
#define   P55_LVDS_PORTA_CTRL6_REG_TXA_PULLDN                0x80  /* [7] Port A weakly pull low: */
#define P55_LVDS_PORTB_CTRL6         0x5536
#define   P55_LVDS_PORTB_CTRL6_REG_TXB_PULLDN                0x80  /* [7] Port B weakly pull low: */

/* --- LVDS Combo DisplayPort Tx Analog PHY CTRL (Page 55) --- */
#define DPTX_PREEM_TXA0              0x5550
#define DPTX_PREEM_TXA1              0x5551
#define DPTX_PREEM_TXB3              0x5552
#define DPTX_PREEM_TXB4              0x5553
#define DPTX_PRE_DRIVER_COMPENSATION 0x5554
#define   DPTX_PRE_DRIVER_COMPENSATION_REG_TXA0_BOOST        0xC0  /* [7:6] TX Pre-driver compensation: */
#define   DPTX_PRE_DRIVER_COMPENSATION_REG_TXA0_BOOST_SHIFT  6
#define   DPTX_PRE_DRIVER_COMPENSATION_REG_TXA1_BOOST        0x30  /* [5:4] TX Pre-driver compensation: */
#define   DPTX_PRE_DRIVER_COMPENSATION_REG_TXA1_BOOST_SHIFT  4
#define   DPTX_PRE_DRIVER_COMPENSATION_REG_TXB3_BOOST        0x0C  /* [3:2] TX Pre-driver compensation: */
#define   DPTX_PRE_DRIVER_COMPENSATION_REG_TXB3_BOOST_SHIFT  2
#define   DPTX_PRE_DRIVER_COMPENSATION_REG_TXB4_BOOST        0x03  /* [1:0] TX Pre-driver compensation: */
#define   DPTX_PRE_DRIVER_COMPENSATION_REG_TXB4_BOOST_SHIFT  0
#define DPTX_CKDELAY                 0x5555
#define   DPTX_CKDELAY_REG_TXA0_CKSYN_DLY                    0xC0  /* [7:6] Delay time of CKSYN Enable signal for DPTX Lane. (Unit in CKRD cycle.) */
#define   DPTX_CKDELAY_REG_TXA0_CKSYN_DLY_SHIFT              6
#define   DPTX_CKDELAY_REG_TXA1_CKSYN_DLY                    0x30  /* [5:4] Delay time of CKSYN Enable signal for DPTX Lane. (Unit in CKRD cycle.) */
#define   DPTX_CKDELAY_REG_TXA1_CKSYN_DLY_SHIFT              4
#define   DPTX_CKDELAY_REG_TXB3_CKSYN_DLY                    0x0C  /* [3:2] Delay time of CKSYN Enable signal for DPTX Lane. (Unit in CKRD cycle.) */
#define   DPTX_CKDELAY_REG_TXB3_CKSYN_DLY_SHIFT              2
#define   DPTX_CKDELAY_REG_TXB4_CKSYN_DLY                    0x03  /* [1:0] Delay time of CKSYN Enable signal for DPTX Lane. (Unit in CKRD cycle.) */
#define   DPTX_CKDELAY_REG_TXB4_CKSYN_DLY_SHIFT              0
#define DPTX_DOUBLE_HALF_CURRENT     0x5556
#define   DPTX_DOUBLE_HALF_CURRENT_REG_TXA0_IB2X_CML         0x80  /* [7] DPTX Pre-driver double current: */
#define   DPTX_DOUBLE_HALF_CURRENT_REG_TXA1_IB2X_CML         0x40  /* [6] DPTX Pre-driver double current: */
#define   DPTX_DOUBLE_HALF_CURRENT_REG_TXB3_IB2X_CML         0x20  /* [5] DPTX Pre-driver double current: */
#define   DPTX_DOUBLE_HALF_CURRENT_REG_TXB4_IB2X_CML         0x10  /* [4] DPTX Pre-driver double current: */
#define   DPTX_DOUBLE_HALF_CURRENT_REG_TXB_IB2X_DP           0x03  /* [1:0] PortB 4/3 DP TX lane current 2x: */
#define   DPTX_DOUBLE_HALF_CURRENT_REG_TXB_IB2X_DP_SHIFT     0
#define DPTX_SWING_CURRENT_1         0x5557
#define   DPTX_SWING_CURRENT_1_REG_TXA0_ISEL_COMBO           0xF0  /* [7:4] DP/LVDS TX LANE current select independently: */
#define   DPTX_SWING_CURRENT_1_REG_TXA0_ISEL_COMBO_SHIFT     4
#define   DPTX_SWING_CURRENT_1_REG_TXA1_ISEL_COMBO           0x0F  /* [3:0] DP/LVDS TX LANE current select independently: */
#define   DPTX_SWING_CURRENT_1_REG_TXA1_ISEL_COMBO_SHIFT     0
#define DPTX_SWING_CURRENT_2         0x5558
#define   DPTX_SWING_CURRENT_2_REG_TXB3_ISEL_COMBO           0xF0  /* [7:4] DP/LVDS TX LANE current select independently: */
#define   DPTX_SWING_CURRENT_2_REG_TXB3_ISEL_COMBO_SHIFT     4
#define   DPTX_SWING_CURRENT_2_REG_TXB4_ISEL_COMBO           0x0F  /* [3:0] DP/LVDS TX LANE current select independently: */
#define   DPTX_SWING_CURRENT_2_REG_TXB4_ISEL_COMBO_SHIFT     0
#define DPTX_Z0_TX_1                 0x5561
#define   DPTX_Z0_TX_1_REG_TXA0_RO_COMBO                     0x08  /* [3] PORT A DP/LVDS TX termination selections: */
#define   DPTX_Z0_TX_1_REG_TXA1_RO_COMBO                     0x04  /* [2] PORT A DP/LVDS TX termination selections: */
#define   DPTX_Z0_TX_1_REG_TXB3_RO_COMBO                     0x02  /* [1] PORT B DP/LVDS TX termination selections: */
#define   DPTX_Z0_TX_1_REG_TXB4_RO_COMBO                     0x01  /* [0] PORT B DP/LVDS TX termination selections: */
#define DPTX_Z0_TX_2                 0x5562
#define   DPTX_Z0_TX_2_REG_TXA0_RO_COMBO                     0xF0  /* [7:4] PORT A DP/LVDS TX termination selections: */
#define   DPTX_Z0_TX_2_REG_TXA0_RO_COMBO_SHIFT               4
#define   DPTX_Z0_TX_2_REG_TXA1_RO_COMBO                     0x0F  /* [3:0] PORT A DP/LVDS TX termination selections: */
#define   DPTX_Z0_TX_2_REG_TXA1_RO_COMBO_SHIFT               0
#define DPTX_Z0_TX_3                 0x5563
#define   DPTX_Z0_TX_3_REG_TXB3_RO_COMBO                     0xF0  /* [7:4] PORT B DP/LVDS TX termination selections: */
#define   DPTX_Z0_TX_3_REG_TXB3_RO_COMBO_SHIFT               4
#define   DPTX_Z0_TX_3_REG_TXB4_RO_COMBO                     0x0F  /* [3:0] PORT B DP/LVDS TX termination selections: */
#define   DPTX_Z0_TX_3_REG_TXB4_RO_COMBO_SHIFT               0
#define DPTX_CMU0                    0x5570
#define   DPTX_CMU0_REG_CMU_PI_I_SEL                         0x07  /* [2:0] Current settings of CMU's PI. */
#define   DPTX_CMU0_REG_CMU_PI_I_SEL_SHIFT                   0
#define DPTX_CMU3                    0x5573
#define   DPTX_CMU3_REG_DPTX_LDO_POWL                        0x40  /* [6] DPTX LDO POWER */
#define   DPTX_CMU3_REG_DPTX_LDO_VSEL_2_0V                   0x30  /* 11 */
#define   DPTX_CMU3_REG_CMU_MODE                             0x01  /* [0] CMU Mode selection: */

#endif /* RTD2556_REGS_PAGE_55_H */
