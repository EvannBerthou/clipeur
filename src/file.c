#include "file.h"

// Opens file and creates a decoder for each stream (only video and audio).
FileContext *open_file(const char *filename) {
  AVFormatContext *c = avformat_alloc_context();
  if (c == NULL) {
    fprintf(stderr, "Error allocating AVFormatContext\n");
    exit(1);
  }

  int ret = avformat_open_input(&c, filename, NULL, NULL);
  if (ret < 0) {
    fprintf(stderr, "Error opening input file\n");
    exit(1);
  }

  ret = avformat_find_stream_info(c, NULL);
  if (ret < 0) {
    fprintf(stderr, "Error finding stream info\n");
    exit(1);
  }

  FileContext *file_ctx = av_calloc(1, sizeof(FileContext));
  file_ctx->format = c;
  file_ctx->streams = av_calloc(c->nb_streams, sizeof(StreamContext));

  for (int i = 0; i < c->nb_streams; i++) {
    AVStream *stream = c->streams[i];
    const AVCodec *dec = avcodec_find_decoder(stream->codecpar->codec_id);
    AVCodecContext *codec_ctx;
    if (!dec) {
      // fprintf(stderr, "Failed to find decoder for stream #%u\n", i);
      continue;
    }

    codec_ctx = avcodec_alloc_context3(dec);
    if (!codec_ctx) {
      fprintf(stderr, "Failed to allocate the decoder context\n");
      exit(1);
    }

    ret = avcodec_parameters_to_context(codec_ctx, stream->codecpar);
    if (ret < 0) {
      fprintf(stderr, "Failed to copy decoder parameters to input decoder\n");
      exit(1);
    }

    if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO ||
        codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
      if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
        codec_ctx->pkt_timebase = stream->time_base;
        codec_ctx->framerate = av_guess_frame_rate(c, stream, NULL);
      }
      /* Open decoder */
      ret = avcodec_open2(codec_ctx, dec, NULL);
      if (ret < 0) {
        fprintf(stderr, "Failed to open decoder for stream #%u\n", i);
        exit(1);
      }
    }
    file_ctx->streams[i].codec = codec_ctx;
  }
  return file_ctx;
}

void free_file(FileContext *file, int input) {
  for (int i = 0; i < file->format->nb_streams; i++) {
    avcodec_free_context(&file->streams[i].codec);
  }
  av_freep(&file->streams);
  if (input) {
    avformat_close_input(&file->format);
  }
  avformat_free_context(file->format);
  av_freep(&file);
}
