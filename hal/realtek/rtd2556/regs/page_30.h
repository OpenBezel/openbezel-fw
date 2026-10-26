/* RTD2556 scaler page 0x30 */
#ifndef RTD2556_REGS_PAGE_30_H
#define RTD2556_REGS_PAGE_30_H

/* --- Off-line Measure Block Overview (Page 30) --- */
#define P30_SYNC_SELECT        0x3000
#define   P30_SYNC_SELECT_CLK_PATH_DIV2       0x80  /* [7] Input Clock Path Divider 2 */
#define   P30_SYNC_SELECT_MEAS_SOURCE         0x0F  /* [3:0] Measure HSYNC/VSYNC Source Select: */
#define   P30_SYNC_SELECT_MEAS_SOURCE_SHIFT   0
#define   P30_SYNC_SELECT_MEAS_SOURCE_D1      0x01  /* 0001 */
#define P30_MEAS_CTL0          0x3001
#define   P30_MEAS_CTL0_TIME_CLK_SEL          0xC0  /* [7:6] Time Clock Select */
#define   P30_MEAS_CTL0_TIME_CLK_SEL_SHIFT    6
#define   P30_MEAS_CTL0_MEAS_HS_DEN_SWAP      0x10  /* [4] HS/ENA Source Swap for Measurement */
#define   P30_MEAS_CTL0_VSMS_INV              0x08  /* [3] VSYNC Measure Invert Enable */
#define   P30_MEAS_CTL0_OFF_MEAS_PWR          0x01  /* [0] Off Line Measure Power Control */
#define P30_MEAS_CTL1          0x3002
#define   P30_MEAS_CTL1_MEAS_POP_UP           0x40  /* [6] Pop Up Period Measurement Result */
#define   P30_MEAS_CTL1_MEAS_CONTI_START      0x20  /* [5] Start Continues HS & VS Period / H & V Resolution & Polarity Measur... */
#define   P30_MEAS_CTL1_MEAS_START            0x01  /* [0] Start One Time HS & VS Period / H & V Resolution & Polarity Measure... */
#define P30_MEAS_ERROR_FLG0    0x3003
#define   P30_MEAS_ERROR_FLG0_VS_TIME_OUT     0x80  /* [7] Time-Out Bit of Input VSYNC Period Measurement (No VSYNC Occurred) */
#define   P30_MEAS_ERROR_FLG0_MS_TIMEOUT_HALF 0x40  /* [6] Half Time-Out Bit of Input VSYNC Period Measurement */
#define   P30_MEAS_ERROR_FLG0_V_OVER_FLOW_A   0x20  /* [5] Over-flow Bit of Input VSYNC Period Measurement */
#define   P30_MEAS_ERROR_FLG0_H_OVER_FLOW_A   0x10  /* [4] Over-flow Bit of Input H-SYNC Period Measurement */
#define P30_MEAS_VS_PERIOD_A_M 0x3008
#define   P30_MEAS_VS_PERIOD_A_M_VS_PERIOD_A  0x0F  /* [3:0] Input VSYNC Period Measurement Result: High Byte[11:8] */
#define P30_MEAS_VS_PERIOD_A_L 0x3009
#define P30_MEAS_HS_PERIOD_D_M 0x300E
#define   P30_MEAS_HS_PERIOD_D_M_HS_PERIOD_D  0x1F  /* [4:0] Input HSYNC Period Measurement Result: High Byte[12:8] */
#define P30_MEAS_HS_PERIOD_D_L 0x300F

#endif /* RTD2556_REGS_PAGE_30_H */
