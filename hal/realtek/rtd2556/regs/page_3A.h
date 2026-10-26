/* RTD2556 scaler page 0x3A */
#ifndef RTD2556_REGS_PAGE_3A_H
#define RTD2556_REGS_PAGE_3A_H

/* --- OSD Frame Control Registers (Page 3A) --- */
#define FRAME_CTRL_00 0x3A00
#define FRAME_CTRL_01 0x3A01
#define FRAME_CTRL_02 0x3A02
#define   FRAME_CTRL_02_HORI_DELAY1_SHIFT       6
#define   FRAME_CTRL_02_VERT_DELAY1_SHIFT       3
#define   FRAME_CTRL_02_DISP_ZONE               0x06  /* [2:1] Display Zone, for Smaller Character Width */
#define   FRAME_CTRL_02_DISP_ZONE_SHIFT         1
#define   FRAME_CTRL_02_DISP_ZONE_LEFT          0x02  /* 01 */
#define   FRAME_CTRL_02_OSD_EN                  0x01  /* [0] OSD Enable */
#define FRAME_CTRL_06 0x3A06
#define   FRAME_CTRL_06_HORI_DELAY_STEP_SEL     0x10  /* [4] Horizontal Delay Step Unit */
#define FRAME_CTRL_07 0x3A07
#define   FRAME_CTRL_07_F4B_COLOR_SEL           0x04  /* [2] 4-bit Font Background */
#define FRAME_CTRL_0A 0x3A0A
#define   FRAME_CTRL_0A_ALPHA_BLEND_TYPE        0x0C  /* [3:2] Alpha Blending Type (blending Type 1) */
#define   FRAME_CTRL_0A_ALPHA_BLEND_TYPE_SHIFT  2
#define   FRAME_CTRL_0A_DB_WIDTH                0x02  /* [1] Double Width Enable (For All OSD Including Windows And Characters) */
#define   FRAME_CTRL_0A_DB_HEIGHT               0x01  /* [0] Double Height Enable (For All OSD Including Windows And Characters) */
#define FRAME_CTRL_0C 0x3A0C
#define FRAME_CTRL_0D 0x3A0D
#define   FRAME_CTRL_0D_FS_BASE_SHIFT           4
#define FRAME_CTRL_0E 0x3A0E
#define FRAME_CTRL_17 0x3A17
#define   FRAME_CTRL_17_FONT_BASE_SHIFT         3
#define FRAME_CTRL_18 0x3A18
#define   FRAME_CTRL_18_FNT_WIN_V_BD_ALIGN      0x10  /* [4] Vertical Boundary Alignment of Font And Windows */
#define FRAME_CTRL_19 0x3A19
#define FRAME_CTRL_1A 0x3A1A
#define FRAME_CTRL_1B 0x3A1B
#define   FRAME_CTRL_1B_FS_BASE0_SHIFT          4
#define FRAME_CTRL_1C 0x3A1C
#define FRAME_CTRL_1D 0x3A1D
#define FRAME_CTRL_24 0x3A24
#define   FRAME_CTRL_24_FONT_A_HORI_DELAY_SHIFT 4
#define FRAME_CTRL_25 0x3A25
#define FRAME_CTRL_26 0x3A26
#define FRAME_CTRL_38 0x3A38
#define   FRAME_CTRL_38_OSD_LEN1                0xF0  /* [7:4] First OSD Length [11:8] */
#define   FRAME_CTRL_38_OSD_LEN1_SHIFT          4
#define   FRAME_CTRL_38_OSD_WID1                0x0F  /* [3:0] First OSD Width [11:8] */
#define   FRAME_CTRL_38_OSD_WID1_SHIFT          0
#define FRAME_CTRL_39 0x3A39
#define FRAME_CTRL_3A 0x3A3A
#define FRAME_CTRL_3E 0x3A3E
#define FRAME_CTRL_3F 0x3A3F
#define FRAME_CTRL_60 0x3A60
#define FRAME_CTRL_64 0x3A64
#define FRAME_CTRL_6C 0x3A6C

#endif /* RTD2556_REGS_PAGE_3A_H */
