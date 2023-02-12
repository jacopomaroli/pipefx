
#ifndef _CONF_H_
#define _CONF_H_

typedef struct _conf_t
{
    char *rec_pcm;       // recording PCM
    char *out_pcm;       // output PCM
    char *playback_fifo; // playback FIFO
    char *in_fifo;       // input FIFO
    char *out_fifo;      // output FIFO
    unsigned rate;
    unsigned rec_channels; // recording channels
    unsigned ref_channels; // reference (playback) channels
    unsigned in_channels;  // audio input channels
    unsigned out_channels; // processed audio output channels
    unsigned bits_per_sample;
    unsigned buffer_size;
    unsigned playback_fifo_size;
    unsigned filter_length;
    unsigned bypass;
    unsigned save_audio;
} conf_t;

#endif // _CONF_H_
