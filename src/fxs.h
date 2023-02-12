#include <stdint.h>

typedef enum _fx_type
{
    t_soft_knee_compressor,
    t_lowpass
} fx_type;

typedef struct fx_chain_item_t fx_chain_item_t;

struct fx_chain_item_t
{
    unsigned type;
    void *data;
    void *context;
    fx_chain_item_t *next;
};

typedef struct _soft_knee_compressor_config_t
{
    double threshold;
    double width;
    float ratio;
    float makeup_gain;
} soft_knee_compressor_config_t;

typedef struct _soft_knee_compressor_context_t
{
    void *env;
} soft_knee_compressor_context_t;

typedef struct _lowpass_config_t
{
    double f;
    unsigned sps;
    double q;
} lowpass_config_t;

typedef struct _lowpass_context_t
{
} lowpass_context_t;

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
    lowpass(int16_t *in, int16_t *out, int size, unsigned n_channels, void *config_data, void *context);

#ifdef __cplusplus
extern "C"
#endif
    void
    lowpass_free(void *config_data, void *context);