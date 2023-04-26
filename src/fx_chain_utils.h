#ifndef __FX_CHAIN_UTILS_H__
#define __FX_CHAIN_UTILS_H__

typedef struct fx_chain_item_t fx_chain_item_t;

struct fx_chain_item_t
{
    unsigned type;
    void* data;
    void* context;
    fx_chain_item_t* next;
};

typedef struct _fx_chain
{
    fx_chain_item_t* first_fx_chain_item;
    fx_chain_item_t* last_fx_chain_item;
} fx_chain;

#ifdef __cplusplus
extern "C"
#endif
    void fx_chain_push(fx_chain* chain, fx_chain_item_t* fx_chain_item);

#ifdef __cplusplus
extern "C"
#endif
    void fx_chain_apply(fx_chain* chain, int16_t* in, int16_t** out, int frame_size, unsigned n_channels, int16_t* fx_out1, int16_t* fx_out2);

#ifdef __cplusplus
extern "C"
#endif
    void fx_chain_free(fx_chain* chain);

#endif /* __FX_CHAIN_UTILS_H__ */