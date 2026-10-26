/* RTD2556 scaler page 0x10 */
#ifndef RTD2556_REGS_PAGE_10_H
#define RTD2556_REGS_PAGE_10_H

/* --- Pin Share Register (Page 10) --- */
#define PIN_SHARE_CTRL10   0x1000
#define PIN_SHARE_CTRL11   0x1001
#define   PIN_SHARE_CTRL11_P1D1_GPO_OD            0x02  /* 010 */
#define PIN_SHARE_CTRL14   0x1004
#define   PIN_SHARE_CTRL14_P1D4_GPO_PP            0x01  /* 001 */
#define PIN_SHARE_CTRL30   0x1008
#define   PIN_SHARE_CTRL30_AUXP0_AUXP0            0x03  /* 011 */
#define PIN_SHARE_CTRL31   0x1009
#define   PIN_SHARE_CTRL31_AUXN0_AUXN0            0x03  /* 011 */
#define PIN_SHARE_CTRL32   0x100A
#define   PIN_SHARE_CTRL32_DDC1                   0x80  /* [7] <Priority>Pin73 */
#define   PIN_SHARE_CTRL32_AUXP1_AUXP1            0x03  /* 011 */
#define PIN_SHARE_CTRL33   0x100B
#define   PIN_SHARE_CTRL33_AUXN1_AUXN1            0x03  /* 011 */
#define PIN_SHARE_CTRL34   0x100C
#define   PIN_SHARE_CTRL34_P3D4_A_ADC0_0          0x03  /* 011 */
#define PIN_SHARE_CTRL35   0x100D
#define   PIN_SHARE_CTRL35_P3D5_A_ADC1_0          0x03  /* 011 */
#define PIN_SHARE_CTRL42   0x1012
#define   PIN_SHARE_CTRL42_P4D2_AUDIO_HOUTL       0x03  /* 011 */
#define PIN_SHARE_CTRL43   0x1013
#define   PIN_SHARE_CTRL43_P4D3_AUDIO_HOUTR       0x03  /* 011 */
#define PIN_SHARE_CTRL46   0x1016
#define   PIN_SHARE_CTRL46_P4D6_AUDIO_REF         0x03  /* 011 */
#define PIN_SHARE_CTRL47   0x1017
#define   PIN_SHARE_CTRL47_P4D7_GPO_PP            0x01  /* 001 */
#define PIN_SHARE_CTRL50   0x1018
#define   PIN_SHARE_CTRL50_P5D0_GPO_PP            0x01  /* 001 */
#define PIN_SHARE_CTRL51   0x1019
#define   PIN_SHARE_CTRL51_P5D1_DPTX_AUX_CH_P_1   0x06  /* 110 */
#define PIN_SHARE_CTRL52   0x101A
#define   PIN_SHARE_CTRL52_P5D2_DPTX_AUX_CH_N_1   0x06  /* 110 */
#define PIN_SHARE_CTRL53   0x101B
#define   PIN_SHARE_CTRL53_P5D3_DPTX_HPD1_ORG     0x03  /* 011 */
#define PIN_SHARE_CTRL62   0x1022
#define   PIN_SHARE_CTRL62_P6D2_GPO_PP            0x01  /* 001 */
#define PIN_SHARE_CTRL64   0x1024
#define   PIN_SHARE_CTRL64_P6D4_GPO_PP            0x01  /* 001 */
#define PIN_SHARE_CTRL65   0x1025
#define   PIN_SHARE_CTRL65_P6D5_PWM5_PP           0x03  /* 011 */
#define PIN_SHARE_CTRL71   0x1029
#define   PIN_SHARE_CTRL71_P7D1_GPO_PP            0x01  /* 001 */
#define PIN_SHARE_CTRL72   0x102A
#define   PIN_SHARE_CTRL72_P7D2_GPO_PP            0x01  /* 001 */
#define PIN_SHARE_CTRL74   0x102C
#define   PIN_SHARE_CTRL74_P7D4_GPO_OD            0x02  /* 010 */
#define PIN_SHARE_CTRL77   0x102F
#define   PIN_SHARE_CTRL77_P7D7_GPO_PP            0x01  /* 001 */
#define PIN_SHARE_CTRL80   0x1030
#define   PIN_SHARE_CTRL80_IICSCL_2_GPO_PP        0x01  /* 001 */
#define PIN_SHARE_CTRL81   0x1031
#define   PIN_SHARE_CTRL81_IICSDA_2_GPO_PP        0x01  /* 001 */
#define PIN_SHARE_CTRL82   0x1032
#define   PIN_SHARE_CTRL82_P8D2_GPO_PP            0x01  /* 001 */
#define PIN_SHARE_CTRL83   0x1033
#define   PIN_SHARE_CTRL83_P8D3_GPO_PP            0x01  /* 001 */
#define PIN_SHARE_CTRL91   0x1039
#define   PIN_SHARE_CTRL91_P9D1_GPO_PP            0x01  /* 001 */
#define PIN_SHARE_CTRL92   0x103A
#define   PIN_SHARE_CTRL92_EEI2CSCL_GPO_PP        0x01  /* 001 */
#define PIN_SHARE_CTRL94   0x103C
#define   PIN_SHARE_CTRL94_DDC2                   0x80  /* [7] <Priority>Pin212 */
#define   PIN_SHARE_CTRL94_DDCSCL2_GPO_OD         0x02  /* 010 */
#define PIN_SHARE_CTRL95   0x103D
#define   PIN_SHARE_CTRL95_DDCSDAS2_GPO_OD        0x02  /* 010 */
#define PIN_SHARE_CTRLA0   0x1040
#define   PIN_SHARE_CTRLA0_PAD0_GPO_PP            0x01  /* 001 */
#define PIN_SCHMITT_CTRL0  0x1077
#define   PIN_SCHMITT_CTRL0_DDC1_SCHMITT_EN       0x80  /* [7] <SMT> Pin73,Pin74 */
#define   PIN_SCHMITT_CTRL0_DDC3_SCHMITT_EN       0x40  /* [6] <SMT> Pin214,Pin215 */
#define   PIN_SCHMITT_CTRL0_DDC2_SCHMITT_EN       0x20  /* [5] <SMT> Pin212,Pin213 */
#define   PIN_SCHMITT_CTRL0_DDCVGA_SCHMITT_EN     0x10  /* [4] <SMT> Pin66,Pin67 */
#define   PIN_SCHMITT_CTRL0_EEI2C_SCHMITT_EN      0x08  /* [3] <SMT> Pin210,Pin211 */
#define   PIN_SCHMITT_CTRL0_I2C_2_SCHMITT_EN      0x04  /* [2] <SMT> Pin198,Pin199 */
#define   PIN_SCHMITT_CTRL0_DPTX_HPD_SCHMITT_EN   0x02  /* [1] <SMT> Pin196 */
#define PIN_SCHMITT_CTRL1  0x1078
#define   PIN_SCHMITT_CTRL1_DPTX_HPD1_SCHMITT_EN  0x80  /* [7] <SMT> Pin173 */
#define   PIN_SCHMITT_CTRL1_I2C_1_SCHMITT_EN      0x40  /* [6] <SMT> Pin179,Pin180 */
#define PIN_DRIVING_CTRL00 0x10B0
#define   PIN_DRIVING_CTRL00_PIN66_67_DRIVE       0x80  /* [7] <E2> Pin66, Pin67 */
#define   PIN_DRIVING_CTRL00_PIN73_74_DRIVE       0x20  /* [5] <E2> Pin73, Pin74 */
#define PIN_DRIVING_CTRL02 0x10B2
#define   PIN_DRIVING_CTRL02_PIN185_DRIVE_4MA     0x80  /* 100 */
#define   PIN_DRIVING_CTRL02_PIN186_188_DRIVE_4MA 0x10  /* 100 */
#define PIN_DRIVING_CTRL03 0x10B3
#define   PIN_DRIVING_CTRL03_PIN212_213_DRIVE     0x20  /* [5] <E2> Pin212, Pin213 */
#define   PIN_DRIVING_CTRL03_PIN214_215_DRIVE     0x10  /* [4] <E2> Pin214, Pin215 */

#endif /* RTD2556_REGS_PAGE_10_H */
