#ifndef BT3D_PICKUP_H
#define BT3D_PICKUP_H

typedef struct AppState AppState;

int bt3d_is_pickup_marker(int marker_type);
/* Gives the player the pickup's effect; returns 0 when the marker is not a pickup. */
int bt3d_apply_pickup(AppState *app, int marker_type);

#endif
