/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2003-2005 Samuel Villarreal <svkaiser@gmail.com>
 *\author Copyright © 1999 by Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman (PrBoom 2.2.6)
 *\author Copyright © 1999-2000 by Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze (PrBoom 2.2.6)
 *\author Copyright © 1993-1996 by id Software, Inc.
 *
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

/*
 *  Floor animation: raising stairs.
 */

// HEADER FILES ------------------------------------------------------------

#include "doom64tc.h"

#include "dmu_lib.h"
#include "p_map.h"
#include "p_mapspec.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/*
 * Move a plane (floor or ceiling) and check for crushing
 */
result_e T_MovePlane(sector_t *sector, fixed_t speed, fixed_t dest,
                     boolean crush, int floorOrCeiling, int direction)
{
    boolean flag;
    fixed_t lastpos;
    fixed_t floorheight, ceilingheight;
    int ptarget = (floorOrCeiling? DMU_CEILING_TARGET : DMU_FLOOR_TARGET);
    int pspeed = (floorOrCeiling? DMU_CEILING_SPEED : DMU_FLOOR_SPEED);

    // Let the engine know about the movement of this plane.
    P_SetFixedp(sector, ptarget, dest);
    P_SetFixedp(sector, pspeed, speed);

    floorheight = P_GetFixedp(sector, DMU_FLOOR_HEIGHT);
    ceilingheight = P_GetFixedp(sector, DMU_CEILING_HEIGHT);

    switch(floorOrCeiling)
    {
    case 0:
        // FLOOR
        switch(direction)
        {
        case -1:
            // DOWN
            if(floorheight - speed < dest)
            {
                // The move is complete.
                lastpos = floorheight;
                P_SetFixedp(sector, DMU_FLOOR_HEIGHT, dest);
                //P_SetFixedp(sector, pspeed, 0);

                flag = P_ChangeSector(sector, crush);
                if(flag == true)
                {
                    // Oh no, the move failed.
                    P_SetFixedp(sector, DMU_FLOOR_HEIGHT, lastpos);
                    P_SetFixedp(sector, ptarget, lastpos);
                    P_ChangeSector(sector, crush);
                }
                return pastdest;
            }
            else
            {
                lastpos = floorheight;
                P_SetFixedp(sector, DMU_FLOOR_HEIGHT, lastpos - speed);
                flag = P_ChangeSector(sector, crush);
                if(flag == true)
                {
                    P_SetFixedp(sector, DMU_FLOOR_HEIGHT, lastpos);
                    P_SetFixedp(sector, ptarget, lastpos);
                    //P_SetFixedp(sector, pspeed, 0);
                    P_ChangeSector(sector, crush);
                    return crushed;
                }
            }
            break;

        case 1:
            // UP
            if(floorheight + speed > dest)
            {
                // The move is complete.
                lastpos = floorheight;
                P_SetFixedp(sector, DMU_FLOOR_HEIGHT, dest);
                //P_SetFixedp(sector, pspeed, 0);
                flag = P_ChangeSector(sector, crush);
                if(flag == true)
                {
                    // Oh no, the move failed.
                    P_SetFixedp(sector, DMU_FLOOR_HEIGHT, lastpos);
                    P_SetFixedp(sector, ptarget, lastpos);
                    P_ChangeSector(sector, crush);
                }
                return pastdest;
            }
            else
            {
                // COULD GET CRUSHED
                lastpos = floorheight;
                P_SetFixedp(sector, DMU_FLOOR_HEIGHT, lastpos + speed);
                flag = P_ChangeSector(sector, crush);
                if(flag == true)
                {
                    if(crush == true)
                        return crushed;

                    P_SetFixedp(sector, DMU_FLOOR_HEIGHT, lastpos);
                    P_SetFixedp(sector, ptarget, lastpos);
                    //P_SetFixedp(sector, pspeed, 0);
                    P_ChangeSector(sector, crush);
                    return crushed;
                }
            }
            break;
        }
        break;

    case 1:
        // CEILING
        switch (direction)
        {
        case -1:
            // DOWN
            if(ceilingheight - speed < dest)
            {
                // The move is complete.
                lastpos = ceilingheight;
                P_SetFixedp(sector, DMU_CEILING_HEIGHT, dest);
                //P_SetFixedp(sector, pspeed, 0);
                flag = P_ChangeSector(sector, crush);
                if(flag == true)
                {
                    P_SetFixedp(sector, DMU_CEILING_HEIGHT, lastpos);
                    P_SetFixedp(sector, ptarget, lastpos);
                    P_ChangeSector(sector, crush);
                }
                return pastdest;
            }
            else
            {
                // COULD GET CRUSHED
                lastpos = ceilingheight;
                P_SetFixedp(sector, DMU_CEILING_HEIGHT, lastpos - speed);
                flag = P_ChangeSector(sector, crush);
                if(flag == true)
                {
                    if(crush == true)
                        return crushed;

                    P_SetFixedp(sector, DMU_CEILING_HEIGHT, lastpos);
                    P_SetFixedp(sector, ptarget, lastpos);
                    //P_SetFixedp(sector, pspeed, 0);
                    P_ChangeSector(sector, crush);

                    return crushed;
                }
            }
            break;

        case 1:
            // UP
            if(ceilingheight + speed > dest)
            {
                // The move is complete.
                lastpos = ceilingheight;
                P_SetFixedp(sector, DMU_CEILING_HEIGHT, dest);
                //P_SetFixedp(sector, pspeed, 0);
                flag = P_ChangeSector(sector, crush);
                if(flag == true)
                {
                    P_SetFixedp(sector, DMU_CEILING_HEIGHT, lastpos);
                    P_SetFixedp(sector, ptarget, lastpos);
                    P_ChangeSector(sector, crush);
                }
                return pastdest;
            }
            else
            {
                lastpos = ceilingheight;
                P_SetFixedp(sector, DMU_CEILING_HEIGHT, lastpos + speed);
                flag = P_ChangeSector(sector, crush);
            }
            break;
        }
        break;

    }
    return ok;
}

/*
 * MOVE A FLOOR TO IT'S DESTINATION (UP OR DOWN)
 */
void T_MoveFloor(floormove_t * floor)
{
    xsector_t *xsec = P_XSector(floor->sector);
    result_e res =
        T_MovePlane(floor->sector, floor->speed, floor->floordestheight,
                    floor->crush, 0, floor->direction);

    if(!(leveltime & 7))
        S_SectorSound(floor->sector, SORG_FLOOR, sfx_stnmov);

    if(res == pastdest)
    {
        P_SetIntp(floor->sector, DMU_FLOOR_SPEED, 0);

        xsec->specialdata = NULL;

        if(floor->direction == 1)
        {
            switch (floor->type)
            {
            case donutRaise:
                xsec->special = floor->newspecial;

                P_SetIntp(floor->sector, DMU_FLOOR_TEXTURE,
                          floor->texture);

            default:
                break;
            }
        }
        else if(floor->direction == -1)
        {
            switch (floor->type)
            {
            case lowerAndChange:
                xsec->special = floor->newspecial;

                P_SetIntp(floor->sector, DMU_FLOOR_TEXTURE,
                          floor->texture);

            default:
                break;
            }
        }
        P_RemoveThinker(&floor->thinker);

        // S_SectorSound(floor->sector, SORG_FLOOR, sfx_pstop); // d64tc
    }
}

/*
 * HANDLE FLOOR TYPES
 */
int EV_DoFloor(line_t *line, floor_e floortype)
{
    int         i, tag;
    int         rtn = 0;
    int         bottomtexture;
    xsector_t  *xsec;
    sector_t   *sec = NULL;
    sector_t   *frontsector;
    line_t     *ln;
    floormove_t *floor;

    // d64tc > bitmip? wha?
    int bitmipL = 0, bitmipR = 0;
    side_t *front = P_GetPtrp(line, DMU_SIDE0);
    side_t *back  = P_GetPtrp(line, DMU_SIDE1);

    bitmipL = P_GetIntp(front, DMU_MIDDLE_TEXTURE_OFFSET_X) >> FRACBITS;
    if(back)
        bitmipR = P_GetIntp(back, DMU_MIDDLE_TEXTURE_OFFSET_X) >> FRACBITS;
    // < d64tc

    tag = P_XLine(line)->tag;
    while((sec = P_IterateTaggedSectors(tag, sec)) != NULL)
    {
        xsec = P_XSector(sec);
        // ALREADY MOVING?  IF SO, KEEP GOING...
        if(xsec->specialdata)
            continue;

        // new floor thinker
        rtn = 1;
        floor = Z_Malloc(sizeof(*floor), PU_LEVSPEC, 0);
        P_AddThinker(&floor->thinker);
        xsec->specialdata = floor;
        floor->thinker.function = T_MoveFloor;
        floor->type = floortype;
        floor->crush = false;

        switch (floortype)
        {
        case lowerFloor:
            floor->direction = -1;
            floor->sector = sec;
            //floor->speed = FLOORSPEED;
            floor->speed = FLOORSPEED * 4; // d64tc
            floor->floordestheight = P_FindHighestFloorSurrounding(sec);
            break;

        case lowerFloorToLowest:
            floor->direction = -1;
            floor->sector = sec;
            //floor->speed = FLOORSPEED;
            floor->speed = FLOORSPEED * 4; // d64tc
            floor->floordestheight = P_FindLowestFloorSurrounding(sec);
            break;

        case turboLower:
            floor->direction = -1;
            floor->sector = sec;
            floor->speed = FLOORSPEED * 4;
            floor->floordestheight = P_FindHighestFloorSurrounding(sec);
            if(floor->floordestheight != P_GetFixedp(sec,
                                                     DMU_FLOOR_HEIGHT))
                floor->floordestheight += 8 * FRACUNIT;
            break;

        case lowerToEight: // d64tc
            floor->direction = -1;
            floor->sector = sec;
            floor->speed = FLOORSPEED;
            floor->floordestheight = P_FindHighestFloorSurrounding(sec);
            if(floor->floordestheight != P_GetFixedp(sec,
                                                     DMU_FLOOR_HEIGHT))
                floor->floordestheight += 8 * FRACUNIT;
            break;

        case customFloor: // d64tc
            if(bitmipR > 0)
            {
                floor->direction = -1;
                floor->sector = sec;
                floor->speed = FLOORSPEED * bitmipL;
                floor->floordestheight = P_FindHighestFloorSurrounding(sec);

                if(floor->floordestheight != P_GetFixedp(sec,
                                                         DMU_FLOOR_HEIGHT))
                    floor->floordestheight += (bitmipR * FRACUNIT);
            }
            else
            {
                floor->direction = 1;
                floor->sector = sec;
                floor->speed = FLOORSPEED * bitmipL;
                floor->floordestheight =
                    P_GetFixedp(floor->sector, DMU_FLOOR_HEIGHT) - bitmipR * FRACUNIT;
            }
            break;

        case customChangeSec: // d64tc
            floor->direction = 1;
            floor->sector = sec;
            floor->speed = FLOORSPEED * 16;
            floor->floordestheight = P_GetFixedp(floor->sector, DMU_FLOOR_HEIGHT);

            // KLUDGE: fake the engine into accepting this special
            P_XSector(sec)->special = bitmipR;
            // < KLUDGE
            break;

        case raiseFloorCrush:
            floor->crush = true;
        case raiseFloor:
            floor->direction = 1;
            floor->sector = sec;
            //floor->speed = FLOORSPEED;
            floor->speed = FLOORSPEED * 4; // d64tc
            floor->floordestheight = P_FindLowestCeilingSurrounding(sec);

            if(floor->floordestheight >
               P_GetFixedp(sec, DMU_CEILING_HEIGHT))
                floor->floordestheight =
                    P_GetFixedp(sec, DMU_CEILING_HEIGHT);

            floor->floordestheight -=
                (8 * FRACUNIT) * (floortype == raiseFloorCrush);
            break;

        case raiseFloorTurbo:
            floor->direction = 1;
            floor->sector = sec;
            //floor->speed = FLOORSPEED * 4;
            floor->speed = FLOORSPEED * 8; // d64tc
            floor->floordestheight =
                P_FindNextHighestFloor(sec, P_GetFixedp(sec,
                                                        DMU_FLOOR_HEIGHT));
            break;

        case raiseFloorToNearest:
            floor->direction = 1;
            floor->sector = sec;
            //floor->speed = FLOORSPEED;
            floor->speed = FLOORSPEED * 8; // d64tc
            floor->floordestheight =
                P_FindNextHighestFloor(sec, P_GetFixedp(sec,
                                                        DMU_FLOOR_HEIGHT));
            break;

        case raiseFloor24:
            floor->direction = 1;
            floor->sector = sec;
            //floor->speed = FLOORSPEED;
            floor->speed = FLOORSPEED * 8; // d64tc
            floor->floordestheight =
                P_GetFixedp(floor->sector,
                            DMU_FLOOR_HEIGHT) + 24 * FRACUNIT;
            break;

        case raiseFloor512:
            floor->direction = 1;
            floor->sector = sec;
            floor->speed = FLOORSPEED;
            floor->floordestheight =
                P_GetFixedp(floor->sector,
                            DMU_FLOOR_HEIGHT) + 512 * FRACUNIT;
            break;

        case raiseFloor24AndChange:
            floor->direction = 1;
            floor->sector = sec;
            //floor->speed = FLOORSPEED;
            floor->speed = FLOORSPEED * 8; // d64tc
            floor->floordestheight =
                P_GetFixedp(floor->sector,
                            DMU_FLOOR_HEIGHT) + 24 * FRACUNIT;

            frontsector = P_GetPtrp(line, DMU_FRONT_SECTOR);

            P_SetIntp(sec, DMU_FLOOR_TEXTURE,
                      P_GetIntp(frontsector, DMU_FLOOR_TEXTURE));

            xsec->special = P_XSector(frontsector)->special;
            break;

        case raiseFloor32: // d64tc
            floor->direction = 1;
            floor->sector = sec;
            floor->speed = FLOORSPEED * 8;
            floor->floordestheight =
                P_GetFixedp(floor->sector,
                            DMU_FLOOR_HEIGHT) + 32 * FRACUNIT;
            break;

        case raiseToTexture:
            {
                int     minsize = MAXINT;
                side_t *side;

                floor->direction = 1;
                floor->sector = sec;
                floor->speed = FLOORSPEED;
                for(i = 0; i < P_GetIntp(sec, DMU_LINE_COUNT); i++)
                {
                    ln = P_GetPtrp(sec, DMU_LINE_OF_SECTOR | i);

                    if(P_GetIntp(ln, DMU_FLAGS) & ML_TWOSIDED)
                    {
                        side = P_GetPtrp(ln, DMU_SIDE0);
                        bottomtexture = P_GetIntp(side, DMU_BOTTOM_TEXTURE);
                        if(bottomtexture >= 0)
                        {
                            Set(DD_TEXTURE_HEIGHT_QUERY, bottomtexture);
                            if(Get(DD_QUERY_RESULT) < minsize)
                                minsize = Get(DD_QUERY_RESULT);
                        }

                        side = P_GetPtrp(ln, DMU_SIDE1);
                        bottomtexture = P_GetIntp(side, DMU_BOTTOM_TEXTURE);
                        if(bottomtexture >= 0)
                        {
                            Set(DD_TEXTURE_HEIGHT_QUERY, bottomtexture);

                            if(Get(DD_QUERY_RESULT) < minsize)
                                minsize = Get(DD_QUERY_RESULT);
                        }
                    }
                }
                floor->floordestheight =
                    P_GetFixedp(floor->sector, DMU_FLOOR_HEIGHT)
                    + minsize;
                break;
            }

        case lowerAndChange:
            floor->direction = -1;
            floor->sector = sec;
            floor->speed = FLOORSPEED;
            floor->floordestheight = P_FindLowestFloorSurrounding(sec);
            floor->texture = P_GetIntp(sec, DMU_FLOOR_TEXTURE);

            for(i = 0; i < P_GetIntp(sec, DMU_LINE_COUNT); i++)
            {
                // Choose the correct texture and special on two sided lines.
                ln = P_GetPtrp(sec, DMU_LINE_OF_SECTOR | i);

                if(P_GetIntp(ln, DMU_FLAGS) & ML_TWOSIDED)
                {
                    if(P_GetPtrp(ln, DMU_FRONT_SECTOR) == sec)
                    {
                        sec = P_GetPtrp(ln, DMU_BACK_SECTOR);
                        if(P_GetFixedp(sec, DMU_FLOOR_HEIGHT) ==
                                                      floor->floordestheight)
                        {
                            floor->texture =
                                P_GetIntp(sec,DMU_FLOOR_TEXTURE);
                            floor->newspecial =
                                P_XSector(sec)->special;
                            break;
                        }
                    }
                    else
                    {
                        sec = P_GetPtrp(ln, DMU_FRONT_SECTOR);
                        if(P_GetFixedp(sec, DMU_FLOOR_HEIGHT) ==
                                                      floor->floordestheight)
                        {
                            floor->texture =
                                P_GetIntp(sec, DMU_FLOOR_TEXTURE);
                            floor->newspecial =
                                P_XSector(sec)->special;
                            break;
                        }
                    }
                }
            }
        default:
            break;
        }
    }
    return rtn;
}

int EV_BuildStairs(line_t *line, stair_e type)
{
    int         i, ok, height, texture, tag;
    int         rtn = 0;
    fixed_t     stairsize = 0;
    fixed_t     speed = 0;
    line_t     *ln;
    xsector_t  *xsec;
    sector_t   *sec = NULL;
    sector_t   *tsec;
    floormove_t *floor;

    tag = P_XLine(line)->tag;
    while((sec = P_IterateTaggedSectors(tag, sec)) != NULL)
    {
        xsec = P_XSector(sec);
        // ALREADY MOVING?  IF SO, KEEP GOING...
        if(xsec->specialdata)
            continue;

        // new floor thinker
        rtn = 1;
        floor = Z_Malloc(sizeof(*floor), PU_LEVSPEC, 0);
        P_AddThinker(&floor->thinker);
        xsec->specialdata = floor;
        floor->thinker.function = T_MoveFloor;
        floor->direction = 1;
        floor->sector = sec;
        switch (type)
        {
        case build8:
            speed = FLOORSPEED / 4;
            stairsize = 8 * FRACUNIT;
            break;
        case turbo16:
            speed = FLOORSPEED * 4;
            stairsize = 16 * FRACUNIT;
            break;
        }
        floor->speed = speed;
        height = P_GetFixedp(sec, DMU_FLOOR_HEIGHT) + stairsize;
        floor->floordestheight = height;

        texture = P_GetIntp(sec, DMU_FLOOR_TEXTURE);

        // Find next sector to raise
        // 1.   Find 2-sided line with same sector side[0]
        // 2.   Other side is the next sector to raise
        do
        {
            ok = 0;
            for(i = 0; i < P_GetIntp(sec, DMU_LINE_COUNT); i++)
            {
                ln = P_GetPtrp(sec, DMU_LINE_OF_SECTOR | i);

                if(!(P_GetIntp(ln, DMU_FLAGS) & ML_TWOSIDED))
                    continue;

                tsec = P_GetPtrp(ln, DMU_FRONT_SECTOR);
                if(sec != tsec)
                    continue;

                tsec = P_GetPtrp(ln, DMU_BACK_SECTOR);
                if(P_GetIntp(tsec, DMU_FLOOR_TEXTURE) != texture)
                    continue;

                height += stairsize;

                if(P_XSector(tsec)->specialdata)
                    continue;

                sec = tsec;
                floor = Z_Malloc(sizeof(*floor), PU_LEVSPEC, 0);

                P_AddThinker(&floor->thinker);

                P_XSector(tsec)->specialdata = floor;
                floor->thinker.function = T_MoveFloor;
                floor->direction = 1;
                floor->sector = sec;
                floor->speed = speed;
                floor->floordestheight = height;
                ok = 1;
                break;
            }
        } while(ok);
    }
    return rtn;
}

int EV_DoDonut(line_t *line)
{
    int         i, tag, rtn = 0;
    sector_t   *s1 = NULL;
    sector_t   *s2;
    sector_t   *s3;
    line_t     *check;
    floormove_t *floor;

    tag = P_XLine(line)->tag;
    while((s1 = P_IterateTaggedSectors(tag, s1)) != NULL)
    {
        // ALREADY MOVING?  IF SO, KEEP GOING...
        if(P_XSector(s1)->specialdata)
            continue;

        rtn = 1;

        s2 = P_GetNextSector(P_GetPtrp(s1, DMU_LINE_OF_SECTOR | 0), s1);
        for(i = 0; i < P_GetIntp(s2, DMU_LINE_COUNT); i++)
        {
            check = P_GetPtrp(s2, DMU_LINE_OF_SECTOR | i);

            s3 = P_GetPtrp(check, DMU_BACK_SECTOR);

            if((!(P_GetIntp(check, DMU_FLAGS) & ML_TWOSIDED)) ||
               s3 == s1)
                continue;

            //  Spawn rising slime
            floor = Z_Malloc(sizeof(*floor), PU_LEVSPEC, 0);
            P_AddThinker(&floor->thinker);

            P_XSector(s2)->specialdata = floor;

            floor->thinker.function = T_MoveFloor;
            floor->type = donutRaise;
            floor->crush = false;
            floor->direction = 1;
            floor->sector = s2;
            floor->speed = FLOORSPEED / 2;
            floor->texture = P_GetIntp(s3, DMU_FLOOR_TEXTURE);
            floor->newspecial = 0;
            floor->floordestheight = P_GetFixedp(s3, DMU_FLOOR_HEIGHT);

            //  Spawn lowering donut-hole
            floor = Z_Malloc(sizeof(*floor), PU_LEVSPEC, 0);
            P_AddThinker(&floor->thinker);

            P_XSector(s1)->specialdata = floor;

            floor->thinker.function = T_MoveFloor;
            floor->type = lowerFloor;
            floor->crush = false;
            floor->direction = -1;
            floor->sector = s1;
            floor->speed = FLOORSPEED / 2;
            floor->floordestheight = P_GetFixedp(s3, DMU_FLOOR_HEIGHT);
            break;
        }
    }
    return rtn;
}
