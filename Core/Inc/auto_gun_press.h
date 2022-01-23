#ifndef AUTO_GUN_PRESS_H
#define AUTO_GUN_PRESS_H
#include <stdint.h>


typedef enum AGPCollectIndex {
    AGP_COLLECT_IDX_NONE = 0,
    AGP_COLLECT_IDX_AK = 1,
} AGPCollectIndex;

typedef struct AGPData {
    int32_t x;
    int32_t y;
} AGPData;

typedef struct AGPContext AGPContext;

int agp_restart(AGPContext *context);
int agp_get_data(AGPContext *context, AGPData *data);
int agp_set_collect(AGPContext *context, uint32_t index);
void agp_close(AGPContext *context);
int agp_open(AGPContext **context);

#endif /* AUTO_GUN_PRESS_H */