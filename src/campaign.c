#include "bt3d_campaign.h"

#include "bt3d_app_state.h"
#include "bt3d_math.h"

#include <stdlib.h>
#include <string.h>

static const char *campaign_map_sequence[] = {
    "MAP_19", "MAP_18", "MAP_17", "MAP_1",  "MAP_2",
    "MAP_3",  "MAP_4",  "MAP_5",  "MAP_6",  "MAP_7",
    "MAP_8",  "MAP_9",  "MAP_10", "MAP_11", "MAP_12",
    "MAP_13", "MAP_14", "MAP_15", "MAP_35", "MAP_30",
    "MAP_31", "MAP_32", "MAP_33", "MAP_34", "MAP_16"
};

int bt3d_build_campaign_map_entry_list(AppState *app) {
    size_t i = 0;
    int count = 0;

    app->session.map_entries = (const DatPackEntry **)calloc(app->content.pack.entry_count, sizeof(DatPackEntry *));
    if (!app->session.map_entries) return 0;

    for (int k = 0; k < ARRAY_COUNT(campaign_map_sequence); ++k) {
        const DatPackEntry *entry = datpack_find(&app->content.pack, campaign_map_sequence[k]);
        if (entry) app->session.map_entries[count++] = entry;
    }
    if (count == 0) {
        for (i = 0; i < app->content.pack.entry_count; ++i) {
            if (strncmp(app->content.pack.entries[i].name, "MAP_", 4) == 0) {
                app->session.map_entries[count++] = &app->content.pack.entries[i];
            }
        }
    }

    app->session.map_entry_count = count;
    return app->session.map_entry_count > 0;
}
