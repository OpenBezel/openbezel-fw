/* RTD2556 scaler page 0x40 */
#ifndef RTD2556_REGS_PAGE_40_H
#define RTD2556_REGS_PAGE_40_H

/* --- Sync Processor Clock Divider (Page 40) --- */
#define SYNCPROC_CLK_DIVIDER 0x4006
#define   SYNCPROC_CLK_DIVIDER_SYNC_CLK_SEL             0x10  /* [4] Syncprocess Clock Select */
#define   SYNCPROC_CLK_DIVIDER_SYNC_CLK_DIV_M2PLL       0x0F  /* [3:0] Divider of Fxtal * M2PLL_M / M2PLL_N Clock. Enable When Bit 4 Is Se... */
#define   SYNCPROC_CLK_DIVIDER_SYNC_CLK_DIV_M2PLL_SHIFT 0

/* --- Measure Block Clock Divider (Page 40) --- */
#define MEASURE_CLK_DIVIDER  0x4007
#define   MEASURE_CLK_DIVIDER_MS_CLK_SEL                0x10  /* [4] Measurement Clock Select */
#define   MEASURE_CLK_DIVIDER_MS_CLK_DIV                0x0F  /* [3:0] Divider of Fxtal * M2PLL_M / M2PLL_N Clock. Enable When Bit 4 Is Se... */
#define   MEASURE_CLK_DIVIDER_MS_CLK_DIV_SHIFT          0

#endif /* RTD2556_REGS_PAGE_40_H */
