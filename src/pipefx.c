#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>

#include "conf.h"
#include "fxs.h"

#define CONFIG_SIZE (256)

const char *usage =
    "Usage:\n %s [options]\n"
    "Options:\n"
    " -c config.cfg     config file path\n"
    " -D                daemonize\n"
    " -v                get the program version\n"
    " -h                display this help text\n"
    "Note:\n"
    " Access audio I/O through named pipes (/tmp/ec.input for playback and /tmp/ec.output for recording)\n"
    "  `cat audio.raw > /tmp/ec.input` to play audio\n"
    "  `cat /tmp/ec.output > out.raw` to get recording audio\n"
    " Only support mono playback\n";

volatile int g_is_quit = 0;
volatile int g_is_reloading_config = 0;
fx_chain_item_t *first_fx_chain_item = NULL;
fx_chain_item_t *last_fx_chain_item = NULL;

extern int fifo_read_setup(conf_t *conf);
extern int fifo_read(void *buf, size_t frames, int timeout_ms);
extern int fifo_write_setup(conf_t *conf);
extern int fifo_write(void *buf, size_t frames);

void int_handler(int signal)
{
    printf("Caught signal INT, quit...\n");

    g_is_quit = 1;
}

void usr1_handler(int signal)
{
    printf("Caught signal USR1, reloading config...\n");

    g_is_reloading_config = 1;
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

void apply_fx_chain(int16_t *in, int16_t **out, int frame_size, unsigned n_channels, int16_t *fx_out1, int16_t *fx_out2)
{
    fx_chain_item_t *fx_chain_item = first_fx_chain_item;
    int16_t *fx_in_ptr = in;
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

void free_fx_chain()
{
    fx_chain_item_t *fx_chain_item = first_fx_chain_item;
    while (fx_chain_item)
    {
        fxs_free[fx_chain_item->type](fx_chain_item->data, fx_chain_item->context);
        fx_chain_item_t *fx_chain_item_new = fx_chain_item->next;
        free(fx_chain_item);
        fx_chain_item = fx_chain_item_new;
    }

    first_fx_chain_item = NULL;
    last_fx_chain_item = NULL;
}

void update_fx_chain(fx_chain_item_t *fx_chain_item)
{
    fx_chain_item->next = NULL;
    if (!first_fx_chain_item)
    {
        first_fx_chain_item = fx_chain_item;
    }
    if (last_fx_chain_item)
    {
        last_fx_chain_item->next = fx_chain_item;
    }
    last_fx_chain_item = fx_chain_item;
}

int parse_config(char *buf, conf_t *config)
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
            soft_knee_compressor_config_t *soft_knee_compressor_config = (soft_knee_compressor_config_t *)malloc(sizeof(soft_knee_compressor_config_t));
            if (sscanf(dummy_str, "%lf,%lf,%f,%f,%lf",
                       &soft_knee_compressor_config->threshold,
                       &soft_knee_compressor_config->width,
                       &soft_knee_compressor_config->ratio,
                       &soft_knee_compressor_config->makeup_gain,
                       &soft_knee_compressor_config->env_release_ms) == 5)
            {
                fx_chain_item_t *fx_chain_item = (fx_chain_item_t *)malloc(sizeof(fx_chain_item_t));
                soft_knee_compressor_context_t *soft_knee_compressor_context = (soft_knee_compressor_context_t *)malloc(sizeof(soft_knee_compressor_context_t));
                soft_knee_compressor_context->env = NULL;
                fx_chain_item->type = t_soft_knee_compressor;
                fx_chain_item->data = soft_knee_compressor_config;
                fx_chain_item->context = soft_knee_compressor_context;
                update_fx_chain(fx_chain_item);
            }
            else
            {
                free(soft_knee_compressor_config);
            }
        }
        if (sscanf(dummy_str, " noise_gate:%s", dummy_str) == 1)
        {
            noise_gate_config_t *noise_gate_config = (noise_gate_config_t *)malloc(sizeof(noise_gate_config_t));
            if (sscanf(dummy_str, "%lf,%lf,%u,%lf,%lf",
                       &noise_gate_config->onset_threshold,
                       &noise_gate_config->release_threshold,
                       &noise_gate_config->attack_window,
                       &noise_gate_config->env_release_ms,
                       &noise_gate_config->gate_env_release_ms) == 5)
            {
                fx_chain_item_t *fx_chain_item = (fx_chain_item_t *)malloc(sizeof(fx_chain_item_t));
                noise_gate_context_t *noise_gate_context = (noise_gate_context_t *)malloc(sizeof(noise_gate_context_t));
                noise_gate_context->env = NULL;
                noise_gate_context->gate_env = NULL;
                fx_chain_item->type = t_noise_gate;
                fx_chain_item->data = noise_gate_config;
                fx_chain_item->context = noise_gate_context;
                update_fx_chain(fx_chain_item);
            }
            else
            {
                free(noise_gate_config);
            }
        }
        if (sscanf(dummy_str, " lowpass:%s", dummy_str) == 1)
        {
            lowpass_config_t *lowpass_config = (lowpass_config_t *)malloc(sizeof(lowpass_config_t));
            if (sscanf(dummy_str, "%lf,%u,%lf",
                       &lowpass_config->f,
                       &lowpass_config->sps,
                       &lowpass_config->q) == 3)
            {
                fx_chain_item_t *fx_chain_item = (fx_chain_item_t *)malloc(sizeof(fx_chain_item_t));
                lowpass_context_t *lowpass_context = (lowpass_context_t *)malloc(sizeof(lowpass_context_t));
                fx_chain_item->type = t_lowpass;
                fx_chain_item->data = lowpass_config;
                fx_chain_item->context = lowpass_context;
                update_fx_chain(fx_chain_item);
            }
            else
            {
                free(lowpass_config);
            }
        }
        if (sscanf(dummy_str, " to_mono:%s", dummy_str) == 1)
        {
            to_mono_config_t *to_mono_config = (to_mono_config_t *)malloc(sizeof(to_mono_config_t));
            if (sscanf(dummy_str, "%u",
                       &to_mono_config->dst_channel) == 1)
            {
                fx_chain_item_t *fx_chain_item = (fx_chain_item_t *)malloc(sizeof(fx_chain_item_t));
                to_mono_context_t *to_mono_context = (to_mono_context_t *)malloc(sizeof(to_mono_context_t));
                fx_chain_item->type = t_to_mono;
                fx_chain_item->data = to_mono_config;
                fx_chain_item->context = to_mono_context;
                update_fx_chain(fx_chain_item);
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

void print_config(conf_t *config)
{
    printf("in_fifo=%s, out_fifo=%s, rate=%u\n", config->in_fifo, config->out_fifo, config->rate);
}

void get_config(conf_t *config, char *config_file_path)
{
    free_fx_chain();
    FILE *f = fopen(config_file_path, "r");
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

int main(int argc, char *argv[])
{
    int16_t *in = NULL;
    int16_t *out = NULL;
    int16_t *fx_out1 = NULL;
    int16_t *fx_out2 = NULL;
    // int16_t *in_single = NULL;
    // int16_t *out_single = NULL;
    FILE *fp_in = NULL;
    FILE *fp_out = NULL;

    int opt = 0;
    int daemon = 0;
    char *config_file_path = 0;

    conf_t config = {
        .in_fifo = "/tmp/pipefx.input",
        .out_fifo = "/tmp/pipefx.output",
        .rate = 16000,
        .in_channels = 1,
        .out_channels = 1,
        .bits_per_sample = 16,
        .buffer_size = 1024 * 16,
        .bypass = 0,
        .save_audio = 0};

    while ((opt = getopt(argc, argv, "D:h:c:v")) != -1)
    {
        switch (opt)
        {
        case 'D':
            daemon = 1;
            break;
        case 'h':
            printf(usage, argv[0]);
            exit(0);
        case 'c':
            config_file_path = optarg;
            break;
        case 'v':
            printf("\nv0.0.1");
            exit(0);
            break;
        case '?':
            printf("\n");
            printf(usage, argv[0]);
            exit(1);
        default:
            break;
        }
    }

    if (daemon)
    {
        daemonize();
    }

    get_config(&config, config_file_path);

    int frame_size = config.rate * 10 / 1000; // 10 ms

    if (config.save_audio)
    {
        fp_in = fopen("/tmp/pipefx_in.raw", "wb");
        fp_out = fopen("/tmp/pipefx_out.raw", "wb");

        if (fp_in == NULL || fp_out == NULL)
        {
            printf("Fail to open file(s)\n");
            exit(1);
        }
    }

    in = (int16_t *)calloc(frame_size * config.in_channels, sizeof(int16_t));
    fx_out1 = (int16_t *)calloc(frame_size * config.in_channels, sizeof(int16_t));
    fx_out2 = (int16_t *)calloc(frame_size * config.in_channels, sizeof(int16_t));
    // out = (int16_t *)calloc(frame_size * config.out_channels, sizeof(int16_t));
    // in_single = (int16_t *)calloc(frame_size, sizeof(int16_t));
    // out_single = (int16_t *)calloc(frame_size, sizeof(int16_t));

    if (in == NULL || fx_out1 == NULL || fx_out2 == NULL)
    {
        printf("Fail to allocate memory\n");
        exit(1);
    }

    // Configures signal handling.
    struct sigaction sig_int_handler;
    sig_int_handler.sa_handler = int_handler;
    sigemptyset(&sig_int_handler.sa_mask);
    sig_int_handler.sa_flags = 0;
    sigaction(SIGINT, &sig_int_handler, NULL);

    struct sigaction sig_usr1_handler;
    sig_usr1_handler.sa_handler = usr1_handler;
    sigemptyset(&sig_usr1_handler.sa_mask);
    sig_usr1_handler.sa_flags = 0;
    sigaction(SIGUSR1, &sig_usr1_handler, NULL);

    fifo_read_setup(&config);
    fifo_write_setup(&config);

    printf("Running... Press Ctrl+C to exit\n");

    int timeout = 200 * 1000 * frame_size / config.rate; // ms

    while (!g_is_quit)
    {
        if (g_is_reloading_config)
        {
            get_config(&config, config_file_path);
            g_is_reloading_config = 0;
        }

        fifo_read(in, frame_size, timeout);

        if (!config.bypass)
        {
            // for (int out_channel = 0; out_channel < config.out_channels; out_channel++)
            // {
            //     for (int i = 0; i < frame_size; i++)
            //     {
            //         in_single[i] = in[config.out_channels * i + out_channel];
            //     }
            //     apply_fx_chain(in_single, &out_single, frame_size, config.out_channels, fx_out1, fx_out2);
            //     for (int i = 0; i < frame_size; i++)
            //     {
            //         out[config.out_channels * i + out_channel] = out_single[i];
            //     }
            // }
            apply_fx_chain(in, &out, frame_size, config.in_channels, fx_out1, fx_out2);

            // for (int in_channel = 0; in_channel < config.out_channels; in_channel++)
            // {
            //     for (int i = 0; i < frame_size; i++)
            //     {
            //         out[i] = out_fx[config.in_channels * i + in_channel];
            //     }
            // }
        }
        else
        {
            out = fx_out1;
            memcpy(out, in, frame_size * config.in_channels * config.bits_per_sample / 8);
        }

        if (fp_in)
        {
            fwrite(in, 2, frame_size * config.in_channels, fp_in);
            fwrite(out, 2, frame_size * config.out_channels, fp_out);
        }

        fifo_write(out, frame_size);
    }

    if (fp_in)
    {
        fflush(fp_in);
        fflush(fp_out);
        fclose(fp_in);
        fclose(fp_out);
    }

    free(in);
    // free(out);
    free(fx_out1);
    free(fx_out2);
    // free(in_single);
    // free(out_single);

    free_fx_chain();

    printf("main terminated\n");

    exit(0);

    return 0;
}
