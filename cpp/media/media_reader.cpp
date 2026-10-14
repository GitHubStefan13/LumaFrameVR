#include "media_reader.h"
#include "media_session.hpp"

int reader_read(Media* media) {
    Reader* r = media->reader;
    int64_t started = r->trace_enabled ? av_gettime_relative() : 0;
    r->reading = 1;
    pthread_mutex_unlock(&r->mutex);
    int result = av_read_frame(media->file, r->pending);
    int64_t elapsed = started ? av_gettime_relative() - started : 0;
    if(result == AVERROR(EAGAIN))
        av_usleep(1000);
    pthread_mutex_lock(&r->mutex);
    if(started) {
        r->trace.read_us += elapsed;
        r->trace.packets += result >= 0;
        if(result >= 0)
            r->trace.packet_bytes += r->pending->size;
    }
    r->reading = 0;
    if(result < 0 && result != AVERROR(EAGAIN))
        r->result = result;
    pthread_cond_broadcast(&r->changed);
    return result;
}

static void* read_packets(void* handle) {
    Media* media = static_cast<Media*>(handle);
    Reader* r = media->reader;
    pthread_mutex_lock(&r->mutex);
    while(!r->stopped) {
        if(reader_filled(r, READ_AHEAD_US))
            r->filling = 0;
        if(!reader_filled(r, READ_AHEAD_US / 2))
            r->filling = 1;
        if(r->paused || r->result || !r->filling) {
            pthread_cond_wait(&r->changed, &r->mutex);
            continue;
        }
        if(!r->limited && reader_read(media) < 0)
            continue;
        if(r->result)
            continue;
        int result = reader_queue(r);
        if(result == AVERROR(EAGAIN))
            pthread_cond_wait(&r->changed, &r->mutex);
        else if(result < 0)
            r->result = result;
    }
    pthread_mutex_unlock(&r->mutex);
    return NULL;
}

int Reader::start(Media& media) {
    owner = &media;
    Reader* r = this;
    int result = pthread_mutex_init(&r->mutex, nullptr);
    if(result)
        return AVERROR(result);
    mutex_ready = 1;
    result = pthread_cond_init(&r->changed, nullptr);
    if(result)
        return AVERROR(result);
    condition_ready = 1;
    r->stream[0] = media.stream;
    r->stream[1] = media.audio_stream;
    for(int i = 0; i < 2; i++) {
        r->packets[i] = av_fifo_alloc2(MAX_PACKETS, sizeof(AVPacket), 0);
        if(r->stream[i] < 0)
            continue;
        AVStream* s = media.file->streams[r->stream[i]];
        r->time_base[i] = s->time_base;
        r->end[i] = s->duration == AV_NOPTS_VALUE || s->start_time == AV_NOPTS_VALUE ? AV_NOPTS_VALUE : av_sat_add64(s->start_time, s->duration);
    }
    r->pending = av_packet_alloc();
    r->filling = 1;
    r->trace_enabled = media.trace_enabled;
    media.reader = r;
    result = !r->pending || !r->packets[0] || !r->packets[1] ? ENOMEM : pthread_create(&r->thread, nullptr, read_packets, &media);
    if(!result)
        thread_started = 1;
    return result ? AVERROR(result) : 0;
}

int media_reader_start(Media* media) {
    if(media->reader)
        return 0;
    try {
        auto reader = std::make_unique<Reader>();
        int result = reader->start(*media);
        if(result < 0)
            return result;
        media->reader = reader.release();
        return 0;
    } catch(const std::bad_alloc&) {
        return AVERROR(ENOMEM);
    }
}

void media_reader_pause(Media* media, int paused) {
    Reader* r = media->reader;
    if(!r)
        return;
    pthread_mutex_lock(&r->mutex);
    r->paused = paused;
    pthread_cond_broadcast(&r->changed);
    while(paused && r->reading) pthread_cond_wait(&r->changed, &r->mutex);
    pthread_mutex_unlock(&r->mutex);
}

Reader::~Reader() {
    if(thread_started) {
        pthread_mutex_lock(&mutex);
        stopped = 1;
        owner->interrupted.store(1);
        pthread_cond_signal(&changed);
        pthread_mutex_unlock(&mutex);
        pthread_join(thread, nullptr);
        media_clear_packets(owner, AV_NOPTS_VALUE);
    }
    for(auto& queue : packets) av_fifo_freep2(&queue);
    av_packet_free(&pending);
    if(condition_ready)
        pthread_cond_destroy(&changed);
    if(mutex_ready)
        pthread_mutex_destroy(&mutex);
    if(owner)
        owner->reader = nullptr;
}

void media_reader_close(Media* media) {
    std::unique_ptr<Reader> reader(media->reader);
}

MediaTrace media_reader_trace(Media* media, int reset) {
    Reader* r = media->reader;
    MediaTrace trace{};
    if(!r)
        return trace;
    pthread_mutex_lock(&r->mutex);
    if(reset)
        r->trace = trace;
    r->trace_enabled = media->trace_enabled;
    trace = r->trace;
    pthread_mutex_unlock(&r->mutex);
    return trace;
}

static media::MediaBuffer packet_buffer(Media& media, bool prefill) {
    Reader* r = media.reader;
    media::MediaBuffer buffer{};
    if(!r)
        return buffer;
    pthread_mutex_lock(&r->mutex);
    buffer = media::MediaBuffer{reader_buffered(r, 0), reader_buffered(r, 1), r->bytes, r->result || r->limited || reader_filled(r, READ_AHEAD_US), r->limited, r->waiting};
    if(prefill && !buffer.ready) {
        r->filling = 1;
        pthread_cond_signal(&r->changed);
    }
    pthread_mutex_unlock(&r->mutex);
    return buffer;
}

media::MediaBuffer media::Session::buffer(bool prefill) {
    return packet_buffer(*get(), prefill);
}
