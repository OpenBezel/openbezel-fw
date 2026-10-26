/* RTD2556 scaler page 0xB0 */
#ifndef RTD2556_REGS_PAGE_B0_H
#define RTD2556_REGS_PAGE_B0_H

/* --- D1 Port DisplayPort 1.2 Digital PHY(Page B0) --- */
#define D1_PHY_DIG_RESET_CTRL        0xB001
#define   D1_PHY_DIG_RESET_CTRL_XCLK_SEL          0x40  /* [6] 1 : use xclk to replace link clock */
#define   D1_PHY_DIG_RESET_CTRL_XCLK_POWON_EN     0x20  /* [5] used for saving power */
#define   D1_PHY_DIG_RESET_CTRL_LANE_NUM          0x18  /* [4:3] The number of lanes. */
#define   D1_PHY_DIG_RESET_CTRL_LANE_NUM_1_LANE   0x08  /* 01 */
#define   D1_PHY_DIG_RESET_CTRL_LANE_NUM_2_LANE   0x10  /* 10 */
#define   D1_PHY_DIG_RESET_CTRL_LANE_NUM_4_LANE   0x18  /* 11 */
#define   D1_PHY_DIG_RESET_CTRL_ENHANCE_MODE      0x04  /* [2] Enhanced mode of control symbols for framing. */
#define   D1_PHY_DIG_RESET_CTRL_PHY_DIG_RESET     0x02  /* [1] digital phy block reset (exclude hdcp block) */
#define   D1_PHY_DIG_RESET_CTRL_HDCP_CKT_RESET    0x01  /* [0] hdcp circuit reset */
#define D1_LANE_MUX                  0xB003
#define D1_SAMPLE_EDGE               0xB005
#define   D1_SAMPLE_EDGE_COMMA_DIS                0x10  /* [4] 1: disable comma detection */
#define   D1_SAMPLE_EDGE_SAMPLE                   0x0F  /* [3:0] analog to digital clock interface */
#define D1_DECODE_10B8B_ERROR        0xB006
#define D1_SCRAMBLE_CTRL             0xB007
#define   D1_SCRAMBLE_CTRL_ENHANCE_VOTE_EN        0x80  /* [7] use voting for judeging enhance mode */
#define   D1_SCRAMBLE_CTRL_SCRAMB_DIS             0x20  /* [5] 0 : enable scramble */
#define   D1_SCRAMBLE_CTRL_SCRAMB_VLD_LN3         0x08  /* [3] scramble is valid in ln3 */
#define   D1_SCRAMBLE_CTRL_SCRAMB_VLD_LN2         0x04  /* [2] scramble is valid in ln2 */
#define   D1_SCRAMBLE_CTRL_SCRAMB_VLD_LN1         0x02  /* [1] scramble is valid in ln1 */
#define   D1_SCRAMBLE_CTRL_SCRAMB_VLD_LN0         0x01  /* [0] scramble is valid in ln0 */
#define D1_BIST_PATTERN_SEL          0xB008
#define   D1_BIST_PATTERN_SEL_RXBIST_METHOD       0x07  /* [2:0] 000: no bist select, clear the error counter */
#define   D1_BIST_PATTERN_SEL_RXBIST_METHOD_SHIFT 0
#define D1_DESKEW_PHY                0xB00E
#define   D1_DESKEW_PHY_DESKEW_2LCLK_EN           0x80  /* [7] deskew 2 link clock enable */
#define   D1_DESKEW_PHY_INTRA_DESKEW_EN           0x40  /* [6] intra-lane deskew enable */
#define D1_EQ_CRC_1                  0xB012
#define   D1_EQ_CRC_1_EQ_CRC_EN                   0x80  /* [7] EQ CRC calculation enable */
#define   D1_EQ_CRC_1_EQ_CRC_SEL                  0x40  /* [6] 1: select boundary from D10.2 -> k28.5- */
#define   D1_EQ_CRC_1_EQ_CRC_CORRECT_LN3          0x08  /* [3] 1: LN3 EQ CRC value is match (only TP2, TP3) */
#define   D1_EQ_CRC_1_EQ_CRC_CORRECT_LN2          0x04  /* [2] 1: LN2 EQ CRC value is match(only TP2, TP3) */
#define   D1_EQ_CRC_1_EQ_CRC_CORRECT_LN1          0x02  /* [1] 1: LN1 EQ CRC value is match(only TP2, TP3) */
#define   D1_EQ_CRC_1_EQ_CRC_CORRECT_LN0          0x01  /* [0] 1: LN0 EQ CRC value is match(only TP2, TP3) */
#define D1_EQ_CRC_3                  0xB014
#define D1_PB0_DP_HDCP_CONTROL       0xB017
#define   D1_PB0_DP_HDCP_CONTROL_HDCP_EN          0x80  /* [7] HDCP FUNCTION ENABLE */
#define   D1_PB0_DP_HDCP_CONTROL_KEY_DL_EN        0x01  /* [0] Device Key Access Port download enable */
#define D1_PB0_DP_HDCP_DOWNLOAD_PORT 0xB018
#define D1_PB0_HDCP_IRQ              0xB01A
#define   D1_PB0_HDCP_IRQ_CP_IRQ_AUTO_SET         0x20  /* [5] '1' : auto set when r0_available */
#define   D1_PB0_HDCP_IRQ_R0_MANUAL_MODE          0x10  /* [4] '1':r0_available is control by firmware */
#define   D1_PB0_HDCP_IRQ_KM_CLK_SEL              0x08  /* [3] 0 : XCLK */
#define   D1_PB0_HDCP_IRQ_HDCP22_EN               0x02  /* [1] 0: HDCP 1.4 */
#define D1_HDCP_INTGT_VRF            0xB01B
#define   D1_HDCP_INTGT_VRF_INTGT_VRF_EN          0x80  /* [7] Link integrity enable. */
#define D1_PB0_HDCP_DEBUG            0xB020
#define   D1_PB0_HDCP_DEBUG_ENC_ENABLE            0x40  /* [6] Km add is done. After receving CPSR */
#define D1_DP_SIG_DET_0              0xB050
#define   D1_DP_SIG_DET_0_DP_SIG_DET_EN           0x80  /* [7] enable signal detection one time, */
#define   D1_DP_SIG_DET_0_DP_SIG_DET_DONE         0x40  /* [6] signal detection is done, clear when you clear DP_SIG_DET_EN = 0 */
#define   D1_DP_SIG_DET_0_LN_CK_SEL               0x20  /* [5] 0: select CK_REF_DP_OUT */
#define   D1_DP_SIG_DET_0_DP_XTAL_CYCLE           0x1F  /* [4:0] Select the counter cycles of Xtal */
#define   D1_DP_SIG_DET_0_DP_XTAL_CYCLE_SHIFT     0
#define D1_DP_SIG_DET_1              0xB051
#define D1_DP_SIG_DET_2              0xB052
#define D1_DP_SIG_DET_3              0xB053

/* --- D1 HDCP 2.2 Module (Page B0) --- */
#define D1_DUMMY3                    0xB096

#endif /* RTD2556_REGS_PAGE_B0_H */
