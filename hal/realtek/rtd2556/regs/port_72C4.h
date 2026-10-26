/* RTD2556 keyhole: HDCP (index 72C3, data 72C4) */
#ifndef RTD2556_REGS_PORT_72C4_H
#define RTD2556_REGS_PORT_72C4_H

#define HDCP_BKSV0     0x00
#define HDCP_BCAPS     0x40
#define   HDCP_BCAPS_HDCP_HDMI_RESERVED 0x80  /* [7] HDMI_RESERVED Use of This Bit Is Reserved. HDCP Receivers Not Capab... */
#define   HDCP_BCAPS_HDCP_REPEATER      0x40  /* [6] HDCP Repeater Capability. When Set to One, This HDCP Receiver Suppo... */
#define   HDCP_BCAPS_HDCP_FAST_REAUTHEN 0x01  /* [0] When Set to 1, The Receiver Is Capable of Receiving (unencrypted) V... */
#define HDCP_SLAVE_ADD 0xC2
#define   HDCP_SLAVE_ADD_SLAVE_ADDR     0x7F  /* [6:0] HDCP Slave_Addr[6:0] */

#endif /* RTD2556_REGS_PORT_72C4_H */
