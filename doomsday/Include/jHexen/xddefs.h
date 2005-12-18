
//**************************************************************************
//**
//** xddefs.h : Heretic 2 : Raven Software, Corp.
//**
//** $RCSfile$
//** $Revision$
//** $Date$
//** $Author$
//**
//**************************************************************************

#ifndef __XDDEFS__
#define __XDDEFS__

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

/*#ifndef __BYTEBOOL__
   #define __BYTEBOOL__
   typedef enum {false, true} h2bool;
   typedef unsigned char byte;
   #define boolean h2bool
   #endif */

//--------------------------------------------------------------------------
//
// Map level types
//
//--------------------------------------------------------------------------

// lump order in a map wad
enum {
    ML_LABEL,
    ML_THINGS,
    ML_LINEDEFS,
    ML_SIDEDEFS,
    ML_VERTEXES,
    ML_SEGS,
    ML_SSECTORS,
    ML_NODES,
    ML_SECTORS,
    ML_REJECT,
    ML_BLOCKMAP,
    ML_BEHAVIOR
};

#define ML_BLOCKING         0x0001
#define ML_BLOCKMONSTERS    0x0002
#define ML_TWOSIDED         0x0004
#define ML_DONTPEGTOP       0x0008
#define ML_DONTPEGBOTTOM    0x0010
#define ML_SECRET           0x0020 // don't map as two sided: IT'S A SECRET!
#define ML_SOUNDBLOCK       0x0040 // don't let sound cross two of these
#define ML_DONTDRAW         0x0080 // don't draw on the automap
#define ML_MAPPED           0x0100 // set if already drawn in automap
#define ML_REPEAT_SPECIAL   0x0200 // special is repeatable
#define ML_SPAC_SHIFT       10
#define ML_SPAC_MASK        0x1c00
#define GET_SPAC(flags) ((flags&ML_SPAC_MASK)>>ML_SPAC_SHIFT)

// Special activation types
#define SPAC_CROSS      0          // when player crosses line
#define SPAC_USE        1          // when player uses line
#define SPAC_MCROSS     2          // when monster crosses line
#define SPAC_IMPACT     3          // when projectile hits line
#define SPAC_PUSH       4          // when player/monster pushes line
#define SPAC_PCROSS     5          // when projectile crosses line

#define NF_SUBSECTOR    0x8000

typedef struct {
    short           tid;
    short         x;
    short         y;
    short           height;
    short           angle;
    short           type;
    short           options;
    byte            special;
    byte            arg1;
    byte            arg2;
    byte            arg3;
    byte            arg4;
    byte            arg5;
} thing_t;

#define MTF_EASY        1
#define MTF_NORMAL      2
#define MTF_HARD        4
#define MTF_AMBUSH      8
#define MTF_DORMANT     16
#define MTF_FIGHTER     32
#define MTF_CLERIC      64
#define MTF_MAGE        128
#define MTF_GSINGLE     256
#define MTF_GCOOP       512
#define MTF_GDEATHMATCH 1024

//--------------------------------------------------------------------------
//
// Texture definition
//
//--------------------------------------------------------------------------
/*
   typedef struct
   {
   short    originx;
   short    originy;
   short    patch;
   short    stepdir;
   short    colormap;
   } mappatch_t;

   typedef struct
   {
   char     name[8];
   boolean      masked;
   short        width;
   short        height;
   void     **columndirectory;  // OBSOLETE
   short        patchcount;
   mappatch_t   patches[1];
   } maptexture_t;

   //--------------------------------------------------------------------------
   //
   // Graphics
   //
   //--------------------------------------------------------------------------

   // posts are runs of non masked source pixels
   typedef struct
   {
   byte     topdelta;       // -1 is the last post in a column
   byte     length;
   // length data bytes follows
   } post_t;

   // column_t is a list of 0 or more post_t, (byte)-1 terminated
   typedef post_t   column_t;

   // a patch holds one or more columns
   // patches are used for sprites and all masked pictures
   typedef struct
   {
   short        width;              // bounding box size
   short        height;
   short        leftoffset;         // pixels to the left of origin
   short        topoffset;          // pixels below the origin
   int          columnofs[8];       // only [width] used
   // the [0] is &columnofs[width]
   } patch_t;

   // a pic is an unmasked block of pixels
   typedef struct
   {
   byte     width,height;
   byte     data;
   } pic_t;
 */
#endif                          // __XDDEFS__
