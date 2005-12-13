/* $Id$
 *
 * Copyright (C) 1993-1996 by id Software, Inc.
 *
 * This source is available for distribution and/or modification
 * only under the terms of the DOOM Source Code License as
 * published by id Software. All rights reserved.
 *
 * The source is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
 * for more details.
 */

/*
 * Sky rendering.
 *
 * TODO: Move contents to another file.
 *       REMOVE FROM SRC TREE
 */

// HEADER FILES ------------------------------------------------------------

#include "r_data.h" // Needed for Flat retrieval.

#ifdef __GNUG__
#  pragma implementation "r_sky.h"
#endif

#include "r_sky.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int     skyflatnum;
int     skytexture;
int     skytexturemid;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/*
 * Called whenever the view size changes.
 */
void R_InitSkyMap(void)
{
    // skyflatnum = R_FlatNumForName ( SKYFLATNAME );
    skytexturemid = 100 * FRACUNIT;
}
