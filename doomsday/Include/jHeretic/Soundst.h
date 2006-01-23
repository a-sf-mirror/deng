
// soundst.h

#ifndef __SOUNDSTH__
#define __SOUNDSTH__

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

#include "jHeretic/r_defs.h"

#include "jHeretic/Sounds.h"                // Sfx and music indices.

void            S_LevelMusic(void);
void            S_SectorSound(sector_t *sector, int sound_id);

#endif
