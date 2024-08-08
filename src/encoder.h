#ifndef ENCODER_H
#define ENCODER_H

#include "file.h"

int64_t encode_frame(FileContext *in, FileContext *out, AVPacket *pkt,
                  AVFrame *frame);

void flush_frame(FileContext *in, FileContext *out, int stream_index,
                 AVFrame *frame);

void clear_start_pts();

#endif
