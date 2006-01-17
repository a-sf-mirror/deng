// DoomData.h

// all external data is defined here
// most of the data is loaded into different structures at run time

#ifndef __DOOMDATA__
#define __DOOMDATA__

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

#ifndef __BYTEBOOL__
#define __BYTEBOOL__
typedef enum { false, true } boolean;
typedef unsigned char byte;
#endif

/*
   ===============================================================================

   map level types

   ===============================================================================
 */

// lump order in a map wad
enum { ML_LABEL, ML_THINGS, ML_LINEDEFS, ML_SIDEDEFS, ML_VERTEXES, ML_SEGS,
    ML_SSECTORS, ML_NODES, ML_SECTORS, ML_REJECT, ML_BLOCKMAP, ML_BEHAVIOR
};

#define ML_BLOCKING         1
#define ML_BLOCKMONSTERS    2
#define ML_TWOSIDED         4      // backside will not be present at all
                                    // if not two sided

// if a texture is pegged, the texture will have the end exposed to air held
// constant at the top or bottom of the texture (stairs or pulled down things)
// and will move with a height change of one of the neighbor sectors
// Unpegged textures allways have the first row of the texture at the top
// pixel of the line for both top and bottom textures (windows)
#define ML_DONTPEGTOP       8
#define ML_DONTPEGBOTTOM    16

#define ML_SECRET           32     // don't map as two sided: IT'S A SECRET!
#define ML_SOUNDBLOCK       64     // don't let sound cross two of these
#define ML_DONTDRAW         128    // don't draw on the automap
#define ML_MAPPED           256    // set if allready drawn in automap

#define NF_SUBSECTOR    0x8000

// This is the common thing_t
typedef struct thing_s {
    short           x;
    short           y;
    short           height;
    short           angle;
    short           type;
    short           options;
} thing_t;

extern thing_t* things;

#define MTF_EASY        1
#define MTF_NORMAL      2
#define MTF_HARD        4
#define MTF_AMBUSH      8

#endif                          // __DOOMDATA__
