#include "transcoder.h"
#include "decoder.h"
#include "encoder.h"
#include "file.h"
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <stdio.h>
#include <stdlib.h>

uint64_t bitrate = 0;

static int compute_bit_rate(AVFormatContext *ctx, double factor) {
  uint64_t duration = ctx->duration / AV_TIME_BASE;
  if (bitrate == 0) {
    bitrate = TARGET_SIZE / duration;
  } else {
    bitrate = bitrate * factor;
  }
  printf("Output bitrate = %lu\n", bitrate);
  return bitrate;
}

static int64_t compute_pts(AVStream *s, int seconds, int offset) {
  int64_t timestamp = seconds * AV_TIME_BASE;
  int64_t pts = av_rescale_q(timestamp, AV_TIME_BASE_Q, s->time_base);
  return pts - offset;
}

static int64_t seek_to_time_seconds(FileContext *in, int seconds) {
  AVStream *s = in->format->streams[0];
  int64_t pts = compute_pts(s, seconds, 0);

  int ret = av_seek_frame(in->format, 0, pts, AVSEEK_FLAG_BACKWARD);
  if (ret < 0) {
    fprintf(stderr, "Error seeking\n");
    exit(1);
  }
  return pts;
}

static FileContext *duplicate_format(FileContext *in_ctx, const char *outfile,
                                     double factor) {
  int ret;
  FileContext *out_ctx = av_calloc(1, sizeof(FileContext));
  if (!out_ctx) {
    fprintf(stderr, "Error creating out_ctx\n");
    exit(1);
  }

  avformat_alloc_output_context2(&out_ctx->format, NULL, NULL, outfile);
  if (!out_ctx) {
    fprintf(stderr, "Could not create output context\n");
    exit(1);
  }

  AVFormatContext *c = in_ctx->format;
  out_ctx->streams = av_calloc(c->nb_streams, sizeof(StreamContext));

  for (int i = 0; i < c->nb_streams; i++) {
    AVCodecContext *enc_ctx;
    AVStream *s = c->streams[i];
    AVCodecContext *dec_ctx = in_ctx->streams[i].codec;
    AVCodecParameters *in_codecpar = s->codecpar;

    if (in_codecpar->codec_type != AVMEDIA_TYPE_VIDEO &&
        in_codecpar->codec_type != AVMEDIA_TYPE_AUDIO) {
      continue;
    }

    const AVCodec *encoder = avcodec_find_encoder(in_codecpar->codec_id);
    if (!encoder) {
      fprintf(stderr, "Encoder not found\n");
      exit(1);
    }

    enc_ctx = avcodec_alloc_context3(encoder);
    if (in_codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
      enc_ctx->height = dec_ctx->height;
      enc_ctx->width = dec_ctx->width;

      enc_ctx->bit_rate = compute_bit_rate(c, factor);

      enc_ctx->sample_aspect_ratio = dec_ctx->sample_aspect_ratio;
      enc_ctx->pix_fmt = dec_ctx->pix_fmt;
      enc_ctx->time_base = av_inv_q(dec_ctx->framerate);
      enc_ctx->framerate = dec_ctx->framerate;
    } else if (in_codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
      enc_ctx->sample_rate = dec_ctx->sample_rate;
      ret = av_channel_layout_copy(&enc_ctx->ch_layout, &dec_ctx->ch_layout);
      if (ret < 0) {
        exit(1);
      }
      enc_ctx->sample_fmt = dec_ctx->sample_fmt;
      enc_ctx->time_base = c->streams[i]->time_base;
    } else {
      fprintf(stderr, "Encoder not found\n");
      exit(1);
    }

    if (out_ctx->format->oformat->flags & AVFMT_GLOBALHEADER)
      enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    ret = avcodec_open2(enc_ctx, encoder, NULL);
    if (ret < 0) {
      fprintf(stderr, "Cannot open %s encoder for stream #%u\n", encoder->name,
              i);
      exit(1);
    }

    AVStream *out_stream = avformat_new_stream(out_ctx->format, NULL);
    ret = avcodec_parameters_from_context(out_stream->codecpar, enc_ctx);
    if (ret < 0) {
      fprintf(stderr,
              "Failed to copy encoder parameters to output stream #%u\n", i);
      exit(1);
    }

    out_stream->time_base = enc_ctx->time_base;
    // TODO: Fixed audio and video index
    out_ctx->streams[i].codec = enc_ctx;
  }

  return out_ctx;
}

static void transcode_stream(FileContext *in, FileContext *out,
                             int64_t start_pts) {
  AVPacket *pkt = av_packet_alloc();
  if (!pkt) {
    fprintf(stderr, "Error allocating packet\n");
    exit(1);
  }

  AVFrame *frame = av_frame_alloc();
  if (!frame) {
    fprintf(stderr, "Error creating frame!\n");
    exit(1);
  }

  int64_t max_pts = compute_pts(in->format->streams[0], 60, start_pts);
  printf("Max pts=%ld\n", max_pts);
  AVRational f = in->streams[0].codec->framerate;
  int framerate = (double)f.num / (double)f.den;

  int x = 0;
  int64_t encoded_pts = 0;
  while (encoded_pts < max_pts) {
    int ret = read_frame(in, pkt, &frame);
    if (ret == AVERROR_EOF) {
      break;
    } else if (ret < 0) {
      fprintf(stderr, "Error reading frame\n");
      exit(1);
    }
    if (in->streams[pkt->stream_index].codec->codec_type ==
        AVMEDIA_TYPE_VIDEO) {
      if (x++ % framerate == 0) {
        printf("%ld / %ld (%f)\n", encoded_pts, max_pts,
               (float)encoded_pts / (float)max_pts);
      }
    }

    int res = encode_frame(in, out, pkt, frame);
    if (res)
      encoded_pts = res;
    av_packet_unref(pkt);
  }

  // Flushing (is it working?)
  for (int i = 0; i < 2; i++) {
    AVCodecContext *c = in->streams[i].codec;
    int ret = avcodec_send_packet(c, NULL);
    while (ret >= 0) {
      ret = avcodec_receive_frame(c, frame);
      if (ret == AVERROR_EOF)
        break;
      else if (ret < 0) {
        fprintf(stderr, "Error flushing decoder\n");
      }
      flush_frame(in, out, i, frame);
    }
  }

  encode_frame(in, out, pkt, NULL);

  av_frame_free(&frame);
  av_packet_free(&pkt);
}

void compress_file(const char *in, const char *out, double factor) {
  FileContext *in_ctx = open_file(in);
  FileContext *out_ctx = duplicate_format(in_ctx, out, factor);

  // Opens output file
  const AVOutputFormat *ofmt = out_ctx->format->oformat;
  if (!(ofmt->flags & AVFMT_NOFILE)) {
    int ret = avio_open(&out_ctx->format->pb, out, AVIO_FLAG_WRITE);
    if (ret < 0) {
      fprintf(stderr, "Could not open output file '%s'", out);
      exit(1);
    }
  }

  clear_start_pts();
  int64_t pts = seek_to_time_seconds(in_ctx, 20);

  // Writes file header for output file
  int ret = avformat_write_header(out_ctx->format, NULL);
  if (ret < 0) {
    fprintf(stderr, "Error occurred when opening output file\n");
    exit(1);
  }

  transcode_stream(in_ctx, out_ctx, pts);

  av_write_trailer(out_ctx->format);

  avio_closep(&out_ctx->format->pb);
  free_file(out_ctx, 0);
  free_file(in_ctx, 1);
}
