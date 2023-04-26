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

#include "util.h"
#include "conf.h"
#include "fxs.h"
#include "fx_chain_utils.h"

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
        .save_audio = 0,
        .chain = 0};

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

    fx_chain chain = {
        .first_fx_chain_item = NULL,
        .last_fx_chain_item = NULL};
    config.chain = &chain;
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

            fx_chain_apply(&chain, in, &out, frame_size, config.in_channels, fx_out1, fx_out2);

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

    fx_chain_free(&chain);

    printf("main terminated\n");

    exit(0);

    return 0;
}
