/* RTD2556 keyhole: sync processor (index 005C, data 005D) */
#ifndef RTD2556_REGS_PORT_005D_H
#define RTD2556_REGS_PORT_005D_H

#define SP_G_CLAMP_START               0x00
#define SP_G_CLAMP_END                 0x01
#define SP_BR_CLAMP_START              0x02
#define SP_BR_CLAMP_END                0x03
#define SP_COAST_CTRL                  0x07
#define   SP_COAST_CTRL_COAST_FRONT       0xF0  /* [7:4] Start of COAST Before DeVS Leading Edge [3:0] */
#define   SP_COAST_CTRL_COAST_FRONT_SHIFT 4
#define   SP_COAST_CTRL_COAST_BACK        0x0F  /* [3:0] End of COAST after DeVS Trailing Edge [3:0] */
#define   SP_COAST_CTRL_COAST_BACK_SHIFT  0
#define SP_CAPTURE_WINDOW_SETTING      0x08
#define SP_DETECTION_TOLERANCE_SETTING 0x09

#endif /* RTD2556_REGS_PORT_005D_H */
