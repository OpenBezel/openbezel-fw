/* RTD2556 scaler page 0x05 */
#ifndef RTD2556_REGS_PAGE_05_H
#define RTD2556_REGS_PAGE_05_H

/* --- FIFO Control (Page 5) --- */
#define IN1_LINE_NUM_H 0x05B7
#define   IN1_LINE_NUM_H_IN1_BK_STA_SEL   0x40  /* [6] IN1 start point to change block_sel */
#define   IN1_LINE_NUM_H_IN1_LNUM_H       0x0F  /* [3:0] IN1_Line_Number [11:8] */
#define   IN1_LINE_NUM_H_IN1_LNUM_H_SHIFT 0
#define IN1_LINE_NUM_L 0x05B8
#define MN_PRRD_VST_L  0x05BD
#define MN_PXL_NUM_H   0x05BE
#define   MN_PXL_NUM_H_MN_PXL_NUM_H       0x0F  /* [3:0] MAIN_ PIXEL_NUM [11:8] */
#define MN_PXL_NUM_L   0x05BF
#define MN_LINE_NUM_H  0x05CD
#define   MN_LINE_NUM_H_MN_LNUM_H         0x0F  /* [3:0] MAIN_LINE_NUMBER [11:8] */
#define   MN_LINE_NUM_H_MN_LNUM_H_SHIFT   0
#define MN_LINE_NUM_L  0x05CE
#define MN_SDR_STATUS  0x05D0
#define   MN_SDR_STATUS_MN_DBWR_EN        0x10  /* [4] MAIN Double-Buffer Write Enable */
#define   MN_SDR_STATUS_MN_IN_FMT_30      0x02  /* [1] Input Format (Data format in SDR) */

#endif /* RTD2556_REGS_PAGE_05_H */
