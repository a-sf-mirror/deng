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
 * rend_fakeradio.c: Faked Radiosity Lighting
 *
 * Perhaps the most distinctive characteristic of radiosity lighting
 * is that the corners of a room are slightly dimmer than the rest of
 * the surfaces.  (It's not the only characteristic, however.)  We
 * will fake these shadowed areas by generating shadow polygons for
 * wall segments and determining, which subsector vertices will be
 * shadowed.
 *
 * In other words, walls use shadow polygons (over entire segs), while
 * planes use vertex lighting.  Since planes are usually tesselated
 * into a great deal of subsectors (and triangles), they are better
 * suited for vertex lighting.  In some cases we will be forced to
 * split a subsector into smaller pieces than strictly necessary in
 * order to achieve better accuracy in the shadow effect.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_refresh.h"
#include "de_render.h"
#include "de_graphics.h"

// MACROS ------------------------------------------------------------------

#define MIN_OPEN .1f

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int rendFakeRadio = true; // cvar

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static sector_t *frontSector;
static float shadowSize, shadowDark;
static float fFloor, fCeil;

// CODE --------------------------------------------------------------------

/*
 * Before calling the other rendering routines, this must be called to
 * initialize the state of the FakeRadio renderer.
 */ 
void Rend_RadioInitForSector(sector_t *sector)
{
	// By default, the shadow is disabled.
	shadowSize = 0;
	
	if(!rendFakeRadio) return; // Disabled...

	// Visible plane heights.
	fFloor = SECT_FLOOR(sector);
	fCeil  = SECT_CEIL(sector);

	if(fCeil <= fFloor) return; // A closed sector.

	frontSector = sector;
	
	// Determine the shadow properties.
	// FIXME: Make cvars out of constants.
	shadowSize = 2 * (8 + 16 - sector->lightlevel/16);
	shadowDark = .9f - sector->lightlevel/430.0f;
}

/*
 * Returns true if the specified flat is non-glowing, i.e. not glowing
 * or a sky.
 */
boolean Rend_RadioNonGlowingFlat(int flatPic)
{
	return !(flatPic == skyflatnum ||
			 R_FlatFlags(flatPic) & TXF_GLOW);
}

/*
 * Set the vertex colors in the rendpoly.
 */
void Rend_RadioSetColor(rendpoly_t *q, float darkness)
{
	int i;

	// Clamp it.
	if(darkness < 0) darkness = 0;
	if(darkness > 1) darkness = 1;
	
	for(i = 0; i < 2; i++)
	{
		// Shadows are black.
		memset(q->vertices[i].color.rgba, 0, 3);
		q->vertices[i].color.rgba[CA] = (DGLubyte) (255 * darkness);
	}
}

/*
 * Returns true if there is open space in the sector.
 */
boolean Rend_IsSectorOpen(sector_t *sector)
{
	return sector && sector->ceilingheight > sector->floorheight;
}

/*
 * Returns the corner shadow factor.
 */
float Rend_RadioLineCorner(line_t *self, line_t *other, sector_t *mySector)
{
	lineinfo_t *selfInfo = LINE_INFO(self);
	lineinfo_t *otherInfo = LINE_INFO(other);
	vertex_t *myVtx[2], *otherVtx[2];

	binangle_t diff = selfInfo->angle - otherInfo->angle;

	// Sort the vertices so they can be compared consistently.
	R_OrderVertices(self, mySector, myVtx);
	R_OrderVertices(other, mySector, otherVtx);
	
	if(myVtx[0] == other->v1 || myVtx[1] == other->v2)
	{
		// The line normals are not facing the same direction.
		diff -= BANG_180;
	}
	if(myVtx[0] == otherVtx[1])
	{
		// The other is on our left side.
		// We want the difference: (leftmost wall) - (rightmost wall)
		diff = -diff;
	}
	if(diff > BANG_180)
	{
		// The corner between the walls faces outwards.
		return 0;
	}
	if(other->frontsector && other->backsector)
	{
		if(Rend_IsSectorOpen(other->frontsector) &&
		   Rend_IsSectorOpen(other->backsector)) return 0;
	}

	if(diff < BANG_45/5)
	{
		// The difference is too small, there won't be a shadow.
		return 0;
	}
	if(diff > BANG_90)
	{
		// 90 degrees is the largest effective difference.
		diff = BANG_90;
	}
	return (float) diff / BANG_90;
}

/*
 * Set the rendpoly's X offset and texture size.  A negative length
 * implies that the texture is flipped horizontally.
 */
void Rend_RadioTexCoordX(rendpoly_t *q, float lineLength, float segOffset)
{
	q->tex.width = lineLength;
	if(lineLength > 0)
		q->texoffx = segOffset;
	else
		q->texoffx = lineLength + segOffset;
}

/*
 * Set the rendpoly's Y offset and texture size.  A negative size
 * implies that the texture is flipped vertically.
 */ 
void Rend_RadioTexCoordY(rendpoly_t *q, float size)
{
	if((q->tex.height = size) > 0)
		q->texoffy = q->top - fCeil;
	else
		q->texoffy = -(q->top - fFloor);
}

/*
 * Create the appropriate FakeRadio shadow polygons for the wall
 * segment.  The quad must be initialized with all the necessary data
 * (normally comes directly from Rend_RenderWallSeg()).
 */
void Rend_RadioWallSection(seg_t *seg, rendpoly_t *origQuad)
{
	sector_t *backSector;
	float bFloor, bCeil, limit, size, segOffset;
	rendpoly_t quad, *q = &quad;
	int i, k, texture;
	lineinfo_t *info;
	lineinfo_side_t *sInfo;
	float corner[2] = { 0, 0 };
	float proxFloorOff[2] = { 0, 0 }, proxCeilOff[2] = { 0, 0 };
	//boolean openNeighbor[2];
	
	if(shadowSize <= 0 || // Disabled?
	   origQuad->flags & RPF_GLOW ||
	   seg->linedef == NULL) return; 

	info       = LINE_INFO(seg->linedef);
	backSector = seg->backsector;
	segOffset  = FIX2FLT(seg->offset);

	// Choose the info of the correct side.
	if(seg->linedef->frontsector == frontSector)
		sInfo = &info->side[0];
	else
		sInfo = &info->side[1];

	// 0 = left neighbour, 1 = right neighbour
	for(i = 0; i < 2; i++)
	{
		if(sInfo->neighbor[i])
		{
			corner[i] = Rend_RadioLineCorner(seg->linedef, sInfo->neighbor[i],
											 frontSector);
			/*openNeighbor[i] = Rend_IsSectorOpen(
			  info->neighbor[i]->frontsector) &&
			  Rend_IsSectorOpen(info->neighbor[i]->backsector);*/
		}
		if(sInfo->proxsector[i])
		{
			proxFloorOff[i] = SECT_FLOOR(sInfo->proxsector[i]) - fFloor;
			proxCeilOff[i]  = SECT_FLOOR(sInfo->proxsector[i]) - fCeil;
		}				
	}
	
	// Back sector visible plane heights.
	if(backSector)
	{
		bFloor = SECT_FLOOR(backSector);
		bCeil  = SECT_CEIL(backSector);
	}

	// FIXME: rendpoly_t is quite large and this gets called *many*
	// times. Better to copy less stuff, or do something more
	// clever... Like, only pass the geometry part of rendpoly_t.
	memcpy(q, origQuad, sizeof(rendpoly_t));
	
	// Init the quad.
	q->flags = RPF_SHADOW;
	q->texoffx = segOffset;
	q->texoffy = 0;
	q->tex.id = GL_PrepareLSTexture(LST_RADIO_CC);
	q->tex.detail = NULL;
	q->tex.width = info->length;
	q->tex.height = shadowSize;
	q->lights = NULL;
	q->intertex.id = 0;
	q->intertex.detail = NULL;
	Rend_RadioSetColor(q, shadowDark);

	// <--FIXME

	//if(!backSector || bCeil <= bFloor)
	{
		// There is no backsector, or the backsector is closed.

#if 1
		// The top shadow will reach this far down.
		limit = fCeil - shadowSize;
		if((q->top > limit && q->bottom < fCeil) &&
			Rend_RadioNonGlowingFlat(frontSector->ceilingpic))
		{
			q->texoffy = q->top - fCeil;
			q->tex.height = shadowSize;

			if(corner[0] <= MIN_OPEN && corner[1] <= MIN_OPEN)
			{
				texture = LST_GRADIENT; // Radio O/O!
				q->texoffx = segOffset;
				q->tex.width = info->length;
			}
			else if(corner[1] <= MIN_OPEN)
			{
				texture = LST_RADIO_CO;
				q->texoffx = segOffset;
				q->tex.width = info->length;
			}
			else if(corner[0] <= MIN_OPEN)
			{
				texture = LST_RADIO_CO; // Flipped!
				q->texoffx = -info->length + segOffset; // Is this right?
				q->tex.width = -info->length;
			}
			else // C/C
			{
				texture = LST_RADIO_CC;
				q->texoffx = segOffset;
				q->tex.width = info->length;
			}
			q->tex.id = GL_PrepareLSTexture(texture);
			RL_AddPoly(q);
		}
#endif
		
#if 1
		// Bottom shadow.
		limit = fFloor + shadowSize;
		if((q->bottom < limit && q->top > fFloor) &&
		   Rend_RadioNonGlowingFlat(frontSector->floorpic))
		{
			Rend_RadioTexCoordY(q, -shadowSize);
			if(corner[0] <= MIN_OPEN && corner[1] <= MIN_OPEN)
			{
				Rend_RadioTexCoordX(q, info->length, segOffset);
				if(sInfo->proxsector[0] && sInfo->proxsector[1])
				{
					if(proxFloorOff[0] > 0 && proxFloorOff[1] < 0)
					{
						texture = LST_RADIO_CO;
						// The shadow can't go over the higher edge.
						if(shadowSize > proxFloorOff[0])
							Rend_RadioTexCoordY(q, -proxFloorOff[0]);
					}
					else if(proxFloorOff[0] < 0 && proxFloorOff[1] > 0)
					{
						// Must flip horizontally!
						texture = LST_RADIO_CO;
						Rend_RadioTexCoordX(q, -info->length, segOffset);
						// The shadow can't go over the higher edge.
						if(shadowSize > proxFloorOff[1])
							Rend_RadioTexCoordY(q, -proxFloorOff[1]);
					}
					else
						texture = LST_GRADIENT; // Possibly C/C?
				}
				else
				{
					texture = LST_GRADIENT; // Radio O/O!
				}
			}
			else if(corner[1] <= MIN_OPEN)
			{
				texture = LST_RADIO_CO;
				Rend_RadioTexCoordX(q, info->length, segOffset);
			}
			else if(corner[0] <= MIN_OPEN)
			{
				texture = LST_RADIO_CO; // Flipped!
				Rend_RadioTexCoordX(q, -info->length, segOffset);
			}
			else // C/C
			{
				texture = LST_RADIO_CC;
				Rend_RadioTexCoordX(q, info->length, segOffset);
			}
			q->tex.id = GL_PrepareLSTexture(texture);
			RL_AddPoly(q);
		}
#endif

#if 1
		// Left shadow.
		if(corner[0] > 0 &&
		   segOffset < shadowSize)
		{
			q->flags |= RPF_HORIZONTAL;
			q->texoffx = segOffset;
			q->texoffy = 0;
			q->tex.width = shadowSize;
			q->tex.height = fCeil - fFloor;
			q->tex.id = GL_PrepareLSTexture(LST_RADIO_CC);
			Rend_RadioSetColor(q, corner[0] * shadowDark);
			RL_AddPoly(q);
		}

		// Right shadow.
		if(corner[1] > 0 &&
		   segOffset + q->length > info->length - shadowSize)
		{
			q->flags |= RPF_HORIZONTAL;
			q->texoffx = -info->length + segOffset;
			q->texoffy = 0;
			q->tex.width = -shadowSize;
			q->tex.height = fCeil - fFloor;
			q->tex.id = GL_PrepareLSTexture(LST_RADIO_CC);
			Rend_RadioSetColor(q, corner[1] * shadowDark);
			RL_AddPoly(q);
		}
#endif
	}
#if 0	
	else
	{
		// There is an opening to the backsector.

		// The top shadow will reach this far down.
		size = shadowSize;
		if(size > fCeil - bCeil) size = fCeil - bCeil;
		limit = fCeil - size;
		if((q->top > limit && q->bottom < fCeil) &&
			Rend_RadioNonGlowingFlat(frontSector->ceilingpic))
		{
			q->texoffy = q->top - fCeil;
			q->tex.height = size;
			RL_AddPoly(q);
		}
			
		// Bottom shadow.
		size = shadowSize;
		if(size > bFloor - fFloor) size = bFloor - fFloor;
		limit = fFloor + size;
		if((q->bottom < limit && q->top > fFloor) &&
		   Rend_RadioNonGlowingFlat(frontSector->floorpic))
		{
			q->texoffy = -bFloor + fFloor;//fFloor - fCeil;// limit - q->top;
			q->tex.height = -size;
			RL_AddPoly(q);
		}

	}
#endif

	// The placement of the shadow has been calculated, give it to the
	// rendering lists.
//	RL_AddPoly(&quad);
}
