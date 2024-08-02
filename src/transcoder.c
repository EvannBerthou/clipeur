#include "transcoder.h"
#include "decoder.h"
#include "encoder.h"
#include "file.h"
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <stdio.h>
#include <stdlib.h>

static FileContext *duplicate_format(FileContext *in_ctx, const char *outfile) {
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

      enc_ctx->bit_rate = 100 * 1000;

      enc_ctx->sample_aspect_ratio = dec_ctx->sample_aspect_ratio;
      enc_ctx->pix_fmt = dec_ctx->pix_fmt;
      enc_ctx->time_base = c->streams[i]->time_base;
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
    out_ctx->streams[i].codec = enc_ctx;
  }

  return out_ctx;
}

static void transcode_stream(FileContext *in, FileContext *out) {
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

  printf("%ld\n", in->format->duration / AV_TIME_BASE);

  int x = 0;
  while (1) {
    int ret = read_frame(in, pkt, &frame);
    if (ret == AVERROR_EOF) {
      break;
    } else if (ret == AVERROR(EAGAIN)) {
      av_packet_unref(pkt);
      continue;
    } else if (ret < 0) {
      fprintf(stderr, "Error reading frame\n");
      exit(1);
    }

    x++;
    if (x % 200 == 0) {
      printf("%d\n", x);
    }

    encode_video(in, out, pkt, frame);
    av_packet_unref(pkt);
  }

  printf("Flushing decoder\n");

  //TODO: Add flusing encoder and decoder before exiting transcoding
  av_packet_unref(pkt);
  int ret = read_frame(in, pkt, &frame);
  if (ret < 0 && ret != AVERROR_EOF) {
      fprintf(stderr, "Error flushing decoder\n");
      exit(1);
  }

  printf("Flushing encoder\n");
  encode_video(in, out, pkt, NULL);

  av_frame_free(&frame);
  av_packet_free(&pkt);
}

void compress_file(const char *in, const char *out) {
  FileContext *in_ctx = open_file(in);
  FileContext *out_ctx = duplicate_format(in_ctx, out);

  // Opens output file
  const AVOutputFormat *ofmt = out_ctx->format->oformat;
  if (!(ofmt->flags & AVFMT_NOFILE)) {
    int ret = avio_open(&out_ctx->format->pb, out, AVIO_FLAG_WRITE);
    if (ret < 0) {
      fprintf(stderr, "Could not open output file '%s'", out);
      exit(1);
    }
  }

  // Writes file header for output file
  int ret = avformat_write_header(out_ctx->format, NULL);
  if (ret < 0) {
    fprintf(stderr, "Error occurred when opening output file\n");
    exit(1);
  }

  transcode_stream(in_ctx, out_ctx);

  av_write_trailer(out_ctx->format);

  avio_closep(&out_ctx->format->pb);
  free_file(out_ctx, 0);
  free_file(in_ctx, 1);
}
