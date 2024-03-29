
#ifndef _CONF_H_
#define _CONF_H_

#include "fx_chain_utils.h"

typedef struct _conf_t
{
    char *in_fifo;  // input FIFO
    char *out_fifo; // output FIFO
    unsigned rate;
    unsigned in_channels;  // audio input channels
    unsigned out_channels; // processed audio output channels
    unsigned bits_per_sample;
    unsigned buffer_size;
    unsigned bypass;
    unsigned save_audio;
    fx_chain *chain;
} conf_t;

#endif // _CONF_H_
