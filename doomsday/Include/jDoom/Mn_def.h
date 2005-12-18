// Menu defines and types.

#ifndef __MENU_DEFS_H_
#define __MENU_DEFS_H_

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

#include "Common/hu_stuff.h"
#include "r_defs.h"

// Macros

#define LEFT_DIR    0
#define RIGHT_DIR   1
#define DIR_MASK    0x1
#define ITEM_HEIGHT 20
#define SELECTOR_XOFFSET (-28)
#define SELECTOR_YOFFSET (-1)
#define SLOTTEXTLEN     16
#define ASCII_CURSOR '['

#define LINEHEIGHT      16
#define LINEHEIGHT_B    15
#define LINEHEIGHT_A    8

#define SKULLXOFF       -32
#define SKULLYOFF       6
#define SKULLW          20
#define SKULLH          19
#define CURSORPREF  "M_SKULL%d"
#define SKULLBASELMP    "M_SKL00"

#define NUMSAVESLOTS    8

#define MAX_EDIT_LEN        256

// Types

typedef struct {
    char    text[MAX_EDIT_LEN];
    char    oldtext[MAX_EDIT_LEN];  // If the current edit is canceled...
    int     firstVisible;
} EditField_t;

typedef enum {
    ITT_EMPTY,
    ITT_EFUNC,
    ITT_LRFUNC,
    ITT_SETMENU,
    ITT_INERT,
    ITT_NAVLEFT,
    ITT_NAVRIGHT
} ItemType_t;

typedef enum {
    MENU_MAIN,
    MENU_EPISODE,
    MENU_SKILL,
    MENU_OPTIONS,
    MENU_OPTIONS2,
    MENU_GAMEPLAY,
    MENU_HUD,
    MENU_MAP,
    MENU_CONTROLS,
    MENU_MOUSE,
    MENU_JOYSTICK,
    MENU_LOAD,
    MENU_SAVE,
    MENU_MULTIPLAYER,
    MENU_GAMESETUP,
    MENU_PLAYERSETUP,
    MENU_WEAPONSETUP,
    MENU_NONE
} MenuType_t;

// menu item flags
#define MIF_NOTALTTXT   0x01  // don't use alt text instead of lump (M_NMARE)

typedef struct {
    ItemType_t      type;
    int             flags;
    char            *text;
    void            (*func) (int option, void *data);
    int             option;
    char           *lumpname;
    void           *data;
} MenuItem_t;

typedef struct {
    int             x;
    int             y;
    void            (*drawFunc) (void);
    int             itemCount;
    const MenuItem_t *items;
    int             lastOn;
    MenuType_t      prevMenu;
    int     noHotKeys;  // 1= hotkeys are disabled on this menu
    dpatch_t       *font;          // Font for menu items.
    float          *color;      // their color.
    int             itemHeight;
    // For multipage menus.
    int             firstItem, numVisItems;
} Menu_t;

extern int      MenuTime;
extern boolean  shiftdown;
extern Menu_t  *currentMenu;
extern short    itemOn;

extern Menu_t   MapDef;

// Multiplayer menus.
extern Menu_t   MultiplayerMenu;
extern Menu_t   GameSetupMenu;
extern Menu_t   PlayerSetupMenu;

void    SetMenu(MenuType_t menu);
void    M_DrawSaveLoadBorder(int x, int y);
void    M_DrawTitle(char *text, int y);
void    M_WriteText(int x, int y, char *string);
void    M_WriteText2(int x, int y, char *string, dpatch_t *font,
                             float red, float green, float blue, float alpha);
void    M_WriteMenuText(const Menu_t * menu, int index, char *text);

// Color widget.
void    DrawColorWidget();
void    SCColorWidget(int index, void *data);
void    M_WGCurrentColor(int option, void *data);

void    M_DrawSaveLoadBorder(int x, int y);
void    M_SetupNextMenu(Menu_t * menudef);
void    M_DrawThermo(int x, int y, int thermWidth, int thermDot);
void    M_DrawSlider(const Menu_t * menu, int index, int width, int dot);
void    M_DrawColorBox(const Menu_t * menu, int index, float r, float g, float b, float a);
int     M_StringWidth(char *string, dpatch_t * font);
int     M_StringHeight(char *string, dpatch_t * font);
void    M_StartControlPanel(void);
void    M_StartMessage(char *string, void *routine, boolean input);
void    M_StopMessage(void);
void    M_ClearMenus(void);
void    M_FloatMod10(float *variable, int option);


void    SCEnterMultiplayerMenu(int option, void *data);

void            MN_TickerEx(void); // The extended ticker.

#endif                          // __MENU_DEFS_H_
