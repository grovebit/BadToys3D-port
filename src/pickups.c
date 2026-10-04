#include "bt3d_app_state.h"
#include "bt3d_audio.h"
#include "bt3d_math.h"
#include "bt3d_pickup.h"

enum {
    PICKUP_KIND_HEALTH = 1,
    PICKUP_KIND_AMMO,
    PICKUP_KIND_SPECIAL_COUNT,
    PICKUP_KIND_KEY,
    PICKUP_KIND_WEAPON,

    PLAYER_MAX_HEALTH = 99,
    PLAYER_MAX_AMMO = 99,
    PLAYER_MAX_SPECIAL_COUNT = 9
};

static const float PICKUP_SOUND_VOLUME = 0.7f;

typedef struct {
    int marker_type;
    int kind;
    int amount;
    int slot;
    int sound_id;
} PickupDef;

static const PickupDef pickup_catalog[] = {
    { 40, PICKUP_KIND_HEALTH, 5,  0, 10 },
    { 41, PICKUP_KIND_HEALTH, 7,  0, 15 },
    { 42, PICKUP_KIND_HEALTH, 5,  0, 11 },
    { 43, PICKUP_KIND_HEALTH, 20, 0, 14 },
    { 44, PICKUP_KIND_AMMO, 10, 4, 13 },
    { 45, PICKUP_KIND_AMMO, 5,  3, 13 },
    { 46, PICKUP_KIND_AMMO, 5,  2, 13 },
    { 47, PICKUP_KIND_SPECIAL_COUNT, 1,  0, 37 },
    { 48, PICKUP_KIND_KEY, 0,  1, 12 },
    { 49, PICKUP_KIND_KEY, 0,  2, 12 },
    { 50, PICKUP_KIND_KEY, 0,  3, 12 },
    { 51, PICKUP_KIND_WEAPON, 5,  3, 13 },
    { 52, PICKUP_KIND_WEAPON, 10, 4, 13 },
};

static const PickupDef *pickup_def(int marker_type) {
    int i;
    for (i = 0; i < ARRAY_COUNT(pickup_catalog); ++i) {
        if (pickup_catalog[i].marker_type == marker_type) return &pickup_catalog[i];
    }
    return NULL;
}

int bt3d_is_pickup_marker(int marker_type) {
    return pickup_def(marker_type) != NULL;
}

int bt3d_apply_pickup(AppState *app, int marker_type) {
    const PickupDef *def = pickup_def(marker_type);
    PlayerState *player = &app->player;

    if (!def) return 0;
    switch (def->kind) {
        case PICKUP_KIND_HEALTH:
            player->health = bt3d_min_i(PLAYER_MAX_HEALTH, player->health + def->amount);
            break;
        case PICKUP_KIND_AMMO:
            player->weapon_ammo[def->slot] = bt3d_min_i(PLAYER_MAX_AMMO, player->weapon_ammo[def->slot] + def->amount);
            break;
        case PICKUP_KIND_SPECIAL_COUNT:
            player->special_count = bt3d_min_i(PLAYER_MAX_SPECIAL_COUNT, player->special_count + def->amount);
            break;
        case PICKUP_KIND_KEY:
            player->keys[def->slot - 1] = 1;
            break;
        case PICKUP_KIND_WEAPON:
            player->weapon_slots[def->slot] = 1;
            player->weapon_ammo[def->slot] = bt3d_min_i(PLAYER_MAX_AMMO, player->weapon_ammo[def->slot] + def->amount);
            player->current_weapon_slot = def->slot;
            break;
    }
    play_sound_id(app, def->sound_id, PICKUP_SOUND_VOLUME);
    return 1;
}
