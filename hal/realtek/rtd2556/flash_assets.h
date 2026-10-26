/* Streaming large tables out of the flash asset partition.
 *
 * The 64 KB code bank is for code.  Tables over ~1 KB live in the asset
 * partition at ASSET_BASE and are streamed into their
 * destination scaler port by the chip's burst engine, which reads flash
 * directly with no CPU involvement - every one of our big
 * tables is destined for a port anyway.
 *
 * RECOVERY CONSTRAINT: nothing here may run before the ISP break-in window in
 * main().  The recovery path (DDC up, EDID served, HPD asserted, 1 s delay)
 * must stay independent of the asset partition, so that a blank or corrupt
 * partition can never cost us the ability to reflash.  board_edid[] therefore
 * stays a __code array and must not be moved here.
 *
 * The firmware cannot read the partition itself - its code window is the first
 * 64 KB - so there is no runtime integrity check.  Callers must be safe when
 * the partition is blank: gamma defaults to bypass, so an unflashed partition
 * cannot affect the boot picture.
 */
#ifndef RTD2556_FLASH_ASSETS_H
#define RTD2556_FLASH_ASSETS_H

#include "ob_types.h"
#include "assets.h"

/* Stream `len` bytes from flash offset `off` (within the asset partition) into
 * `page`:`port`, with the port address held fixed - the destination is a FIFO
 * port, not a range of registers.  The MCU halts for the duration. */
void asset_burst_to_port(u16 off, u16 len, u8 page, u8 port);

/* The same from an absolute flash address (the HDCP keys in flash bank 1:
 * the bytes go flash -> port without ever passing through the CPU). */
void flash_burst_to_port(u32 addr, u16 len, u8 page, u8 port);

/* Set once bring-up has passed the ISP break-in window.  asset_burst_to_port()
 * is a no-op until then, so a regression that pulls an asset load earlier
 * fails safe instead of risking the recovery path. */
void assets_enable(void);

#endif /* RTD2556_FLASH_ASSETS_H */
