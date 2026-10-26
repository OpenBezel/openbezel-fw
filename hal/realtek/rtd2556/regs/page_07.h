/* RTD2556 scaler page 0x07 */
#ifndef RTD2556_REGS_PAGE_07_H
#define RTD2556_REGS_PAGE_07_H

/* --- DCR Part1 (Page 7) --- */
#define DCR_ADDRESS_PORT 0x07D8
#define   DCR_ADDRESS_PORT_DCR_ADDR_SHIFT 2
#define   DCR_ADDRESS_PORT_RESULT_READ    0x02  /* [1] 0: Disable Read to refresh measure result. */
#define   DCR_ADDRESS_PORT_MEASURE_START  0x01  /* [0] 0: Finish or disable */
#define DCR_DATA_PORT    0x07D9

#endif /* RTD2556_REGS_PAGE_07_H */
