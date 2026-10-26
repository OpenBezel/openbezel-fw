"""osdsim - render OpenBezel's OSD (menu, popups) on the host.

The firmware's own menu/GUI C code is compiled natively against recording stubs
of the chip's OSD HAL; a per-chip Python model then turns the recorded OSD state
(SRAM, LUT, registers) into RGBA pixels."""
