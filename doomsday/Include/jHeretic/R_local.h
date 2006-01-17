// R_local.h

#ifndef __R_LOCAL__
#define __R_LOCAL__

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

#include "p_xg.h"

#define ANGLETOSKYSHIFT     22     // sky map is 256*128*4 maps

#define BASEYCENTER         100

#define MAXWIDTH            1120
#define MAXHEIGHT           832

#define PI                  3.141592657

#define CENTERY             (SCREENHEIGHT/2)

#define MINZ            (FRACUNIT*4)

#define FIELDOFVIEW     2048       // fineangles in the SCREENWIDTH wide window

//
// lighting constants
//
#define LIGHTLEVELS         16
#define LIGHTSEGSHIFT       4
#define MAXLIGHTSCALE       48
#define LIGHTSCALESHIFT     12
#define MAXLIGHTZ           128
#define LIGHTZSHIFT         20
#define NUMCOLORMAPS        32     // number of diminishing
#define INVERSECOLORMAP     32

/*
   ==============================================================================

   INTERNAL MAP TYPES

   ==============================================================================
 */

//================ used by play and refresh

struct line_s;

typedef struct xsector_s {
    short           special, tag;
    int             soundtraversed; // 0 = untraversed, 1,2 = sndlines -1
    mobj_t         *soundtarget;   // thing that made a sound (or null)
    void           *specialdata;   // thinker_t for reversable actions
    int             origfloor, origceiling, origlight;
    byte            origrgb[3];
    /*seqtype_t*/ byte       seqType;       // NOT USED ATM
    xgsector_t     *xg;
} xsector_t;

typedef struct xline_s {
    short           special;
    short           tag;
    void           *specialdata;   // thinker_t for reversable actions
    xgline_t       *xg;            // Extended generalized lines.
} xline_t;

enum                               // Subsector reverb data indices.
{
    SSRD_VOLUME,
    SSRD_SPACE,
    SSRD_DECAY,
    SSRD_DAMPING
};

extern int numvertexes;
extern int numsegs;
extern int numsectors;
extern int numsubsectors;
extern int numnodes;
extern int numlines;
extern int numsides;
extern int numthings;

/*
   ==============================================================================

   OTHER TYPES

   ==============================================================================
 */

//=============================================================================

#define viewangle       Get(DD_VIEWANGLE)

extern player_t *viewplayer;

extern angle_t  clipangle;

extern int      viewangletox[FINEANGLES / 2];
extern angle_t  xtoviewangle[SCREENWIDTH + 1];
extern fixed_t  finetangent[FINEANGLES / 2];

extern fixed_t  rw_distance;
extern angle_t  rw_normalangle;

//
// R_main.c
//
//extern    int             viewwidth, viewheight, viewwindowx, viewwindowy;
extern int      centerx, centery;
extern int      flyheight;
extern fixed_t  centerxfrac;
extern fixed_t  centeryfrac;
extern fixed_t  projection;

//extern    int             validcount;
#define Validcount      (*gi.validcount)

extern int      sscount, linecount, loopcount;

extern int      extralight;

extern fixed_t  viewcos, viewsin;

extern int      detailshift;       // 0 = high, 1 = low

/*int       R_PointOnSide (fixed_t x, fixed_t y, node_t *node);
   int      R_PointOnSegSide (fixed_t x, fixed_t y, seg_t *line); */
angle_t         R_PointToAngle(fixed_t x, fixed_t y);

//#define R_PointToAngle2           gi.R_PointToAngle2
//fixed_t   R_PointToDist (fixed_t x, fixed_t y);
fixed_t         R_ScaleFromGlobalAngle(angle_t visangle);

//#define R_PointInSubsector        gi.R_PointInSubsector
void            R_AddPointToBox(int x, int y, fixed_t *box);

#define skyflatnum      Get(DD_SKYFLATNUM)

// SKY, store the number for name.
#define SKYFLATNAME  "F_SKY1"

extern xsector_t *xsectors;
extern xline_t *xlines;

void P_SetupForThings(int num);
void P_SetupForLines(int num);
void P_SetupForSides(int num);
void P_SetupForSectors(int num);

int P_HandleMapDataProperty(int id, int dtype, int prop, int type, void *data);

#endif                          // __R_LOCAL__
