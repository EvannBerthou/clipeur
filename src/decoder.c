#include "decoder.h"

int read_frame(FileContext *in, AVPacket *pkt, AVFrame **out_frame) {
  int ret;
  while (1) {
    ret = av_read_frame(in->format, pkt);
    if (ret < 0) {
      break;
    }

    if (pkt->stream_index >= 2) {
      av_packet_unref(pkt);
      continue;
    }

    int stream_index = pkt->stream_index;
    StreamContext *s = &in->streams[stream_index];
    ret = avcodec_send_packet(s->codec, pkt);
    while (ret >= 0) {
      ret = avcodec_receive_frame(s->codec, *out_frame);
      if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        break;
      } else if (ret < 0) {
        return ret;
      }
      return 0;
    }
  }
  return ret;
}
