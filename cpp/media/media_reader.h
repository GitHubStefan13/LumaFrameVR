#pragma once
#include "media_internal.h"
#include <pthread.h>
#ifdef __cplusplus
extern "C" {
#endif
#include <libavutil/time.h>
#ifdef __cplusplus
}
#endif

enum { READ_AHEAD_US = AV_TIME_BASE, MAX_PACKET_BYTES = 64 * 1024 * 1024, MAX_PACKETS = 4096 };

struct Reader {
    pthread_t thread;
    pthread_mutex_t mutex;
    pthread_cond_t changed;
    AVFifo* packets[2];
    AVPacket* pending;
    AVRational time_base[2];
    int stream[2], ended[2];
    int64_t end[2];
    size_t bytes;
    int result, paused, reading, stopped, filling, limited, waiting, trace_enabled;
    MediaTrace trace;
    Media* owner;
    int mutex_ready, condition_ready, thread_started;
#ifdef __cplusplus
    Reader() = default;
    ~Reader();
    Reader(const Reader&) = delete;
    Reader& operator=(const Reader&) = delete;
    int start(Media& media);
    int64_t buffered(int stream) const;
    int filled(int64_t target) const;
    int take(int stream, AVPacket& packet);
    int queue();
#endif
};

#ifdef __cplusplus
extern "C" {
#endif
int64_t reader_buffered(Reader* r, int stream);
int reader_filled(Reader* r, int64_t target);
int reader_take(Reader* r, int stream, AVPacket* packet);
int reader_queue(Reader* r);
int reader_read(Media* media);
#ifdef __cplusplus
}
#endif
