/* RTD2556 scaler page 0x77 */
#ifndef RTD2556_REGS_PAGE_77_H
#define RTD2556_REGS_PAGE_77_H

/* --- D3 MHL2.0 MAC (Page 77) --- */
#define MHL_CTRL_00 0x77A0
#define   MHL_CTRL_00_PN_SWAP   0x20  /* [5] Revert CDR Data Polarity */
#define   MHL_CTRL_00_MHL_EN    0x01  /* [0] MHL Function Enable */
#define MHL_CTRL_12 0x77B2
#define   MHL_CTRL_12_GB_REF_EN 0x80  /* [7] Guard Band Reference Enable for Data Remapping. */

#endif /* RTD2556_REGS_PAGE_77_H */
