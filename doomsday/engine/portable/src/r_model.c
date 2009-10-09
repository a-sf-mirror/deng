/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

/**
 * r_model.c: 3D Model Resources
 *
 * MD2/DMD2 loading and setup.
 * My variable naming convention is a bit incoherent.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_platform.h"

#include <ctype.h>
#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_graphics.h"
#include "de_refresh.h"
#include "de_play.h"
#include "de_misc.h"

#include "def_main.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct {
    float           pos[3];
} vector_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

float rModelAspectMod = 1 / 1.2f; //.833334f;

// The dummy is used for model zero.
model_t dummy = { true, "Dummy-Dummy" };
model_t* modellist[MAX_MODELS] = { &dummy };
byte useModels = true;

modeldef_t* modefs = NULL;
int numModelDefs;

float avertexnormals[NUMVERTEXNORMALS][3] = {
#include "tab_anorms.h"
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int maxModelDefs;
static modeldef_t** stateModefs;

// CODE --------------------------------------------------------------------

/**
 * Packed: pppppppy yyyyyyyy. Yaw is on the XY plane.
 */
static void UnpackVector(unsigned short packed, float vec[3])
{
    float               yaw = (packed & 511) / 512.0f * 2 * PI;
    float               pitch = ((packed >> 9) / 127.0f - 0.5f) * PI;
    float               cosp = (float) cos(pitch);

    vec[VX] = (float) cos(yaw) * cosp;
    vec[VY] = (float) sin(yaw) * cosp;
    vec[VZ] = (float) sin(pitch);
}

/**
 * Returns an index if the specified model has already been loaded.
 */
static int R_FindModelFor(const char* filename)
{
    int                 i;

    for(i = 0; i < MAX_MODELS; ++i)
        if(modellist[i] && !stricmp(modellist[i]->fileName, filename))
            return i;
    return -1;
}

/**
 * Allocates a new model. Returns the index.
 */
static int R_NewModelFor(void /*const char *filename*/)
{
    int                 i;

    // Take the first empty spot.
    for(i = 0; i < MAX_MODELS; ++i)
        if(!modellist[i])
        {
            modellist[i] = M_Calloc(sizeof(model_t));
            return i;
        }
    // Dang, we're out of models.
    return -1;
}

/**
 * Calculate vertex normals. Only with -renorm.
 */
#if 0 // unused atm.
static void R_VertexNormals(model_t *mdl)
{
    int         tris = mdl->lodInfo[0].numTriangles;
    int         verts = mdl->info.numVertices;
    int         i, k, j, n, cnt;
    vector_t   *normals, norm;
    model_vertex_t *list;
    dmd_triangle_t *tri;

    // Renormalizing?
    if(!ArgCheck("-renorm"))
        return;

    normals = Z_Malloc(sizeof(vector_t) * tris, PU_STATIC, 0);

    // Calculate the normal for each vertex.
    for(i = 0; i < mdl->info.numFrames; ++i)
    {
        list = mdl->frames[i].vertices;
        for(k = 0; k < tris; ++k)
        {
            tri = mdl->lods[0].triangles + k;
            // First calculate surface normals, combine them to vertex ones.
            M_PointCrossProduct(list[tri->vertexIndices[0]].vertex,
            list[tri->vertexIndices[2]].vertex,
            list[tri->vertexIndices[1]].vertex, normals[k].pos);
            M_Normalize(normals[k].pos);
        }

        for(k = 0; k < verts; ++k)
        {
            memset(&norm, 0, sizeof(norm));
            for(j = 0, cnt = 0; j < tris; ++j)
            {
                tri = mdl->lods[0].triangles + j;
                for(n = 0; n < 3; ++n)
                    if(tri->vertexIndices[n] == k)
                    {
                        cnt++;
                        for(n = 0; n < 3; ++n)
                            norm.pos[n] += normals[j].pos[n];
                        break;
                    }
            }

            if(!cnt)
                continue; // Impossible...

            // Calculate the average.
            for(n = 0; n < 3; ++n)
                norm.pos[n] /= cnt;

            // Normalize it.
            M_Normalize(norm.pos);
            memcpy(list[k].normal, norm.pos, sizeof(norm.pos));
        }
    }
    Z_Free(normals);
}
#endif

static void *AllocAndLoad(DFILE *file, int offset, int len)
{
    void       *ptr = M_Malloc(len);

    F_Seek(file, offset, SEEK_SET);
    F_Read(ptr, len, file);
    return ptr;
}

static void R_MissingModel(const char* fn)
{
    //if(verbose)
    Con_Printf("Warning: Failed to locate model \"%s\".\n", fn);
}

static void R_LoadModelMD2(DFILE *file, model_t *mdl)
{
    md2_header_t        oldhd;
    dmd_header_t       *hd = &mdl->header;
    dmd_info_t         *inf = &mdl->info;
    model_frame_t      *frame;
    byte               *frames;
    int                 i, k, c;
    const int           axis[3] = { 0, 2, 1 };

    // Read the header.
    F_Read(&oldhd, sizeof(oldhd), file);

    // Convert it to DMD.
    hd->magic = MD2_MAGIC;
    hd->version = 8;
    hd->flags = 0;
    mdl->vertexUsage = NULL;

    inf->skinWidth = LONG(oldhd.skinWidth);
    inf->skinHeight = LONG(oldhd.skinHeight);
    inf->frameSize = LONG(oldhd.frameSize);
    inf->numLODs = 1;
    inf->numSkins = LONG(oldhd.numSkins);
    inf->numTexCoords = LONG(oldhd.numTexCoords);
    inf->numVertices = LONG(oldhd.numVertices);
    inf->numFrames = LONG(oldhd.numFrames);
    inf->offsetSkins = LONG(oldhd.offsetSkins);
    inf->offsetTexCoords = LONG(oldhd.offsetTexCoords);
    inf->offsetFrames = LONG(oldhd.offsetFrames);
    inf->offsetLODs = LONG(oldhd.offsetEnd);    // Doesn't exist.

    mdl->lodInfo[0].numTriangles = LONG(oldhd.numTriangles);
    mdl->lodInfo[0].numGlCommands = LONG(oldhd.numGlCommands);
    mdl->lodInfo[0].offsetTriangles = LONG(oldhd.offsetTriangles);
    mdl->lodInfo[0].offsetGlCommands = LONG(oldhd.offsetGlCommands);
    inf->offsetEnd = LONG(oldhd.offsetEnd);

    // The frames need to be unpacked.
    frames =
        AllocAndLoad(file, inf->offsetFrames, inf->frameSize * inf->numFrames);
    mdl->frames = M_Malloc(sizeof(model_frame_t) * inf->numFrames);
    for(i = 0, frame = mdl->frames; i < inf->numFrames; ++i, frame++)
    {
        md2_packedFrame_t *pfr =
            (md2_packedFrame_t *) (frames + inf->frameSize * i);
        md2_triangleVertex_t *pVtx;

        memcpy(frame->name, pfr->name, sizeof(pfr->name));
        frame->vertices = M_Malloc(sizeof(model_vertex_t) * inf->numVertices);
        frame->normals = M_Malloc(sizeof(model_vertex_t) * inf->numVertices);

        // Translate each vertex.
        for(k = 0, pVtx = pfr->vertices; k < inf->numVertices; ++k, pVtx++)
        {
            memcpy(frame->normals[k].xyz,
                   avertexnormals[pVtx->lightNormalIndex], sizeof(float) * 3);

            for(c = 0; c < 3; ++c)
            {
                frame->vertices[k].xyz[axis[c]] =
                    pVtx->vertex[c] * FLOAT(pfr->scale[c]) +
                    FLOAT(pfr->translate[c]);
            }

            // Aspect undoing.
            frame->vertices[k].xyz[VY] *= rModelAspectMod;

            for(c = 0; c < 3; ++c)
            {
                if(!k || frame->vertices[k].xyz[c] < frame->min[c])
                    frame->min[c] = frame->vertices[k].xyz[c];
                if(!k || frame->vertices[k].xyz[c] > frame->max[c])
                    frame->max[c] = frame->vertices[k].xyz[c];
            }
        }
    }
    M_Free(frames);

    mdl->lods[0].glCommands =
        AllocAndLoad(file, mdl->lodInfo[0].offsetGlCommands,
                     sizeof(int) * mdl->lodInfo[0].numGlCommands);

    // Load skins.
    mdl->skins = M_Calloc(sizeof(dmd_skin_t) * inf->numSkins);
    F_Seek(file, inf->offsetSkins, SEEK_SET);
    for(i = 0; i < inf->numSkins; ++i)
        F_Read(mdl->skins[i].name, 64, file);
}

static void R_LoadModelDMD(DFILE *file, model_t *mo)
{
    dmd_chunk_t         chunk;
    char               *temp;
    dmd_info_t         *inf = &mo->info;
    model_frame_t      *frame;
    int                 i, k, c;
    dmd_triangle_t     *triangles[MAX_LODS];
    const int           axis[3] = { 0, 2, 1 };

    // Read the chunks.
    F_Read(&chunk, sizeof(chunk), file);

    while(LONG(chunk.type) != DMC_END)
    {
        switch (LONG(chunk.type))
        {
        case DMC_INFO:          // Standard DMD information chunk.
            F_Read(inf, LONG(chunk.length), file);
            inf->skinWidth = LONG(inf->skinWidth);
            inf->skinHeight = LONG(inf->skinHeight);
            inf->frameSize = LONG(inf->frameSize);
            inf->numSkins = LONG(inf->numSkins);
            inf->numVertices = LONG(inf->numVertices);
            inf->numTexCoords = LONG(inf->numTexCoords);
            inf->numFrames = LONG(inf->numFrames);
            inf->numLODs = LONG(inf->numLODs);
            inf->offsetSkins = LONG(inf->offsetSkins);
            inf->offsetTexCoords = LONG(inf->offsetTexCoords);
            inf->offsetFrames = LONG(inf->offsetFrames);
            inf->offsetLODs = LONG(inf->offsetLODs);
            inf->offsetEnd = LONG(inf->offsetEnd);
            break;

        default:
            // Just skip all unknown chunks.
            temp = M_Malloc(LONG(chunk.length));
            F_Read(temp, LONG(chunk.length), file);
            free(temp);
        }
        // Read the next chunk header.
        F_Read(&chunk, sizeof(chunk), file);
    }

    // Allocate and load in the data.
    mo->skins = M_Calloc(sizeof(dmd_skin_t) * inf->numSkins);
    F_Seek(file, inf->offsetSkins, SEEK_SET);
    for(i = 0; i < inf->numSkins; ++i)
        F_Read(mo->skins[i].name, 64, file);

    temp = AllocAndLoad(file, inf->offsetFrames,
        inf->frameSize * inf->numFrames);
    mo->frames = M_Malloc(sizeof(model_frame_t) * inf->numFrames);
    for(i = 0, frame = mo->frames; i < inf->numFrames; ++i, frame++)
    {
        dmd_packedFrame_t *pfr =
            (dmd_packedFrame_t *) (temp + inf->frameSize * i);
        dmd_packedVertex_t *pVtx;

        memcpy(frame->name, pfr->name, sizeof(pfr->name));
        frame->vertices = M_Malloc(sizeof(model_vertex_t) * inf->numVertices);
        frame->normals = M_Malloc(sizeof(model_vertex_t) * inf->numVertices);

        // Translate each vertex.
        for(k = 0, pVtx = pfr->vertices; k < inf->numVertices; ++k, pVtx++)
        {
            UnpackVector(USHORT(pVtx->normal), frame->normals[k].xyz);
            for(c = 0; c < 3; ++c)
            {
                frame->vertices[k].xyz[axis[c]] =
                    pVtx->vertex[c] * FLOAT(pfr->scale[c]) +
                    FLOAT(pfr->translate[c]);
            }
            // Aspect undo.
            frame->vertices[k].xyz[1] *= rModelAspectMod;

            for(c = 0; c < 3; ++c)
            {
                if(!k || frame->vertices[k].xyz[c] < frame->min[c])
                    frame->min[c] = frame->vertices[k].xyz[c];
                if(!k || frame->vertices[k].xyz[c] > frame->max[c])
                    frame->max[c] = frame->vertices[k].xyz[c];
            }
        }
    }
    M_Free(temp);

    F_Seek(file, inf->offsetLODs, SEEK_SET);
    F_Read(mo->lodInfo, sizeof(dmd_levelOfDetail_t) * inf->numLODs, file);

    for(i = 0; i < inf->numLODs; ++i)
    {
        mo->lodInfo[i].numTriangles = LONG(mo->lodInfo[i].numTriangles);
        mo->lodInfo[i].numGlCommands = LONG(mo->lodInfo[i].numGlCommands);
        mo->lodInfo[i].offsetTriangles = LONG(mo->lodInfo[i].offsetTriangles);
        mo->lodInfo[i].offsetGlCommands = LONG(mo->lodInfo[i].offsetGlCommands);

        triangles[i] =
            AllocAndLoad(file, mo->lodInfo[i].offsetTriangles,
                         sizeof(dmd_triangle_t) * mo->lodInfo[i].numTriangles);
        mo->lods[i].glCommands =
            AllocAndLoad(file, mo->lodInfo[i].offsetGlCommands,
                         sizeof(int) * mo->lodInfo[i].numGlCommands);
    }

    // Determine vertex usage at each LOD level.
    // This code assumes there will never be more than 8 LOD levels.
    mo->vertexUsage = M_Calloc(inf->numVertices);
    for(i = 0; i < inf->numLODs; ++i)
        for(k = 0; k < mo->lodInfo[i].numTriangles; ++k)
            for(c = 0; c < 3; ++c)
                mo->vertexUsage[SHORT(triangles[i][k].vertexIndices[c])]
                    |= 1 << i;

    // We don't need the triangles any more.
    for(i = 0; i < inf->numLODs; ++i)
        M_Free(triangles[i]);
}

static void R_RegisterModelSkin(model_t* mdl, int index)
{
    mdl->skins[index].id = R_RegisterSkin(NULL, mdl->skins[index].name,
        mdl->fileName, false, FILENAME_T_MAXLEN);

    if(!mdl->skins[index].id)
    {   // Not found!
        VERBOSE(Con_Printf("  %s (#%i) not found.\n",
                           mdl->skins[index].name, index));
    }
}

/**
 * Finds the existing model or loads in a new one.
 */
static int R_LoadModel(char* origfn)
{
    int                 i, index;
    model_t*            mdl;
    DFILE*              file = NULL;
    filename_t          filename;

    if(!origfn || !origfn[0])
        return 0; // No model specified.

    if(!R_FindResource2(RT_MODEL, DDRC_MODEL, filename, origfn, NULL,
                        FILENAME_T_MAXLEN))
    {
        R_MissingModel(origfn);
        return 0;
    }

    // Has this been already loaded?
    if((index = R_FindModelFor(filename)) < 0)
    {
        // Not loaded yet, try to open the file.
        if((file = F_Open(filename, "rb")) == NULL)
        {
            R_MissingModel(filename);
            return 0;
        }

        // Allocate a new model_t.
        if((index = R_NewModelFor(/*filename*/)) < 0)
        {
            F_Close(file);
            return 0;
        }
    }

    mdl = modellist[index];
    if(mdl->loaded)
    {
        if(file)
            F_Close(file);
        return index; // Already loaded.
    }

    // Now we can load in the data.
    F_Read(&mdl->header, sizeof(mdl->header), file);
    if(LONG(mdl->header.magic) == MD2_MAGIC)
    {   // Load as MD2.
        F_Rewind(file);
        R_LoadModelMD2(file, mdl);
    }
    else if(LONG(mdl->header.magic) == DMD_MAGIC)
    {   // Load as DMD.
        R_LoadModelDMD(file, mdl);
    }
    else
    {   // Bad magic!
        // Cancel the loading.
        M_Free(mdl);
        modellist[index] = 0;
        F_Close(file);
        return 0;
    }

    // We're done.
    mdl->loaded = true;
    mdl->allowTexComp = true;
    F_Close(file);
    strncpy(mdl->fileName, filename, FILENAME_T_MAXLEN);

    // Determine the actual (full) paths.
    for(i = 0; i < mdl->info.numSkins; ++i)
    {
        R_RegisterModelSkin(mdl, i);
    }

    return index;
}

int R_ModelFrameNumForName(int modelnum, char *fname)
{
    int                 i;
    model_t            *mdl;

    if(!modelnum)
        return 0;

    mdl = modellist[modelnum];
    for(i = 0; i < mdl->info.numFrames; ++i)
    {
        if(!stricmp(mdl->frames[i].name, fname))
            return i;
    }
    return 0;
}

/**
 * Returns the appropriate modeldef for the given state.
 */
static modeldef_t *GetStateModel(state_t *st, int select)
{
    modeldef_t         *modef, *iter;
    int                 mosel;

    if(!st || !stateModefs[st - states])
        return 0;

    modef = stateModefs[st - states];
    mosel = select & DDMOBJ_SELECTOR_MASK;

    if(select)
    {
        boolean             found;

        // Choose the correct selector, or selector zero if the given
        // one not available.
        found = false;
        for(iter = modef; iter && !found; iter = iter->selectNext)
            if(iter->select == mosel)
            {
                modef = iter;
                found = true;
            }
    }

    return modef;
}

modeldef_t *R_CheckIDModelFor(const char *id)
{
    int                 i;

    if(!id[0])
        return NULL;

    for(i = 0; i < numModelDefs; ++i)
        if(!strcmp(modefs[i].id, id))
            return modefs + i;
    return NULL;
}

/**
 * Is there a model for this mobj? The decision is made based on the
 * state and tics of the mobj. Returns the modeldefs that are in effect
 * at the moment (interlinks checked appropriately).
 */
float R_CheckModelFor(mobj_t *mo, modeldef_t **modef, modeldef_t **nextmodef)
{
    float               interp = -1;
    state_t            *st = mo->state;
    modeldef_t         *mdit;
    boolean             worldTime = false;

    // By default there are no models.
    *nextmodef = NULL;
    *modef = GetStateModel(st, mo->selector);
    if(!*modef)
        return -1; // No model available.

    // World time animation?
    if((*modef)->flags & MFF_WORLD_TIME_ANIM)
    {
        float               duration = (*modef)->interRange[0];
        float               offset = (*modef)->interRange[1];

        // Validate/modify the values.
        if(duration == 0)
            duration = 1;
        if(offset == -1)
        {
            offset = M_CycleIntoRange(MOBJ_TO_ID(mo), duration);
        }
        interp = M_CycleIntoRange(ddMapTime / duration + offset, 1);
        worldTime = true;
    }
    else
    {
        // Calculate the currently applicable intermark.
        interp = 1.0f - (mo->tics - frameTimePos) / (float) st->tics;
    }
/*
#if _DEBUG
if(mo->dPlayer)
    Con_Printf("itp:%f mot:%i stt:%i\n", interp, mo->tics, st->tics);
#endif
*/

    // First find the modef for the interpoint. Intermark is 'stronger'
    // than interrange.

    // Scan interlinks.
    while((*modef)->interNext && (*modef)->interNext->interMark <= interp)
        *modef = (*modef)->interNext;

    if(!worldTime)
    {
        // Scale to the modeldef's interpolation range.
        interp =
            (*modef)->interRange[0] + interp * ((*modef)->interRange[1] -
                                                (*modef)->interRange[0]);
    }

    // What would be the next model? Check interlinks first.
    if((*modef)->interNext)
    {
        *nextmodef = (*modef)->interNext;
    }
    else if(worldTime)
    {
        *nextmodef = GetStateModel(st, mo->selector);
    }
    else if(st->nextState > 0) // Check next state.
    {
        int                 max;
        boolean             foundNext;
        state_t            *it;

        // Find the appropriate state based on interrange.
        it = states + st->nextState;
        foundNext = false;
        if((*modef)->interRange[1] < 1)
        {
            boolean             stopScan;

            // Current modef doesn't interpolate to the end, find the
            // proper destination modef (it isn't just the next one).
            // Scan the states that follow (and interlinks of each).
            stopScan = false;
            max = 20; // Let's not be here forever...
            while(!stopScan)
            {
                if(!((!stateModefs[it - states] ||
                      GetStateModel(it, mo->selector)->interRange[0] > 0) &&
                     it->nextState > 0))
                {
                    stopScan = true;
                }
                else
                {
                    // Scan interlinks, then go to the next state.
                    if((mdit = GetStateModel(it, mo->selector)) && mdit->interNext)
                    {
                        boolean                 isDone = false;
                        while(!isDone)
                        {
                            mdit = mdit->interNext;
                            if(mdit)
                            {
                                if(mdit->interRange[0] <= 0) // A new beginning?
                                {
                                    *nextmodef = mdit;
                                    foundNext = true;
                                }
                            }

                            if(!mdit || foundNext)
                            {
                                isDone = true;
                            }
                        }
                    }

                    if(foundNext)
                    {
                        stopScan = true;
                    }
                    else
                    {
                        it = states + it->nextState;
                    }
                }

                if(max-- <= 0)
                    stopScan = true;
            }
            // \fixme What about max == -1? What should 'it' be then?
        }

        if(!foundNext)
            *nextmodef = GetStateModel(it, mo->selector);
    }

    // Is this group disabled?
    if(useModels >= 2 && (*modef)->group & useModels)
    {
        *modef = *nextmodef = NULL;
        return -1;
    }

    return interp;
}

static model_frame_t* R_GetModelFrame(int model, int frame)
{
    return modellist[model]->frames + frame;
}

static void R_GetModelBounds(int model, int frame, float min[3],
                             float max[3])
{
    model_frame_t*      mframe = R_GetModelFrame(model, frame);

    if(!mframe)
        Con_Error("R_GetModelBounds: bad model/frame.\n");

    memcpy(min, mframe->min, sizeof(float)*3);
    memcpy(max, mframe->max, sizeof(float)*3);
}

/**
 * Height range, really ("horizontal range" comes to mind...).
 */
static float R_GetModelHRange(int model, int frame, float *top, float *bottom)
{
    float               min[3], max[3];

    R_GetModelBounds(model, frame, min, max);
    *top = max[VY];
    *bottom = min[VY];
    return max[VY] - min[VY];
}

/**
 * Scales the given model so that it'll be 'destHeight' units tall.
 * The measurements are based on submodel zero. The scaling is done
 * uniformly!
 */
static void R_ScaleModel(modeldef_t *mf, float destHeight, float offset)
{
    submodeldef_t      *smf = &mf->sub[0];
    int                 i;
    float               top, bottom, height;
    float               scale;

    if(!smf->model)
        return;                 // No model to scale!

    // Find the top and bottom heights.
    height = R_GetModelHRange(smf->model, smf->frame, &top, &bottom);
    if(!height)
        height = 1;

    scale = destHeight / height;

    for(i = 0; i < 3; ++i)
        mf->scale[i] = scale;
    mf->offset[VY] = -bottom * scale + offset;
}

static void R_ScaleModelToSprite(modeldef_t* mf, int sprite, int frame)
{
    int                 off;
    spritedef_t*        spr = &sprites[sprite];
    material_snapshot_t ms;

    if(!spr->numFrames || spr->spriteFrames == NULL)
        return;
    Materials_Prepare(spr->spriteFrames[frame].mats[0], 0, NULL, &ms);

    off = spriteTextures[ms.units[MTU_PRIMARY].texInst->tex->ofTypeID]->offY -
        ms.height;
    if(off < 0)
        off = 0;

    R_ScaleModel(mf, ms.height, off);
}

float R_GetModelVisualRadius(modeldef_t* mf)
{
    int                 i;
    float               maxRadius = 0;

    if(!mf->sub[0].model)
        return 0;

    // Use the first frame bounds.
    for(i = 0; i < MAX_FRAME_MODELS; ++i)
    {
        float               min[3], max[3], radius;

        if(!mf->sub[i].model)
            break;

        R_GetModelBounds(mf->sub[i].model, mf->sub[i].frame, min, max);

        // Half the distance from bottom left to top right.
        radius = (mf->scale[VX] * (max[VX] - min[VX]) +
                  mf->scale[VZ] * (max[VZ] - min[VZ])) / 3.5f;
        if(radius > maxRadius)
            maxRadius = radius;
    }

    return maxRadius;
}

/**
 * Allocate room for a new skin file name. This allows using more than
 * the maximum number of skins.
 */
static short R_NewModelSkin(model_t *mdl, const char *fileName)
{
    int                 added = mdl->info.numSkins, i;

    mdl->skins =
        M_Realloc(mdl->skins, sizeof(dmd_skin_t) * ++mdl->info.numSkins);
    memset(mdl->skins + added, 0, sizeof(dmd_skin_t));
    strncpy(mdl->skins[added].name, fileName, 64);
    R_RegisterModelSkin(mdl, added);

    // Did we get a dupe?
    for(i = 0; i < mdl->info.numSkins - 1; ++i)
    {
        if(mdl->skins[i].id == mdl->skins[added].id)
        {
            // This is the same skin file.
            // We did a lot of unnecessary work...
            mdl->info.numSkins--;
            mdl->skins =
                M_Realloc(mdl->skins, sizeof(dmd_skin_t) * mdl->info.numSkins);
            return i;
        }
    }

    return added;
}

/**
 * Create a new modeldef or find an existing one. This is for ID'd models.
 */
static modeldef_t *R_GetIDModelDef(const char *id)
{
    modeldef_t         *md;

    // ID defined?
    if(!id[0])
        return NULL;

    // First try to find an existing modef.
    if((md = R_CheckIDModelFor(id)) != NULL)
        return md;

    // Get a new entry.
    md = modefs + numModelDefs++;
    memset(md, 0, sizeof(*md));
    strncpy(md->id, id, MODELDEF_ID_MAXLEN);
    return md;
}

/**
 * Create a new modeldef or find an existing one. There can be only one
 * model definition associated with a state/intermark pair.
 */
static modeldef_t *R_GetModelDef(int state, float interMark, int select)
{
    int                 i;
    modeldef_t         *md;

    if(state < 0 || state >= countStates.num)
        return NULL; // Not a valid state.

    // First try to find an existing modef.
    for(i = 0; i < numModelDefs; ++i)
        if(modefs[i].state == &states[state] &&
           modefs[i].interMark == interMark && modefs[i].select == select)
        {
            // Models are loaded in reverse order; this one already has
            // a model.
            return NULL;
        }

    // This is impossible, but checking won't hurt...
    if(numModelDefs >= maxModelDefs)
        return NULL;

    md = modefs + numModelDefs++;
    memset(md, 0, sizeof(*md));

    // Set initial data.
    md->state = &states[state];
    md->interMark = interMark;
    md->select = select;
    return md;
}

/**
 * Creates a modeldef based on the given DED info.
 * A pretty straightforward operation. No interlinks are set yet.
 * Autoscaling is done and the scale factors set appropriately.
 * After this has been called for all available Model DEDs, each
 * State that has a model will have a pointer to the one with the
 * smallest intermark (start of a chain).
 */
static void setupModel(ded_model_t *def)
{
    modeldef_t         *modef;
    int                 modelScopeFlags = def->flags | defs.modelFlags;
    ded_submodel_t     *subdef;
    submodeldef_t      *sub;
    int                 i, k, statenum = Def_GetStateNum(def->state);
    float               min[3], max[3];

    // Is this an ID'd model?
    if((modef = R_GetIDModelDef(def->id)) == NULL)
    {
        // No, normal State-model.
        if(statenum < 0)
        {
            Con_Message("R_SetupModel: Undefined state '%s'.\n", def->state);
            return;
        }

        modef =
            R_GetModelDef(statenum + def->off, def->interMark, def->selector);

        if(!modef)
            return; // Can't get a modef, quit!
    }

    // Init modef info (state & intermark already set).
    modef->def = def;
    modef->group = def->group;
    modef->flags = modelScopeFlags;
    for(i = 0; i < 3; ++i)
    {
        modef->offset[i] = def->offset[i];
        modef->scale[i] = def->scale[i];
    }
    modef->offset[VY] += defs.modelOffset; // Common Y axis offset.
    modef->scale[VY] *= defs.modelScale;   // Common Y axis scaling.
    modef->resize = def->resize;
    modef->skinTics = def->skinTics;
    for(i = 0; i < 2; ++i)
    {
        modef->interRange[i] = def->interRange[i];
    }
    if(modef->skinTics < 1)
        modef->skinTics = 1;

    // Submodels.
    for(i = 0, subdef = def->sub, sub = modef->sub; i < MAX_FRAME_MODELS;
        ++i, subdef++, sub++)
    {
        sub->model = R_LoadModel(subdef->filename.path);

        if(!sub->model)
            continue;

        sub->frame = R_ModelFrameNumForName(sub->model, subdef->frame);
        sub->frameRange = subdef->frameRange;
        // Frame range must always be greater than zero.
        if(sub->frameRange < 1)
            sub->frameRange = 1;

        // Submodel-specific flags cancel out model-scope flags!
        sub->flags = modelScopeFlags ^ subdef->flags;
        if(subdef->skinFilename.path[0])
        {
            // A specific file name has been given for the skin.
            sub->skin =
                R_NewModelSkin(modellist[sub->model],
                               subdef->skinFilename.path);
        }
        else
        {
            sub->skin = subdef->skin;
        }

        sub->skinRange = subdef->skinRange;
        // Skin range must always be greater than zero.
        if(sub->skinRange < 1)
            sub->skinRange = 1;

        // Offset within the model.
        for(k = 0; k < 3; ++k)
            sub->offset[k] = subdef->offset[k];

        sub->alpha = (byte) (subdef->alpha * 255);
        sub->shinySkin = R_RegisterSkin(subdef->filename.path,
            subdef->shinySkin, NULL, true, DED_PATH_LEN);

        // Should we allow texture compression with this model?
        if(sub->flags & MFF_NO_TEXCOMP)
        {
            // All skins of this model will no longer use compression.
            modellist[sub->model]->allowTexComp = false;
        }
    }

    // Do scaling, if necessary.
    if(modef->resize)
    {
        R_ScaleModel(modef, modef->resize, modef->offset[VY]);
    }
    else if(modef->state && modef->sub[0].flags & MFF_AUTOSCALE)
    {
        int                 sprNum = Def_GetSpriteNum(def->sprite.id);
        int                 sprFrame = def->spriteFrame;

        if(sprNum < 0)
        {   // No sprite ID given.
            sprNum = modef->state->sprite;
            sprFrame = modef->state->frame;
        }

        R_ScaleModelToSprite(modef, sprNum, sprFrame);
    }

    if(modef->state)
    {
        int                 stateNum = modef->state - states;

        // Associate this modeldef with its state.
        if(!stateModefs[stateNum])
        {
            // No modef; use this.
            stateModefs[stateNum] = modef;
        }
        else
        {   // Must check intermark; smallest wins!
            modeldef_t         *other = stateModefs[stateNum];

            if((modef->interMark <= other->interMark && // Should never be ==
                modef->select == other->select) || modef->select < other->select) // Smallest selector?
                stateModefs[stateNum] = modef;
        }
    }

    // Calculate the particle offset for each submodel.
    for(i = 0, sub = modef->sub; i < MAX_FRAME_MODELS; ++i, sub++)
    {
        if(sub->model)
        {
            R_GetModelBounds(sub->model, sub->frame, min, max);

            // Apply the various scalings and offsets.
            for(k = 0; k < 3; ++k)
            {
                modef->ptcOffset[i][k] =
                    ((max[k] + min[k]) / 2 + sub->offset[k]) * modef->scale[k] +
                    modef->offset[k];
            }
        }
        else
        {
            memset(modef->ptcOffset[i], 0, sizeof(modef->ptcOffset[i]));
        }
    }

    // Calculate visual radius for shadows.
    if(def->shadowRadius)
    {
        modef->visualRadius = def->shadowRadius;
    }
    else
    {
        modef->visualRadius = R_GetModelVisualRadius(modef);
    }
}

/**
 * States must be initialized before this.
 */
void R_InitModels(void)
{
    int                 i, k, minsel;
    float               minmark;
    modeldef_t*         me, *other, *closest;
    uint                usedTime;

    // Dedicated servers do nothing with models.
    if(isDedicated || ArgCheck("-nomd2"))
        return;

    Con_Message("R_InitModels: Initializing MD2 models.\n");
    usedTime = Sys_GetRealTime();

    if(modefs)
        M_Free(modefs);

    // There can't be more modeldefs than there are DED Models.
    // There can be fewer, though.
    maxModelDefs = defs.count.models.num;
    modefs = M_Malloc(sizeof(modeldef_t) * maxModelDefs);
    numModelDefs = 0;

    // Clear the modef pointers of all States.
    stateModefs = M_Realloc(stateModefs, countStates.num * sizeof(*stateModefs));
    memset(stateModefs, 0, countStates.num * sizeof(*stateModefs));

    // Read in the model files and their data.
    // Use the latest definition available for each sprite ID.
    for(i = defs.count.models.num - 1; i >= 0; --i)
    {
        //Con_Progress(1, PBARF_DONTSHOW);
        setupModel(defs.models + i);
    }

    // Create interlinks. Note that the order in which the defs were loaded
    // is important. We want to allow "patch" definitions, right?

    // For each modeldef we will find the "next" def.
    for(i = numModelDefs - 1, me = modefs + i; i >= 0; --i, --me)
    {
        minmark = 2; // max = 1, so this is "out of bounds".
        closest = NULL;
        for(k = numModelDefs - 1, other = modefs + k; k >= 0; --k, --other)
        {
            // Same state and a bigger order are the requirements.
            if(other->state == me->state && other->def > me->def && // Defined after me.
               other->interMark > me->interMark &&
               other->interMark < minmark)
            {
                minmark = other->interMark;
                closest = other;
            }
        }

        me->interNext = closest;
    }

    // Create selectlinks.
    for(i = numModelDefs - 1, me = modefs + i; i >= 0; --i, --me)
    {
        minsel = DDMAXINT;
        closest = NULL;

        // Start scanning from the next definition.
        for(k = numModelDefs - 1, other = modefs + k; k >= 0; --k, --other)
        {
            // Same state and a bigger order are the requirements.
            if(other->state == me->state && other->def > me->def && // Defined after me.
               other->select > me->select && other->select < minsel &&
               other->interMark >= me->interMark)
            {
                minsel = other->select;
                closest = other;
            }
        }

        me->selectNext = closest;

/*#if _DEBUG
if(closest)
   Con_Message("%s -> %s\n", defs.states[me->state - states].id,
               defs.states[closest->state - states].id);
#endif*/
    }

    Con_Message("R_InitModels: Done in %.2f seconds.\n",
                (Sys_GetRealTime() - usedTime) / 1000.0f);
}

/**
 * Frees all memory allocated for models.
 * \fixme Why only centralized memory deallocation? Bad design...
 */
void R_ShutdownModels(void)
{
    int                 i, k;
    model_t            *m;

    if(modefs)
        M_Free(modefs);
    modefs = NULL;
    if(stateModefs)
        M_Free(stateModefs);
    stateModefs = NULL;

    for(i = 1; i < MAX_MODELS; ++i)
    {
        if(!(m = modellist[i]))
            continue;

        M_Free(m->skins);
        //M_Free(modellist[i]->texCoords);
        for(k = 0; k < m->info.numFrames; ++k)
        {
            M_Free(m->frames[k].vertices);
            M_Free(m->frames[k].normals);
        }
        M_Free(m->frames);

        for(k = 0; k < m->info.numLODs; ++k)
        {
            //M_Free(modellist[i]->lods[k].triangles);
            M_Free(m->lods[k].glCommands);
        }
        M_Free(m->vertexUsage);
        M_Free(m);
        modellist[i] = NULL;
    }
}

void R_SetModelFrame(modeldef_t* modef, int frame)
{
    int                 k;
    model_t*            mdl;

    for(k = 0; k < DED_MAX_SUB_MODELS; ++k)
    {
        if(!modef->sub[k].model)
            continue;

        mdl = modellist[modef->sub[k].model];
        // Modify the modeldef itself: set the current frame.
        modef->sub[k].frame = frame % mdl->info.numFrames;
    }
}

void R_PrecacheModelSkins(modeldef_t* modef)
{
    int                 k, sub;
    model_t*            mdl;

    if(!(useModels && precacheSkins))
        return;

    // Precache this.
    for(sub = 0; sub < MAX_FRAME_MODELS; ++sub)
    {
        const skinname_t*       sn;

        if(!modef->sub[sub].model)
            continue;

        mdl = modellist[modef->sub[sub].model];
        // Load all skins.
        for(k = 0; k < mdl->info.numSkins; ++k)
        {
            if((sn = R_GetSkinNameByIndex(mdl->skins[k].id)))
            {
                texture_load_params_t params;

                memset(&params, 0, sizeof(params));
                params.flags = (!mdl->allowTexComp? TLF_NO_COMPRESSION : 0);

                GL_PrepareGLTexture(sn->id, &params, NULL);
            }
        }

        // Load the shiny skin.
        if((sn = R_GetSkinNameByIndex(modef->sub[sub].shinySkin)))
        {
            GL_PrepareGLTexture(sn->id, NULL, NULL);
        }
    }
}

void R_PrecacheSkinsForState(int stateIndex)
{
    if(stateIndex <= 0 || stateIndex >= defs.count.states.num ||
       !stateModefs[stateIndex])
        return;

    R_PrecacheModelSkins(stateModefs[stateIndex]);
}

/**
 * The skins are also bound here once so they should be ready for use the
 * next time they're needed.
 */
int R_PrecacheSkinsForMobj(void* p, void* context)
{
    int                 i;
    mobj_t*             mo = (mobj_t*) p;
    modeldef_t*         modef;

    // Check through all the model definitions.
    for(i = 0, modef = modefs; i < numModelDefs; ++i, modef++)
    {
        if(!modef->state)
            continue;
        if(mo->type < 0 || mo->type >= defs.count.mobjs.num)
            continue; // Hmm?
        if(stateOwners[modef->state - states] != &mobjInfo[mo->type])
            continue;

        R_PrecacheModelSkins(modef);
    }

    return true; // Used as iterator.
}
