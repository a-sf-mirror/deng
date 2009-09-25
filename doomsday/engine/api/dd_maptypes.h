/* Generated by .\..\..\engine\scripts\makedmt.py */

#ifndef __DOOMSDAY_PLAY_PUBLIC_MAP_DATA_TYPES_H__
#define __DOOMSDAY_PLAY_PUBLIC_MAP_DATA_TYPES_H__

// think_t is a function pointer to a routine to handle an actor
typedef void    (*think_t) ();

/**
 * \todo thinker_t should not be part of the public API in its current form.
 */
typedef struct thinker_s {
    struct thinker_s *prev, *next;
    think_t         function;
    boolean         inStasis;
    thid_t          id; // Only used for mobjs (zero is not an ID).
} thinker_t;

#define DMT_VERTEX_POS  DDVT_FLOAT


#define DMT_HEDGE_SIDEDEF       DDVT_PTR
#define DMT_HEDGE_LINEDEF       DDVT_PTR
#define DMT_HEDGE_SEC           DDVT_PTR
#define DMT_HEDGE_SUBSECTOR     DDVT_PTR
#define DMT_HEDGE_ANGLE         DDVT_ANGLE
#define DMT_HEDGE_SIDE          DDVT_BYTE
#define DMT_HEDGE_FLAGS         DDVT_BYTE
#define DMT_HEDGE_LENGTH        DDVT_FLOAT
#define DMT_HEDGE_OFFSET        DDVT_FLOAT

#define DMT_HEDGE_V DDVT_PTR           // [Start, End] of the hedge.
#define DMT_HEDGE_TWIN DDVT_PTR
#define DMT_HEDGE_NEXT DDVT_PTR
#define DMT_HEDGE_PREV DDVT_PTR
#define DMT_HEDGE_FACE DDVT_PTR

#define DMT_FACE_HEDGECOUNT     DDVT_UINT
#define DMT_FACE_POLYOBJ        DDVT_PTR
#define DMT_FACE_SECTOR         DDVT_PTR

#define DMT_FACE_HEDGE DDVT_PTR        // First half-edge of this subsector.

#define DMT_MATERIAL_MNAMESPACE DDVT_INT
#define DMT_MATERIAL_FLAGS DDVT_SHORT  // MATF_* flags
#define DMT_MATERIAL_WIDTH DDVT_SHORT  // Defined width & height of the material (not texture!).
#define DMT_MATERIAL_HEIGHT DDVT_SHORT

#define DMT_SURFACE_FLAGS DDVT_INT     // SUF_ flags
#define DMT_SURFACE_MATERIAL DDVT_PTR
#define DMT_SURFACE_BLENDMODE DDVT_BLENDMODE
#define DMT_SURFACE_NORMAL DDVT_FLOAT  // Surface normal
#define DMT_SURFACE_OFFSET DDVT_FLOAT  // [X, Y] Planar offset to surface material origin.
#define DMT_SURFACE_RGBA DDVT_FLOAT    // Surface color tint

#define DMT_PLANE_SOUNDORG DDVT_PTR    // Sound origin for plane
#define DMT_PLANE_SECTOR DDVT_PTR      // Owner of the plane (temp)
#define DMT_PLANE_HEIGHT DDVT_FLOAT    // Current height
#define DMT_PLANE_GLOW DDVT_FLOAT      // Glow amount
#define DMT_PLANE_GLOWRGB DDVT_FLOAT   // Glow color
#define DMT_PLANE_TARGET DDVT_FLOAT    // Target height
#define DMT_PLANE_SPEED DDVT_FLOAT     // Move speed

#define DMT_SECTOR_VALIDCOUNT DDVT_INT // if == validCount, already checked.
#define DMT_SECTOR_LIGHTLEVEL DDVT_FLOAT
#define DMT_SECTOR_RGB DDVT_FLOAT
#define DMT_SECTOR_MOBJLIST DDVT_PTR   // List of mobjs in the sector.
#define DMT_SECTOR_LINEDEFCOUNT DDVT_UINT
#define DMT_SECTOR_LINEDEFS DDVT_PTR   // [lineDefCount+1] size.
#define DMT_SECTOR_FACECOUNT DDVT_UINT
#define DMT_SECTOR_FACES DDVT_PTR      // [faceCount+1] size.
#define DMT_SECTOR_SOUNDORG DDVT_PTR
#define DMT_SECTOR_PLANECOUNT DDVT_UINT
#define DMT_SECTOR_REVERB DDVT_FLOAT

#define DMT_SIDEDEF_LINE DDVT_PTR
#define DMT_SIDEDEF_SECTOR DDVT_PTR
#define DMT_SIDEDEF_FLAGS DDVT_SHORT

#define DMT_LINEDEF_SEC    DDVT_PTR
#define DMT_LINEDEF_V      DDVT_PTR
#define DMT_LINEDEF_SIDE   DDVT_PTR

#define DMT_LINEDEF_FLAGS DDVT_INT     // Public DDLF_* flags.
#define DMT_LINEDEF_SLOPETYPE DDVT_INT
#define DMT_LINEDEF_VALIDCOUNT DDVT_INT
#define DMT_LINEDEF_DX DDVT_FLOAT
#define DMT_LINEDEF_DY DDVT_FLOAT
#define DMT_LINEDEF_BBOX DDVT_FLOAT

#define DMT_NODE_BBOX DDVT_FLOAT       // Bounding box for each child.
#define DMT_NODE_CHILDREN DDVT_UINT    // If NF_SUBSECTOR it's a subsector.

#endif
