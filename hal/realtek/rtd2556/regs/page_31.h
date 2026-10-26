/* RTD2556 scaler page 0x31 */
#ifndef RTD2556_REGS_PAGE_31_H
#define RTD2556_REGS_PAGE_31_H

/* --- Format Conversion (Page 31) --- */
#define FC_INPUT_SEL_P01 0x3100
#define   FC_INPUT_SEL_P01_PATH0_INPUT       0xF0  /* [7:4] Source Index for Path 0: D0 As Default */
#define   FC_INPUT_SEL_P01_PATH0_INPUT_SHIFT 4

/* --- I Domain Double Buffer (Page 31) --- */
#define I_DB_CTRL0       0x31A0
#define   I_DB_CTRL0_I_DB_EN                 0x80  /* [7] I Domain And FRC Capture Double Buffer Enable */
#define   I_DB_CTRL0_I_M1_DB_READY           0x08  /* [3] M1 I Domain And FRC Capture Double Buffer Ready */

/* --- D Domain Double Buffer (Page 31) --- */
#define D_DB_CTRL0       0x31B0
#define   D_DB_CTRL0_D_DB_EN                 0x80  /* [7] FRC Display, D Domain, Highlight Window And Blending Border Double ... */
#define   D_DB_CTRL0_D_DB_READY              0x40  /* [6] FRC Display, D Domain, Highlight Window And Blending Border Double ... */

/* --- IMD Domain Double Buffer (Page 31) --- */
#define I_DB_CTRL        0x31C0
#define   I_DB_CTRL_IMD_DB_EN                0x80  /* [7] IMD Double Buffer Mechanism Enable */
#define   I_DB_CTRL_IMD_DB_READY             0x40  /* [6] IMD Double Buffer Mechanism Ready */
#define D_DB_CTRL        0x31C1

#endif /* RTD2556_REGS_PAGE_31_H */
