#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>

#include "pa_ringbuffer.h"
#include "conf.h"
#include "util.h"

extern int g_is_quit;

PaUtilRingBuffer g_out_ringbuffer;
PaUtilRingBuffer g_in_ringbuffer;

void *fifo_write_thread(void *ptr)
{
    conf_t *conf = (conf_t *)ptr;
    ring_buffer_size_t size1, size2, available;
    void *data1, *data2;
    int fd = open(conf->out_fifo, O_WRONLY); // will block until reader is available
    if (fd < 0)
    {
        printf("failed to open %s, error %d\n", conf->out_fifo, fd);
        return NULL;
    }

    // ignore SIGPIPE
    struct sigaction sig_pipe_handler;
    sig_pipe_handler.sa_handler = SIG_IGN;
    sigemptyset(&sig_pipe_handler.sa_mask);
    sig_pipe_handler.sa_flags = 0;
    sigaction(SIGPIPE, &sig_pipe_handler, NULL);

    // ignore SIGUSR1
    // struct sigaction sig_usr1_handler;
    // sig_usr1_handler.sa_handler = SIG_IGN;
    // sigemptyset(&sig_usr1_handler.sa_mask);
    // sig_usr1_handler.sa_flags = 0;
    // sigaction(SIGUSR1, &sig_usr1_handler, NULL);

    // clear
    PaUtil_AdvanceRingBufferReadIndex(&g_out_ringbuffer, PaUtil_GetRingBufferReadAvailable(&g_out_ringbuffer));
    while (!g_is_quit)
    {
        available = PaUtil_GetRingBufferReadAvailable(&g_out_ringbuffer);
        PaUtil_GetRingBufferReadRegions(&g_out_ringbuffer, available, &data1, &size1, &data2, &size2);
        if (size1 > 0)
        {
            int result = write(fd, data1, size1 * g_out_ringbuffer.elementSizeBytes);
            // printf("write %d of %d\n", result / 2, size1);
            if (result > 0)
            {
                PaUtil_AdvanceRingBufferReadIndex(&g_out_ringbuffer, result / g_out_ringbuffer.elementSizeBytes);
            }
            else
            {
                sleep(1);
            }
        }
        else
        {
            usleep(100000);
        }
    }

    close(fd);

    printf("fifo_write_thread terminated\n");

    return NULL;
}

void *fifo_read_thread(void *ptr)
{
    unsigned chunk_bytes;
    unsigned frame_bytes;
    char *chunk = NULL;
    unsigned chunk_size = 1024;

    conf_t *conf = (conf_t *)ptr;
    ring_buffer_size_t size1, size2, available;
    void *data1, *data2;

    frame_bytes = conf->in_channels * 2;
    chunk_bytes = chunk_size * frame_bytes;
    chunk = (char *)malloc(chunk_bytes);
    if (chunk == NULL)
    {
        fprintf(stderr, "not enough memory\n");
        exit(1);
    }

    int fd = open(conf->in_fifo, O_RDONLY | O_NONBLOCK);
    if (fd < 0)
    {
        fprintf(stderr, "failed to open %s, error %d\n", conf->in_fifo, fd);
        exit(1);
    }
    long pipe_size = (long)fcntl(fd, F_GETPIPE_SZ);
    if (pipe_size == -1)
    {
        perror("get pipe size failed.");
    }
    printf("default pipe size: %ld\n", pipe_size);

    int ret = fcntl(fd, F_SETPIPE_SZ, chunk_bytes * 4);
    if (ret < 0)
    {
        perror("set pipe size failed.");
    }

    pipe_size = (long)fcntl(fd, F_GETPIPE_SZ);
    if (pipe_size == -1)
    {
        perror("get pipe size 2 failed.");
    }
    printf("new pipe size: %ld\n", pipe_size);

    // ignore SIGUSR1
    // struct sigaction sig_usr1_handler;
    // sig_usr1_handler.sa_handler = SIG_IGN;
    // sigemptyset(&sig_usr1_handler.sa_mask);
    // sig_usr1_handler.sa_flags = 0;
    // sigaction(SIGUSR1, &sig_usr1_handler, NULL);

    // clear
    // fsync(fd);
    PaUtil_AdvanceRingBufferWriteIndex(&g_in_ringbuffer, PaUtil_GetRingBufferWriteAvailable(&g_in_ringbuffer));
    while (!g_is_quit)
    {
        available = PaUtil_GetRingBufferWriteAvailable(&g_in_ringbuffer);
        PaUtil_GetRingBufferWriteRegions(&g_in_ringbuffer, available, &data1, &size1, &data2, &size2);
        if (size1 > 0)
        {
            int result = read(fd, data1, size1 * g_in_ringbuffer.elementSizeBytes);
            // printf("write %d of %d\n", result / 2, size1);
            if (result > 0)
            {
                PaUtil_AdvanceRingBufferWriteIndex(&g_in_ringbuffer, result / g_in_ringbuffer.elementSizeBytes);
            }
            else
            {
                sleep(1);
            }
        }
        else
        {
            usleep(100000);
        }
    }

    printf("fifo_read_thread terminated\n");

    return NULL;
}

int fifo_write_setup(conf_t *conf)
{
    pthread_t writer;
    struct stat st;

    unsigned buffer_size = power2(conf->buffer_size);
    unsigned buffer_bytes = conf->out_channels * conf->bits_per_sample / 8;

    void *buf = calloc(buffer_size, buffer_bytes);
    if (buf == NULL)
    {
        fprintf(stderr, "Fail to allocate memory.\n");
        exit(1);
    }

    ring_buffer_size_t ret = PaUtil_InitializeRingBuffer(&g_out_ringbuffer, buffer_bytes, buffer_size, buf);
    if (ret == -1)
    {
        fprintf(stderr, "Initialize ring buffer but element count is not a power of 2.\n");
        exit(1);
    }

    if (stat(conf->out_fifo, &st) != 0)
    {
        mkfifo(conf->out_fifo, 0666);
    }
    else if (!S_ISFIFO(st.st_mode))
    {
        remove(conf->out_fifo);
        mkfifo(conf->out_fifo, 0666);
    }

    pthread_create(&writer, NULL, fifo_write_thread, conf);

    return 0;
}

int fifo_read_setup(conf_t *conf)
{
    pthread_t reader;
    struct stat st;

    unsigned buffer_size = power2(conf->buffer_size);
    unsigned buffer_bytes = conf->in_channels * conf->bits_per_sample / 8;

    void *buf = calloc(buffer_size, buffer_bytes);
    if (buf == NULL)
    {
        fprintf(stderr, "Fail to allocate memory.\n");
        exit(1);
    }

    ring_buffer_size_t ret = PaUtil_InitializeRingBuffer(&g_in_ringbuffer, buffer_bytes, buffer_size, buf);
    if (ret == -1)
    {
        fprintf(stderr, "Initialize ring buffer but element count is not a power of 2.\n");
        exit(1);
    }

    if (stat(conf->in_fifo, &st) != 0)
    {
        mkfifo(conf->in_fifo, 0666);
    }
    else if (!S_ISFIFO(st.st_mode))
    {
        remove(conf->in_fifo);
        mkfifo(conf->in_fifo, 0666);
    }

    pthread_create(&reader, NULL, fifo_read_thread, conf);

    return 0;
}

int fifo_write(void *buf, size_t frames)
{
    return PaUtil_WriteRingBuffer(&g_out_ringbuffer, buf, frames);
}

int fifo_read(void *buf, size_t frames, int timeout_ms)
{
    while (!g_is_quit && PaUtil_GetRingBufferReadAvailable(&g_in_ringbuffer) < frames && timeout_ms > 0)
    {
        usleep(1000);
        timeout_ms--;
    }

    return PaUtil_ReadRingBuffer(&g_in_ringbuffer, buf, frames);
}
