/* $Id$
 * Copyright (C) 2005 Jaakko Ker�nen <jaakko.keranen@iki.fi>
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
 * Common controls menu
 */

// HEADER FILES ------------------------------------------------------------

#if __JDOOM__
# include "doomdef.h"
# include "doomstat.h"
# include "m_menu.h"
# include "Mn_def.h"
# include "D_Action.h"
# include "hu_stuff.h"
# include "s_sound.h"
# include "g_game.h"
# include "d_config.h"
# include "jDoom/m_ctrl.h"
#elif __JHERETIC__
# include "jHeretic/Doomdef.h"
# include "jHeretic/Mn_def.h"
# include "jHeretic/H_Action.h"
# include "jHeretic/h_config.h"
# include "jHeretic/m_ctrl.h"
#elif __JHEXEN__
# include "jHexen/h2def.h"
# include "jHexen/mn_def.h"
# include "jHexen/h2_actn.h"
# include "jHexen/x_config.h"
# include "Common/hu_stuff.h"
# include "jHexen/m_ctrl.h"
#endif

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern float   menu_alpha;
extern int menusnds[];
extern Menu_t  ControlsDef;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

const Control_t *grabbing = NULL;

// Binding classes (for the dynamic event responder chain)
bindclass_t BindClasses[] = {
    {"map", GBC_CLASS1, 0, 0},
    {"mapfollowoff", GBC_CLASS2, 0, 0},
    {"menu", GBC_CLASS3, 0, 1},
    {"menuhotkey", GBC_MENUHOTKEY, 1, 0},
    {"chat", GBC_CHAT, 0, 0},
    {"message", GBC_MESSAGE, 0, 1},
    {NULL}
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

CTLCFG_TYPE SCControlConfig(int option, void *data)
{
    grabbing = controls + option;
}

/*
 * spacecat
 */
void spacecat(char *str, const char *catstr)
{
    if(str[0])
        strcat(str, " ");

    if(!stricmp(catstr, "smcln"))
        catstr = ";";

    strcat(str, catstr);
}

/*
 * M_DrawControlsMenu
 */
void M_DrawControlsMenu(void)
{
    int     i;
    char    controlCmd[80];
    char    buff[80], prbuff[80], *token;
    const Menu_t *menu = &ControlsDef;
    const MenuItem_t *item = menu->items + menu->firstItem;
    const Control_t *ctrl;

#if __JDOOM__
    M_DrawTitle("CONTROLS", menu->y - 28);
    sprintf(buff, "PAGE %i/%i", menu->firstItem / menu->numVisItems + 1,
            menu->itemCount / menu->numVisItems + 1);
    M_WriteText2(160 - M_StringWidth(buff, hu_font_a) / 2, menu->y - 12, buff,
                 hu_font_a, 1, .7f, .3f, menu_alpha);
#else
    M_WriteText2(120, 2, "CONTROLS", hu_font_b, cfg.menuColor[0],
                 cfg.menuColor[1], cfg.menuColor[2], menu_alpha);

    gl.Color4f( 1, 1, 1, menu_alpha);

    // Draw the page arrows.
    token = (!menu->firstItem || MenuTime & 8) ? "invgeml2" : "invgeml1";
    GL_DrawPatch_CS(menu->x, menu->y - 12, W_GetNumForName(token));
    token = (menu->firstItem + menu->numVisItems >= menu->itemCount ||
             MenuTime & 8) ? "invgemr2" : "invgemr1";
    GL_DrawPatch_CS(312 - menu->x, menu->y - 12, W_GetNumForName(token));
#endif

    for(i = 0; i < menu->numVisItems && menu->firstItem + i < menu->itemCount;
        i++, item++)
    {
        if(item->type == ITT_EMPTY)
            continue;

        ctrl = controls + item->option;
        strcpy(buff, "");
        if(ctrl->flags & CLF_ACTION)
            sprintf(controlCmd, "+%s", ctrl->command);
        else
            strcpy(controlCmd, ctrl->command);
        // Let's gather all the bindings for this command in all bind classes.
        if(!B_BindingsForCommand(controlCmd, buff, -1))
            strcpy(buff, "NONE");

        // Now we must interpret what the bindings string says.
        // It may contain characters we can't print.
        strcpy(prbuff, "");
        token = strtok(buff, " ");
        while(token)
        {
            if(token[0] == '+')
            {
                spacecat(prbuff, token + 1);
            }
            if((token[0] == '*' && !(ctrl->flags & CLF_REPEAT)) ||
               token[0] == '-')
            {
                spacecat(prbuff, token);
            }
            token = strtok(NULL, " ");
        }
        strupr(prbuff);

        if(grabbing == ctrl)
        {
            // We're grabbing for this control.
            spacecat(prbuff, "...");
        }
#if __JHEXEN__
        M_WriteText2(menu->x + 134, menu->y + (i * menu->itemHeight), prbuff,
                        hu_font_a, 1, 0.7f, 0.3f, menu_alpha);
#else
        M_WriteText2(menu->x + 134, menu->y + (i * menu->itemHeight), prbuff,
                        hu_font_a, 1, 1, 1, menu_alpha);
#endif
    }
}

/*
 * Set default bindings for unbound Controls.
 */
void G_DefaultBindings(void)
{
    int     i;
    const Control_t *ctr;
    char    evname[80], cmd[256], buff[256];
    event_t event;

    // Check all Controls.
    for(i = 0; controls[i].command[0]; i++)
    {
        ctr = controls + i;
        // If this command is bound to something, skip it.
        sprintf(cmd, "%s%s", ctr->flags & CLF_ACTION ? "+" : "", ctr->command);
        memset(buff, 0, sizeof(buff));
        if(B_BindingsForCommand(cmd, buff, -1))
            continue;

        // This Control has no bindings, set it to the default.
        sprintf(buff, "\"%s\"", ctr->command);
        if(ctr->defKey)
        {
            event.type = ev_keydown;
            event.data1 = ctr->defKey;
            B_EventBuilder(evname, &event, false);
            sprintf(cmd, "%s bdc%d %s %s",
                    ctr->flags & CLF_REPEAT ? "safebindr" : "safebind",
                    controls[i].bindClass, evname + 1, buff);
            DD_Execute(cmd, true);
        }
        if(ctr->defMouse)
        {
            event.type = ev_mousebdown;
            event.data1 = 1 << (ctr->defMouse - 1);
            B_EventBuilder(evname, &event, false);
            sprintf(cmd, "%s bdc%d %s %s",
                    ctr->flags & CLF_REPEAT ? "safebindr" : "safebind",
                    controls[i].bindClass, evname + 1, buff);
            DD_Execute(cmd, true);
        }
        if(ctr->defJoy)
        {
            event.type = ev_joybdown;
            event.data1 = 1 << (ctr->defJoy - 1);
            B_EventBuilder(evname, &event, false);
            sprintf(cmd, "%s bdc%d %s %s",
                    ctr->flags & CLF_REPEAT ? "safebindr" : "safebind",
                    controls[i].bindClass, evname + 1, buff);
            DD_Execute(cmd, true);
        }
    }
}

/*
 *  findtoken
 */
int findtoken(char *string, char *token, char *delim)
{
    char   *ptr = strtok(string, delim);

    while(ptr)
    {
        if(!stricmp(ptr, token))
            return true;
        ptr = strtok(NULL, delim);
    }
    return false;
}

/*
 *  D_PrivilegedResponder
 */
int D_PrivilegedResponder(event_t *event)
{
    char    cmd[256], buff[256], evname[80];

    // We're interested in key or button down events.
    if(grabbing &&
       (event->type == ev_keydown || event->type == ev_mousebdown ||
        event->type == ev_joybdown || event->type == ev_povdown))
    {
        // We'll grab this event.
        boolean del = false;

        // Check for a cancel.
        if(event->type == ev_keydown)
        {
            if(event->data1 == DDKEY_ESCAPE)
            {
                grabbing = NULL;
                return true;
            }
        }

        // We shall issue a silent console command, but first we need
        // a textual representation of the event.
        B_EventBuilder(evname, event, false);    // "Deconstruct" into a name.

        // If this binding already exists, remove it.
        sprintf(cmd, "%s%s", grabbing->flags & CLF_ACTION ? "+" : "",
                grabbing->command);

        memset(buff, 0, sizeof(buff));

        // Check for bindings in this class only?
        if(B_BindingsForCommand(cmd, buff, grabbing->bindClass))
            if(findtoken(buff, evname, " "))    // Get rid of it?
            {
                del = true;
                strcpy(buff, "");
            }

        if(!del)
            sprintf(buff, "\"%s\"", grabbing->command);

        sprintf(cmd, "%s bdc%d %s %s",
                grabbing->flags & CLF_REPEAT ? "bindr" : "bind",
                grabbing->bindClass, evname + 1, buff);

        DD_Execute(cmd, false);

        // We've finished the grab.
        grabbing = NULL;
        S_LocalSound(menusnds[5], NULL);

        return true;
    }

    // Process the screen shot key right away.
    if(devparm && event->data1 == DDKEY_F1)
    {
        if(event->type == ev_keydown)
            G_ScreenShot();
        // All F1 events are eaten.
        return true;
    }
    return false;
}

/*
 * Registers the additional bind classes the game requires
 *
 * (Doomsday manages the bind class stack which forms the
 * dynamic event responder chain).
 */
void G_BindClassRegistration(void)
{
    int i;

    Con_Message("G_PreInit: Registering Bind Classes...\n");

    for(i = 0; BindClasses[i].name; i++)
        DD_AddBindClass(BindClasses + i);
}
