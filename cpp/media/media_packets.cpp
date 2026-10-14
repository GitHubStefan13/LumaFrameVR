#include "media_packets.hpp"
#include <utility>

namespace {
class ReaderLock {
   public:
    explicit ReaderLock(Reader& reader) : mutex_(&reader.mutex) { pthread_mutex_lock(mutex_); }
    ~ReaderLock() { pthread_mutex_unlock(mutex_); }
    ReaderLock(const ReaderLock&) = delete;
    ReaderLock& operator=(const ReaderLock&) = delete;

   private:
    pthread_mutex_t* mutex_;
};
class PacketReference {
   public:
    PacketReference() = default;
    ~PacketReference() { av_packet_unref(&packet_); }
    PacketReference(PacketReference&& other) noexcept { av_packet_move_ref(&packet_, &other.packet_); }
    PacketReference& operator=(PacketReference&& other) noexcept {
        if(this != &other) {
            av_packet_unref(&packet_);
            av_packet_move_ref(&packet_, &other.packet_);
        }
        return *this;
    }
    PacketReference(const PacketReference&) = delete;
    PacketReference& operator=(const PacketReference&) = delete;
    AVPacket* get() { return &packet_; }
    AVPacket release() { return std::exchange(packet_, AVPacket{}); }

   private:
    AVPacket packet_{};
};
int64_t timestamp(const AVPacket& packet) {
    return packet.dts == AV_NOPTS_VALUE ? packet.pts : packet.dts;
}
bool has_space(const Reader& reader, int stream) {
    return av_fifo_can_write(reader.packets[stream]) && static_cast<size_t>(reader.pending->size) <= MAX_PACKET_BYTES - reader.bytes;
}
}  // namespace

int64_t Reader::buffered(int stream) const {
    const Reader* r = this;
    size_t count = av_fifo_can_read(r->packets[stream]);
    if(!count)
        return 0;
    AVPacket first{}, last{};
    av_fifo_peek(r->packets[stream], &first, 1, 0);
    av_fifo_peek(r->packets[stream], &last, 1, count - 1);
    int64_t start = timestamp(first), end = timestamp(last);
    if(start == AV_NOPTS_VALUE || end == AV_NOPTS_VALUE || end < start)
        return -1;
    int64_t span = av_sat_add64(av_sat_sub64(end, start), FFMAX(last.duration, 0));
    return av_rescale_q(span, r->time_base[stream], AVRational{1, AV_TIME_BASE});
}
int Reader::filled(int64_t target) const {
    for(int i = 0; i < 2; i++)
        if(stream[i] >= 0 && !(i && ended[i]) && buffered(i) < target)
            return 0;
    return 1;
}
void media_clear_packets(Media* media, int64_t target) {
    Reader* r = media->reader;
    if(!r)
        return;
    ReaderLock lock(*r);
    for(int i = 0; i < 2; i++) {
        while(av_fifo_can_read(r->packets[i])) {
            PacketReference packet;
            av_fifo_read(r->packets[i], packet.get(), 1);
        }
        r->ended[i] = i && r->stream[i] >= 0 && target != AV_NOPTS_VALUE && r->end[i] != AV_NOPTS_VALUE && av_compare_ts(target, r->time_base[0], r->end[i], r->time_base[i]) >= 0;
    }
    av_packet_unref(r->pending);
    r->bytes = 0;
    r->result = r->limited = r->waiting = 0;
    r->filling = 1;
}
int Reader::take(int stream, AVPacket& packet) {
    Reader* r = this;
    av_packet_unref(&packet);
    r->waiting &= ~(1 << stream);
    if(av_fifo_can_read(r->packets[stream])) {
        av_fifo_read(r->packets[stream], &packet, 1);
        r->bytes -= packet.size;
        pthread_cond_broadcast(&r->changed);
        return 0;
    }
    if(r->result)
        return r->result;
    if(stream && r->ended[stream])
        return AVERROR_EOF;
    r->waiting |= 1 << stream;
    int destination = r->pending->stream_index == r->stream[0] ? 0 : 1;
    return r->limited && !has_space(*r, destination) ? AVERROR(ENOBUFS) : AVERROR(EAGAIN);
}
int Reader::queue() {
    Reader* r = this;
    AVPacket* p = r->pending;
    int stream = p->stream_index == r->stream[0] ? 0 : p->stream_index == r->stream[1] ? 1 : -1;
    if(stream < 0) {
        av_packet_unref(p);
        return 0;
    }
    if(p->size < 0 || p->size > MAX_PACKET_BYTES)
        return AVERROR(ENOBUFS);
    r->limited = !has_space(*r, stream);
    if(r->limited)
        return AVERROR(EAGAIN);
    r->waiting &= ~(1 << stream);
    r->bytes += p->size;
    if(p->pts != AV_NOPTS_VALUE && r->end[stream] != AV_NOPTS_VALUE && p->duration > 0 && p->pts >= av_sat_sub64(r->end[stream], p->duration))
        r->ended[stream] = 1;
    PacketReference queued;
    av_packet_move_ref(queued.get(), p);
    const AVPacket transferred = queued.release();
    av_fifo_write(r->packets[stream], &transferred, 1);
    return 0;
}
int media::packet(Media& media, int audio, AVPacket& packet) {
    int result = media_reader_start(&media);
    if(result < 0)
        return result;
    Reader* r = media.reader;
    ReaderLock lock(*r);
    result = r->take(audio, packet);
    while(result == AVERROR(EAGAIN) && r->paused) {
        if(reader_read(&media) == AVERROR(EAGAIN))
            break;
        if(!r->result) {
            int queued = r->queue();
            if(queued < 0)
                r->result = queued == AVERROR(EAGAIN) ? AVERROR(ENOBUFS) : queued;
        }
        result = r->take(audio, packet);
    }
    return result;
}
int64_t reader_buffered(Reader* r, int stream) {
    return r->buffered(stream);
}
int reader_filled(Reader* r, int64_t target) {
    return r->filled(target);
}
int reader_take(Reader* r, int stream, AVPacket* packet) {
    return r->take(stream, *packet);
}
int reader_queue(Reader* r) {
    return r->queue();
}
int media_packet(Media* media, int audio, AVPacket* packet) {
    return media::packet(*media, audio, *packet);
}
int media_read_packet(Media* media) {
    int result = media::packet(*media, false, *media->packet);
    if(result >= 0 && media->seek_target >= 0 && media->packet->pts == AV_NOPTS_VALUE)
        return AVERROR_INVALIDDATA;
    return result;
}
