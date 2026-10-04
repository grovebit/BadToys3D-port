#include "bt3d_app_state.h"
#include "bt3d_constants.h"
#include "bt3d_enemy.h"
#include "bt3d_math.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    const int *frames;
    int count;
} EnemyAnimationFrames;

typedef struct {
    const char *prefix;
    int max_sprite_suffix;
    float attack_range;
    int aggro_sound_id;
    int attack_sound_id;
    int death_sound_id;
    int drop_marker_type;
    int base_health;
    float attack_cooldown;
    int base_damage;
    int damage_falls_off;
    int projectile_class_id;
    int projectile_damage;
    float projectile_blast_radius;
    int idle_frame;
    /* Indexed by ENEMY_ANIM_*. */
    EnemyAnimationFrames animation[4];
} EnemyClassDef;

#define MELEE_ATTACK_RANGE (67.0f / 64.0f)
#define RANGED_ATTACK_RANGE 9999.0f

static const int enemy_walk_0[] = { 1, 2, 3, 4, 5 };
static const int enemy_shoot_0[] = { 13, 14, 15, 16, 17 };
static const int enemy_pain_0[] = { 18 };
static const int enemy_die_0[] = { 18, 19, 20, 20, 21, 22, 23, 22 };
static const int enemy_walk_1[] = { 5, 0, 1, 2, 3, 4 };
static const int enemy_shoot_1[] = { 4, 5, 6, 7 };
static const int enemy_pain_1[] = { 8 };
static const int enemy_die_1[] = { 8, 11, 10, 12, 13, 14, 15 };
static const int enemy_walk_2[] = { 0, 1, 2, 3, 4, 5, 6, 5, 4, 3, 2, 1 };
static const int enemy_shoot_2[] = { 7, 8, 9, 8, 7 };
static const int enemy_pain_2[] = { 10, 10 };
static const int enemy_die_2[] = { 10, 11, 12, 13, 14, 15, 16, 17, 18, 19 };
static const int enemy_walk_3[] = { 8, 0, 1, 2, 3, 4, 5 };
static const int enemy_shoot_3[] = { 6, 7, 8, 9 };
static const int enemy_pain_3[] = { 10 };
static const int enemy_die_3[] = { 10, 11, 12, 13, 14, 15, 16, 17 };
static const int enemy_walk_4[] = { 25, 0, 1, 2, 1, 3, 4, 5, 4 };
static const int enemy_shoot_4[] = { 6, 6, 6, 7, 7, 8, 4 };
static const int enemy_pain_4[] = { 9 };
static const int enemy_die_4[] = { 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14 };
static const int enemy_walk_5[] = { 1, 2, 3, 4, 3, 2, 1 };
static const int enemy_shoot_5[] = { 6, 7, 8, 6 };
static const int enemy_pain_5[] = { 5 };
static const int enemy_die_5[] = { 5, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18 };

#define ANIM_FRAMES(frames) { frames, ARRAY_COUNT(frames) }
#define ENEMY_ANIM(walk, shoot, pain, die) { ANIM_FRAMES(walk), ANIM_FRAMES(shoot), ANIM_FRAMES(pain), ANIM_FRAMES(die) }

static const EnemyClassDef enemy_catalog[] = {
    {
        .prefix = "PNS",
        .max_sprite_suffix = 23,
        .attack_range = MELEE_ATTACK_RANGE,
        .aggro_sound_id = 60,
        .attack_sound_id = 63,
        .death_sound_id = 64,
        .drop_marker_type = 0,
        .base_health = 4,
        .attack_cooldown = 0.858f,
        .base_damage = 3,
        .damage_falls_off = 0,
        .projectile_class_id = -1,
        .projectile_damage = 0,
        .projectile_blast_radius = 0.0f,
        .idle_frame = 1,
        .animation = ENEMY_ANIM(enemy_walk_0, enemy_shoot_0, enemy_pain_0, enemy_die_0),
    },
    {
        .prefix = "BLB",
        .max_sprite_suffix = 15,
        .attack_range = 4.5f,
        .aggro_sound_id = 70,
        .attack_sound_id = 1,
        .death_sound_id = 74,
        .drop_marker_type = 46,
        .base_health = 10,
        .attack_cooldown = 0.726f,
        .base_damage = 6,
        .damage_falls_off = 1,
        .projectile_class_id = -1,
        .projectile_damage = 0,
        .projectile_blast_radius = 0.0f,
        .idle_frame = 5,
        .animation = ENEMY_ANIM(enemy_walk_1, enemy_shoot_1, enemy_pain_1, enemy_die_1),
    },
    {
        .prefix = "PRK",
        .max_sprite_suffix = 19,
        .attack_range = RANGED_ATTACK_RANGE,
        .aggro_sound_id = 0,
        .attack_sound_id = 3,
        .death_sound_id = 81,
        .drop_marker_type = 52,
        .base_health = 45,
        .attack_cooldown = 1.122f,
        .base_damage = 10,
        .damage_falls_off = 1,
        .projectile_class_id = 7,
        .projectile_damage = 30,
        .projectile_blast_radius = 1.35f,
        .idle_frame = 0,
        .animation = ENEMY_ANIM(enemy_walk_2, enemy_shoot_2, enemy_pain_2, enemy_die_2),
    },
    {
        .prefix = "RCK",
        .max_sprite_suffix = 17,
        .attack_range = 6.5f,
        .aggro_sound_id = 85,
        .attack_sound_id = 2,
        .death_sound_id = 64,
        .drop_marker_type = 51,
        .base_health = 14,
        .attack_cooldown = 0.924f,
        .base_damage = 15,
        .damage_falls_off = 0,
        .projectile_class_id = -1,
        .projectile_damage = 0,
        .projectile_blast_radius = 0.0f,
        .idle_frame = 8,
        .animation = ENEMY_ANIM(enemy_walk_3, enemy_shoot_3, enemy_pain_3, enemy_die_3),
    },
    {
        .prefix = "RBT",
        .max_sprite_suffix = 14,
        .attack_range = RANGED_ATTACK_RANGE,
        .aggro_sound_id = 100,
        .attack_sound_id = 103,
        .death_sound_id = 101,
        .drop_marker_type = 50,
        .base_health = 750,
        .attack_cooldown = 1.122f,
        .base_damage = 25,
        .damage_falls_off = 0,
        .projectile_class_id = 8,
        .projectile_damage = 40,
        .projectile_blast_radius = 1.35f,
        .idle_frame = 25,
        .animation = ENEMY_ANIM(enemy_walk_4, enemy_shoot_4, enemy_pain_4, enemy_die_4),
    },
    {
        .prefix = "VOS",
        .max_sprite_suffix = 18,
        .attack_range = RANGED_ATTACK_RANGE,
        .aggro_sound_id = 90,
        .attack_sound_id = 93,
        .death_sound_id = 94,
        .drop_marker_type = 0,
        .base_health = 23,
        .attack_cooldown = 0.924f,
        .base_damage = 18,
        .damage_falls_off = 0,
        .projectile_class_id = 6,
        .projectile_damage = 23,
        .projectile_blast_radius = 0.0f,
        .idle_frame = 1,
        .animation = ENEMY_ANIM(enemy_walk_5, enemy_shoot_5, enemy_pain_5, enemy_die_5),
    },
    {
        .prefix = "KOU",
        .max_sprite_suffix = 4,
        .attack_range = MELEE_ATTACK_RANGE,
        .base_health = 1,
        .attack_cooldown = 0.35f,
        .projectile_class_id = -1,
    },
    {
        .prefix = "ELK",
        .max_sprite_suffix = 4,
        .attack_range = MELEE_ATTACK_RANGE,
        .base_health = 1,
        .attack_cooldown = 0.35f,
        .projectile_class_id = -1,
    },
    {
        .prefix = "ROB",
        .max_sprite_suffix = 7,
        .attack_range = MELEE_ATTACK_RANGE,
        .base_health = 1,
        .attack_cooldown = 0.35f,
        .projectile_class_id = -1,
    },
    {
        .prefix = "PSK",
        .max_sprite_suffix = 2,
        .attack_range = MELEE_ATTACK_RANGE,
        .base_health = 1,
        .attack_cooldown = 0.35f,
        .projectile_class_id = -1,
    },
};

_Static_assert(ARRAY_COUNT(enemy_catalog) == BT3D_SPRITE_CLASS_COUNT, "enemy catalog must cover every sprite class");

static const EnemyClassDef enemy_fallback = {
    .max_sprite_suffix = -1,
    .attack_range = MELEE_ATTACK_RANGE,
    .base_health = 1,
    .attack_cooldown = 0.35f,
    .projectile_class_id = -1,
};

#undef ENEMY_ANIM
#undef ANIM_FRAMES

static const EnemyClassDef *enemy_class_for(int class_id) {
    if (class_id >= 0 && class_id < ARRAY_COUNT(enemy_catalog)) {
        return &enemy_catalog[class_id];
    }
    return &enemy_fallback;
}

int bt3d_enemy_aggro_sound_id(int class_id) {
    return enemy_class_for(class_id)->aggro_sound_id;
}

int bt3d_enemy_attack_sound_id(int class_id) {
    return enemy_class_for(class_id)->attack_sound_id;
}

int bt3d_enemy_death_sound_id(int class_id) {
    return enemy_class_for(class_id)->death_sound_id;
}

int bt3d_drop_marker_type_for_enemy_class(int class_id) {
    return enemy_class_for(class_id)->drop_marker_type;
}

void bt3d_sprite_entry_name_for_class(int class_id, int frame, char *out, size_t out_size) {
    const EnemyClassDef *def = enemy_class_for(class_id);
    int frame_count = def->max_sprite_suffix + 1;

    if (!out || out_size == 0) return;
    out[0] = '\0';
    if (!def->prefix || frame_count <= 0) return;
    snprintf(out, out_size, "%s_%d", def->prefix, ((frame % frame_count) + frame_count) % frame_count);
}

int bt3d_is_sprite_entry_name(const char *name) {
    int i;
    for (i = 0; i < ARRAY_COUNT(enemy_catalog); ++i) {
        size_t length = strlen(enemy_catalog[i].prefix);
        if (strncmp(name, enemy_catalog[i].prefix, length) == 0 && name[length] == '_') return 1;
    }
    return 0;
}

static const float difficulty_hp_mult[3]            = { 0.6f, 1.0f, 1.4f };
static const float difficulty_attack_cooldown_mult[3] = { 1.5f, 1.0f, 0.7f };
static const float difficulty_initial_delay_mult[3] = { 2.5f, 1.5f, 1.2f };
static const float difficulty_damage_mult[3]        = { 0.6f, 1.0f, 1.4f };

static int clamp_difficulty(int d) {
    return bt3d_clamp_i(d, BT3D_DIFFICULTY_EASY, BT3D_DIFFICULTY_HARD);
}

const char *bt3d_difficulty_name(int difficulty) {
    static const char *const names[] = { "Easy", "Medium", "Hard" };
    return names[clamp_difficulty(difficulty)];
}

/* Rounds base * multiplier for the session's difficulty, never below 1. */
static int scale_for_difficulty(const AppState *app, int base, const float *multipliers) {
    return bt3d_max_i(1, (int)floorf((float)base * multipliers[app->session.difficulty] + 0.5f));
}

static int enemy_health_for_class(const AppState *app, int class_id) {
    return scale_for_difficulty(app, enemy_class_for(class_id)->base_health, difficulty_hp_mult);
}

float bt3d_enemy_attack_range_for_class(int class_id) {
    return enemy_class_for(class_id)->attack_range;
}

int bt3d_enemy_damage_for_class(const AppState *app, int class_id, float distance) {
    const EnemyClassDef *def = enemy_class_for(class_id);
    int base = def->base_damage;
    if (def->damage_falls_off) {
        base = (int)floorf(((float)def->base_damage * fmaxf(0.0f, 20.0f - distance)) / 20.0f);
    }
    if (base <= 0) return 0;
    return scale_for_difficulty(app, base, difficulty_damage_mult);
}

float bt3d_enemy_attack_cooldown_for_class(const AppState *app, int class_id) {
    return enemy_class_for(class_id)->attack_cooldown * difficulty_attack_cooldown_mult[app->session.difficulty];
}

float bt3d_enemy_initial_attack_delay_for_class(const AppState *app, int class_id) {
    float base_cooldown = bt3d_enemy_attack_cooldown_for_class(app, class_id);
    float random_scale = 0.35f + ((float)GetRandomValue(0, 1000) / 1000.0f) * 0.65f;
    return base_cooldown * random_scale * difficulty_initial_delay_mult[app->session.difficulty];
}

int bt3d_enemy_uses_projectile_for_class(int class_id) {
    return enemy_class_for(class_id)->projectile_class_id >= 0;
}

int bt3d_enemy_projectile_class_for_enemy_class(int class_id) {
    return enemy_class_for(class_id)->projectile_class_id;
}

int bt3d_enemy_projectile_damage_for_class(int class_id) {
    return enemy_class_for(class_id)->projectile_damage;
}

float bt3d_enemy_projectile_blast_radius_for_class(int class_id) {
    return enemy_class_for(class_id)->projectile_blast_radius;
}

int bt3d_enemy_projectile_frame(int projectile_class_id, float animation_timer) {
    static const int three_tick_frames[] = { 0, 0, 1 };
    static const int four_tick_frames[] = { 0, 0, 1, 2 };
    int tick = (int)floorf(animation_timer / GAME_TICK_SECS);
    switch (projectile_class_id) {
        case 6:
        case 7:
            return three_tick_frames[tick % ARRAY_COUNT(three_tick_frames)];
        case 8:
            return four_tick_frames[tick % ARRAY_COUNT(four_tick_frames)];
        default:
            return 0;
    }
}

int bt3d_enemy_idle_frame_for_class(int class_id) {
    return enemy_class_for(class_id)->idle_frame;
}

static const EnemyAnimationFrames *enemy_frames_for_state(int class_id, int anim_state) {
    if (anim_state < ENEMY_ANIM_WALK || anim_state > ENEMY_ANIM_DIE) anim_state = ENEMY_ANIM_WALK;
    return &enemy_class_for(class_id)->animation[anim_state];
}

void bt3d_set_enemy_animation_state(EnemyRuntime *enemy, int next_state) {
    const EnemyAnimationFrames *anim;
    if (!enemy) return;
    if (enemy->anim_state == next_state) return;
    enemy->anim_state = next_state;
    enemy->anim_cursor = 0;
    enemy->anim_timer = 0.0f;
    anim = enemy_frames_for_state(enemy->class_id, enemy->anim_state);
    enemy->frame = anim->count > 0 ? anim->frames[0] : bt3d_enemy_idle_frame_for_class(enemy->class_id);
}

void bt3d_advance_enemy_animation(EnemyRuntime *enemy, float dt) {
    const EnemyAnimationFrames *anim;
    if (!enemy) return;
    anim = enemy_frames_for_state(enemy->class_id, enemy->anim_state);
    if (anim->count <= 0) {
        enemy->frame = bt3d_enemy_idle_frame_for_class(enemy->class_id);
        return;
    }
    enemy->anim_timer += dt;
    while (enemy->anim_timer >= GAME_TICK_SECS) {
        enemy->anim_timer -= GAME_TICK_SECS;
        if (enemy->anim_state == ENEMY_ANIM_DIE || enemy->anim_state == ENEMY_ANIM_PAIN) {
            /* One-shot animations hold their last frame. */
            if (enemy->anim_cursor < anim->count - 1) {
                enemy->anim_cursor++;
            }
        } else {
            enemy->anim_cursor = (enemy->anim_cursor + 1) % anim->count;
        }
    }
    if (enemy->anim_cursor >= anim->count) enemy->anim_cursor = anim->count - 1;
    enemy->frame = anim->frames[enemy->anim_cursor];
}

void bt3d_init_enemy_runtime(const AppState *app, EnemyRuntime *enemy, const MapObject *object, int spawn_index) {
    if (!enemy || !object) return;
    memset(enemy, 0, sizeof(*enemy));
    enemy->position = (Vector3){
        (float)object->pos_x / 65536.0f,
        0.45f,
        (float)object->pos_y / 65536.0f
    };
    enemy->class_id = object->class_id;
    enemy->alive = 1;
    enemy->aggroed = 0;
    enemy->health = enemy_health_for_class(app, object->class_id);
    enemy->attack_cooldown = bt3d_enemy_attack_cooldown_for_class(app, object->class_id) * (0.35f + ((float)(spawn_index % 7) / 6.0f) * 0.65f);
    enemy->frame = bt3d_enemy_idle_frame_for_class(object->class_id);
    enemy->anim_state = ENEMY_ANIM_WALK;
    enemy->anim_cursor = 0;
    enemy->anim_timer = 0.0f;
    enemy->attack_timer = 0.0f;
    enemy->hurt_timer = 0.0f;
}
