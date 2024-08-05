#ifndef ENCODER_H
#define ENCODER_H

#include "file.h"

void encode_frame(FileContext *in, FileContext *out, AVPacket *pkt,
                  AVFrame *frame);

void flush_frame(FileContext *in, FileContext *out, int stream_index,
                 AVFrame *frame);

#endif
