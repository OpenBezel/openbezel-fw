/* Dynamic contrast (DCR): with ob_settings.dcr on and a picture up, the backlight
 * follows the picture's average level - full BL level for a bright scene, down to
 * DCR_FLOOR % of it for a black one - which deepens blacks in dark scenes.  The
 * backlight alone, the gains are not touched.
 *
 * The level comes from the scaler's DCR measurement block (page 07, 07D8/07D9
 * index port): with threshold 1 at 0 it counts every pixel and sums R+G+B, so
 * sum / count / 3 is the average picture level, 0..255.  Index 02-04 = pixel
 * count (1920x1080 = 1FA400), 05-08 = the sum; it measures continuously once
 * 07D8 bit 0 is set. */
#ifndef OB_DCR_H
#define OB_DCR_H

#include "ob_types.h"

void dcr_tick(u16 t0ovf) __banked;      /* main loop; cheap between 50-ms steps */

#endif
