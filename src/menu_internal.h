#ifndef BT3D_MENU_INTERNAL_H
#define BT3D_MENU_INTERNAL_H

#include "bt3d_app_state.h"
#include "bt3d_menu.h"

#define MENU_MAX_ITEMS (MAX_SAVE_ENTRIES + 1)
#define MENU_VISIBLE_SAVES 5

typedef enum {
    MENU_ITEM_CONTINUE,
    MENU_ITEM_OPEN_SAVE,
    MENU_ITEM_OPEN_OPTIONS,
    MENU_ITEM_EXIT_SESSION,
    MENU_ITEM_OPEN_CAMPAIGN,
    MENU_ITEM_QUIT,
    MENU_ITEM_NEW_GAME,
    MENU_ITEM_OPEN_LOAD,
    MENU_ITEM_START,
    MENU_ITEM_LOAD,
    MENU_ITEM_SAVE,
    MENU_ITEM_BACK,
    MENU_ITEM_SENSITIVITY,
    MENU_ITEM_MUSIC,
    MENU_ITEM_SOUND,
    MENU_ITEM_DISPLAY,
    MENU_ITEM_DEBUG,
    MENU_ITEM_GOD,
    MENU_ITEM_UNLOCK,
    MENU_ITEM_KEYS
} MenuAction;

typedef struct {
    MenuAction action;
    int arg;                /* the difficulty or save index */
    char label[64];
    Rectangle rect;         /* empty while scrolled out of view */
} MenuItem;

/* The current screen's items and where they are drawn and clicked. */
typedef struct {
    const char *title;
    const char *hint;       /* NULL for none */
    Rectangle panel;
    Vector2 title_position;
    Rectangle name_box;     /* the save dialog's name field */
    int count;
    MenuItem items[MENU_MAX_ITEMS];
} MenuPage;

void menu_build_page(const AppState *app, MenuPage *page);
/* The volume slider drawn at the right of an item. */
Rectangle menu_slider_rect(Rectangle item);

#endif
