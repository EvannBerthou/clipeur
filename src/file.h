#ifndef FILE_CTX_H
#define FILE_CTX_H

// Each Stream has a decoder for reading a new frame and an encoder to export
// the new frame in an other format. dec_frame is the decoded frame
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

typedef struct StreamContext {
  AVCodecContext *codec;
} StreamContext;

typedef struct {
  AVFormatContext *format;
  StreamContext *streams;
} FileContext;

FileContext *open_file(const char *filename);
void free_file(FileContext *file, int input);

#endif
