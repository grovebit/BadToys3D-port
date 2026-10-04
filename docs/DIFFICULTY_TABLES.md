# Difficulty Tables

Balance is defined in `src/enemies.c`, `src/combat.c` and `src/weapons.c`.

## Difficulty multipliers

| Stat | Easy | Medium | Hard |
|------|:----:|:------:|:----:|
| Enemy HP | ×0.6 | ×1.0 | ×1.4 |
| Attack cooldown | ×1.5 (slower) | ×1.0 | ×0.7 (faster) |
| Initial reaction delay | ×2.5 | ×1.5 | ×1.2 |
| Enemy damage to player | ×0.6 | ×1.0 | ×1.4 |

Player-weapon damage is **not** scaled by difficulty.

## Per-class stats

Base values (before multipliers):

| Class | HP | Cooldown (s) | Direct damage (point-blank) |
|:-----:|:--:|:------------:|:--------------------:|
| 0     |  4 | 0.858        | 3 |
| 1 (BLB) | 10 | 0.726      | 6  (6·max(0, 20-d)/20) |
| 2     | 45 | 1.122        | 10 (10·max(0, 20-d)/20) |
| 3     | 14 | 0.924        | 15 |
| 4 (boss RBT) | 750 | 1.122  | 25 |
| 5     | 23 | 0.924        | 18 |

Class 2 and class 4 also fire explosive projectiles with base damage 30 and
40 respectively (blast radius 1.35); the table lists their direct attack damage.
Damage with distance falloff is rounded down before difficulty scaling. Health
and difficulty-scaled damage round to the nearest integer, with a minimum of 1
for a positive base value.

### Class 1 example with all multipliers applied

| | Easy | Medium | Hard |
|---|:---:|:---:|:---:|
| HP | 6 | 10 | 14 |
| Attack cooldown | 1.09s | 0.73s | 0.51s |
| Initial reaction | 0.95–2.72s | 0.38–1.09s | 0.21–0.61s |
| Direct damage (point-blank) | 4 | 6 | 8 |

Initial reaction = `cooldown_effective × random(0.35..1.0) × initial_delay_mult`.

## Shots-to-kill per weapon

Player-weapon base damage:
- Fist (slot 1): 3
- Pistol (slot 2): 6
- DD7 (slot 3): 10
- Plasma ELM500 (slot 4): 30 at the blast epicentre (radius falloff applies)

Shots-to-kill is `ceil(effective_HP / weapon_damage)` against a healthy enemy,
counting point-blank plasma hits.

### Easy (HP × 0.6)

| Enemy | HP | Fist | Pistol | DD7 | Plasma |
|-------|:--:|:----:|:------:|:---:|:------:|
| c0 | 2 | 1 | 1 | 1 | 1 |
| c1 (BLB) | 6 | 2 | 1 | 1 | 1 |
| c2 | 27 | 9 | 5 | 3 | 1 |
| c3 | 8 | 3 | 2 | 1 | 1 |
| c4 (boss RBT) | 450 | 150 | 75 | 45 | 15 |
| c5 | 14 | 5 | 3 | 2 | 1 |

### Medium (HP × 1.0)

| Enemy | HP | Fist | Pistol | DD7 | Plasma |
|-------|:--:|:----:|:------:|:---:|:------:|
| c0 | 4 | 2 | 1 | 1 | 1 |
| c1 | 10 | 4 | 2 | 1 | 1 |
| c2 | 45 | 15 | 8 | 5 | 2 |
| c3 | 14 | 5 | 3 | 2 | 1 |
| c4 | 750 | 250 | 125 | 75 | 25 |
| c5 | 23 | 8 | 4 | 3 | 1 |

### Hard (HP × 1.4)

| Enemy | HP | Fist | Pistol | DD7 | Plasma |
|-------|:--:|:----:|:------:|:---:|:------:|
| c0 | 6 | 2 | 1 | 1 | 1 |
| c1 | 14 | 5 | 3 | 2 | 1 |
| c2 | 63 | 21 | 11 | 7 | 3 |
| c3 | 20 | 7 | 4 | 2 | 1 |
| c4 | 1050 | 350 | 175 | 105 | 35 |
| c5 | 32 | 11 | 6 | 4 | 2 |
