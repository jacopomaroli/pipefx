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
#include "audio.h"
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

int parse_config(char *buf, conf_t *config)
{
    char dummy[CONFIG_SIZE];
    char dummy_str[CONFIG_SIZE];
    if (sscanf(buf, " %s", dummy) == EOF)
        return 0; // blank line
    if (sscanf(buf, " %[#]", dummy) == 1)
        return 0; // comment
    if (sscanf(buf, " playback_fifo = %s", dummy_str) == 1)
    {
        config->playback_fifo = malloc((strlen(dummy_str) + 1) * sizeof(char));
        strcpy(config->playback_fifo, dummy_str);
        return 0;
    }
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
            if (sscanf(dummy_str, "%lf,%lf,%f,%f",
                       &soft_knee_compressor_config->threshold,
                       &soft_knee_compressor_config->width,
                       &soft_knee_compressor_config->ratio,
                       &soft_knee_compressor_config->makeup_gain) == 4)
            {
                fx_chain_item_t *fx_chain_item = (fx_chain_item_t *)malloc(sizeof(fx_chain_item_t));
                soft_knee_compressor_context_t *soft_knee_compressor_context = (soft_knee_compressor_context_t *)malloc(sizeof(soft_knee_compressor_context_t));
                soft_knee_compressor_context->env = NULL;
                fx_chain_item->type = t_soft_knee_compressor;
                fx_chain_item->data = soft_knee_compressor_config;
                fx_chain_item->context = soft_knee_compressor_context;
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
            else
            {
                free(soft_knee_compressor_config);
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
            else
            {
                free(lowpass_config);
            }
        }
        return 0;
    }
    return 3; // syntax error
}

void print_config(conf_t *config)
{
    printf("playback_fifo=%s, rate=%u\n", config->playback_fifo, config->rate);
}

void free_fx_chain()
{
    fx_chain_item_t *fx_chain_item = first_fx_chain_item;
    while (fx_chain_item)
    {
        if (fx_chain_item->type == t_soft_knee_compressor)
        {
            compressor_free(fx_chain_item->data, fx_chain_item->context);
        }
        if (fx_chain_item->type == t_lowpass)
        {
            lowpass_free(fx_chain_item->data, fx_chain_item->context);
        }
        fx_chain_item_t *fx_chain_item_new = fx_chain_item->next;
        free(fx_chain_item);
        fx_chain_item = fx_chain_item_new;
    }

    first_fx_chain_item = NULL;
    last_fx_chain_item = NULL;
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
    FILE *fp_in = NULL;
    FILE *fp_out = NULL;

    int opt = 0;
    int delay = 0;
    int daemon = 0;
    char *config_file_path = 0;

    conf_t config = {
        .rec_pcm = "default",
        .out_pcm = "default",
        .playback_fifo = "/tmp/pipefx.playback",
        .in_fifo = "/tmp/pipefx.input",
        .out_fifo = "/tmp/pipefx.output",
        .rate = 16000,
        .rec_channels = 1,
        .ref_channels = 1,
        .in_channels = 1,
        .out_channels = 1,
        .bits_per_sample = 16,
        .buffer_size = 1024 * 16,
        .playback_fifo_size = 1024 * 4,
        .filter_length = 4096,
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
        fp_in = fopen("/tmp/in.raw", "wb");
        fp_out = fopen("/tmp/out.raw", "wb");

        if (fp_in == NULL || fp_out == NULL)
        {
            printf("Fail to open file(s)\n");
            exit(1);
        }
    }

    in = (int16_t *)calloc(frame_size * config.ref_channels, sizeof(int16_t));
    fx_out1 = (int16_t *)calloc(frame_size * config.out_channels, sizeof(int16_t));
    fx_out2 = (int16_t *)calloc(frame_size * config.out_channels, sizeof(int16_t));

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

    // system delay between recording and playback
    printf("skip frames %d\n", capture_skip(delay));

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
            fx_chain_item_t *fx_chain_item = first_fx_chain_item;
            int16_t *fx_in_ptr = in;
            while (fx_chain_item)
            {
                if (fx_chain_item->type == t_soft_knee_compressor)
                {
                    compressor(fx_in_ptr, fx_out1, frame_size, config.out_channels, fx_chain_item->data, fx_chain_item->context);
                }
                if (fx_chain_item->type == t_lowpass)
                {
                    lowpass(fx_in_ptr, fx_out1, frame_size, config.out_channels, fx_chain_item->data, fx_chain_item->context);
                }
                fx_chain_item = fx_chain_item->next;

                fx_in_ptr = fx_out1;
                fx_out1 = fx_out2;
                fx_out2 = fx_in_ptr;
            }
            out = fx_in_ptr;
        }
        else
        {
            memcpy(out, in, frame_size * config.ref_channels * config.bits_per_sample / 8);
        }

        if (fp_in)
        {
            fwrite(in, 2, frame_size, fp_in);
            fwrite(out, 2, frame_size * config.out_channels, fp_out);
        }

        fifo_write(out, frame_size);
    }

    if (fp_in)
    {
        fclose(fp_in);
        fclose(fp_out);
    }

    free(in);
    free(fx_out1);
    free(fx_out2);
    free_fx_chain();

    printf("main terminated\n");

    exit(0);

    return 0;
}
