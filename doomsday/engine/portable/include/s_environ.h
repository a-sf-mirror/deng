/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2009 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * s_environ.h: Environmental Sound Effects
 */

#ifndef __DOOMSDAY_SOUND_ENVIRON_H__
#define __DOOMSDAY_SOUND_ENVIRON_H__

#include "p_maptypes.h"

/**
 * @defgroup materialEnvironmentFlags Material Environment Flags
 */
/*@{*/
#define MEF_BLEND           0x1 // Enable blending between materials.
/*@}*/

typedef struct {
    const char  name[9];    // Material type name.
    byte        flags; // MEF_* flags.
    int         volumeMul;
    int         decayMul;
    int         dampingMul;
} materialenvinfo_t;

material_env_class_t S_MaterialClassForName(const char* name,
                                            material_namespace_t mnamespace);
const materialenvinfo_t* S_MaterialEnvDef(material_env_class_t id);
#endif
