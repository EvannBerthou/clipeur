#include "encoder.h"
#include "file.h"
#include <libavcodec/packet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int64_t start_pts = -1;

static int64_t encode_video_frame(FileContext *in, FileContext *out,
                                  int stream_index, AVFrame *frame) {
  StreamContext *stream = &out->streams[stream_index];
  AVPacket *enc_pkt = av_packet_alloc();

  int old_frame_pts = 0;

  StreamContext *dec_stream = &in->streams[stream_index];
  if (frame != NULL) {
    frame->pict_type = AV_PICTURE_TYPE_NONE;
    old_frame_pts = frame->pts - start_pts;
    frame->pts = av_rescale_q(old_frame_pts, dec_stream->codec->pkt_timebase,
                              stream->codec->time_base);
  }

  int ret = avcodec_send_frame(stream->codec, frame);
  if (ret < 0) {
    fprintf(stderr, "Error during encoding. Error code: %s\n", av_err2str(ret));
    return 0;
  }
  while (ret >= 0) {
    ret = avcodec_receive_packet(stream->codec, enc_pkt);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
      return frame == NULL ? 0 : old_frame_pts;
    } else if (ret < 0) {
      fprintf(stderr, "Error during encoding. Error code: %s\n",
              av_err2str(ret));
      exit(1);
    }

    enc_pkt->stream_index = stream_index;
    av_packet_rescale_ts(enc_pkt, stream->codec->time_base,
                         out->format->streams[stream_index]->time_base);
    ret = av_interleaved_write_frame(out->format, enc_pkt);
    if (ret < 0) {
      fprintf(stderr, "Failed to write\n");
      exit(1);
    }
  }
  av_packet_free(&enc_pkt);
  return 0;
}

static int64_t remux_audio(FileContext *in, FileContext *out, AVPacket *pkt) {
  int stream_index = pkt->stream_index;
  AVStream *out_s = out->format->streams[stream_index];
  pkt->pts -= start_pts;
  pkt->dts -= start_pts;
  av_packet_rescale_ts(pkt, in->format->streams[stream_index]->time_base,
                       out_s->time_base);
  int ret = av_interleaved_write_frame(out->format, pkt);
  if (ret < 0) {
    fprintf(stderr, "Error interleaved\n");
    exit(1);
  }
  return 0;
}

int64_t encode_frame(FileContext *in, FileContext *out, AVPacket *pkt,
                     AVFrame *frame) {

  // TODO: Second condition is ok to do ?
  if (start_pts == -1 || pkt->pts < start_pts) {
    start_pts = pkt->pts;
  }

  int stream_index = pkt->stream_index;
  StreamContext *s = &in->streams[stream_index];
  if (s->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
    return encode_video_frame(in, out, stream_index, frame);
  } else {
    return remux_audio(in, out, pkt);
  }
}

void flush_frame(FileContext *in, FileContext *out, int stream_index,
                 AVFrame *frame) {
  if (stream_index == 0) {
    encode_video_frame(in, out, 0, frame);
  } else if (stream_index == 1) {
    // Nothing to do ?
  } else {
    fprintf(stderr, "Impossible stream index (%d)\n", stream_index);
  }
}

void clear_start_pts() {
  start_pts = -1;
}
