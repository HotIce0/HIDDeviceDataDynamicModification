#ifndef AUTO_GUN_PRESS_H
#define AUTO_GUN_PRESS_H
#include <stdint.h>

// can dynamic change
#define DEFAULT_COEFFICIENT 2.5
#define DEFAULT_SENSITIVE   1.4

// can't dynamic change
#define AGP_SPLIT_NUM   12

typedef enum AGPCollectIndex {
    AGP_COLLECT_IDX_NONE = 0,
    AGP_COLLECT_IDX_AK47 = 1,
    AGP_COLLECT_IDX_M4A4 = 2,
} AGPCollectIndex;

typedef struct AGPData {
    int32_t x;
    int32_t y;
} AGPData;

typedef struct AGPContext AGPContext;

int agp_coefficient_change(AGPContext *context, uint8_t is_add);
int agp_sensitive_change(AGPContext *context, uint8_t is_add);

int agp_restart(AGPContext *context);
int agp_get_data(AGPContext *context, AGPData *data);
int agp_set_collect(AGPContext *context, uint32_t index);
void agp_close(AGPContext *context);
int agp_open(AGPContext **context);

#endif /* AUTO_GUN_PRESS_H */