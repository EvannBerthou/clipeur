#ifndef DECODER_H
#define DECODER_H

#include "file.h"

int read_frame(FileContext *in, AVPacket *pkt, AVFrame **out_frame);

#endif
