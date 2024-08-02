#include "decoder.h"

int read_frame(FileContext *in, AVPacket *pkt, AVFrame **out_frame) {
  int ret;
  ret = av_read_frame(in->format, pkt);
  if (ret < 0) {
      return ret;
  }

  // TODO: Proper check to keep only primary audio and video
  if (pkt->stream_index >= 2) {
    av_packet_unref(pkt);
    return AVERROR(EAGAIN);
  }

  int stream_index = pkt->stream_index;
  StreamContext *s = &in->streams[stream_index];
  ret = avcodec_send_packet(s->codec, pkt);
  if (ret < 0) {
    fprintf(stderr, "Error send packet %d\n", ret);
    exit(1);
  }

  return avcodec_receive_frame(s->codec, *out_frame);
}
