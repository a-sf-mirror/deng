/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2009 Daniel Swanson <danij@dengine.net>
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
 * b_context.c: Bindings Contexts.
 */

// HEADER FILES ------------------------------------------------------------

#include "doomsday.h"
#include "de_base.h"
#include "de_console.h"
#include "de_misc.h"

#include "b_main.h"
#include "b_context.h"
#include "p_control.h"

#include <string.h>

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int bindContextCount;
static bcontext_t** bindContexts;

// CODE --------------------------------------------------------------------

/**
 * Destroy all binding contexts and the bindings within the contexts.
 * Called at shutdown time.
 */
void B_DestroyAllContexts(void)
{
    int                 i;

    if(bindContexts)
    {
        // Do not use the global bindContextCount to control the loop; it is
        // changed as a sideeffect of B_DestroyContext() and also, the
        // bindContexts array itself is shifted back to idx 0 afterwards.
        int                 numBindClasses = bindContextCount;

        for(i = 0; i < numBindClasses; ++i)
        {
            B_DestroyContext(bindContexts[0]);
        }
        M_Free(bindContexts);
    }
    bindContexts = NULL;
    bindContextCount = 0;
}

/**
 * Marks all device states with the highest-priority binding context to
 * which they have a connection via device bindings. This ensures that if a
 * high-priority context is using a particular device state, lower-priority
 * contexts will not be using the same state for their own controls.
 *
 * Called automatically whenever a context is activated or deactivated.
 */
void B_UpdateDeviceStateAssociations(void)
{
    int                 i;
    uint                k;
    bcontext_t*         bc;
    evbinding_t*        eb;
    controlbinding_t*   conBin;
    dbinding_t*         db;

    I_ClearDeviceContextAssociations();

    // We need to iterate through all the device bindings in all context.
    for(i = 0; i < bindContextCount; ++i)
    {
        bc = bindContexts[i];
        // Skip inactive contexts.
        if(!(bc->flags & BCF_ACTIVE))
            continue;

        // Mark all event bindings in the context.
        for(eb = bc->commandBinds.next; eb != &bc->commandBinds; eb = eb->next)
        {
            inputdev_t*         dev = I_GetDevice(eb->device, false);

            switch(eb->type)
            {
            case E_TOGGLE:
                if(!dev->keys[eb->id].bContext)
                    dev->keys[eb->id].bContext = bc;
                break;

            case E_AXIS:
                if(!dev->axes[eb->id].bContext)
                    dev->axes[eb->id].bContext = bc;
                break;

            case E_ANGLE:
                if(!dev->hats[eb->id].bContext)
                    dev->hats[eb->id].bContext = bc;
                break;

            case E_SYMBOLIC:
                break;

            default:
                Con_Error("B_UpdateDeviceStateAssociations: Invalid value, "
                          "eb->type = %i.", (int) eb->type);
                break;
            }
        }

        // All controls in the context.
        for(conBin = bc->controlBinds.next; conBin != &bc->controlBinds;
            conBin = conBin->next)
        {
            for(k = 0; k < DDMAXPLAYERS; ++k)
            {
                // Associate all the device bindings.
                for(db = conBin->deviceBinds[k].next; db != &conBin->deviceBinds[k];
                    db = db->next)
                {
                    inputdev_t*         dev = I_GetDevice(db->device, false);

                    switch(db->type)
                    {
                    case CBD_TOGGLE:
                        if(!dev->keys[db->id].bContext)
                            dev->keys[db->id].bContext = bc;
                        break;

                    case CBD_AXIS:
                        if(!dev->axes[db->id].bContext)
                            dev->axes[db->id].bContext = bc;
                        break;

                    case CBD_ANGLE:
                        if(!dev->hats[db->id].bContext)
                            dev->hats[db->id].bContext = bc;
                        break;

                    default:
                        Con_Error("B_UpdateDeviceStateAssociations: Invalid value, "
                                  "db->type = %i.", (int) db->type);
                        break;
                    }
                }
            }
        }

        // If the context have made a broad device acquisition, mark all
        // relevant states.
        if(bc->flags & BCF_ACQUIRE_KEYBOARD)
        {
            inputdev_t*         dev = I_GetDevice(IDEV_KEYBOARD, false);

            for(k = 0; k < dev->numKeys; ++k)
            {
                if(!dev->keys[k].bContext)
                    dev->keys[k].bContext = bc;
            }
        }

        if(bc->flags & BCF_ACQUIRE_ALL)
        {
            int                 j;
            for(j = 0; j < NUM_INPUT_DEVICES; ++j)
            {
                inputdev_t*         dev = I_GetDevice(j, true);

                if(!dev)
                    continue;

                for(k = 0; k < dev->numKeys; ++k)
                {
                    if(!dev->keys[k].bContext)
                        dev->keys[k].bContext = bc;
                }
                for(k = 0; k < dev->numAxes; ++k)
                {
                    if(!dev->axes[k].bContext)
                        dev->axes[k].bContext = bc;
                }
                for(k = 0; k < dev->numHats; ++k)
                {
                    if(!dev->hats[k].bContext)
                        dev->hats[k].bContext = bc;
                }
            }
        }
    }
}

static void B_SetContextCount(int count)
{
    bindContexts = M_Realloc(bindContexts, sizeof(bcontext_t*) * count);
    bindContextCount = count;
}

static void B_InsertContext(bcontext_t* bc, int contextIdx)
{
    B_SetContextCount(bindContextCount + 1);
    if(contextIdx < bindContextCount - 1)
    {
        // We need to make room for this new binding context.
        memmove(&bindContexts[contextIdx + 1], &bindContexts[contextIdx],
                sizeof(bcontext_t*) * (bindContextCount - 1 - contextIdx));
    }
    bindContexts[contextIdx] = bc;
}

static void B_RemoveContext(bcontext_t* bc)
{
    int                 contextIdx = B_GetContextPos(bc);

    if(contextIdx >= 0)
    {
        memmove(&bindContexts[contextIdx], &bindContexts[contextIdx + 1],
                sizeof(bcontext_t*) * (bindContextCount - 1 - contextIdx));

        B_SetContextCount(bindContextCount - 1);
    }
}

/**
 * Creates a new binding context. The new context has the highest priority
 * of all existing contexts, and is inactive.
 */
bcontext_t* B_NewContext(const char* name)
{
    bcontext_t*           bc = M_Calloc(sizeof(bcontext_t));

    bc->name = strdup(name);
    B_InitCommandBindingList(&bc->commandBinds);
    B_InitControlBindingList(&bc->controlBinds);
    B_InsertContext(bc, 0);
    return bc;
}

void B_DestroyContext(bcontext_t* bc)
{
    if(!bc)
        return;

    B_RemoveContext(bc);
    M_Free(bc->name);
    B_ClearContext(bc);
    M_Free(bc);
}

void B_ClearContext(bcontext_t* bc)
{
    B_DestroyCommandBindingList(&bc->commandBinds);
    B_DestroyControlBindingList(&bc->controlBinds);
}

void B_ActivateContext(bcontext_t* bc, boolean doActivate)
{
    if(!bc)
        return;

    VERBOSE( Con_Message("B_ActivateContext: %s context \"%s\".\n",
                         doActivate? "Activating" : "Deactivating",
                         bc->name) );

    bc->flags &= ~BCF_ACTIVE;
    if(doActivate)
        bc->flags |= BCF_ACTIVE;
    B_UpdateDeviceStateAssociations();

    if(bc->flags & BCF_ACQUIRE_ALL)
    {
        int i;
        for(i = 0; i < NUM_INPUT_DEVICES; ++i)
        {
            I_DeviceReset(i);
        }
    }
}

void B_AcquireKeyboard(bcontext_t* bc, boolean doAcquire)
{
    bc->flags &= ~BCF_ACQUIRE_KEYBOARD;
    if(doAcquire)
        bc->flags |= BCF_ACQUIRE_KEYBOARD;
    B_UpdateDeviceStateAssociations();
}

void B_AcquireAll(bcontext_t* bc, boolean doAcquire)
{
    bc->flags &= ~BCF_ACQUIRE_ALL;
    if(doAcquire)
        bc->flags |= BCF_ACQUIRE_ALL;
    B_UpdateDeviceStateAssociations();
}

void B_SetContextFallback(const char* name, int (*responderFunc)(event_t*))
{
    bcontext_t *ctx = B_ContextByName(name);

    if(!ctx)
        return;

    ctx->fallbackResponder = responderFunc;
}

bcontext_t* B_ContextByName(const char* name)
{
    int                 i;

    for(i = 0; i < bindContextCount; ++i)
    {
        if(!strcasecmp(name, bindContexts[i]->name))
            return bindContexts[i];
    }

    return NULL;
}

bcontext_t* B_ContextByPos(int pos)
{
    if(pos < 0 || pos >= bindContextCount)
        return NULL;

    return bindContexts[pos];
}

int B_ContextCount(void)
{
    return bindContextCount;
}

int B_GetContextPos(bcontext_t* bc)
{
    int                 i;

    for(i = 0; i < bindContextCount; ++i)
    {
        if(bindContexts[i] == bc)
            return i;
    }

    return -1;
}

void B_ReorderContext(bcontext_t* bc, int pos)
{
    B_RemoveContext(bc);
    B_InsertContext(bc, pos);
}

controlbinding_t* B_NewControlBinding(bcontext_t* bc)
{
    int                 i;

    controlbinding_t* conBin = M_Calloc(sizeof(controlbinding_t));
    conBin->bid = B_NewIdentifier();
    for(i = 0; i < DDMAXPLAYERS; ++i)
    {
        B_InitDeviceBindingList(&conBin->deviceBinds[i]);
    }

    // Link it in.
    conBin->next = &bc->controlBinds;
    conBin->prev = bc->controlBinds.prev;
    bc->controlBinds.prev->next = conBin;
    bc->controlBinds.prev = conBin;

    return conBin;
}

controlbinding_t* B_FindControlBinding(bcontext_t* bc, int control)
{
    controlbinding_t*   i;

    for(i = bc->controlBinds.next; i != &bc->controlBinds; i = i->next)
    {
        if(i->control == control)
            return i;
    }
    return NULL;
}

controlbinding_t* B_GetControlBinding(bcontext_t* bc, int control)
{
    controlbinding_t* b = B_FindControlBinding(bc, control);

    if(!b)
    {
        // Create a new one.
        b = B_NewControlBinding(bc);
        b->control = control;
    }
    return b;
}

void B_DestroyControlBinding(controlbinding_t* conBin)
{
    int                 i;

    if(!conBin)
        return;

    assert(conBin->bid != 0);

    // Unlink first, if linked.
    if(conBin->prev)
    {
        conBin->prev->next = conBin->next;
        conBin->next->prev = conBin->prev;
    }

    for(i = 0; i < DDMAXPLAYERS; ++i)
    {
        B_DestroyDeviceBindingList(&conBin->deviceBinds[i]);
    }
    M_Free(conBin);
}

void B_InitControlBindingList(controlbinding_t* listRoot)
{
    memset(listRoot, 0, sizeof(*listRoot));
    listRoot->next = listRoot->prev = listRoot;
}

void B_DestroyControlBindingList(controlbinding_t* listRoot)
{
    while(listRoot->next != listRoot)
    {
        B_DestroyControlBinding(listRoot->next);
    }
}

/**
 * @return  @c true, if the binding was found and deleted.
 */
boolean B_DeleteBinding(bcontext_t* bc, int bid)
{
    int                 i;
    evbinding_t*        eb = 0;
    controlbinding_t*   conBin = 0;
    dbinding_t*         db = 0;

    // Check if it is one of the command bindings.
    for(eb = bc->commandBinds.next; eb != &bc->commandBinds; eb = eb->next)
    {
        if(eb->bid == bid)
        {
            B_DestroyCommandBinding(eb);
            return true;
        }
    }

    // How about one fo the control bindings?
    for(conBin = bc->controlBinds.next; conBin != &bc->controlBinds; conBin = conBin->next)
    {
        if(conBin->bid == bid)
        {
            B_DestroyControlBinding(conBin);
            return true;
        }

        // It may also be a device binding.
        for(i = 0; i < DDMAXPLAYERS; ++i)
        {
            for(db = conBin->deviceBinds[i].next; db != &conBin->deviceBinds[i]; db = db->next)
            {
                if(db->bid == bid)
                {
                    B_DestroyDeviceBinding(db);
                    return true;
                }
            }
        }
    }

    return false;
}

boolean B_TryEvent(ddevent_t* event)
{
    int                 i;
    evbinding_t*        eb;
    event_t             ev;

    DD_ConvertEvent(event, &ev);

    for(i = 0; i < bindContextCount; ++i)
    {
        bcontext_t*           bc = bindContexts[i];

        if(!(bc->flags & BCF_ACTIVE))
            continue;

        // See if the command bindings will have it.
        for(eb = bc->commandBinds.next; eb != &bc->commandBinds; eb = eb->next)
        {
            if(B_TryCommandBinding(eb, event, bc))
                return true;
        }

        // Try the fallback.
        if(bc->fallbackResponder && bc->fallbackResponder(&ev))
            return true;
    }
    // Nobody used it.
    return false;
}

/**
 * Looks through the bindings to find the ones that are bound to the
 * specified command. The result is a space-separated list of bindings
 * such as (idnum is the binding ID number):
 *
 * <tt>idnum@@game:key-space-down idnum@@game:key-e-down</tt>
 *
 * @param cmd           Command to look for.
 * @param buf           Output buffer for the result.
 * @param bufSize       Size of output buffer.
 *
 * @return              Number of bindings found for the command.
 */
int B_BindingsForCommand(const char* cmd, char* buf, size_t bufSize)
{
    ddstring_t          result;
    ddstring_t          str;
    bcontext_t*         bc;
    int                 i;
    int                 numFound = 0;

    Str_Init(&result);
    Str_Init(&str);

    for(i = 0; i < bindContextCount; ++i)
    {
        evbinding_t* e;
        bc = bindContexts[i];
        for(e = bc->commandBinds.next; e != &bc->commandBinds; e = e->next)
        {
            if(strcmp(e->command, cmd))
                continue;
            // It's here!
            if(numFound)
            {
                Str_Append(&result, " ");
            }
            numFound++;
            B_EventBindingToString(e, &str);
            Str_Appendf(&result, "%i@%s:%s", e->bid, bc->name, Str_Text(&str));
        }
    }

    // Copy the result to the return buffer.
    memset(buf, 0, bufSize);
    strncpy(buf, Str_Text(&result), bufSize - 1);

    Str_Free(&result);
    Str_Free(&str);
    return numFound;
}

/**
 * Looks through the bindings to find the ones that are bound to the
 * specified control. The result is a space-separated list of bindings.
 *
 * @param localPlayer   Number of the local player (first one always 0).
 * @param controlName   Name of the player control.
 * @param inverse       One of BFCI_*.
 * @param buf           Output buffer for the result.
 * @param bufSize       Size of output buffer.
 *
 * @return              Number of bindings found for the command.
 */
int B_BindingsForControl(int localPlayer, const char* controlName,
                         int inverse, char* buf, size_t bufSize)
{
    ddstring_t          result;
    ddstring_t          str;
    bcontext_t*         bc;
    int                 i;
    int                 numFound = 0;

    if(localPlayer < 0 || localPlayer >= DDMAXPLAYERS)
        return 0;

    Str_Init(&result);
    Str_Init(&str);

    for(i = 0; i < bindContextCount; ++i)
    {
        controlbinding_t*   c;
        dbinding_t*         d;

        bc = bindContexts[i];
        for(c = bc->controlBinds.next; c != &bc->controlBinds; c = c->next)
        {
            const char*         name = P_PlayerControlById(c->control)->name;

            if(strcmp(name, controlName))
                continue; // Wrong control.

            for(d = c->deviceBinds[localPlayer].next; d != &c->deviceBinds[localPlayer]; d = d->next)
            {
                if(inverse == BFCI_BOTH ||
                   (inverse == BFCI_ONLY_NON_INVERSE && !(d->flags & CBDF_INVERSE)) ||
                   (inverse == BFCI_ONLY_INVERSE && (d->flags & CBDF_INVERSE)))
                {
                    // It's here!
                    if(numFound)
                    {
                        Str_Append(&result, " ");
                    }
                    numFound++;
                    B_DeviceBindingToString(d, &str);
                    Str_Appendf(&result, "%i@%s:%s", d->bid, bc->name, Str_Text(&str));
                }
            }
        }
    }

    // Copy the result to the return buffer.
    memset(buf, 0, bufSize);
    strncpy(buf, Str_Text(&result), bufSize - 1);

    Str_Free(&result);
    Str_Free(&str);
    return numFound;
}

boolean B_FindMatchingBinding(bcontext_t* bc, evbinding_t* match1,
                              dbinding_t* match2, evbinding_t** evResult,
                              dbinding_t** dResult)
{
    evbinding_t*        e;
    controlbinding_t*   c;
    dbinding_t*         d;
    int                 i;

    *evResult = NULL;
    *dResult = NULL;

    for(e = bc->commandBinds.next; e != &bc->commandBinds; e = e->next)
    {
        // TODO: A bit lazy here, should also match all the conditions.
        // Now we just consider all bindings with conditions unique...
        if(e->numConds)
            continue;

        if(match1 && match1->bid != e->bid)
        {
            if(match1->device == e->device && match1->id == e->id &&
               match1->type == e->type && match1->state == e->state)
            {
                *evResult = e;
                return true;
            }
        }
        if(match2)
        {
            if(match2->device == e->device && match2->id == e->id &&
               match2->type == e->type)
            {
                *evResult = e;
                return true;
            }
        }
    }

    for(c = bc->controlBinds.next; c != &bc->controlBinds; c = c->next)
    {
        for(i = 0; i < DDMAXPLAYERS; ++i)
        {
            for(d = c->deviceBinds[i].next; d != &c->deviceBinds[i]; d = d->next)
            {
                // Should also match all the conditions, now we just
                // consider all bindings with conditions unique...
                if(d->numConds)
                    continue;

                if(match1)
                {
                    if(match1->device == d->device && match1->id == d->id &&
                       match1->type == d->type)
                    {
                        *dResult = d;
                        return true;
                    }
                }

                if(match2 && match2->bid != d->bid)
                {
                    if(match2->device == d->device && match2->id == d->id &&
                       match2->type == d->type)
                    {
                        *dResult = d;
                        return true;
                    }
                }
            }
        }
    }

    // Nothing found.
    return false;
}

void B_PrintContexts(void)
{
    int                 i;
    bcontext_t*         bc;

    Con_Printf("%i binding contexts defined:\n", bindContextCount);

    for(i = 0; i < bindContextCount; ++i)
    {
        bc = bindContexts[i];
        Con_Printf("[%3i] \"%s\" (%s)\n", i, bc->name,
                   (bc->flags & BCF_ACTIVE)? "active" : "inactive");
    }
}

void B_PrintAllBindings(void)
{
    int                 i, k, count;
    bcontext_t*         bc;
    evbinding_t*        e;
    controlbinding_t*   c;
    dbinding_t*         d;
    ddstring_t*         str = Str_New();

    Con_Printf("%i binding contexts defined.\n", bindContextCount);

#define BIDFORMAT   "[%3i]"
    for(i = 0; i < bindContextCount; ++i)
    {
        bc = bindContexts[i];

        Con_Printf("Context \"%s\" (%s):\n", bc->name,
                   (bc->flags & BCF_ACTIVE)? "active" : "inactive");

        // Commands.
        for(count = 0, e = bc->commandBinds.next; e != &bc->commandBinds; e = e->next, count++);

        if(count)
            Con_Printf("  %i event bindings:\n", count);

        for(e = bc->commandBinds.next; e != &bc->commandBinds; e = e->next)
        {
            B_EventBindingToString(e, str);
            Con_Printf("  "BIDFORMAT" %s : %s\n", e->bid, Str_Text(str),
                       e->command);
        }

        // Controls.
        for(count = 0, c = bc->controlBinds.next; c != &bc->controlBinds; c = c->next, count++);

        if(count)
            Con_Printf("  %i control bindings.\n", count);

        for(c = bc->controlBinds.next; c != &bc->controlBinds; c = c->next)
        {
            const char* controlName = P_PlayerControlById(c->control)->name;

            Con_Printf("  Control \"%s\" "BIDFORMAT":\n", controlName,
                       c->bid);

            for(k = 0; k < DDMAXPLAYERS; ++k)
            {
                for(count = 0, d = c->deviceBinds[k].next; d != &c->deviceBinds[k];
                    d = d->next, count++);

                if(!count)
                    continue;

                Con_Printf("    Local player %i has %i device bindings for \"%s\":\n",
                           k + 1, count, controlName);
                for(d = c->deviceBinds[k].next; d != &c->deviceBinds[k]; d = d->next)
                {
                    B_DeviceBindingToString(d, str);
                    Con_Printf("    "BIDFORMAT" %s\n", d->bid, Str_Text(str));
                }
            }
        }
    }

    Str_Delete(str);
}

void B_WriteContextToFile(const bcontext_t* bc, FILE* file)
{
    evbinding_t*        e;
    controlbinding_t*   c;
    dbinding_t*         d;
    int                 k;
    ddstring_t*         str = Str_New();

    // Commands.
    for(e = bc->commandBinds.next; e != &bc->commandBinds; e = e->next)
    {
        B_EventBindingToString(e, str);
        fprintf(file, "bindevent \"%s:%s\" \"", bc->name, Str_Text(str));
        M_WriteTextEsc(file, e->command);
        fprintf(file, "\"\n");
    }

    // Controls.
    for(c = bc->controlBinds.next; c != &bc->controlBinds; c = c->next)
    {
        const char*         controlName =
            P_PlayerControlById(c->control)->name;

        for(k = 0; k < DDMAXPLAYERS; ++k)
        {
            for(d = c->deviceBinds[k].next; d != &c->deviceBinds[k]; d = d->next)
            {
                B_DeviceBindingToString(d, str);
                fprintf(file, "bindcontrol local%i-%s \"%s\"\n", k + 1,
                        controlName, Str_Text(str));
            }
        }
    }

    Str_Delete(str);
}
