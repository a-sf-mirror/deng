/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Ker�nen <jaakko.keranen@iki.fi>
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
 * m_filehash.h: File Name Hash Table
 */

#ifndef __DOOMSDAY_MISC_FILE_HASH_H__
#define __DOOMSDAY_MISC_FILE_HASH_H__

void	FH_Clear(void);
void	FH_Init(const char *pathList);
boolean	FH_Find(const char *name, char *foundPath);

#endif 
