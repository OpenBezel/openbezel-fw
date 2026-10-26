/* RTD2556 scaler page 0x16 */
#ifndef RTD2556_REGS_PAGE_16_H
#define RTD2556_REGS_PAGE_16_H

/* --- On-line Measure Block Overview (Page 16) --- */
#define P16_MEAS_CTL0            0x1601
#define   P16_MEAS_CTL0_TIME_CLK_SEL                           0xC0  /* [7:6] Time Clock Select */
#define   P16_MEAS_CTL0_TIME_CLK_SEL_SHIFT                     6
#define P16_MEAS_CTL1            0x1602
#define   P16_MEAS_CTL1_MEAS_CONTI_START                       0x20  /* [5] Start Continues HS & VS Period / H & V Resolution & Polarity Measur... */
#define   P16_MEAS_CTL1_MEAS_START                             0x01  /* [0] Start One Time HS & VS Period / H & V Resolution & Polarity Measure... */
#define P16_MEAS_HS_VS_DELTA_CTL 0x1615
#define   P16_MEAS_HS_VS_DELTA_CTL_HS_DELTA_SEL_16_CLOCK       0xA0  /* 101 */
#define   P16_MEAS_HS_VS_DELTA_CTL_VS_DELTA_SEL_4_HSYNC        0x18  /* 11 */
#define   P16_MEAS_HS_VS_DELTA_CTL_HS_PULSE_DELTA_SEL_16_CLOCK 0x05  /* 101 */
#define MEAS_HS_VS_DELTA_WD      0x1617
#define   MEAS_HS_VS_DELTA_WD_HS_OVER_RANGE_WD_EN              0x04  /* [2] Enable WD When Input HSYNC Changed */
#define   MEAS_HS_VS_DELTA_WD_VS_OVER_RANGE_WD_EN              0x02  /* [1] Enable WD When Input VSYNC Changed */
#define   MEAS_HS_VS_DELTA_WD_HS_PULSE_OVER_RANGE_WD_EN        0x01  /* [0] Enable WD When Input H-Sync Pulse Changed */
#define MEAS_HS_VS_ERROR_WD      0x1618
#define   MEAS_HS_VS_ERROR_WD_VS_OVERFLOW_A_WD_EN              0x20  /* [5] Enable WD When V-Sync Measure Counter Overflow Occurred for Analog ... */
#define   MEAS_HS_VS_ERROR_WD_HS_OVERFLOW_A_WD_EN              0x10  /* [4] Enable WD When H-Sync Measure Counter Overflow Occurred for Analog ... */
#define M1_MEASURE_DUMMY0        0x161C
#define   M1_MEASURE_DUMMY0_VS_TIME_OUT_WD_EN                  0x80  /* [7] Enable WD When V-Sync Timeout Detected */

#endif /* RTD2556_REGS_PAGE_16_H */
