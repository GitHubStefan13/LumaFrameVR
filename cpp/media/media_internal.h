#pragma once
#include "media_types.hpp"
#include "media_trace.h"
#include "media_v4l2.hpp"
#ifdef __cplusplus
#include <atomic>
using std::atomic_int;
using std::atomic_uint;
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
#include <libavutil/fifo.h>
#ifdef __cplusplus
}
#else
#include <stdatomic.h>
#endif
#include <stdio.h>
#include <stdlib.h>

typedef struct Audio Audio;
typedef struct Reader Reader;

typedef struct Media {
    atomic_uint references;
    atomic_int interrupted;
    uint64_t decoder_generation;
    AVFormatContext* file;
    AVCodecContext* decoder;
    std::unique_ptr<media::V4l2Playback> v4l2;
    AVPacket* packet;
    AVFrame* frame;
    int stream, flushing_at_end, preview_requested, preview_repeat_requested;

    int software_decoding;
    // Set when PulseAudio stops answering; the picture keeps playing without sound.
    int audio_unavailable;

    int skip_hardware_decoder;

    int first_frame_observed;
    // Set when the VPU ignores drm_prime and returns a CPU frame instead of a
    // DMA-BUF. The frame is still valid; it takes the CPU upload path.
    int cpu_frames;
    int cpu_frame_reported;

    int64_t stall_started_us;
    double time_base, start, fallback_pts, step, seek_target;
    Audio* audio;
    Reader* reader;
    int audio_stream, audio_present, metadata[3];
    double duration;
    int profile_enabled, trace_enabled, continuous_trace;
    MediaTrace trace;

    media::MediaInfo info;
#ifdef __cplusplus
    ~Media();
#endif
} Media;

typedef struct {
    AVFrame* frame;
    Media* media;
} FrameOwner;

#ifdef __cplusplus
extern "C" {
#endif
int media_error(char* error, size_t capacity, const char* operation, int code);
int media_read_packet(Media* media);
int media_packet(Media* media, int audio, AVPacket* packet);
void media_clear_packets(Media* media, int64_t target);
int media_reader_start(Media* media);
void media_reader_pause(Media* media, int paused);
void media_reader_close(Media* media);
MediaTrace media_reader_trace(Media* media, int reset);
void media_read_metadata(Media* media);
void media_report(Media* media);
float media_sample_aspect_ratio(Media* media, AVFrame* frame);
int media_open_decoder(Media* media);
int open_software_decoder(Media* media);
int decodable(const AVCodecParameters* p);
int video_stream(AVFormatContext** file, const char* path);
media::ColorInfo read_frame_color(AVFrame* frame, Media* media);
int validate_software_frame(AVFrame* frame, char* error, size_t capacity);
void fill_software_descriptor(media::FrameData* out, AVFrame* frame);
int validate_dma_buf_frame(AVFrame* frame, char* error, size_t capacity);
void describe_frame(media::FrameData* out, AVFrame* frame, Media* media);
int receive_decoded_frame(Media* media, char* error, size_t capacity);

int media_vpu_written_off(void);
void media_vpu_write_off(void);

const AVCodec* software_decoder(const AVCodecParameters* p);
void media_fill_info(Media* media);

const char* decoder_name(const AVCodecParameters* p);
int unsupported_video(AVCodecParameters* p, const char* path, char* error, size_t capacity);
int startup_failure(AVCodecParameters* p, const char* path, const char* operation, int code, char* error, size_t capacity);
int media_seek_file(Media* media, int64_t target);
int audio_open(Media* media);
void audio_close(Audio* a);
int audio_reset(Audio* a, double target);
int audio_tick(Media* media, int paused, double* clock, double* limit, int* done);
double audio_preroll(Audio* a);
int64_t media_trace_start(Media* media);
void media_trace_end(int64_t started, int64_t* elapsed);

#ifdef __cplusplus
}
#endif
