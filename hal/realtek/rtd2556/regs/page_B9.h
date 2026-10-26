/* RTD2556 scaler page 0xB9 */
#ifndef RTD2556_REGS_PAGE_B9_H
#define RTD2556_REGS_PAGE_B9_H

/* --- D1 DisplayPort Video SEC DATA (Page B9) --- */
#define D1_MN_STRM_ATTR_CTRL      0xB900
#define   D1_MN_STRM_ATTR_CTRL_STRM_ATTR_POPUP        0x80  /* [7] Popup The Main Stream Attributes. */
#define   D1_MN_STRM_ATTR_CTRL_MN_VIDEO_POPUP         0x40  /* [6] Popup The Mvid/Nvid. */
#define   D1_MN_STRM_ATTR_CTRL_MN_AUDIO_POPUP         0x20  /* [5] Popup The Maud/Naud. */
#define   D1_MN_STRM_ATTR_CTRL_BE_HIT_VBID0           0x10  /* [4] Check If Vbid_0 = 1'b1 Overlap with Be */
#define   D1_MN_STRM_ATTR_CTRL_AUTO_CLR_MN_EN         0x08  /* [3] Auto Clear Mvid/Nvid, Maud/Naud When Main-stream Attribute Changed. */
#define   D1_MN_STRM_ATTR_CTRL_STRM_ATTR_ERRC_EN      0x02  /* [1] MSA Debounce Enable */
#define D1_DP_VBID                0xB901
#define   D1_DP_VBID_AMUTE                            0x10  /* [4] The VB-ID[4] Information. */
#define   D1_DP_VBID_NO_VIDEO                         0x08  /* [3] The VB-ID[3] Information. */
#define   D1_DP_VBID_VBLK                             0x01  /* [0] The VB-ID[0] Information */
#define D1_MSA_HTT_0              0xB908
#define D1_MSA_HWD_0              0xB90C
#define D1_MSA_HSW_0              0xB90E
#define D1_MSA_VTTE_0             0xB910
#define D1_MSA_VHT_0              0xB916
#define D1_MN_RS_DEC_CTRL         0xB92B
#define   D1_MN_RS_DEC_CTRL_RS_DEC_EN                 0x80  /* [7] Reed-Solomon Decoder Enable */
#define   D1_MN_RS_DEC_CTRL_RS_ERR1                   0x08  /* [3] One Error Has Been Found in RS Decoder. */
#define   D1_MN_RS_DEC_CTRL_RS_ERR2                   0x04  /* [2] One Error Has Been Found in RS Decoder. */
#define   D1_MN_RS_DEC_CTRL_RS_ERR2_ENH_MODE          0x02  /* [1] Force The Rest Data As Being with 2-nibble RS Code Error Once 1-nib... */
#define   D1_MN_RS_DEC_CTRL_RS_ERR2_MODE              0x01  /* [0] Force The Rest Data As Being with 2-nibble RS Code Error Once 2-nib... */
#define D1_DP_GLB_STATUS          0xB936
#define   D1_DP_GLB_STATUS_VBID3_CHG                  0x40  /* [6] VB-ID[3] Has Been Changed. (No Video Stream) */
#define D1_DP_VWD_CTRL            0xB938
#define   D1_DP_VWD_CTRL_VWD_BY_VFIFO_OVF_EN          0x20  /* [5] Video Watch Dog for Video FIFO Overflow. */
#define   D1_DP_VWD_CTRL_VWD_BY_VFIFO_UDF_EN          0x10  /* [4] Video Watch Dog for Video FIFO Underflow. */
#define D1_DP_IRQ_CTRL0           0xB93A
#define D1_DP_IRQ_CTRL1           0xB93B

/* --- D1 DisplayPort Audio SEC DATA (Page B9) --- */
#define D1_AUD_FREQUENY_DET_0     0xB950
#define   D1_AUD_FREQUENY_DET_0_AFREQ_DET             0x80  /* [7] Audio Frequency Detection Enable */
#define   D1_AUD_FREQUENY_DET_0_FREQ_CHANGE_INT       0x20  /* [5] Audio Sample Freq. Change IRQ Flag */
#define   D1_AUD_FREQUENY_DET_0_AUD_FREQ_TIME         0x0C  /* [3:2] Frequency Monitor Times */
#define   D1_AUD_FREQUENY_DET_0_AUD_FREQ_TIME_SHIFT   2
#define   D1_AUD_FREQUENY_DET_0_AUTO_STOP_TRK_EN      0x02  /* [1] Auto Stop Tracking for Depth/boundary/ Trend Method */
#define D1_AUD_FREQUENY_DET_1     0xB951
#define D1_AUD_FREQUENY_TH_0      0xB952
#define D1_AUD_FREQUENY_TH_1      0xB953
#define D1_AUD_FREQUENY_TH_2      0xB954
#define D1_AUD_FREQUENY_TH_3      0xB955
#define D1_AUD_FREQUENY_TH_4      0xB956
#define D1_AUD_FREQUENY_TH_5      0xB957
#define D1_SCODE_0                0xB958
#define D1_SCODE_1                0xB959
#define D1_SCODE_2                0xB95A
#define D1_DCODE_0                0xB95B
#define D1_DCODE_1                0xB95C
#define D1_DCODE_2                0xB95D
#define D1_DCODE_3                0xB95E
#define D1_DCODE_4                0xB95F
#define D1_DCODE_5                0xB960
#define D1_AUD_DEPTH_IGAIN_1      0xB96E
#define   D1_AUD_DEPTH_IGAIN_1_AUD_FREQ_STABLE        0x80  /* [7] AUD Frequency Detection Was Already Stable */
#define D1_AUD_BUFFER_CTRL_0      0xB970
#define   D1_AUD_BUFFER_CTRL_0_ABUF_EMPTY             0x08  /* [3] Audio Buffer Empty, Write 1 to Clear */
#define   D1_AUD_BUFFER_CTRL_0_ABUF_FULL              0x04  /* [2] Audio Buffer Full, Write 1 to Clear */
#define   D1_AUD_BUFFER_CTRL_0_ABUF_UDFL              0x02  /* [1] Audio Buffer under Flow, Write 1 to Clear */
#define   D1_AUD_BUFFER_CTRL_0_ABUF_OVFL              0x01  /* [0] Audio Buffer Overflow, Write 1 to Clear */
#define D1_AUD_FSM_CTRL_0         0xB974
#define   D1_AUD_FSM_CTRL_0_AUD_EN                    0x80  /* [7] Audio FSM Enable */
#define   D1_AUD_FSM_CTRL_0_AUD_WAIT_EMPTY_EN         0x40  /* [6] If No Audio In, Read Out The Remaining Samples from Buffer And Then... */
#define   D1_AUD_FSM_CTRL_0_AUD_FAST_MUTE_EN          0x10  /* [4] 0: Decided by AUD_FREQ_TIME */
#define   D1_AUD_FSM_CTRL_0_LYO_CHG_MUTE_EN           0x04  /* [2] When Layout Change between 2 And 8 Ch Layout, */
#define D1_DVC_CTRL               0xB976
#define   D1_DVC_CTRL_DPAUD_FAN_EN                    0x80  /* [7] FAN in And FAN Out DP Audio Signal Enable. */
#define   D1_DVC_CTRL_DVC_DOWN_DB                     0x0C  /* [3:2] 'b00: -1dB (gain1=gain1*7/8) */
#define   D1_DVC_CTRL_DVC_DOWN_DB_SHIFT               2
#define   D1_DVC_CTRL_DVC_UP_DB                       0x03  /* [1:0] 'b00: +1dB (gain1=gain1*9/8) */
#define   D1_DVC_CTRL_DVC_UP_DB_SHIFT                 0
#define D1_DVC_VA                 0xB977
#define D1_DVC_WINDOW_CONTROL_0   0xB978
#define D1_DVC_WINDOW_CONTROL_1   0xB979
#define D1_AUD_PS_CTRL_0          0xB980
#define   D1_AUD_PS_CTRL_0_AUD_SDM_EN                 0x80  /* [7] DP Audio SDM Control Enable */
#define   D1_AUD_PS_CTRL_0_D_CODE_AUTO_EN             0x40  /* [6] Auto Load D Code from Frequency Detection Part. */
#define D1_AUD_BDRY_0             0xB981
#define   D1_AUD_BDRY_0_TR_BDRY_EN                    0x80  /* [7] Boundary Tracking Enable. */
#define   D1_AUD_BDRY_0_ABUF_BOUNDARY                 0x3F  /* [5:0] ABUF_BOUNDARY < */
#define   D1_AUD_BDRY_0_ABUF_BOUNDARY_SHIFT           0
#define D1_AUD_BDRY_2             0xB983
#define D1_AUD_BDRY_3             0xB984
#define D1_AUD_BDRY_4             0xB985
#define D1_AUD_BDRY_5             0xB986
#define D1_AUD_TRND_0             0xB992
#define   D1_AUD_TRND_0_TR_TRND_EN                    0x80  /* [7] Trend Tracking Enable. */
#define   D1_AUD_TRND_0_DP_TRND_MODE                  0x40  /* [6] 0: Old mode (use ~bs to measure the water level) */
#define   D1_AUD_TRND_0_TRN_MAX_TIME                  0x07  /* [2:0] When Max. FIFO Depth Increase for N Times Or Min. FIFO Depth Decrea... */
#define   D1_AUD_TRND_0_TRN_MAX_TIME_SHIFT            0
#define D1_AUD_TRND_7             0xB999
#define D1_AUD_TRND_8             0xB99A
#define D1_AUD_TRND_9             0xB99B
#define D1_AUD_TRND_10            0xB99C
#define D1_SPDIF_TX_0             0xB9A9
#define   D1_SPDIF_TX_0_CS_AUTO                       0x80  /* [7] Including Channel Status, User Bit, Validity Bit , Load Auto by H/W */
#define   D1_SPDIF_TX_0_SP_ST_EN                      0x40  /* [6] Sample State Enable */
#define   D1_SPDIF_TX_0_DP_SPDIF_3_SWITCH             0x08  /* [3] 0:: Disable (output SPDIF 3) */
#define   D1_SPDIF_TX_0_DP_SPDIF_2_SWITCH             0x04  /* [2] 0:: Disable (output SPDIF 2) */
#define   D1_SPDIF_TX_0_DP_SPDIF_1_SWITCH             0x02  /* [1] 0:: Disable (output SPDIF 1) */
#define   D1_SPDIF_TX_0_DP_SPDIF_0_SWITCH             0x01  /* [0] 0:: Disable (output SPDIF 0) */
#define D1_DP_AUD_CTRL            0xB9B0
#define   D1_DP_AUD_CTRL_ABUF_EN                      0x80  /* [7] Enable Audio Buffer */
#define   D1_DP_AUD_CTRL_ATYPE_SRC                    0x20  /* [5] The Source to Determine The Audio Coding Type. */
#define D1_DP_AUD_ID              0xB9B1
#define D1_DP_I2S_CTRL            0xB9B8
#define   D1_DP_I2S_CTRL_DP_I2S_3_SWITCH              0x08  /* [3] I2s_3 Serial Data Enable Control */
#define   D1_DP_I2S_CTRL_DP_I2S_2_SWITCH              0x04  /* [2] I2s_2 Serial Data Enable Control */
#define   D1_DP_I2S_CTRL_DP_I2S_1_SWITCH              0x02  /* [1] I2s_1 Serial Data Enable Control */
#define   D1_DP_I2S_CTRL_DP_I2S_0_SWITCH              0x01  /* [0] I2s_0 Serial Data Enable Control */

/* --- D1 DisplayPort AUD PLL (Page B9) --- */
#define D1_PB9_MN_SCLKG_DIVM      0xB9D1
#define D1_PB9_MN_SCLKG_DIVN      0xB9D2
#define D1_PB9_MN_SCLKG_PLL_PWR   0xB9D3
#define   D1_PB9_MN_SCLKG_PLL_PWR_REG_DPLL_PWDN       0x80  /* [7] DPLL Power Down */
#define   D1_PB9_MN_SCLKG_PLL_PWR_REG_DPLL_CALSW      0x10  /* [4] Calibration Validated */
#define   D1_PB9_MN_SCLKG_PLL_PWR_REG_DPLL_CALLCH     0x08  /* [3] Latch Calibration */
#define   D1_PB9_MN_SCLKG_PLL_PWR_REG_DPLL_CMPEN      0x04  /* [2] CMP Enable */
#define   D1_PB9_MN_SCLKG_PLL_PWR_REG_DPLL_O          0x03  /* [1:0] DPLL Output Divider */
#define D1_PB9_MN_SCLKG_PLL_WD    0xB9D5
#define   D1_PB9_MN_SCLKG_PLL_WD_REG_DPLL_WDO         0x80  /* [7] DPLL WD Status Register */
#define   D1_PB9_MN_SCLKG_PLL_WD_REG_DPLL_VCOMD       0x18  /* [4:3] DPLL VCO Default Mode */
#define   D1_PB9_MN_SCLKG_PLL_WD_REG_DPLL_VCOMD_SHIFT 3
#define   D1_PB9_MN_SCLKG_PLL_WD_REG_DPLL_RESERVE     0x04  /* [2] DPLL Phase Swallow Circuit */
#define D1_PB9_MN_SCLKG_PLL_INSEL 0xB9D6
#define   D1_PB9_MN_SCLKG_PLL_INSEL_REG_DPLL_VCORSTB  0x01  /* [0] DPLL VCO Reset */

#endif /* RTD2556_REGS_PAGE_B9_H */
