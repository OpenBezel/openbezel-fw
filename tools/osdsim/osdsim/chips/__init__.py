"""Chip models.  A chip module provides:

  NAME, PANEL (w, h)
  HOST_SOURCES      C files under tools/osdsim/host/ replacing the chip's OSD HAL
  FIRMWARE_SOURCES  firmware C files compiled for the host (repo-relative)
  INCLUDE_DIRS      firmware include dirs (repo-relative)
  STATE_ARRAYS      {name: bytes} the host build records (exported as osdsim_<name>)
  render(state) -> Frame (image RGBA at the OSD's scale, pos on the panel, scale)

Boards map to a chip and supply the board directory (board.h, assets)."""
from . import rtd2556

CHIPS = {rtd2556.NAME: rtd2556}

BOARDS = {
    "intouch156b": {"chip": "rtd2556", "dir": "boards/intouch156b"},
}


def chip_for(board):
    return CHIPS[BOARDS[board]["chip"]]
