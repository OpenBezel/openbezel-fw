/* RTD2556 keyhole: DP RX DPCD (index BAC1-BAC3, data BAC4) */
#ifndef RTD2556_REGS_PORT_BAC4_H
#define RTD2556_REGS_PORT_BAC4_H

#define DPCD_REV                        0x00000
#define DPCD_MAX_LINK_RATE              0x00001
#define DPCD_MAX_LINK_COUNT             0x00002
#define DPCD_MAX_DOWNSPREAD             0x00003
#define DPCD_NORP                       0x00004
#define DPCD_DOWNSTREAM_PORT            0x00005
#define DPCD_MAIN_LINK_CH_CODING        0x00006
#define DPCD_DOWNSTREAM_PORT_COUNT      0x00007
#define DPCD_RX_PORT0_CAP0              0x00008
#define DPCD_RX_PORT0_CAP1              0x00009
#define DPCD_RX_PORT1_CAP0              0x0000A
#define DPCD_RX_PORT1_CAP1              0x0000B
#define DPCD_E_RESERVE                  0x0000E
#define DPCD_MSTM_CAP                   0x00021
#define DPCD_80                         0x00080
#define DPCD_LINK_BW_SET                0x00100
#define DPCD_LINK_COUNT_SET             0x00101
#define DPCD_TRAINING_PATTERN_SET       0x00102
#define   DPCD_TRAINING_PATTERN_SET_SCRAMBLING_DISABLE          0x20  /* [5] Bit 5=SCRAMBLING_DISABLE */
#define   DPCD_TRAINING_PATTERN_SET_TRAINING_PATTERN_SET        0x03  /* [1:0] Bits1:0= TRAINING_PATTERN_SET */
#define DPCD_TRAINING_LANE0_SET         0x00103
#define DPCD_DOWNSPREAD_CTRL            0x00107
#define DPCD_EDP_CON_SET                0x0010A
#define DPCD_SINK_COUNT                 0x00200
#define DPCD_DEVICE_IRQ_VECTOR          0x00201
#define DPCD_LANE0_1_STATUS             0x00202
#define DPCD_LANE2_3_STATUS             0x00203
#define DPCD_LANE_ALIGN_STATUS_UPDATED  0x00204
#define DPCD_SINK_STATUS                0x00205
#define DPCD_ADJUST_REQUEST_LANE0_1     0x00206
#define DPCD_ADJUST_REQUEST_LANE2_3     0x00207
#define DPCD_SYMBOL_ERROR_COUNT_LANE0_L 0x00210
#define DPCD_SYMBOL_ERROR_COUNT_LANE0_H 0x00211
#define   DPCD_SYMBOL_ERROR_COUNT_LANE0_H_LANE0_ERR_COUNT_VALID 0x80  /* [7] Error count valid */
#define DPCD_TEST_SINK_MISC             0x00246
#define DPCD_SINK_VENDOR_IEEE_OUI_L     0x00400
#define DPCD_SINK_VENDOR_IEEE_OUI_M     0x00401
#define DPCD_SINK_VENDOR_IEEE_OUI_H     0x00402
#define DPCD_SINK_EQUALIZER_COUNT       0x00410
#define DPCD_ELT_PRESET_COUNT           0x00412
#define DPCD_500                        0x00500
#define DPCD_501                        0x00501
#define DPCD_502                        0x00502
#define DPCD_SINK_CONTROL               0x00600
#define DPCD_HDCP_BKSV_BYTE_0           0x68000
#define DPCD_HDCP_BCAPS                 0x68028

#endif /* RTD2556_REGS_PORT_BAC4_H */
