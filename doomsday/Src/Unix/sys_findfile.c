/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * sys_findfile.c: Win32-Style File Finding (findfirst/findnext)
 */

// HEADER FILES ------------------------------------------------------------

#include <glob.h>

#include "Unix/sys_findfile.h"

// MACROS ------------------------------------------------------------------

#define FIND_ERROR  -1

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/*
 * Returns zero if successful.
 */
int myfindfirst(const char *filename, finddata_t *dta)
{
/*	dta->hFile = _findfirst(filename, &dta->data);
	
	dta->date = dta->data.time_write;
	dta->time = dta->data.time_write;
	dta->size = dta->data.size;
	dta->name = dta->data.name;
	dta->attrib = dta->data.attrib;

	return dta->hFile<0;*/
	return FIND_ERROR;
}

/*
 * Returns zero if successful.
 */
int myfindnext(finddata_t *dta)
{
/*	int r = _findnext(dta->hFile, &dta->data);

	dta->date = dta->data.time_write;
	dta->time = dta->data.time_write;
	dta->size = dta->data.size;
	dta->name = dta->data.name;
	dta->attrib = dta->data.attrib;

	return r;*/
	return FIND_ERROR;
}

void myfindend(finddata_t *dta)
{
//	_findclose(dta->hFile);
}
