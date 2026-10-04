#include "bt3d_app_state.h"
#include "bt3d_weapon.h"
#include "player_internal.h"

int bt3d_player_cycle_weapon(AppState *app, int direction) {
    int slot;
    int i;

    if (direction == 0) return 0;
    slot = app->player.current_weapon_slot;
    for (i = 0; i < 4; ++i) {
        slot += direction > 0 ? 1 : -1;
        if (slot < 1) slot = 4;
        if (slot > 4) slot = 1;
        if (bt3d_player_weapon_selectable(app, slot)) {
            app->player.current_weapon_slot = slot;
            return 1;
        }
    }
    return 0;
}

int bt3d_player_weapon_selectable(const AppState *app, int slot) {
    const Bt3dPlayerWeaponDef *weapon;

    if (slot < 1 || slot > 4) return 0;
    weapon = bt3d_player_weapon_def(slot);
    if (!weapon->uses_ammo) return app->player.weapon_slots[slot] != 0;
    if (!app->player.weapon_slots[slot]) return 0;
    return app->player.weapon_ammo[weapon->ammo_slot] >= weapon->ammo_cost;
}

void bt3d_select_previous_available_weapon(AppState *app) {
    if (!bt3d_player_cycle_weapon(app, -1)) app->player.current_weapon_slot = 1;
}
