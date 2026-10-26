/* RTD2556 scaler page 0xFD */
#ifndef RTD2556_REGS_PAGE_FD_H
#define RTD2556_REGS_PAGE_FD_H

/* --- Cache Controller (Page FD) --- */
#define CACHE_FUNCTION 0xFDA0
#define   CACHE_FUNCTION_CACHE_EN     0x01  /* [0] Enable Cache Function */
#define CACHE_OPTION   0xFDA1
#define   CACHE_OPTION_COMMON_PAGE_EN 0x02  /* [1] Common Page Mode Enable */
#define   CACHE_OPTION_CACHE_RESET    0x01  /* [0] Reset The Data of Cache, Any Data Byte Should Be Rewrite to Take Ef... */

#endif /* RTD2556_REGS_PAGE_FD_H */
