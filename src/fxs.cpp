#include "fxs.h"

#include <q/support/literals.hpp>
#include <q/detail/db_table.hpp>
#include <q/fx/envelope.hpp>
#include <q/fx/dynamic.hpp>
#include <q/fx/biquad.hpp>
#include <q/fx/noise_gate.hpp>
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

int16_t clip_add(int16_t a, int16_t b)
{
    if (b > 0 && a > INT16_MAX - b)
    {
        return INT16_MAX;
    }
    if (b < 0 && a < INT16_MIN - b)
    {
        return INT16_MIN;
    }
    return a + b;
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
    auto makeup_gain = as_float(q::decibel{soft_knee_compressor_config->makeup_gain, q::decibel::direct});
    auto env_release = q::duration{ double(soft_knee_compressor_config->env_release_ms * 1e-3) };

    q::peak_envelope_follower *envs = static_cast<q::peak_envelope_follower *>(soft_knee_compressor_context->env);
    if (!envs)
    {
        // placement-new
        void *raw_memory = operator new[](n_channels * sizeof(q::peak_envelope_follower));
        envs = static_cast<q::peak_envelope_follower *>(raw_memory);
        for (int i = 0; i < n_channels; i++)
        {
            new (&envs[i]) q::peak_envelope_follower(env_release, 16000);
        }
        soft_knee_compressor_context->env = (void *)envs;
        soft_knee_compressor_context->n_channels = n_channels;
    }

    for (int channel = 0; channel < n_channels; channel++)
    {
        for (auto i = 0; i != size; ++i)
        {
            auto pos = n_channels * i + channel;
            auto s = int16_to_bipNorm(in[pos], INT16_TO_BIPNORM_SLOPE);
            auto env = q::decibel(envs[channel](std::abs(s)));
            auto gain = as_float(comp(env)) * makeup_gain;
            auto res = s * gain;
            out[pos] = bipNorm_to_int16(res, BIPNORM_TO_INT16_SLOPE);
        }
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

    unsigned n_channels = soft_knee_compressor_context->n_channels;

    if (q::peak_envelope_follower *envs = static_cast<q::peak_envelope_follower *>(soft_knee_compressor_context->env))
    {
        for (int i = n_channels - 1; i >= 0; --i)
        {
            envs[i].~peak_envelope_follower();
        }
        operator delete[](envs);
    }

    free(soft_knee_compressor_context);
}

extern "C" void
noise_gate(int16_t * in, int16_t * out, int size, unsigned n_channels, void* config_data, void* context)
{
    noise_gate_config_t* noise_gate_config = (noise_gate_config_t*)config_data;
    noise_gate_context_t* noise_gate_context = (noise_gate_context_t*)context;

    auto gate = q::basic_noise_gate<10>{
        q::decibel{noise_gate_config->onset_threshold, q::decibel::direct},
        q::decibel{noise_gate_config->release_threshold, q::decibel::direct} };
    auto env_release = q::duration{ double(noise_gate_config->env_release_ms * 1e-3) };
    auto gate_env_release = q::duration{ double(noise_gate_config->gate_env_release_ms * 1e-3) };

    q::peak_envelope_follower* envs = static_cast<q::peak_envelope_follower*>(noise_gate_context->env);
    q::peak_envelope_follower* gate_envs = static_cast<q::peak_envelope_follower*>(noise_gate_context->gate_env);
    if (!envs)
    {
        // placement-new
        void* raw_memory_env = operator new[](n_channels * sizeof(q::peak_envelope_follower));
        void* raw_memory_gate_env = operator new[](n_channels * sizeof(q::peak_envelope_follower));
        envs = static_cast<q::peak_envelope_follower*>(raw_memory_env);
        gate_envs = static_cast<q::peak_envelope_follower*>(raw_memory_gate_env);
        for (int i = 0; i < n_channels; i++)
        {
            new (&envs[i]) q::peak_envelope_follower(env_release, 16000);
            new (&gate_envs[i]) q::peak_envelope_follower(gate_env_release, 16000);
        }
        noise_gate_context->env = (void*)envs;
        noise_gate_context->gate_env = (void*)gate_envs;
        noise_gate_context->n_channels = n_channels;
    }

    for (int channel = 0; channel < n_channels; channel++)
    {
        for (auto i = 0; i != size; ++i)
        {
            auto pos = n_channels * i + channel;
            auto s = int16_to_bipNorm(in[pos], INT16_TO_BIPNORM_SLOPE);
            auto env = envs[channel](std::abs(s));
            auto gate_val = gate(env);
            auto gate_env = gate_envs[channel](gate_val);
            auto res = s * gate_env;
            out[pos] = bipNorm_to_int16(res, BIPNORM_TO_INT16_SLOPE);
        }
    }
}

extern "C" void
noise_gate_free(void* config_data, void* context)
{
    // free config
    noise_gate_config_t* noise_gate_config = (noise_gate_config_t*)config_data;
    free(noise_gate_config);

    // free context
    noise_gate_context_t* noise_gate_context = (noise_gate_context_t*)context;

    unsigned n_channels = noise_gate_context->n_channels;

    q::peak_envelope_follower* envs = static_cast<q::peak_envelope_follower*>(noise_gate_context->env);
    q::peak_envelope_follower* gate_envs = static_cast<q::peak_envelope_follower*>(noise_gate_context->gate_env);
    if (envs && gate_envs)
    {
        for (int i = n_channels - 1; i >= 0; --i)
        {
            envs[i].~peak_envelope_follower();
            gate_envs[i].~peak_envelope_follower();
        }
        operator delete[](envs);
    }

    free(noise_gate_context);
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

    for (int channel = 0; channel < n_channels; channel++)
    {
        for (auto i = 0; i != size; ++i)
        {
            auto pos = n_channels * i + channel;
            auto s = int16_to_bipNorm(in[pos], INT16_TO_BIPNORM_SLOPE);
            auto res = lp1(s);
            out[pos] = bipNorm_to_int16(res, BIPNORM_TO_INT16_SLOPE);
        }
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

extern "C" void
to_mono(int16_t *in, int16_t *out, int size, unsigned n_channels, void *config_data, void *context)
{
    // to_mono_config_t *to_mono_config = (to_mono_config_t *)config_data;
    // to_mono_context_t *to_mono_context = (to_mono_context_t *)context;

    unsigned dst_channel = 0;

    for (auto i = 0; i != size; ++i)
    {
        out[i] = 0;
        for (int channel = 0; channel < n_channels; channel++)
        {
            auto pos = n_channels * i + channel;
            out[i] = clip_add(out[i], in[pos]);
        }
    }
}

extern "C" void
to_mono_free(void *config_data, void *context)
{
    // free config
    to_mono_config_t *to_mono_config = (to_mono_config_t *)config_data;
    free(to_mono_config);

    // free context
    to_mono_context_t *to_mono_context = (to_mono_context_t *)context;
    free(to_mono_context);
}
