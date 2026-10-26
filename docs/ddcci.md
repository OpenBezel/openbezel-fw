# DDC/CI and MCCS

Host control of the settings over the HDMI DDC bus: VESA DDC/CI 1.1 transport, MCCS 2.2a
command set.  Works with stock tools, e.g. `ddcutil --bus 6 capabilities` / `getvcp 6B` /
`setvcp 6B 40`.

Code: `hal/realtek/rtd2556/ddcci_hw.c` (engine), `core/mccs.c` (protocol + VCP table),
`core/vesa_mccs.h` (the values DDC/CI 1.1 and MCCS 2.2a define; OpenBezel's own codes are in `core/mccs.h`).
Initialized after the ISP recovery window, polled on every main-loop spin.

## Protocol (DDC/CI 1.1, as implemented)

| Direction | Bytes on the wire |
|---|---|
| host write, addr `0x6E` | `51  80\|len  opcode ...  chk` - chk = XOR of everything **including `0x6E`** |
| host read, addr `0x6F` | `6E  80\|len  opcode ...  chk` - chk = XOR starting from `0x50` (virtual host address) |
| null message | `6E 80 BE` - "nothing to say yet / command done" |

Commands handled: Get VCP `01` -> `02` reply (`RC CP TP MH ML SH SL`), Set VCP `03` (no reply),
Timing Request `07` -> `4E` (length byte is `06`, no bit 7: `SS HH HL VH VL`, H in 10 Hz, V in
0.01 Hz), Save `0C`, Capabilities `F3` -> `E3` (offset-addressed fragments; zero data bytes marks the end).
A bad checksum is dropped silently - DDC/CI has no NAK, the host retries.

## Hardware (page FF, `IIC_*`)

Slave address decode `IIC_SET_SLAVE` (`FF23`), the first byte after the address latched as a
sub-address (`IIC_SUB_IN`, the DDC/CI `0x51` source byte), a **16-byte FIFO shared between
receive and transmit** (`IIC_DATA_IN` / `IIC_DATA_OUT`), direction by `IIC_IRQ_CONTROL2`
`DATA_BUF_WEN`, and a hardware null-message responder (`DDCCI_NULL_BYTE1..3`, enabled by
`IIC_CHANNEL_CONTROL` `NULL_MSG_EN`).  Channel auto-switch is on so we follow whichever DDC
channel the host uses.  `IIC_STATUS` flags are write-0-to-clear.

Behavior of the engine (measured with raw i2c transfers):

* **Host bytes written while WEN is set are discarded.**  The FIFO must be turned back to
  receive as soon as the host has drained a reply, not at the next `SUB_I`: a polled `SUB_I`
  comes too late, and a Set VCP following a read reply would be lost.  `ddcci_hw_poll()` clears
  WEN once the FIFO is empty.
* **Never reset the FIFO on `SUB_I`.**  A slow poll finds the whole message already in the FIFO
  (sometimes a Set VCP and the verifying Get VCP together); a reset drops it.  Stock's ISR
  only clears WEN there too.  `mccs.c` splits back-to-back messages by their length bytes.
* A host read with received bytes still in the FIFO returns **those bytes** (`82 01 10 AC ...`),
  not the null message.  Harmless - the framing check fails host-side and it retries - but it is
  what a late poll looks like on the bus.

Replies are kept to <= 16 bytes so they fit the FIFO: capabilities go out 8 data bytes a
fragment (5 + 8 + 1 = 14).

## No clock stretching - deliberate

Stock holds SCL low on DDC1 from the end of a host message until its
main loop queues the reply.  We do not.  If the firmware ever hung in that window it would hold
the HDMI DDC bus down indefinitely, and that bus is the only reflash path.
Without it the reply has to be ready when the host reads (~40-50 ms after asking).  Polling every
spin makes that true in almost every case (38/40 at 45 ms, 20/20 at 200 ms, ddcutil verify
10/10); an occasional main-loop stall costs the host one null message and a retry, which the
spec allows.

## VCP codes

| Code | Name | Maps to | Notes |
|---|---|---|---|
| 02 | New control value | menu change flag | set by the menu; host writes 01 to clear |
| 04 / 05 / 08 | Restore defaults: all / lum+contrast / color | `settings_defaults` etc. | momentary |
| 10 | Luminance | **video** brightness (CTS/BRI offset), 0..100 = -50..+50 | not the backlight |
| 12 | Contrast | contrast, 0..100 = -50..+50 | |
| 14 | Color preset | Warm = 04 (5000 K), Normal = 05 (6500 K), Cool = 08 (9300 K), User = 0B | 01/02 -> Normal, 03 -> Warm, 06-0A -> Cool |
| 16 / 18 / 1A | Video gain R / G / B | the User preset's gains, 0..100 (100 = neutral, lower cuts) | writing one selects User (as AOC's firmware does) |
| 52 | Active control | last code changed from the menu | single-slot FIFO |
| 60 | Input source | 11 (HDMI 1) | read-only in practice: one input |
| 6B | Backlight level: white | PWM5 duty, 0..100 % (floor 5 %) | the backlight dimmer |
| 13 | Backlight (MCCS 2.0) | alias of 6B | not in the capabilities string - deprecated in 2.2 |
| 72 | Gamma | presets 1.8 / 2.0 / 2.2 / 2.4, native 2.2, bypass | see below |
| 87 | Sharpness | scale-up filter preset 0..4 | |
| 8A | Saturation | 0..200, 100 = normal | |
| AC / AE | H / V frequency | measured timing, 0 when not active | Hz (24-bit) / 0.01 Hz |
| B2 / B6 | Sub-pixel layout / technology | 01 RGB stripe / 03 LCD active matrix | fixed |
| C0 | Display usage time | hours with the panel lit | booked every 20 min into the settings log; kept by a reset |
| C8 | Controller | 09 = RealTek, chip 002556 | fixed |
| C9 | Firmware level | 0.1 | fixed |
| CA | OSD | 01 = menu locked out, 02 = enabled | |
| DC | Display application | picture mode: 00 Standard, 01 Text (productivity), 03 Movie, 05 Game; reads 04 (user defined) when the settings match no preset | core/picmode.c |
| CC | OSD language | the build's LANGS as MCCS codes (02 en, 04 de, 03 fr, 0A es, 05 it) | others ignored |
| D6 | Power mode | 01 on, 02-05 backlight off (third gate beside the user setting and powersave) | |
| DF | VCP version | 2.2 | |
| E0 | *mfr* Boot logo | `boot_logo` 0/1 | |
| E1 | *mfr* Mode popup | `res_popup` 0/1 | |
| E2 | *mfr* Powersave | `powersave` 0..3 (off, 5 s, 30 s, 2 min) | |
| E3 | *mfr* Input dither | `idither` 0/1 | |
| E4 | *mfr* Display dither | `ddither` 0/1 | |
| E5 | *mfr* Gamma preset | `gamma` 0 = off, 1..4 | the raw index; 72h is the standard route |
| E6 | *mfr* Low blue light | `low_blue` 0 off, 1-3 low / medium / high | blue gain cut on top of the color preset |
| EA | *mfr* Dynamic contrast | `dcr` 0/1 | the backlight follows the picture's average level (core/dcr.h) |
| E7 | *mfr* UI mode | 0 splash, 1 input (HDMI / USB-C), 2 debug console (with `CONFIG_DEBUG_CONSOLE`) | |
| E8 | *mfr* Video blank | force background on/off | |
| E9 | *mfr* Key repeat | 0 off, 1 slow, 2 normal, 3 fast | |
| 86 | Display scaling | 02 Fit (03, 09 accepted), 05 Full (06, 08 accepted) | |

## Gamma (72h)

The four gamma presets are corrections on top of the panel's own response, not absolute curves:
`out = in ^ (preset / native)` per channel (docs/building.md, "The gamma tables"), so they are
1.8, 2.0, 2.2 and 2.4 on a panel whose native gamma is ~2.2 - inferred from stock's tables,
whose exponents come in exactly those ratios, not measured with a meter.  The tolerance is 05.
Capabilities: `72(05 78 FA 50 64 78 8C)` - tolerance 5 %, native 2.2, specific presets with
bypass, then 1.8 / 2.0 / 2.2 / 2.4.

* Set, SL `00` (white absolute): snaps to the nearest preset.
* Set, SL `04` (white relative to default): SH `00` = our default (bypass); `01`-`0A` /
  `11`-`1A` = 2.2 -/+ 0.1 steps, snapped; `20` = bypass.
* Set, SL `05`: bypass.  SL `01`-`03` (single channel) is ignored - there is no per-channel
  LUT choice.
* Get: `SH` = the preset's value with SL `00`, or `78 05` (native, disabled) when bypassed.

The menu shows the presets by these values (Off / 1.8 / 2.0 / 2.2 / 2.4).

## Hosts that use it on their own

**KDE Plasma (PowerDevil) drives 10h** through libddcutil: on every hotplug - and a resolution
change is one - it probes 10h and writes its own saved brightness back, so KDE's "100 %" sets
our video brightness to +50 after each mode change.  That is the host doing what MCCS says,
not a firmware fault.  Either use KDE's brightness slider as the video brightness control, or turn
DDC/CI off for the monitor in KDE's Display Configuration.  The debug console's row 2 (`dc`
count, last code, last value) shows every Set VCP a host sends.

System > **DDC/CI** Off makes the firmware stop answering (the engine is still drained), for when
a host misbehaves.

## Debug peek (not advertised)

The debug codes - F0h-FFh, ECh-EFh and the Set VCP log (`mccs_log`) - are built only with
`CONFIG_DEBUG_VCP` (Kconfig > Debug, on by default); without it they answer "unsupported".

`setvcp F0 <addr>` then `getvcp F1` returns the byte at that XDATA / scaler address (SH) and the
next (SL); `setvcp F2 <idx>` then `getvcp F2` does the same through the display-format port
(002A/002B).  Reads have their normal side effects, so not for data ports or read-to-clear status.
It reads RAM live, without ISP halting the chip.

## Stock, for comparison

Capabilities string at flash `0x389E6`:
`vcp(02 04 05 06 08 0B 0C 10 12 14(01 02 04 05 06 08 0B) 16 18 1A 52 60(01 03 04 0F 10 11 12)
87 AC AE B2 B6 C6 C8 CA CC(01 02 03 04 06 0A 0D) D6(01 04 05) DF FD FF)`, model `RTK`.  It is a
generic Realtek string (six inputs on a one-input board) and has no 6Bh, so its 10h most likely
drove the backlight PWM - unconfirmed.

Stock's IE1 ISR clears WEN within microseconds of SUB_I.

## Candidates, from the MCCS code list

In rough value order - each is a feature, not just a code:

* **86 Display scaling** - no scaling / fill (Fit and Full)
* **6C / 6E / 70 black level R/G/B** - the CTS/BRI offsets, per channel.
* **0B / 0C color temperature increment / request** - Kelvin-addressed color temp, needs a
  Kelvin -> gains table.
* **B0 Settings** (store/restore).
