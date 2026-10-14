#include "test_registry.hpp"
#include "test_assertions.hpp"
#include "media_session.hpp"
#include "media_reader.h"

namespace {
class QueueFixture {
   public:
    QueueFixture() : session(new Media{}) {
        auto* m = session.get();
        m->references = 1;
        m->reader = new Reader{};
        auto& r = *m->reader;
        r.owner = m;
        require(pthread_mutex_init(&r.mutex, nullptr) == 0, "initialize reader mutex");
        r.mutex_ready = 1;
        require(pthread_cond_init(&r.changed, nullptr) == 0, "initialize reader condition");
        r.condition_ready = 1;
        r.stream[0] = 0;
        r.stream[1] = 1;
        r.time_base[0] = r.time_base[1] = {1, 1000};
        r.end[0] = r.end[1] = AV_NOPTS_VALUE;
        r.packets[0] = av_fifo_alloc2(MAX_PACKETS, sizeof(AVPacket), 0);
        r.packets[1] = av_fifo_alloc2(MAX_PACKETS, sizeof(AVPacket), 0);
        r.pending = av_packet_alloc();
        m->packet = av_packet_alloc();
        require(r.packets[0] && r.packets[1] && r.pending && m->packet, "allocate bounded queues");
    }
    ~QueueFixture() { media_clear_packets(session.get(), AV_NOPTS_VALUE); }
    ::media::Session session;
};
}  // namespace

int lumaframevr::tests::packet_limits(int, char**) {
    return acceptance_main([] {
        QueueFixture fixture;
        auto& session = fixture.session;
        auto* m = session.get();
        auto& r = *m->reader;
        auto pending = [&](int bytes) {
            require(av_new_packet(r.pending, bytes) == 0, "allocate packet payload");
            r.pending->stream_index = r.stream[0];
        };
        // No worker is started: every queue transition is deterministic.
        pending(MAX_PACKET_BYTES);
        require(r.queue() == 0, "exact byte cap fits");
        require(r.buffered(0) == -1 && !r.filled(READ_AHEAD_US), "missing timestamps cannot pretend to be prefilled");
        pending(1);
        require(r.queue() == AVERROR(EAGAIN) && r.limited && r.bytes == MAX_PACKET_BYTES,
                "one byte beyond cap retains pending reference");
        require(r.take(1, *m->packet) == AVERROR(ENOBUFS), "blocked opposite stream reports byte pressure");
        require(r.take(0, *m->packet) == 0 && m->packet->size == MAX_PACKET_BYTES, "dequeue releases byte budget");
        require(r.queue() == 0 && !r.limited && r.bytes == 1, "pending packet resumes after dequeue");
        media_clear_packets(m, AV_NOPTS_VALUE);
        for(int i = 0; i <= MAX_PACKETS; ++i) {
            pending(1);
            require(r.queue() == (i == MAX_PACKETS ? AVERROR(EAGAIN) : 0), "exact packet-count cap and one-past boundary");
        }
        require(r.limited && r.pending->buf && r.bytes == MAX_PACKETS, "packet-count pressure preserves pending ownership");
        require(r.take(0, *m->packet) == 0 && r.queue() == 0 && !r.limited, "packet-count budget resumes after dequeue");
        const auto buffer = session.buffer(false);
        require(!buffer.ready && buffer.video_us == -1 && buffer.bytes == MAX_PACKETS,
                "typed Session cannot infer readiness from packet count without timestamps");
        media_clear_packets(m, AV_NOPTS_VALUE);
        require(!r.bytes && !r.limited && r.filling && !r.pending->buf && !av_fifo_can_read(r.packets[0]),
                "clear releases payloads and resets pressure");
        require(r.take(0, *m->packet) == AVERROR(EAGAIN), "empty queue is nonblocking");
    });
}
