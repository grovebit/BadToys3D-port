#ifndef BT3D_TYPES_H
#define BT3D_TYPES_H

#include "raylib.h"
#include "map242.h"

#include <time.h>

typedef struct {
    MapEvent base;
    float openness;
    float wait;
    int hold_open;
} DoorEvent;

typedef struct {
    MapMarker base;
    int collected;
} MarkerRuntime;

typedef struct {
    Vector3 position;
    int class_id;
    int alive;
    int aggroed;
    int health;
    float attack_cooldown;
    int frame;
    int anim_state;
    int anim_cursor;
    float anim_timer;
    float attack_timer;
    float hurt_timer;
    int had_los_last_tick;
} EnemyRuntime;

typedef struct {
    Vector3 position;
    Vector3 velocity;
    int max_damage;
    float blast_radius;
    float life;
} PlayerProjectile;

typedef struct {
    Vector3 position;
    Vector3 velocity;
    int projectile_class_id;
    int max_damage;
    float blast_radius;
    float life;
    float animation_timer;
} EnemyProjectile;

typedef struct {
    Vector3 position;
    float age;
    float life;
} PlayerShotImpact;

/* An image from the pack. Patches (sprites and overlays) also record the
   extent of their opaque texels; other images leave the bounds at zero. */
typedef struct {
    Texture2D texture;
    int min_opaque_x;
    int max_opaque_x;
    int min_opaque_y;
    int max_opaque_y;
    int visual_min_opaque_y;
} PackTexture;

typedef struct {
    char path[256];
    char label[64];
    time_t mod_time;
} SaveEntry;

#endif
