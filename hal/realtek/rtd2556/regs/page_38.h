/* RTD2556 scaler page 0x38 */
#ifndef RTD2556_REGS_PAGE_38_H
#define RTD2556_REGS_PAGE_38_H

/* --- LVDS Combo DisplayPort Tx Digital PHY CTRL (Page 38) --- */
#define DPTX_PN_SWAP1     0x3880
#define   DPTX_PN_SWAP1_DPTX_PN_SWAP              0x0F  /* [3:0] Enable DPTx Main Link P/N Pin Swap. */
#define DPTX_LANE_SWAP3_2 0x3883
#define   DPTX_LANE_SWAP3_2_LANE3_SOURCE          0x30  /* [5:4] The Source Selector of Lane3 to analog PHY. */
#define   DPTX_LANE_SWAP3_2_LANE3_SOURCE_SHIFT    4
#define   DPTX_LANE_SWAP3_2_LANE2_SOURCE          0x03  /* [1:0] The Source Selector of Lane2 to analog PHY. */
#define   DPTX_LANE_SWAP3_2_LANE2_SOURCE_SHIFT    0
#define DPTX_LANE_SWAP1_0 0x3884
#define   DPTX_LANE_SWAP1_0_LANE1_SOURCE          0x30  /* [5:4] The Source Selector of Lane1 to analog PHY. */
#define   DPTX_LANE_SWAP1_0_LANE1_SOURCE_SHIFT    4
#define   DPTX_LANE_SWAP1_0_LANE0_SOURCE          0x03  /* [1:0] The Source Selector of Lane0 to analog PHY. */
#define   DPTX_LANE_SWAP1_0_LANE0_SOURCE_SHIFT    0
#define PHY0_TXBIST_00_H2 0x3885
#define   PHY0_TXBIST_00_H2_REG_BIST_REPEAT       0x80  /* [7] 1: Repeat bist pattern */
#define   PHY0_TXBIST_00_H2_REG_BIST_TP_SEL       0x40  /* [6] 1: Training pattern selected */
#define   PHY0_TXBIST_00_H2_REG_DELAY_SEL0        0x38  /* [5:3] LANE0 Delay Stage Number */
#define   PHY0_TXBIST_00_H2_REG_DELAY_SEL1        0x07  /* [2:0] LANE1 Delay Stage Number */
#define   PHY0_TXBIST_00_H2_REG_DELAY_SEL1_SHIFT  0
#define PHY0_TXBIST_00_H1 0x3886
#define   PHY0_TXBIST_00_H1_REG_DELAY_SEL2        0xE0  /* [7:5] LANE2 Delay Stage Number */
#define   PHY0_TXBIST_00_H1_REG_DELAY_SEL2_SHIFT  5
#define   PHY0_TXBIST_00_H1_REG_DELAY_SEL3        0x1C  /* [4:2] LANE3 Delay Stage Number */
#define   PHY0_TXBIST_00_H1_REG_DELAY_SEL3_SHIFT  2
#define PHY0_TXBIST_00_L1 0x3888
#define   PHY0_TXBIST_00_L1_REG_TP_TYPE_SEL       0x20  /* [5] 1: TP1 transmited */
#define   PHY0_TXBIST_00_L1_REG_SCRAMBLE_DEFAULT  0x10  /* [4] 1: Scramble default value = 16'hffff */
#define   PHY0_TXBIST_00_L1_REG_TXBIST_MODE       0x0C  /* [3:2] Tx Bist Mode Select: */
#define   PHY0_TXBIST_00_L1_REG_TXBIST_MODE_SHIFT 2
#define   PHY0_TXBIST_00_L1_REG_PRBS_SEL          0x02  /* [1] PRBS Pattern Select: */
#define   PHY0_TXBIST_00_L1_REG_X1X2_SEL          0x01  /* [0] Interface Clock Rate Selection */
#define PHY0_TXBIST_01_H2 0x3889
#define   PHY0_TXBIST_01_H2_REG_SCRAMBLE_MODE     0x40  /* [6] 0: vby1 */
#define   PHY0_TXBIST_01_H2_REG_VBY1_EDP_MODE     0x20  /* [5] 0: vby1 */
#define   PHY0_TXBIST_01_H2_REG_D2A_CLKINV        0x10  /* [4] The sampling clock between digital PHY and analog PHY, Note that do... */
#define   PHY0_TXBIST_01_H2_REG_ENHANCE_MODE      0x08  /* [3] Enhance Control Mode */
#define   PHY0_TXBIST_01_H2_REG_SCRAMBLE_DIS      0x02  /* [1] Disable Scrambling */
#define PHY0_TXBIST_02_H1 0x388E
#define   PHY0_TXBIST_02_H1_REG_LANE_ENABLE       0x0F  /* [3:0] 1'b1: Enable Lane X, 1'b0: Disable Lane X, X = 3 - 0 */
#define PHY_RST           0x3891
#define   PHY_RST_VBY1_DPHY_RST_N                 0x01  /* [0] Vby1 Digital Phy Reset Control */

/* --- LVDS Combo DisplayPort Tx Analog PHY CTRL SSC (Page 38) --- */
#define DPTX_CMU4         0x38D4
#define   DPTX_CMU4_PLL_WD_CKRDY                  0x80  /* [7] PLL Clock Ready */
#define DPTX_CMU5         0x38D5
#define   DPTX_CMU5_EN_SSC                        0x20  /* [5] CMU SSC Or Jitter Generation Mode Enable. */
#define   DPTX_CMU5_ORDER                         0x10  /* [4] Sigma-Delta Modulator Order Select. */
#define   DPTX_CMU5_SEL_MODE                      0x08  /* [3] Triangular Wave Or Square Wave Select in Jitter Generation Mode. */
#define DPTX_CMU12        0x38DD
#define   DPTX_CMU12_N_F_AUTO_DB                  0x80  /* [7] Double Buffer N.F CODE. */
#define   DPTX_CMU12_N_CODE                       0x1F  /* [4:0] N Code of CMU Divider. */
#define   DPTX_CMU12_N_CODE_SHIFT                 0
#define DPTX_CMU13        0x38DE
#define DPTX_CMU14        0x38DF
#define DPTX_CMU16        0x38E1
#define DPTX_CMU17        0x38E2
#define DPTX_CMU19        0x38E4

#endif /* RTD2556_REGS_PAGE_38_H */
