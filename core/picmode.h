/* Picture modes: presets that set a bundle of picture settings at once
 * (Standard, Movie, Game, Text).  The mode shown is derived, not stored: if the
 * settings match a preset that preset is the mode, otherwise it is Custom - so
 * editing any of them afterwards needs no hook.  Used by the menu and DDC/CI. */
#ifndef OB_PICMODE_H
#define OB_PICMODE_H

#include "ob_types.h"

#define PICMODE_CUSTOM   0
#define PICMODE_STANDARD 1
#define PICMODE_MOVIE    2
#define PICMODE_GAME     3
#define PICMODE_TEXT     4
#define PICMODE_COUNT    5

u8 picmode_now(void);           /* PICMODE_*: the preset the settings match, or CUSTOM */
void picmode_apply(u8 mode);    /* set the preset's settings and push them to the hardware */

#endif
