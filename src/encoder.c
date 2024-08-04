#include "encoder.h"
#include "file.h"
#include <stdio.h>
#include <stdlib.h>

static void encode_write_frame(FileContext *in, FileContext *out,
                               int stream_index, AVFrame *frame) {
  StreamContext *stream = &out->streams[stream_index];
  AVPacket *enc_pkt = av_packet_alloc();

  StreamContext *dec_stream = &in->streams[stream_index];
  if (frame != NULL) {
    frame->pict_type = AV_PICTURE_TYPE_NONE;
    frame->pts = av_rescale_q(frame->pts, dec_stream->codec->pkt_timebase,
                              stream->codec->time_base);
  }
  int ret = avcodec_send_frame(stream->codec, frame);
  if (ret < 0) {
    fprintf(stderr, "Error during encoding. Error code: %s\n", av_err2str(ret));
    return;
  }
  while (ret >= 0) {
    ret = avcodec_receive_packet(stream->codec, enc_pkt);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
      break;
    }
    enc_pkt->stream_index = stream_index;
    av_packet_rescale_ts(enc_pkt, in->format->streams[stream_index]->time_base,
                         out->format->streams[stream_index]->time_base);
    enc_pkt->pos = -1;
    ret = av_interleaved_write_frame(out->format, enc_pkt);
    if (ret < 0) {
      fprintf(stderr, "Failed to write\n");
      exit(1);
    }
    av_packet_unref(enc_pkt);
  }
  av_packet_unref(enc_pkt);
  av_packet_free(&enc_pkt);
}

// Also compress audio bitrate.
// TODO: Separate audio and video encofing
void encode_video(FileContext *in, FileContext *out, AVPacket *pkt,
                  AVFrame *frame) {
  int stream_index = pkt->stream_index;
  StreamContext *s = &in->streams[stream_index];
  if (s->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
    encode_write_frame(in, out, stream_index, frame);
  } else {
    AVStream *out_s = out->format->streams[stream_index];
    av_packet_rescale_ts(pkt, in->format->streams[stream_index]->time_base,
                         out_s->time_base);
    pkt->stream_index = 1;
    int ret = av_interleaved_write_frame(out->format, pkt);
    if (ret < 0) {
      fprintf(stderr, "Error interleaved\n");
      exit(1);
    }
  }
}
