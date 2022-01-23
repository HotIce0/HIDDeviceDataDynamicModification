#include <stdlib.h>

#include "auto_gun_press.h"
#include "auto_gun_press_data.h"

#define ENABLE_LOG
#include "log.h"



typedef struct AGPCollect {
    int32_t *data; // FORMAT: x, y, ms_time_span, (repeat)...
    uint32_t len;
} AGPCollect;

typedef struct AGPContext {
    AGPCollect *current_collect;
    uint32_t data_index;
    uint32_t last_tick_ms;
} AGPContext;


#define AGP_DATA_ARRAY_LEN 2

AGPCollect s_agp_collet_arr[AGP_DATA_ARRAY_LEN] = {
    {.data = AGP_DATA_NONE, .len= (sizeof(AGP_DATA_NONE) / sizeof(uint32_t))},
    {.data = AGP_DATA_AK, .len = (sizeof(AGP_DATA_AK) / sizeof(uint32_t))}
};

int agp_restart(AGPContext *context)
{
    if (context == NULL) {
        ERROR("param context can't be NULL");
        return -1;
    }
    context->data_index = 0;
    context->last_tick_ms = HAL_GetTick();
    return 0;
}

int agp_get_data(AGPContext *context, AGPData *data)
{
    AGPCollect *collect;
    int32_t *raw_data;
    uint32_t time_span;

    if (context == NULL) {
        ERROR("param context can't be NULL");
        return -1;
    }
    if (data == NULL) {
        ERROR("param data can't be NULL");
        return -1;
    }
    collect = context->current_collect;

    if (context->data_index >= collect->len) {
        context->data_index = 0;
    }

    raw_data = collect->data + context->data_index;
    time_span = raw_data[2];

    if ((HAL_GetTick() - context->last_tick_ms) < time_span) {
        // need more wait
        return -1;
    }
    context->last_tick_ms = HAL_GetTick();
    context->data_index += 3;

    data->x = raw_data[0];
    data->y = raw_data[1];
    return 0;
}

int agp_set_collect(AGPContext *context, uint32_t index)
{
    if (context == NULL) {
        ERROR("param context can't be NULL");
        return -1;
    }
    if (index >= AGP_DATA_ARRAY_LEN) {
        ERROR("param index(%lu) is invalid", index);
        return -1;
    }
    context->current_collect = &s_agp_collet_arr[index];
    return 0;
}

void agp_close(AGPContext *context)
{
    if (context) {
        free(context);
        context = NULL;
    }
}

int agp_open(AGPContext **context)
{
    AGPContext *ctx = NULL;
    if (context == NULL) {
        ERROR("param context can't be NULL");
        return -1;
    }
    
    ctx = (AGPContext *)malloc(sizeof(AGPContext));
    if (ctx == NULL) {
        ERROR("allocate agp context failed");
        return -1;
    }

    (void)agp_set_collect(ctx, AGP_COLLECT_IDX_AK);
    (void)agp_restart(ctx);
    *context = ctx;
    return 0;
}