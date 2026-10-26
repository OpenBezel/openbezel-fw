/* RTD2556 scaler page 0x9C */
#ifndef RTD2556_REGS_PAGE_9C_H
#define RTD2556_REGS_PAGE_9C_H

/* --- DisplayPort Tx Digital PHY CTRL (Page BB) --- */
#define DP_PHY_CTRL            0x9C00                     /* from BB00 */
#define   DP_PHY_CTRL_V2ANALOG                      0xF0  /* [7:4] Indicator Which Lane Output to Analog Phy Is Valid */
#define   DP_PHY_CTRL_V2ANALOG_SHIFT                4
#define DPTX_ML_PAT_SEL        0x9C01                     /* from BB01 */
#define   DPTX_ML_PAT_SEL_TX_ML_PAT_SEL             0xF0  /* [7:4] Main Link Pattern Select */
#define   DPTX_ML_PAT_SEL_TX_ML_PAT_SEL_EN          0x02  /* [1] Enable Double Buffer by Vsync */
#define   DPTX_ML_PAT_SEL_PAT_SEL_DBUF              0x01  /* [0] Write 1 to Force Main Link Pattern Change to Current Select Immedia... */
#define DPTX_PHY_CTRL          0x9C0D                     /* from BB0D */
#define   DPTX_PHY_CTRL_SR_INSERT_EN                0x10  /* [4] Enable BS Symbol Replaced by SR Symbol. (only Active in SST Mode) */

/* --- DisplayPort Tx Analog PHY CTRL (Page BC) --- */
#define DPTX_SFIFO_CTRL0       0x9C10                     /* from BCA7 */
#define   DPTX_SFIFO_CTRL0_TX_EN                    0x80  /* [7] Write Position Counter */
#define DPTX_SFIFO_LANE_SWAP1  0x9C13                     /* from BCAA */
#define   DPTX_SFIFO_LANE_SWAP1_LANE0_SOURCE_LANE0  0x00  /* 00 */
#define   DPTX_SFIFO_LANE_SWAP1_LANE1_SOURCE_LANE1  0x10  /* 01 */
#define   DPTX_SFIFO_LANE_SWAP1_LANE2_SOURCE_LANE2  0x08  /* 10 */
#define   DPTX_SFIFO_LANE_SWAP1_LANE3_SOURCE_LANE3  0x03  /* 11 */

/* --- DisplayPort Tx HPD Detection (Page BB) --- */
#define HPD_CTRL               0x9C70                     /* from BB70 */
#define   HPD_CTRL_HPD_EN                           0x80  /* [7] HPD Detection Function ENABLE */
#define   HPD_CTRL_HPD_DEB                          0x04  /* [2] HPD Signal after Debounce */
#define HPD_IRQ                0x9C71                     /* from BB71 */
#define   HPD_IRQ_IHPD_INT                          0x80  /* [7] Immediately HPD Plug Event. */
#define   HPD_IRQ_SHPD_INT                          0x40  /* [6] Short Hot Plug (low Pulse Width:0.5ms~1ms) */
#define   HPD_IRQ_LHPD_INT                          0x20  /* [5] Long Hot Plug (10ms>low Pulse Width>2ms) */
#define   HPD_IRQ_UHPD_INT                          0x10  /* [4] HPD Unplug Event(low Level>10ms) */
#define   HPD_IRQ_UNHPD_INT                         0x08  /* [3] Unknown Hot Plug (low Pulse Width:1ms ~2ms) */
#define   HPD_IRQ_RHPD_INT                          0x04  /* [2] Replug Event (low Pulse Width>10ms And Replug) */
#define   HPD_IRQ_FHPD_INT                          0x02  /* [1] First HPD from 0 to 1(active When HPD_EN =1),clea by Write 1 Or Dis... */
#define HPD_IRQ_EN             0x9C72                     /* from BB72 */
#define   HPD_IRQ_EN_SHPD_INT_EN                    0x40  /* [6] Short Hot Plug Enable(0.5ms~1ms) */
#define HPD_TIMER1             0x9C73                     /* from BB73 */
#define   HPD_TIMER1_HPD_T10                        0x0F  /* [3:0] 1ms Target Timer */
#define   HPD_TIMER1_HPD_T10_SHIFT                  0
#define HPD_TIMER3             0x9C75                     /* from BB75 */
#define HPD_TIMER6             0x9C78                     /* from BB78 */
#define HPD_TIMER7             0x9C79                     /* from BB79 */

/* --- DisplayPort Tx MAC CTRL0 (Page BB) --- */
#define DP_MAC_CTRL            0x9CA0                     /* from BBA0 */
#define   DP_MAC_CTRL_ENHANCE_MD                    0x04  /* [2] DP Tx Enhanced Mode Enable */
#define   DP_MAC_CTRL_LANE_NUM                      0x03  /* [1:0] Set DP Tx Lane Count Number. */
#define   DP_MAC_CTRL_LANE_NUM_2_LANE               0x02  /* 10 */
#define DP_RESET_CTRL          0x9CA1                     /* from BBA1 */
#define   DP_RESET_CTRL_PG_RESET                    0x80  /* [7] Pixel Generation Block Reset */
#define   DP_RESET_CTRL_MSA_RESET                   0x40  /* [6] MSA Generation Block Reset,also for SEC_TOP Reset */
#define DP_DEBUG_CTRL          0x9CA2                     /* from BBA2 */
#define   DP_DEBUG_CTRL_DPTX_DEBUG_SEL              0x0F  /* [3:0] Dptx Debug Signals Selection */
#define   DP_DEBUG_CTRL_DPTX_DEBUG_SEL_SHIFT        0
#define PG_FIFO_CTRL           0x9CA4                     /* from BBA4 */
#define   PG_FIFO_CTRL_LFIFO_FULL                   0x80  /* [7] Long FIFO Full(write 1 to Clear). */
#define   PG_FIFO_CTRL_LFIFO_UDF                    0x40  /* [6] Long FIFO Underflow (write 1 to Clear). */
#define   PG_FIFO_CTRL_LFIFO_UDF_UNI                0x20  /* [5] Long FIFO Underflow Prior Than Small FIFO Overflow */
#define   PG_FIFO_CTRL_SFIFO_FUL                    0x10  /* [4] Small FIFO Full(write 1 to Clear). */
#define   PG_FIFO_CTRL_SFIFO_OVE                    0x08  /* [3] Small FIFO Overflow(write 1 to Clear) */
#define   PG_FIFO_CTRL_SFIFO_OVE_UNI                0x04  /* [2] Small FIFO Overflow Prior Than Long FIFO Underflow */
#define   PG_FIFO_CTRL_LFIFO_MAXWL_CLEAR            0x02  /* [1] Long FIFO Max Waterlevel Clear(write 1 to Clear) */
#define MN_VID_AUTO_EN_1       0x9CA8                     /* from BBA8 */
#define   MN_VID_AUTO_EN_1_MN_VID_AUTO_EN           0x80  /* [7] MN Code Generation Mode: */
#define   MN_VID_AUTO_EN_1_MN_VID_DB                0x40  /* [6] Double Buffer Control of Mvid/nvid Value */
#define MN_M_VID_H             0x9CA9                     /* from BBA9 */
#define MN_M_VID_M             0x9CAA                     /* from BBAA */
#define MN_M_VID_L             0x9CAB                     /* from BBAB */
#define MN_N_VID_H             0x9CAC                     /* from BBAC */
#define MN_N_VID_M             0x9CAD                     /* from BBAD */
#define MN_N_VID_L             0x9CAE                     /* from BBAE */
#define MVID_AUTO_H            0x9CAF                     /* from BBAF */
#define MVID_AUTO_M            0x9CB0                     /* from BBB0 */
#define MVID_AUTO_L            0x9CB1                     /* from BBB1 */
#define NVID_ASYNC_M           0x9CB2                     /* from BBB2 */
#define NVID_ASYNC_L           0x9CB3                     /* from BBB3 */
#define MSA_CTRL               0x9CB4                     /* from BBB4 */
#define   MSA_CTRL_MSA_DB                           0x80  /* [7] Double Buffer MSA Value */
#define   MSA_CTRL_MSA_DB_VBLANK                    0x40  /* [6] 0: Double buffer by bit[7] */
#define MSA_MISC0              0x9CB5                     /* from BBB5 */
#define   MSA_MISC0_COLORBIT_6_BIT                  0x00  /* 000 */
#define   MSA_MISC0_COMPONENT_FORMAT_RGB            0x00  /* 00 */
#define MN_STRM_ATTR_HTT_M     0x9CB7                     /* from BBB7 */
#define MN_STRM_ATTR_HTT_L     0x9CB8                     /* from BBB8 */
#define MN_STRM_ATTR_HST_M     0x9CB9                     /* from BBB9 */
#define MN_STRM_ATTR_HST_L     0x9CBA                     /* from BBBA */
#define MN_STRM_ATTR_HWD_M     0x9CBB                     /* from BBBB */
#define MN_STRM_ATTR_HWD_L     0x9CBC                     /* from BBBC */
#define MN_STRM_ATTR_HSW_M     0x9CBD                     /* from BBBD */
#define MN_STRM_ATTR_HSW_L     0x9CBE                     /* from BBBE */
#define MN_STRM_ATTR_VTTE_M    0x9CBF                     /* from BBBF */
#define MN_STRM_ATTR_VTTE_L    0x9CC0                     /* from BBC0 */
#define MN_STRM_ATTR_VST_M     0x9CC1                     /* from BBC1 */
#define MN_STRM_ATTR_VST_L     0x9CC2                     /* from BBC2 */
#define MN_STRM_ATTR_VHT_M     0x9CC3                     /* from BBC3 */
#define MN_STRM_ATTR_VHT_L     0x9CC4                     /* from BBC4 */
#define MN_STRM_ATTR_VSW_M     0x9CC5                     /* from BBC5 */
#define MN_STRM_ATTR_VSW_L     0x9CC6                     /* from BBC6 */
#define VBID                   0x9CC7                     /* from BBC7 */
#define   VBID_AUDIOMUTE_FLAG                       0x04  /* [2] VBID Audiomute_flag */
#define   VBID_NOVIDEO_FLAG                         0x02  /* [1] This Bit Must Be Set to 1 When Preceding BS Is Inserted While No Vi... */
#define ARBITER_CTRL           0x9CC9                     /* from BBC9 */
#define   ARBITER_CTRL_VACTIVE_MD                   0x02  /* [1] VACTIVE MODE */
#define   ARBITER_CTRL_ARBITER_EN                   0x01  /* [0] ARBITER Circuit Enable to Stuffing And Framing Video Frame in DP Fo... */
#define V_DATA_PER_LINE0       0x9CCA                     /* from BBCA */
#define V_DATA_PER_LINE1       0x9CCB                     /* from BBCB */
#define TU_SIZE                0x9CCC                     /* from BBCC */
#define TU_DATA_SIZE0          0x9CCD                     /* from BBCD */
#define TU_DATA_SIZE1          0x9CCE                     /* from BBCE */
#define HDELAY0                0x9CCF                     /* from BBCF */
#define HDELAY1                0x9CD0                     /* from BBD0 */
#define AUTO_HDELAY0           0x9CD1                     /* from BBD1 */
#define AUTO_HDELAY1           0x9CD2                     /* from BBD2 */
#define LFIFO_WL_SET           0x9CD3                     /* from BBD3 */
#define   LFIFO_WL_SET_WL_MID_REGEN                 0x80  /* [7] Middle Water Level Signal Regeneration */
#define ARBITER_SEC_END_CNT_HB 0x9CD4                     /* from BBD4 */
#define ARBITER_SEC_END_CNT_LB 0x9CD5                     /* from BBD5 */
#define ARBITER_DEBUG          0x9CD6                     /* from BBD6 */
#define   ARBITER_DEBUG_DEN_LINE_END_OCCUR          0x10  /* [4] Monitor During DEN (data Enable), Line End Occur Or Not */
#define   ARBITER_DEBUG_V_ACTIVE_OCCUR              0x08  /* [3] Monitor V_active Happened Or Not */
#define   ARBITER_DEBUG_SEC_TX_END_NV_MISS          0x04  /* [2] Sec Transmit Period End Counter Do Not Encounter The End Signal at ... */
#define   ARBITER_DEBUG_VSYNC_DE                    0x01  /* [0] Vsync & Data Enable Signal */
#define DPTX_CTSFIFO_CTRL      0x9CD7                     /* from BBD7 */
#define   DPTX_CTSFIFO_CTRL_DPTX_FORCE_CTSFIFO_RSTN 0x04  /* [2] CTS FIFO ON/OFF Ctrl */
#define DPTX_TOP_CTL           0x9CD9                     /* from BBD9 */

#endif /* RTD2556_REGS_PAGE_9C_H */
