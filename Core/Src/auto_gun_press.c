#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "auto_gun_press.h"
#include "auto_gun_press_data.h"

#define ENABLE_LOG
#include "log.h"

typedef struct AGPCollect {
    float *data; // FORMAT: x, y, ms_time_span, (repeat)...
    int len;
    int multiple;
    int sleepsuber;
} AGPCollect;

typedef struct AGPContext {
    AGPCollect *collect; // changed by agp_set_collect()

    // fix param
    float coefficient;
    float sensitive;

    int32_t *arr_x;
    int32_t *arr_y;
    int32_t *arr_ts; 
    int arr_length;

    int arr_index;
    uint32_t last_tick_ms;
} AGPContext;

#define AGP_DATA_ARRAY_LEN 3

AGPCollect s_agp_collet_arr[AGP_DATA_ARRAY_LEN] = {
    {.data = AGP_RAW_DATA_NONE, .len = (sizeof(AGP_RAW_DATA_NONE) / sizeof(float)), .multiple = 1, .sleepsuber = 0},
    {.data = AGP_RAW_DATA_AK47, .len = (sizeof(AGP_RAW_DATA_AK47) / sizeof(float)), .multiple = 12, .sleepsuber = 0},
    {.data = AGP_RAW_DATA_M4A4, .len = (sizeof(AGP_RAW_DATA_M4A4) / sizeof(float)), .multiple = 12, .sleepsuber = 0},
};

void PrintFloat(float value)
{
    int tmp,tmp1,tmp2,tmp3,tmp4,tmp5,tmp6;
    tmp = (int)value;
    tmp1=(int)((value-tmp)*10)%10;
    tmp2=(int)((value-tmp)*100)%10;
    tmp3=(int)((value-tmp)*1000)%10;
    tmp4=(int)((value-tmp)*10000)%10;
    tmp5=(int)((value-tmp)*100000)%10;
    tmp6=(int)((value-tmp)*1000000)%10;
    printf2("f-value=%d.%d%d%d%d%d%d\r\n",tmp,tmp1,tmp2,tmp3,tmp4,tmp5,tmp6);
}

int agp_coefficient_change(AGPContext *context, uint8_t is_add)
{
    if (context == NULL) {
        ERROR("param context can't be NULL");
        return -1;
    }
    context->coefficient += (is_add ? 0.1: -0.1);
    INFO("coefficient=");
    PrintFloat(context->coefficient);
    return 0;
}

int agp_sensitive_change(AGPContext *context, uint8_t is_add)
{
    if (context == NULL) {
        ERROR("param context can't be NULL");
        return -1;
    }
    context->sensitive += (is_add ? 0.1: -0.1);
    INFO("sensitive=");
    PrintFloat(context->sensitive);
    return 0;
}

int agp_restart(AGPContext *context)
{
    if (context == NULL) {
        ERROR("param context can't be NULL");
        return -1;
    }
    context->last_tick_ms = HAL_GetTick();
    context->arr_index = 0;
    return 0;
}

int agp_get_data(AGPContext *context, AGPData *data)
{
    if (context == NULL) {
        ERROR("param context can't be NULL");
        return -1;
    }
    if (data == NULL) {
        ERROR("param data can't be NULL");
        return -1;
    }

    if (HAL_GetTick() > context->last_tick_ms + context->arr_ts[context->arr_index]) {
        context->last_tick_ms += context->arr_ts[context->arr_index];

        data->x = context->arr_x[context->arr_index];
        data->y = -1 * context->arr_y[context->arr_index];
        
        context->arr_index++;
        return 0;
    }
    return -1;
}

static void free_fixed_data(AGPContext *ctx)
{
    assert(ctx);
    if (ctx->arr_x) {
        free(ctx->arr_x);
        ctx->arr_x = NULL;
    }
    if (ctx->arr_y) {
        free(ctx->arr_y);
        ctx->arr_y = NULL;
    }
    if (ctx->arr_ts) {
        free(ctx->arr_ts);
        ctx->arr_ts = NULL;
    }
    ctx->arr_length = 0;
    ctx->arr_index = 0;
}

static int generate_fixed_data(AGPContext *ctx)
{
    AGPCollect *coll = ctx->collect;
    int raw_idx = 0;
    int idx = 0;
    int i, j;
    float x, y, ts;
    int32_t sx, sy, sts;
    int32_t fixx, fixy, fixts;
    float sumx = 0, sumy = 0, sumts = 0;
    float sumxo = 0, sumyo = 0, sumtso = 0;

    ctx->arr_length = coll->multiple * coll->len / 3;
    ctx->arr_x = (int32_t *)malloc(sizeof(int32_t) * ctx->arr_length);
    ctx->arr_y = (int32_t *)malloc(sizeof(int32_t) * ctx->arr_length);
    ctx->arr_ts = (int32_t *)malloc(sizeof(int32_t) * ctx->arr_length);

    if (ctx->arr_x == NULL || ctx->arr_y == NULL || ctx->arr_ts == NULL) {
        ERROR("allocate fixed data array failed, length=%d", ctx->arr_length);
        free_fixed_data(ctx);
        return -1;
    }
    // INFO("length=%d", coll->len / 3);

    for (i = 0; i < coll->len / 3; i++) {
        x = coll->data[raw_idx + 0] * ctx->coefficient / ctx->sensitive;
        y = coll->data[raw_idx + 1] * ctx->coefficient / ctx->sensitive;
        ts = coll->data[raw_idx + 2];
        raw_idx += 3;
        
        sx = (int32_t)floorf(x / coll->multiple);
        sy = (int32_t)floorf(y / coll->multiple);
        sts = (int32_t)floorf(ts / coll->multiple);

        sumx += sx * coll->multiple;
        sumy += sy * coll->multiple;
        sumts += sts * coll->multiple;

        sumxo += x;
        sumyo += y;
        sumtso += ts;

        fixx = (int32_t)roundf(sumxo - sumx);
        fixy = (int32_t)roundf(sumyo - sumy);
        fixts = (int32_t)roundf(sumtso - sumts);

        // printf2("x=%d, y=%d, ts=%d\n\r", (int)x, (int)y, (int)ts);
        // printf2("sx=%d, sx=%d, sts=%d\n\r", (int)sx, (int)sy, (int)sts);
        // printf2("sum, x=%d, y=%d, ts=%d\n\r", (int)sumx, (int)sumy, (int)sumts);
        // printf2("sumo, x=%d, y=%d, ts=%d\n\r", (int)sumxo, (int)sumyo, (int)sumtso);
        // printf2("fix, x=%d, y=%d, ts=%d\n\r", (int)fixx, (int)fixy, (int)fixts);
        
        for (j = 0; j < coll->multiple; j++) {
            ctx->arr_x[idx] = sx;
            ctx->arr_y[idx] = sy;
            ctx->arr_ts[idx] = sts;

            if (fixx > 0) {
                ctx->arr_x[idx] += 1;
                sumx += 1;
                fixx--;
            }
            if (fixy > 0) {
                ctx->arr_y[idx] += 1;
                sumy += 1;
                fixy--;
            }
            if (fixts > 0) {
                ctx->arr_ts[idx] += 1;
                sumts += 1;
                fixts--;
            }

            idx++;
        }
    }
    
    // // DEBUG
    // for (i = 0; i < ctx->arr_length; i++) {
    //     printf2("%d %d %d\n\r", ctx->arr_x[i], ctx->arr_y[i], ctx->arr_ts[i]);
    // }

    return 0;
}

int agp_set_collect(AGPContext *context, uint32_t index)
{
    int ret;
    if (context == NULL) {
        ERROR("param context can't be NULL");
        return -1;
    }
    if (index >= AGP_DATA_ARRAY_LEN) {
        ERROR("param index(%lu) is invalid", index);
        return -1;
    }
    context->collect = &s_agp_collet_arr[index];
    free_fixed_data(context);
    ret = generate_fixed_data(context);
    if (ret < 0) {
        ERROR("generate fixed data failed");
        return -1;
    }
    return 0;
}

void agp_close(AGPContext *context)
{
    if (context) {
        free_fixed_data(context);

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
    memset(ctx, 0, sizeof(AGPContext));

    ctx->coefficient = DEFAULT_COEFFICIENT;
    ctx->sensitive = DEFAULT_SENSITIVE;

    (void)agp_set_collect(ctx, AGP_COLLECT_IDX_AK47);
    (void)agp_restart(ctx);
    *context = ctx;
    return 0;
}