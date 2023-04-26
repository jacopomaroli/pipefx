#ifndef __FXS_H__
#define __FXS_H__

#include <stdint.h>

typedef enum _fx_type
{
    t_soft_knee_compressor,
    t_noise_gate,
    t_lowpass,
    t_to_mono
} fx_type;

typedef struct _soft_knee_compressor_config_t
{
    double threshold;
    double width;
    float ratio;
    float makeup_gain;
    double env_release_ms;
} soft_knee_compressor_config_t;

typedef struct _soft_knee_compressor_context_t
{
    void *env;
    unsigned n_channels;
} soft_knee_compressor_context_t;

typedef struct _noise_gate_config_t
{
    double onset_threshold;
    double release_threshold;
    unsigned attack_window;
    double env_release_ms;
    double gate_env_release_ms;
} noise_gate_config_t;

typedef struct _noise_gate_context_t
{
    void* env;
    void* gate_env;
    unsigned n_channels;
} noise_gate_context_t;

typedef struct _lowpass_config_t
{
    double f;
    unsigned sps;
    double q;
} lowpass_config_t;

typedef struct _lowpass_context_t
{
    void* dummy; // avoid "C requires that a struct or union has at least one member"
} lowpass_context_t;

typedef struct _to_mono_config_t
{
    unsigned dst_channel;
} to_mono_config_t;

typedef struct _to_mono_context_t
{
    void* dummy; // avoid "C requires that a struct or union has at least one member"
} to_mono_context_t;

#ifdef __cplusplus
extern "C"
#endif
    void
    compressor(int16_t *in, int16_t *out, int size, unsigned n_channels, void *config_data, void *context);

#ifdef __cplusplus
extern "C"
#endif
    void
    compressor_free(void *config_data, void *context);

#ifdef __cplusplus
extern "C"
#endif
    void
    noise_gate(int16_t * in, int16_t * out, int size, unsigned n_channels, void* config_data, void* context);

#ifdef __cplusplus
extern "C"
#endif
    void
    noise_gate_free(void* config_data, void* context);

#ifdef __cplusplus
extern "C"
#endif
    void
    lowpass(int16_t *in, int16_t *out, int size, unsigned n_channels, void *config_data, void *context);

#ifdef __cplusplus
extern "C"
#endif
    void
    lowpass_free(void *config_data, void *context);

#ifdef __cplusplus
extern "C"
#endif
    void
    to_mono(int16_t *in, int16_t *out, int size, unsigned n_channels, void *config_data, void *context);

#ifdef __cplusplus
extern "C"
#endif
    void
    to_mono_free(void *config_data, void *context);

typedef void (*fx_fn)(int16_t* in, int16_t* out, int size, unsigned n_channels, void* config_data, void* context);

// WARNING: items needs to be in the same order of fx_type
static fx_fn fxs[] = {
    compressor,
    noise_gate,
    lowpass,
    to_mono
};

typedef void (*fx_free_fn)(void* config_data, void* context);

// WARNING: items needs to be in the same order of fx_type
static fx_free_fn fxs_free[] = {
    compressor_free,
    noise_gate_free,
    lowpass_free,
    to_mono_free
};

#endif /* __FXS_H__ */
