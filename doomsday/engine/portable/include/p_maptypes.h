/* Generated by .\..\..\engine\scripts\makedmt.py */

#ifndef DOOMSDAY_PLAY_MAP_DATA_TYPES_H
#define DOOMSDAY_PLAY_MAP_DATA_TYPES_H

#include "gl_texmanager.h"

typedef struct shadowvert_s {
    float           inner[2];
    float           extended[2];
} shadowvert_t;

#define LO_prev     link[0]
#define LO_next     link[1]

typedef struct lineowner_s {
    struct linedef_s* lineDef;
    struct lineowner_s* link[2];    // {prev, next} (i.e. {anticlk, clk}).
    binangle_t      angle;          // between this and next clockwise.
    shadowvert_t    shadowOffsets;
} lineowner_t;

typedef struct mvertex_s {
    uint            numLineOwners; // Number of line owners.
    lineowner_t*    lineOwners; // Lineowner base ptr [numlineowners] size. A doubly, circularly linked list. The base is the line with the lowest angle and the next-most with the largest angle.

    // Vertex index. Always valid after loading and pruning of unused
    // vertices has occurred.
    int             index;

    // Reference count. When building normal node info, unused vertices
    // will be pruned.
    int             refCount;

    // Usually NULL, unless this vertex occupies the same location as a
    // previous vertex. Only used during the pruning phase.
    struct vertex_s* equiv;

    struct edgetip_s* tipSet; // Set of wall_tips.
} mvertex_t;

// Helper macros for accessing seg data elements.
#define FRONT 0
#define BACK  1

// Seg frame flags
#define SEGINF_FACINGFRONT      0x0001
#define SEGINF_BACKSECSKYFIX    0x0002

#define HE_v1                   vertex
#define HE_v2                   twin->vertex

#define HE_VERTEX(hEdge, v)     ((v) ? (hEdge)->HE_v2 : (hEdge)->HE_v1)
#define HE_FRONTSIDEDEF(hEdge)  ((hEdge)->data ? ((seg_t*) (hEdge)->data)->sideDef : NULL)
#define HE_BACKSIDEDEF(hEdge)   ((hEdge)->twin->data ? ((seg_t*) (hEdge)->twin->data)->sideDef : NULL)

#define HE_LINESIDE(hEdge)      (((seg_t*) (hEdge)->data)->side)

#define HE_FRONTSUBSECTOR(hEdge)((hEdge)->face ? (subsector_t*) (hEdge)->face->data : NULL)
#define HE_BACKSUBSECTOR(hEdge) ((hEdge)->twin->face ? ((subsector_t*) (hEdge)->twin->face->data) : NULL)

#define HE_FRONTSECTOR(hEdge)   ((hEdge)->face ? ((subsector_t*) (hEdge)->face->data)->sector : NULL)
#define HE_BACKSECTOR(hEdge)    ((hEdge)->twin->face ? ((subsector_t*) (hEdge)->twin->face->data)->sector : NULL)

typedef struct seg_s {
    struct hedge_s*     hEdge;
    struct sidedef_s*   sideDef;
    angle_t             angle;
    byte                side;          // 0=front, 1=back
    float               length;        // Accurate length of the segment (v1 -> v2).
    float               offset;
    struct biassurface_s* bsuf[3];     // 0=middle, 1=top, 2=bottom
    short               frameFlags;
} seg_t;

typedef struct subsector_s {
    struct face_s*      face;
    unsigned int        hEdgeCount;
    struct polyobj_s*   polyObj;       // NULL, if there is no polyobj.
    struct sector_s*    sector;
    int                 addSpriteCount; // frame number of last R_AddSprites
    int                 validCount;
    unsigned int        reverb[NUM_REVERB_DATA];
    float               bBox[2][2];    // Min and max points.
    float               worldGridOffset[2]; // Offset to align the top left of the bBox to the world grid.
    float               midPoint[2];   // Center of the subsector.
    struct shadowlink_s* shadows;
    struct biassurface_s** bsuf;       // [sector->planeCount] size.
    struct hedge_s*     firstFanHEdge;
    boolean             useMidPoint;
} subsector_t;

typedef struct materiallayer_s {
    byte            flags; // MLF_* flags, @see materialLayerFlags
    gltextureid_t   tex;
    float           texOrigin[2];
    float			texPosition[2]; // Current interpolated position.
    float           moveAngle;
    float           moveSpeed;
} material_layer_t;

typedef enum {
    MEC_UNKNOWN = -1,
    MEC_METAL = 0,
    MEC_ROCK,
    MEC_WOOD,
    MEC_WATER,
    MEC_CLOTH,
    NUM_MATERIAL_ENV_CLASSES
} material_env_class_t;

typedef struct material_s {
    material_namespace_t mnamespace;
    boolean             isAutoMaterial; // Was generated automatically.
    const struct ded_material_s* def;  // Can be NULL.
    short               flags;         // MATF_* flags
    short               width;         // Defined width & height of the material (not texture!).
    short               height;
    material_layer_t    layers[DDMAX_MATERIAL_LAYERS];
    byte                numLayers;
    material_env_class_t envClass;     // Used for environmental sound properties.
    struct ded_detailtexture_s* detail;
    struct ded_decor_s* decoration;
    struct ded_ptcgen_s* ptcGen;
    struct ded_reflection_s* reflection;
    boolean             inAnimGroup;   // True if belongs to some animgroup.
    struct material_s*  current;
    struct material_s*  next;
    float               inter;
    struct material_s*  globalNext;    // Linear list linking all materials.
} material_t;

// Internal surface flags:
#define SUIF_PVIS             0x0001
#define SUIF_MATERIAL_FIX     0x0002 // Current texture is a fix replacement
                                     // (not sent to clients, returned via DMU etc).
#define SUIF_BLEND            0x0004 // Surface possibly has a blended texture.
#define SUIF_NO_RADIO         0x0008 // No fakeradio for this surface.

#define SUIF_UPDATE_FLAG_MASK 0xff00
#define SUIF_UPDATE_DECORATIONS 0x8000

// Will the specified surface be added to the sky mask?
#define IS_SKYSURFACE(s)         ((s) && (s)->material && ((s)->material->flags & MATF_SKYMASK))

// Decoration types.
typedef enum {
    DT_LIGHT,
    DT_MODEL,
    NUM_DECORTYPES
} decortype_t;

// Helper macros for accessing decor data.
#define DEC_LIGHT(x)         (&((x)->data.light))
#define DEC_MODEL(x)         (&((x)->data.model))

typedef struct surfacedecor_s {
    float               pos[3]; // World coordinates of the decoration.
    decortype_t         type;
    subsector_t*        subsector;
    union surfacedecor_data_u {
        struct surfacedecor_light_s {
            const struct ded_decorlight_s* def;
        } light;
        struct surfacedecor_model_s {
            const struct ded_decormodel_s* def;
            struct modeldef_s* mf;
            float               pitch, yaw;
        } model;
    } data;
} surfacedecor_t;

typedef struct surface_s {
    void*               owner;         // Either @c DMU_SIDEDEF, or @c DMU_PLANE
    int                 flags;         // SUF_ flags
    int                 oldFlags;
    material_t*         material;
    material_t*         materialB;
    float               matBlendFactor;
    blendmode_t         blendMode;
    float               normal[3];     // Surface normal
    float               oldNormal[3];
    float               offset[2];     // [X, Y] Planar offset to surface material origin.
    float               oldOffset[2][2];
    float               visOffset[2];
    float               visOffsetDelta[2];
    float               rgba[4];       // Surface color tint
    short               inFlags;       // SUIF_* flags
    unsigned int        numDecorations;
    surfacedecor_t      *decorations;
} surface_t;

typedef enum {
    PLN_FLOOR,
    PLN_CEILING,
    PLN_MID,
    NUM_PLANE_TYPES
} planetype_t;

#define PS_normal               surface.normal
#define PS_material             surface.material
#define PS_offset               surface.offset
#define PS_visoffset            surface.visOffset
#define PS_rgba                 surface.rgba
#define PS_flags                surface.flags
#define PS_inflags              surface.inFlags

typedef struct plane_s {
    ddmobj_base_t       soundOrg;      // Sound origin for plane
    struct sector_s*    sector;        // Owner of the plane (temp)
    surface_t           surface;
    float               height;        // Current height
    float               oldHeight[2];
    float               glow;          // Glow amount
    float               glowRGB[3];    // Glow color
    float               target;        // Target height
    float               speed;         // Move speed
    float               visHeight;     // Visible plane height (smoothed)
    float               visHeightDelta;
    planetype_t         type;          // PLN_* type.
    int                 planeID;
} plane_t;

// Helper macros for accessing sector floor/ceiling plane data elements.
#define SP_plane(n)             planes[(n)]

#define SP_planesurface(n)      SP_plane(n)->surface
#define SP_planeheight(n)       SP_plane(n)->height
#define SP_planenormal(n)       SP_plane(n)->surface.normal
#define SP_planematerial(n)     SP_plane(n)->surface.material
#define SP_planeoffset(n)       SP_plane(n)->surface.offset
#define SP_planergb(n)          SP_plane(n)->surface.rgba
#define SP_planeglow(n)         SP_plane(n)->glow
#define SP_planeglowrgb(n)      SP_plane(n)->glowRGB
#define SP_planetarget(n)       SP_plane(n)->target
#define SP_planespeed(n)        SP_plane(n)->speed
#define SP_planesoundorg(n)     SP_plane(n)->soundOrg
#define SP_planevisheight(n)    SP_plane(n)->visHeight

#define SP_ceilsurface          SP_planesurface(PLN_CEILING)
#define SP_ceilheight           SP_planeheight(PLN_CEILING)
#define SP_ceilnormal           SP_planenormal(PLN_CEILING)
#define SP_ceilmaterial         SP_planematerial(PLN_CEILING)
#define SP_ceiloffset           SP_planeoffset(PLN_CEILING)
#define SP_ceilrgb              SP_planergb(PLN_CEILING)
#define SP_ceilglow             SP_planeglow(PLN_CEILING)
#define SP_ceilglowrgb          SP_planeglowrgb(PLN_CEILING)
#define SP_ceiltarget           SP_planetarget(PLN_CEILING)
#define SP_ceilspeed            SP_planespeed(PLN_CEILING)
#define SP_ceilsoundorg         SP_planesoundorg(PLN_CEILING)
#define SP_ceilvisheight        SP_planevisheight(PLN_CEILING)

#define SP_floorsurface         SP_planesurface(PLN_FLOOR)
#define SP_floorheight          SP_planeheight(PLN_FLOOR)
#define SP_floornormal          SP_planenormal(PLN_FLOOR)
#define SP_floormaterial        SP_planematerial(PLN_FLOOR)
#define SP_flooroffset          SP_planeoffset(PLN_FLOOR)
#define SP_floorrgb             SP_planergb(PLN_FLOOR)
#define SP_floorglow            SP_planeglow(PLN_FLOOR)
#define SP_floorglowrgb         SP_planeglowrgb(PLN_FLOOR)
#define SP_floortarget          SP_planetarget(PLN_FLOOR)
#define SP_floorspeed           SP_planespeed(PLN_FLOOR)
#define SP_floorsoundorg        SP_planesoundorg(PLN_FLOOR)
#define SP_floorvisheight       SP_planevisheight(PLN_FLOOR)

#define S_skyfix(n)             skyFix[(n)]
#define S_floorskyfix           S_skyfix(PLN_FLOOR)
#define S_ceilskyfix            S_skyfix(PLN_CEILING)

// Sector frame flags
#define SIF_VISIBLE         0x1     // Sector is visible on this frame.
#define SIF_FRAME_CLEAR     0x1     // Flags to clear before each frame.
#define SIF_LIGHT_CHANGED   0x2

// Sector flags.
#define SECF_UNCLOSED       0x1     // An unclosed sector (some sort of fancy hack).

typedef struct msector_s {
    // Sector index. Always valid after loading & pruning.
    int         index;

    // Suppress superfluous mini warnings.
    int         warnedFacing;
    int         refCount;
} msector_t;

typedef struct sector_s {
    int                 frameFlags;
    int                 validCount;    // if == validCount, already checked.
    int                 flags;
    float               bBox[4];       // Bounding box for the sector.
    float               approxArea;    // Rough approximation of sector area.
    float               lightLevel;
    float               oldLightLevel;
    float               rgb[3];
    float               oldRGB[3];
    struct mobj_s*      mobjList;      // List of mobjs in the sector.
    unsigned int        lineDefCount;
    struct linedef_s**  lineDefs;      // [lineDefCount+1] size.
    unsigned int        subsectorCount;
    struct subsector_s** subsectors;   // [subsectorCount+1] size.
    unsigned int        numReverbSubsectorAttributors;
    struct subsector_s** reverbSubsectors; // [numReverbSubsectorAttributors] size.
    ddmobj_base_t       soundOrg;
    unsigned int        planeCount;
    struct plane_s**    planes;        // [planeCount+1] size.
    struct sector_s*    lightSource;   // Main sky light source.
    unsigned int        blockCount;    // Number of gridblocks in the sector.
    unsigned int        changedBlockCount; // Number of blocks to mark changed.
    unsigned short*     blocks;        // Light grid block indices.
    float               reverb[NUM_REVERB_DATA];
    msector_t           buildData;
} sector_t;

// Parts of a wall segment.
typedef enum segsection_e {
    SEG_MIDDLE,
    SEG_TOP,
    SEG_BOTTOM
} segsection_t;

// Helper macros for accessing sidedef top/middle/bottom section data elements.
#define SW_surface(n)           sections[(n)]
#define SW_surfaceflags(n)      SW_surface(n).flags
#define SW_surfaceinflags(n)    SW_surface(n).inFlags
#define SW_surfacematerial(n)   SW_surface(n).material
#define SW_surfacenormal(n)     SW_surface(n).normal
#define SW_surfaceoffset(n)     SW_surface(n).offset
#define SW_surfacevisoffset(n)  SW_surface(n).visOffset
#define SW_surfacergba(n)       SW_surface(n).rgba
#define SW_surfaceblendmode(n)  SW_surface(n).blendMode

#define SW_middlesurface        SW_surface(SEG_MIDDLE)
#define SW_middleflags          SW_surfaceflags(SEG_MIDDLE)0
#define SW_middleinflags        SW_surfaceinflags(SEG_MIDDLE)
#define SW_middlematerial       SW_surfacematerial(SEG_MIDDLE)
#define SW_middlenormal         SW_surfacenormal(SEG_MIDDLE)
#define SW_middletexmove        SW_surfacetexmove(SEG_MIDDLE)
#define SW_middleoffset         SW_surfaceoffset(SEG_MIDDLE)
#define SW_middlevisoffset      SW_surfacevisoffset(SEG_MIDDLE)
#define SW_middlergba           SW_surfacergba(SEG_MIDDLE)
#define SW_middleblendmode      SW_surfaceblendmode(SEG_MIDDLE)

#define SW_topsurface           SW_surface(SEG_TOP)
#define SW_topflags             SW_surfaceflags(SEG_TOP)
#define SW_topinflags           SW_surfaceinflags(SEG_TOP)
#define SW_topmaterial          SW_surfacematerial(SEG_TOP)
#define SW_topnormal            SW_surfacenormal(SEG_TOP)
#define SW_toptexmove           SW_surfacetexmove(SEG_TOP)
#define SW_topoffset            SW_surfaceoffset(SEG_TOP)
#define SW_topvisoffset         SW_surfacevisoffset(SEG_TOP)
#define SW_toprgba              SW_surfacergba(SEG_TOP)

#define SW_bottomsurface        SW_surface(SEG_BOTTOM)
#define SW_bottomflags          SW_surfaceflags(SEG_BOTTOM)
#define SW_bottominflags        SW_surfaceinflags(SEG_BOTTOM)
#define SW_bottommaterial       SW_surfacematerial(SEG_BOTTOM)
#define SW_bottomnormal         SW_surfacenormal(SEG_BOTTOM)
#define SW_bottomtexmove        SW_surfacetexmove(SEG_BOTTOM)
#define SW_bottomoffset         SW_surfaceoffset(SEG_BOTTOM)
#define SW_bottomvisoffset      SW_surfacevisoffset(SEG_BOTTOM)
#define SW_bottomrgba           SW_surfacergba(SEG_BOTTOM)

#define FRONT                   0
#define BACK                    1

typedef struct msidedef_s {
    // SideDef index. Always valid after loading & pruning.
    int         index;
    int         refCount;
} msidedef_t;

// Used with FakeRadio.
typedef struct {
    float           corner;
    struct sector_s* proximity;
    float           pOffset;
    float           pHeight;
} shadowcorner_t;

typedef struct {
    float           length;
    float           shift;
} edgespan_t;

typedef struct {
    int                 fakeRadioUpdateCount; // frame number of last update
    shadowcorner_t      topCorners[2];
    shadowcorner_t      bottomCorners[2];
    shadowcorner_t      sideCorners[2];
    edgespan_t          spans[2];      // [left, right]
} sideradioconfig_t;

typedef struct sidedef_s {
    surface_t           sections[3];
    struct linedef_s*   lineDef;
    struct sector_s*    sector;
    short               flags;
    sideradioconfig_t   radioConfig;
    msidedef_t          buildData;
} sidedef_t;

// Helper macros for accessing linedef data elements.
#define L_v1                    hEdges[0]->vertex
#define L_v2                    hEdges[1]->twin->vertex

#define L_vo(n)                 vo[(n)]
#define L_vo1                   L_vo(0)
#define L_vo2                   L_vo(1)

#define LINE_VERTEX(l, v)       ((v) ? (l)->L_v2 : (l)->L_v1)
#define LINE_FRONTSIDE(l)       (((seg_t*) (l)->hEdges[0]->data)->sideDef)
#define LINE_BACKSIDE(l)        (((seg_t*) (l)->hEdges[0]->twin->data) ? ((seg_t*) (l)->hEdges[0]->twin->data)->sideDef : NULL)
#define LINE_SIDE(l, s)         ((s)? LINE_BACKSIDE(l) : LINE_FRONTSIDE(l))

#define LINE_FRONTSECTOR(l)     (LINE_FRONTSIDE(l)->sector)
#define LINE_BACKSECTOR(l)      (LINE_BACKSIDE(l)->sector)
#define LINE_SECTOR(l, s)       ((s)? LINE_BACKSECTOR(l) : LINE_FRONTSECTOR(l))

// Is this line self-referencing (front sec == back sec)?
#define LINE_SELFREF(l)         (LINE_FRONTSIDE(l) && LINE_BACKSIDE(l) && \
                                 LINE_FRONTSECTOR(l) == LINE_BACKSECTOR(l))

// Internal flags:
#define LF_POLYOBJ              0x1 // Line is part of a polyobject.

typedef struct mlinedef_s {
    struct vertex_s* v[2];
    struct sidedef_s* sideDefs[2];
    // LineDef index. Always valid after loading & pruning of zero
    // length lines has occurred.
    int         index;

    // One-sided linedef used for a special effect (windows).
    // The value refers to the opposite sector on the back side.
    struct sector_s* windowEffect;

    // Normally NULL, except when this linedef directly overlaps an earlier
    // one (a rarely-used trick to create higher mid-masked textures).
    // No segs should be created for these overlapping linedefs.
    struct linedef_s* overlap;
} mlinedef_t;

typedef struct linedef_s {
    struct lineowner_s* vo[2];         // Links to vertex line owner nodes [left, right]
    struct hedge_s*     hEdges[2];     // [leftmost front seg, rightmost front seg]
    int                 flags;         // Public DDLF_* flags.
    byte                inFlags;       // Internal LF_* flags
    slopetype_t         slopeType;
    int                 validCount;
    binangle_t          angle;         // Calculated from front side's normal
    float               dX;
    float               dY;
    float               length;        // Accurate length
    float               bBox[4];
    boolean             mapped[DDMAXPLAYERS]; // Whether the line has been mapped by each player yet.
    mlinedef_t          buildData;
    unsigned short      shadowVisFrame[2]; // Framecount of last time shadows were drawn for this line, for each side [right, left].
} linedef_t;

#define RIGHT                   0
#define LEFT                    1

/**
 * An infinite line of the form point + direction vectors.
 */
typedef struct partition_s {
    float        x, y;
    float        dX, dY;
} partition_t;

typedef struct node_s {
    partition_t         partition;
    float               bBox[2][4];    // Bounding box for each child.
    unsigned int        children[2];   // If NF_SUBSECTOR it's a subsector.
} node_t;

#define MAX_SKY_LAYERS        2
#define MAX_SKY_MODELS        32

typedef struct skylayer_s {
    boolean			enabled;
    float           fadeoutColorLimit; // .3 by default.
} skylayer_t;

typedef struct skymodel_s {
    const struct ded_skymodel_s* def;
    struct modeldef_s* model;
    int             frame;
    int             timer;
    int             maxTimer;
    float           yaw;
} skymodel_t;

typedef struct sky_s {
    const struct ded_sky_s* def;
    material_t*         material;      // Used with sky sphere only.
    skylayer_t          layers[MAX_SKY_LAYERS];
    int                 firstLayer;    // -1 denotes 'no active layers'.
    int                 activeLayers;
    boolean             modelsInited;
    skymodel_t          models[MAX_SKY_MODELS];
} sky_t;

#endif
