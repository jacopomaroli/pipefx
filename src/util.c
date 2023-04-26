#include <stdio.h>
#include <unistd.h>

#include "conf.h"
#include "fxs.h"
#include "fx_chain_utils.h"

#define CONFIG_SIZE (256)

// from http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
unsigned power2(unsigned v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;

    return v;
}

void daemonize(void)
{
    pid_t pid, sid;

    /* Fork off the parent process */
    pid = fork();
    if (pid < 0)
    {
        printf("fork() failed\n");
        exit(1);
    }
    /* If we got a good PID, then
        we can exit the parent process. */
    if (pid > 0)
    {
        exit(0);
    }

    /* Change the file mode mask */
    umask(0);

    /* Open any logs here */

    /* Create a new SID for the child process */
    sid = setsid();
    if (sid < 0)
    {
        printf("setsid() failed\n");
        exit(1);
    }

    /* Change the current working directory */
    if ((chdir("/")) < 0)
    {
        printf("chdir() failed\n");
        exit(1);
    }
}

int parse_config(char* buf, conf_t* config)
{
    char dummy[CONFIG_SIZE];
    char dummy_str[CONFIG_SIZE];
    if (sscanf(buf, " %s", dummy) == EOF)
        return 0; // blank line
    if (sscanf(buf, " %[#]", dummy) == 1)
        return 0; // comment#
    if (sscanf(buf, " in_fifo = %s", dummy_str) == 1)
    {
        config->in_fifo = malloc((strlen(dummy_str) + 1) * sizeof(char));
        strcpy(config->in_fifo, dummy_str);
        return 0;
    }
    if (sscanf(buf, " out_fifo = %s", dummy_str) == 1)
    {
        config->out_fifo = malloc((strlen(dummy_str) + 1) * sizeof(char));
        strcpy(config->out_fifo, dummy_str);
        return 0;
    }
    if (sscanf(buf, " in_channels = %d", &config->in_channels) == 1)
    {
        return 0;
    }
    if (sscanf(buf, " out_channels = %d", &config->out_channels) == 1)
    {
        return 0;
    }
    if (sscanf(buf, " rate = %u", &config->rate) == 1)
    {
        return 0;
    }
    if (sscanf(buf, " save_audio = %u", &config->save_audio) == 1)
    {
        return 0;
    }
    if (sscanf(buf, " bypass = %u", &config->bypass) == 1)
    {
        return 0;
    }
    if (sscanf(buf, " fx = %s", dummy_str) == 1)
    {
        if (sscanf(dummy_str, " soft_knee_compressor:%s", dummy_str) == 1)
        {
            soft_knee_compressor_config_t* soft_knee_compressor_config = (soft_knee_compressor_config_t*)malloc(sizeof(soft_knee_compressor_config_t));
            if (sscanf(dummy_str, "%lf,%lf,%f,%f,%lf",
                &soft_knee_compressor_config->threshold,
                &soft_knee_compressor_config->width,
                &soft_knee_compressor_config->ratio,
                &soft_knee_compressor_config->makeup_gain,
                &soft_knee_compressor_config->env_release_ms) == 5)
            {
                fx_chain_item_t* fx_chain_item = (fx_chain_item_t*)malloc(sizeof(fx_chain_item_t));
                soft_knee_compressor_context_t* soft_knee_compressor_context = (soft_knee_compressor_context_t*)malloc(sizeof(soft_knee_compressor_context_t));
                soft_knee_compressor_context->env = NULL;
                fx_chain_item->type = t_soft_knee_compressor;
                fx_chain_item->data = soft_knee_compressor_config;
                fx_chain_item->context = soft_knee_compressor_context;
                fx_chain_push(config->chain, fx_chain_item);
            }
            else
            {
                free(soft_knee_compressor_config);
            }
        }
        if (sscanf(dummy_str, " noise_gate:%s", dummy_str) == 1)
        {
            noise_gate_config_t* noise_gate_config = (noise_gate_config_t*)malloc(sizeof(noise_gate_config_t));
            if (sscanf(dummy_str, "%lf,%lf,%u,%lf,%lf",
                &noise_gate_config->onset_threshold,
                &noise_gate_config->release_threshold,
                &noise_gate_config->attack_window,
                &noise_gate_config->env_release_ms,
                &noise_gate_config->gate_env_release_ms) == 5)
            {
                fx_chain_item_t* fx_chain_item = (fx_chain_item_t*)malloc(sizeof(fx_chain_item_t));
                noise_gate_context_t* noise_gate_context = (noise_gate_context_t*)malloc(sizeof(noise_gate_context_t));
                noise_gate_context->env = NULL;
                noise_gate_context->gate_env = NULL;
                fx_chain_item->type = t_noise_gate;
                fx_chain_item->data = noise_gate_config;
                fx_chain_item->context = noise_gate_context;
                fx_chain_push(config->chain, fx_chain_item);
            }
            else
            {
                free(noise_gate_config);
            }
        }
        if (sscanf(dummy_str, " lowpass:%s", dummy_str) == 1)
        {
            lowpass_config_t* lowpass_config = (lowpass_config_t*)malloc(sizeof(lowpass_config_t));
            if (sscanf(dummy_str, "%lf,%u,%lf",
                &lowpass_config->f,
                &lowpass_config->sps,
                &lowpass_config->q) == 3)
            {
                fx_chain_item_t* fx_chain_item = (fx_chain_item_t*)malloc(sizeof(fx_chain_item_t));
                lowpass_context_t* lowpass_context = (lowpass_context_t*)malloc(sizeof(lowpass_context_t));
                fx_chain_item->type = t_lowpass;
                fx_chain_item->data = lowpass_config;
                fx_chain_item->context = lowpass_context;
                fx_chain_push(config->chain, fx_chain_item);
            }
            else
            {
                free(lowpass_config);
            }
        }
        if (sscanf(dummy_str, " to_mono:%s", dummy_str) == 1)
        {
            to_mono_config_t* to_mono_config = (to_mono_config_t*)malloc(sizeof(to_mono_config_t));
            if (sscanf(dummy_str, "%u",
                &to_mono_config->dst_channel) == 1)
            {
                fx_chain_item_t* fx_chain_item = (fx_chain_item_t*)malloc(sizeof(fx_chain_item_t));
                to_mono_context_t* to_mono_context = (to_mono_context_t*)malloc(sizeof(to_mono_context_t));
                fx_chain_item->type = t_to_mono;
                fx_chain_item->data = to_mono_config;
                fx_chain_item->context = to_mono_context;
                fx_chain_push(config->chain, fx_chain_item);
            }
            else
            {
                free(to_mono_config);
            }
        }
        return 0;
    }
    return 3; // syntax error
}

void print_config(conf_t* config)
{
    printf("in_fifo=%s, out_fifo=%s, rate=%u\n", config->in_fifo, config->out_fifo, config->rate);
}

void get_config(conf_t* config, char* config_file_path)
{
    fx_chain_free(config->chain);
    FILE* f = fopen(config_file_path, "r");
    char buf[CONFIG_SIZE];
    int line_number = 0;
    while (fgets(buf, sizeof buf, f))
    {
        ++line_number;
        int err = parse_config(buf, config);
        if (err)
            fprintf(stderr, "error line %d: %d\n", line_number, err);
    }
    print_config(config);
}