#include <stdint.h>

#include "fxs.h"
#include "fx_chain_utils.h"

void fx_chain_push(fx_chain* chain, fx_chain_item_t* fx_chain_item)
{
    fx_chain_item->next = NULL;
    if (!chain->first_fx_chain_item)
    {
        chain->first_fx_chain_item = fx_chain_item;
    }
    if (chain->last_fx_chain_item)
    {
        chain->last_fx_chain_item->next = fx_chain_item;
    }
    chain->last_fx_chain_item = fx_chain_item;
}

void fx_chain_apply(fx_chain* chain, int16_t* in, int16_t** out, int frame_size, unsigned n_channels, int16_t* fx_out1, int16_t* fx_out2)
{
    fx_chain_item_t* fx_chain_item = chain->first_fx_chain_item;
    int16_t* fx_in_ptr = in;
    while (fx_chain_item)
    {
        fxs[fx_chain_item->type](fx_in_ptr, fx_out1, frame_size, n_channels, fx_chain_item->data, fx_chain_item->context);
        fx_chain_item = fx_chain_item->next;
        fx_in_ptr = fx_out1;
        fx_out1 = fx_out2;
        fx_out2 = fx_in_ptr;
    }
    *out = fx_in_ptr;
}

void fx_chain_free(fx_chain* chain)
{
    fx_chain_item_t* fx_chain_item = chain->first_fx_chain_item;
    while (fx_chain_item)
    {
        fxs_free[fx_chain_item->type](fx_chain_item->data, fx_chain_item->context);
        fx_chain_item_t* fx_chain_item_new = fx_chain_item->next;
        free(fx_chain_item);
        fx_chain_item = fx_chain_item_new;
    }

    chain->first_fx_chain_item = NULL;
    chain->last_fx_chain_item = NULL;
}
