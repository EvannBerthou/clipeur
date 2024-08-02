#ifndef ENCODER_H
#define ENCODER_H

#include "file.h"

void encode_video(FileContext *in, FileContext *out, AVPacket *pkt, AVFrame *frame);

#endif
