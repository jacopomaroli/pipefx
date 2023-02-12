#include "fxs.h"

#include <q/support/literals.hpp>
#include <q/detail/db_table.hpp>
#include <q/fx/envelope.hpp>
#include <q/fx/dynamic.hpp>
#include <q/fx/biquad.hpp>
#include <limits.h>
#include <math.h>

namespace q = cycfi::q;
using namespace q::literals;

float clip_mul(float a, float b)
{
    float x = a * b;
    if (a != 0 && x / a != b)
    {
        x = (a < 0 || b < 0) ? FLT_MIN : FLT_MAX;
    }
    return x;
}

float clip_float_to_int16(float a)
{
    if (a > INT16_MAX)
    {
        return INT16_MAX;
    }
    if (a < INT16_MIN)
    {
        return INT16_MIN;
    }
    return floorf(a);
}

double round(double d)
{
    return floor(d + 0.5);
}

float int16_to_bipNorm(int16_t input, float slope)
{
    float input_start = INT16_MIN;
    float output_start = -1.0f;
    return output_start + (slope * (input - input_start));
}

int16_t bipNorm_to_int16(float input, float slope)
{
    float input_start = -1.0f;
    float output_start = INT16_MIN;
    return output_start + round(slope * (input - input_start));
}

float get_slope(float input_start, float input_end, float output_start, float output_end)
{
    return 1.0 * (output_end - output_start) / (input_end - input_start);
}

#define BIPNORM_TO_INT16_SLOPE 32767.5
#define INT16_TO_BIPNORM_SLOPE 1.0 / BIPNORM_TO_INT16_SLOPE

extern "C" void
compressor(int16_t *in, int16_t *out, int size, unsigned n_channels, void *config_data, void *context)
{
    soft_knee_compressor_config_t *soft_knee_compressor_config = (soft_knee_compressor_config_t *)config_data;
    soft_knee_compressor_context_t *soft_knee_compressor_context = (soft_knee_compressor_context_t *)context;

    auto comp = q::soft_knee_compressor{
        q::decibel{soft_knee_compressor_config->threshold, q::decibel::direct},
        q::decibel{soft_knee_compressor_config->width, q::decibel::direct},
        soft_knee_compressor_config->ratio};
    auto makeup_gain = soft_knee_compressor_config->makeup_gain;

    q::peak_envelope_follower *env = (q::peak_envelope_follower *)soft_knee_compressor_context->env;
    if (!env)
    {
        env = new q::peak_envelope_follower(10_ms, 16000);
        soft_knee_compressor_context->env = (void *)env;
    }

    for (auto i = 0; i != size; ++i)
    {
        auto pos = i * n_channels;
        auto ch1 = pos;
        auto s = int16_to_bipNorm(in[i], INT16_TO_BIPNORM_SLOPE);
        auto env_out = q::decibel((*env)(std::abs(s)));
        auto gain = as_float(comp(env_out)) * makeup_gain;
        auto res = s * gain;
        out[ch1] = bipNorm_to_int16(res, BIPNORM_TO_INT16_SLOPE);
    }
}

extern "C" void
compressor_free(void *config_data, void *context)
{
    // free config
    soft_knee_compressor_config_t *soft_knee_compressor_config = (soft_knee_compressor_config_t *)config_data;
    free(soft_knee_compressor_config);

    // free context
    soft_knee_compressor_context_t *soft_knee_compressor_context = (soft_knee_compressor_context_t *)context;

    if (q::peak_envelope_follower *env = (q::peak_envelope_follower *)soft_knee_compressor_context->env)
    {
        delete env;
    }

    free(soft_knee_compressor_context);
}

extern "C" void
lowpass(int16_t *in, int16_t *out, int size, unsigned n_channels, void *config_data, void *context)
{
    lowpass_config_t *lowpass_config = (lowpass_config_t *)config_data;
    // lowpass_context_t *lowpass_context = (lowpass_context_t *)context;
    auto lp1 = q::lowpass{
        q::frequency(lowpass_config->f),
        lowpass_config->sps,
        lowpass_config->q};

    for (auto i = 0; i != size; ++i)
    {
        auto pos = i * n_channels;
        auto ch1 = pos;
        auto s = int16_to_bipNorm(in[i], INT16_TO_BIPNORM_SLOPE);
        auto res = lp1(s);
        out[ch1] = bipNorm_to_int16(res, BIPNORM_TO_INT16_SLOPE);
    }
}

extern "C" void
lowpass_free(void *config_data, void *context)
{
    // free config
    lowpass_config_t *lowpass_config = (lowpass_config_t *)config_data;
    free(lowpass_config);

    // free context
    lowpass_context_t *lowpass_context = (lowpass_context_t *)context;
    free(lowpass_context);
}
