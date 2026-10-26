/* RTD2556 scaler page 0x00 */
#ifndef RTD2556_REGS_PAGE_00_H
#define RTD2556_REGS_PAGE_00_H

/* --- Chip Control Part1 (Page 0) --- */
#define HOST_CTRL                     0x0001
#define   HOST_CTRL_SOFT_RESET                          0x01  /* [0] Software Reset Whole Chip (Low Pulse at Least 8ms) */

/* --- IRQ Status (Page 0) --- */
#define STATUS0                       0x0002
#define   STATUS0_BUFFER_OVF1                           0x02  /* [1] M1 FIFO Overflow Status */
#define   STATUS0_BUFFER_UDF1                           0x01  /* [0] M1 Line Buffer Underflow Status */
#define STATUS1                       0x0003
#define   STATUS1_DVS_START                             0x08  /* [3] DVS Start Event Status */
#define IRQ_CTRL0                     0x0004
#define   IRQ_CTRL0_IRQ_EN                              0x80  /* [7] Internal IRQ Enable: (Global) */

/* --- Power Control (Page 0) --- */
#define TOP_POWER_CTRL                0x0009
#define   TOP_POWER_CTRL_TOP_PWRCUT_ISO_CTRL            0x02  /* [1] Top Power Digital Isolation Control */
#define POWER_CTRL                    0x000B
#define   POWER_CTRL_SCDC_CLK_SEL                       0x80  /* [7] HDMI 2.0 SCDC Function Reference Clock Select */
#define   POWER_CTRL_AUX_PHY_PS_CLK_SEL                 0x30  /* [5:4] Aux_phy_ps_top Clock Select */

/* --- Watch Dog (Page 0) --- */
#define WATCH_DOG_CTRL0               0x000C
#define   WATCH_DOG_CTRL0_HV_ERR_WDEN                   0x80  /* [7] M1 Auto Switch When Input HSYNC/VSYNC Error */
#define   WATCH_DOG_CTRL0_DVS_TIMEOUT_WDEN              0x20  /* [5] Auto Switch When Display VSYNC Timeout */
#define   WATCH_DOG_CTRL0_WD_FREERUN_EN                 0x04  /* [2] Watch-Dog Action If Event Happened (for Display Timing) */
#define   WATCH_DOG_CTRL0_M1_BG_EN                      0x02  /* [1] M1 Watch-Dog Action If Event Happened (for Display Data) */

/* --- Input Video Capture (Page 0) --- */
#define M1_VGIP_CTRL                  0x0010
#define   M1_VGIP_CTRL_DOUBLE_ENABLE                    0x10  /* [4] VGIP Double Buffer Mode Enable */
#define   M1_VGIP_CTRL_IN_SEL                           0x0C  /* [3:2] Input Pixel Format */
#define   M1_VGIP_CTRL_IN_SEL_SHIFT                     2
#define   M1_VGIP_CTRL_DIGITAL_MODE                     0x02  /* [1] Input Graphic/Video Mode */
#define   M1_VGIP_CTRL_IVRUN_EN                         0x01  /* [0] Input Sampling Run Enable */
#define M1_VGIP_SIGINV                0x0011
#define   M1_VGIP_SIGINV_FIELD_HS_INV                   0x20  /* [5] HS Signal Inverted for Field Detection */
#define   M1_VGIP_SIGINV_VS_INV                         0x08  /* [3] Input VS Signal Polarity Inverted */
#define M1_VGIP_DELAY_CTRL            0x0012
#define   M1_VGIP_DELAY_CTRL_HDLY_ONE                   0x10  /* [4] Add One Clock Delay to IHS Delay */
#define   M1_VGIP_DELAY_CTRL_HS_SYNCEDGE                0x08  /* [3] HSYNC Synchronize Edge */
#define   M1_VGIP_DELAY_CTRL_VS_SYNCEDGE                0x04  /* [2] VSYNC Synchronize Edge */

/* --- Input Frame Window (Page 0) --- */
#define M1_IPH_ACT_STA_H              0x0014
#define   M1_IPH_ACT_STA_H_IH_ACT_WID_SHIFT             4
#define M1_IPH_ACT_STA_L              0x0015
#define M1_IPH_ACT_WID_L              0x0017
#define M1_IPV_ACT_STA_H              0x0018
#define   M1_IPV_ACT_STA_H_IV_ACT_LEN                   0xF0  /* [7:4] Input Video Vertical Active Height : */
#define   M1_IPV_ACT_STA_H_IV_ACT_LEN_SHIFT             4
#define M1_IPV_ACT_STA_L              0x0019
#define M1_IPV_ACT_LEN_L              0x001B
#define M1_IVS_DELAY                  0x001C
#define M1_IHS_DELAY                  0x001D
#define M1_VGIP_HV_DELAY              0x001E
#define   M1_VGIP_HV_DELAY_IHS_DLY                      0x04  /* [2] Input HSYNC Delay for Capture: High Byte[9] (Counted by Input Pixel... */
#define   M1_VGIP_HV_DELAY_IVS_DLY                      0x02  /* [1] Input VSYNC Delay for Capture: High Byte[8] (Counted by Input HSYNC) */
#define   M1_VGIP_HV_DELAY_IHS_DLY_                     0x01  /* [0] Input HSYNC Delay for Capture: High Byte[8] (Counted by Input Pixel... */

/* --- FIFO Frequency (Page 0) --- */
#define M1_FIFO_CLOCK_SELECT          0x0022
#define   M1_FIFO_CLOCK_SELECT_XTAL_SEL                 0x04  /* [2] Internal Xtal Frequency */

/* --- Scaling Down Control (Page 0) --- */
#define SD_ADDRESS_PORT_M1            0x0024
#define SD_FILTER_CONTROL_REG_M1      0x0026
#define   SD_FILTER_CONTROL_REG_M1_SD_MAIN_FCOEF_WR_EN  0x80  /* [7] Enable Filter Coefficient Access */
#define   SD_FILTER_CONTROL_REG_M1_SD_WCOEF_SEL2        0x40  /* [6] Select H/V User Defined Filter Coefficient Table for Access Channel */
#define SD_USER_FILTER_ACCESS_PORT_M1 0x0027

/* --- Display Format (Page 0) --- */
#define VIDEO_DISPLAY_CONTROL_REG     0x0028
#define   VIDEO_DISPLAY_CONTROL_REG_FORCE_EN            0x80  /* [7] Force Display Timing Generator Enable: (Should Be Set When in Free-... */
#define   VIDEO_DISPLAY_CONTROL_REG_FORCE_BG_EN         0x20  /* [5] Main Display Output Force to Background Color */
#define   VIDEO_DISPLAY_CONTROL_REG_FRAME_SYNC_EN       0x08  /* [3] Frame Sync Mode Enable */
#define   VIDEO_DISPLAY_CONTROL_REG_DOUT_EN             0x02  /* [1] Display Output Run Enable */
#define   VIDEO_DISPLAY_CONTROL_REG_DTIME_EN            0x01  /* [0] Display Timing Run Enable */
#define DISPLAY_FORMAT_ADDR_PORT      0x002A
#define DISPLAY_FORMAT_DATA_PORT      0x002B

/* --- FIFO Window (Page 0) --- */
#define FIFO_WINDOW_ADDR_PORT         0x0030
#define FIFO_WINDOW_DATA_PORT         0x0031

/* --- Scaling Up Function (Page 0) --- */
#define SCALE_CONTROL_REG             0x0032
#define   SCALE_CONTROL_REG_DISP_SRAM_BUSY              0x20  /* [5] Display Line Buffer Ready */
#define   SCALE_CONTROL_REG_MAIN_ENABLE_V               0x02  /* [1] Enable The Vertical Filter Function: */
#define   SCALE_CONTROL_REG_MAIN_ENABLE_H               0x01  /* [0] Enable The Horizontal Filter Function: */
#define SF_ADDR_PORT                  0x0033
#define SF_DATA_PORT                  0x0034
#define FILTER_CONTROL_REG            0x0035
#define   FILTER_CONTROL_REG_MAIN_FCOEF_WR_EN           0x80  /* [7] Enable Filter Coefficient Access */
#define   FILTER_CONTROL_REG_WCOEF_SEL2                 0x40  /* [6] Select H/V User Defined Filter Coefficient Table for Access Channel */
#define   FILTER_CONTROL_REG_HOR_RCOEF_SEL2             0x20  /* [5] Select Horizontal User Defined Filter Coefficient Table */
#define   FILTER_CONTROL_REG_VER_RCOEF_SEL2             0x10  /* [4] Select Vertical User Defined Filter Coefficient Table */
#define USER_DEF_FILTER_ACCESS_PORT   0x0036

/* --- Frame Sync Fine Tune (Page 0) --- */
#define IV_DV_DELAY_CLK_FINE          0x003D
#define   IV_DV_DELAY_CLK_FINE_IV_DV_ICLK1_INI          0xF0  /* [7:4] IV_DV_DELAY_CLK _ODD_addition[3:0], Step Is One Pixel */
#define   IV_DV_DELAY_CLK_FINE_IV_DV_ICLK1_INI_SHIFT    4
#define IVS2DVS_DLY_LINE_H            0x003E
#define   IVS2DVS_DLY_LINE_H_IV_DV_LINE_INI             0x1F  /* [4:0] IVS to DVS Lines, High Byte[12:8]: (Only for FrameSync Mode) */
#define DISPLAY_ALIGN_CONTROL         0x003F
#define   DISPLAY_ALIGN_CONTROL_IV_DV_16ICLK1_INI       0x10  /* [4] Frame Sync Mode Delay Fine Tune [8] , Step Is 16ICLK */
#define IVS2DVS_DLY_LINE_L            0x0040
#define IVS2DVS_DLY_TUNE_ODD          0x0041
#define FS_DELAY_FINE_TUNE            0x0043
#define   FS_DELAY_FINE_TUNE_IV_DV_LINE_SKIP            0x08  /* [3] If this bit is 0, */
#define LAST_LINE_H                   0x0044
#define   LAST_LINE_H_TOTAL_LINE_SEL                    0x80  /* [7] Last-line-width / DV-Total Selector : */
#define   LAST_LINE_H_SEL_MEAS_VALUE                    0x0F  /* [3:0] DV Total Or Last Line Width[11:8] Before Sync in Frame Sync Mode */
#define   LAST_LINE_H_SEL_MEAS_VALUE_SHIFT              0
#define LAST_LINE_L                   0x0045

/* --- Sync Processor Overview (Page 0) --- */
#define SYNC_PROC_PORT_ADDR           0x005C

/* --- Contrast & Brightness (Page 0) --- */
#define SRGB_CTRL                     0x0062
#define   SRGB_CTRL_SRGB_WR_READY                       0x80  /* [7] SRGB Coefficient Write/Read Ready */
#define   SRGB_CTRL_SRGB_PORT_RESET                     0x40  /* [6] Write 1 to Reset Port Address, Auto Cleared after Reset. */
#define   SRGB_CTRL_SRGB_COEF_WR_ENABLE                 0x38  /* [5:3] 000: Auto Increase */
#define   SRGB_CTRL_SRGB_ENABLE                         0x04  /* [2] Enable SRGB Function */
#define   SRGB_CTRL_CTS_ENABLE                          0x02  /* [1] Enable Contrast Function: */
#define   SRGB_CTRL_BRI_ENABLE                          0x01  /* [0] Enable Brightness Function: */

/* --- Color Processor Control (Page 0) --- */
#define SRGB_ACCESS_PORT_SETA         0x0063

/* --- Contrast & Brightness (Page 0) --- */
#define CTS_BRI_PORT_ADD              0x0064
#define CTS_BRI_PORT_DATA             0x0065

/* --- Gamma Control (Page 0) --- */
#define GAMMA_PORT_SETA               0x0066
#define GAMMA_CTRL_SETA               0x0067
#define   GAMMA_CTRL_SETA_GAMMA_ACCESS_PORT             0x80  /* [7] Enable Access Channels for Gamma Correction Coefficient:(Reset Addr... */
#define   GAMMA_CTRL_SETA_GAMMA_ENABLE                  0x40  /* [6] Gamma Table Enable */
#define   GAMMA_CTRL_SETA_GAMMA_CHANNEL_CTRL            0x30  /* [5:4] Color Channel of Gamma Table*[1] */
#define   GAMMA_CTRL_SETA_GAMMA_CHANNEL_CTRL_SHIFT      4
#define   GAMMA_CTRL_SETA_GAMMA_PORT_ADD_ENABLE         0x08  /* [3] Gamma Port Address Access Enable */
#define   GAMMA_CTRL_SETA_GAMMA_SRAM_LATCH_SEL          0x04  /* [2] Write Table to SRAM Or Latch Select */

/* --- Color Processor Control (Page 0) --- */
#define GAMMA_BIST_DITHER_SETA        0x0068
#define   GAMMA_BIST_DITHER_SETA_SRGB_PRECI             0x60  /* [6:5] SRGB Precision */
#define   GAMMA_BIST_DITHER_SETA_SRGB_PRECI_SHIFT       5

/* --- DDither Control (Page 0) --- */
#define D_DITHER_DATA_ACCESS          0x0069
#define D_DITHER_COMMON_CTRL1         0x006A
#define   D_DITHER_COMMON_CTRL1_DITHER_ACCESS           0xC0  /* [7:6] Enable Access Control */
#define   D_DITHER_COMMON_CTRL1_DITHER_ENABLE           0x20  /* [5] Enable Dither Function */
#define   D_DITHER_COMMON_CTRL1_DITHER_TEMPORAL_EN      0x10  /* [4] Enable Dither Temporal */
#define D_DITHER_REGISTER_ADDR        0x006B

/* --- Back ground color (Page 0) --- */
#define OVERLAY_CTRL                  0x006C
#define   OVERLAY_CTRL_BG_COLOR_ACCESS_EN               0x20  /* [5] Background Color Access Enable */
#define   OVERLAY_CTRL_OVERLAY_PORT_EN                  0x01  /* [0] Overlay Port Enable: */
#define BG_COLOR_DATA_PORT_M1         0x006D

/* --- OSD Color Palette (Page 0) --- */
#define OVERLAY_LUT_ADDR              0x006E
#define   OVERLAY_LUT_ADDR_LUT_ACC_EN                   0x80  /* [7] Overlay_color_palette_access_enable: */
#define   OVERLAY_LUT_ADDR_LUTADR                       0x3F  /* [5:0] Overlay64*24 Look-Up-Table Write Address [5:0] */
#define COLOR_LUT_PORT                0x006F

/* --- Image Auto Function (Page 0) --- */
#define H_BOUNDARY_H                  0x0070
#define   H_BOUNDARY_H_HB_END                           0x0F  /* [3:0] Horizontal Boundary End: High Byte [11:8] */
#define   H_BOUNDARY_H_HB_END_SHIFT                     0
#define H_BOUNDARY_STA_L              0x0071
#define H_BOUNDARY_END_L              0x0072
#define V_BOUNDARY_H                  0x0073
#define   V_BOUNDARY_H_VB_END                           0x0F  /* [3:0] Vertical Boundary End: High Byte [11:8] */
#define   V_BOUNDARY_H_VB_END_SHIFT                     0
#define V_BOUNDARY_STA_L              0x0074
#define V_BOUNDARY_END_L              0x0075
#define GRN_NOISE_MARGIN              0x0077
#define   GRN_NOISE_MARGIN_HB_END                       0x01  /* [0] Horizontal Boundary End: High Byte [12] */
#define BLU_NOISE_MARGIN              0x0078
#define   BLU_NOISE_MARGIN_HX_STA                       0x02  /* [1] Active region horizontal START measurement result: bit[12] */
#define   BLU_NOISE_MARGIN_HX_END                       0x01  /* [0] Active region horizontal END measurement result: bit[12] */
#define AUTO_ADJ_CTRL0                0x007A
#define HW_AUTO_PHASE_CTRL0           0x007B
#define   HW_AUTO_PHASE_CTRL0_AUTOSOD_STEP_NUM          0xF8  /* [7:3] Number of Auto-Phase Step  (Value+1) */
#define   HW_AUTO_PHASE_CTRL0_AUTOSOD_STEP_NUM_SHIFT    3
#define AUTO_ADJ_CTRL1                0x007D
#define   AUTO_ADJ_CTRL1_M_VGEN_EN                      0x80  /* [7] Measure Digital Enable Info when boundary search active */
#define   AUTO_ADJ_CTRL1_AUTOPHASE_EN                   0x40  /* [6] Hardware / Software Auto Phase Switch */
#define   AUTO_ADJ_CTRL1_DIFF_EN                        0x02  /* [1] Function (Phase/Balance) Selection */
#define   AUTO_ADJ_CTRL1_NOW_AF                         0x01  /* [0] Start Auto-Function: */
#define VER_START_END_H               0x007E
#define   VER_START_END_H_VX_STA_SHIFT                  4
#define   VER_START_END_H_VX_END                        0x0F  /* [3:0] Active region vertical END measurement result: bit[11:8] */
#define VER_START_L                   0x007F
#define VER_END_L                     0x0080
#define H_START_END_H                 0x0081
#define   H_START_END_H_HX_STA_SHIFT                    4
#define   H_START_END_H_HX_END                          0x0F  /* [3:0] Active region horizontal END measurement result: bit[11:8] */
#define H_START_L                     0x0082
#define H_END_L                       0x0083

/* --- IDither Control (Page 0) --- */
#define I_DITHER_DATA_ACCESS_SETA     0x0088
#define I_DITHER_COMMON_CTRL1_SETA    0x0089
#define   I_DITHER_COMMON_CTRL1_SETA_DITHER_ENABLE      0x20  /* [5] Enable Dither Function */
#define   I_DITHER_COMMON_CTRL1_SETA_DITHER_TEMPORAL_EN 0x10  /* [4] Enable Dither Temporal */

/* --- Back ground color (Page 0) --- */
#define OVERLAY_CTRL1_M1              0x008F

/* --- OSD Access Port (Page 0) --- */
#define OSD_ADR_PORT_MSB              0x0090
#define   OSD_ADR_PORT_MSB_OSD_ADDR_MSB                 0x7F  /* [6:0] OSD MSB 7-bit Address */
#define OSD_ADR_PORT_LSB              0x0091
#define OSD_DATA_PORT                 0x0092
#define OSD_MISC_PORT                 0x0093
#define   OSD_MISC_PORT_DB_ENABLE                       0x80  /* [7] Global Double Buffer Enable */
#define OSD_WRITE_OPTION              0x0094
#define   OSD_WRITE_OPTION_BYTESEL                      0xE0  /* [7:5] Select Which Byte to Write: */
#define   OSD_WRITE_OPTION_BYTESEL_ALL                  0xE0  /* 111 */

/* --- I-Domain Color Conversion (Page 0) --- */
#define RGB2YCC_CTRL                  0x009C
#define   RGB2YCC_CTRL_Y_OUT_SHIFT                      0x80  /* [7] Y_Out Shift */
#define   RGB2YCC_CTRL_CB_CR_OUT_SHIFT                  0x60  /* [6:5] CbCr_Out Shift */
#define   RGB2YCC_CTRL_CB_CR_OUT_SHIFT_SHIFT            5

/* --- Page Control(Page 0) --- */
#define P00_PAGE_SEL                  0x009F

/* --- Embedded ADC (Page 0) --- */
#define ADC_POWER                     0x00C6
#define   ADC_POWER_REG_ADC_MBIAS_POW                   0x08  /* [3] Bandgap/MBIAS Power On */
#define ADC_SOG_CTRL                  0x00D4
#define   ADC_SOG_CTRL_REG_ADC_SOG_RES_500K             0x40  /* 01 */
#define   ADC_SOG_CTRL_REG_ADC_SOG_IB_SOURCE_SEL        0x08  /* [3] SOG0 Bias Current Source */

/* --- LVR (Page 0) --- */
#define VCCKOFF_CONTROL0              0x00F0
#define   VCCKOFF_CONTROL0_REG_POR_VCCKOFF_TOP          0x10  /* [4] vcckoff control bit */

#endif /* RTD2556_REGS_PAGE_00_H */
