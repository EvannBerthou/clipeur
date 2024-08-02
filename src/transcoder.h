#ifndef TRANSCODER_H
#define TRANSCODER_H

#include <libavformat/avformat.h>

void compress_file(const char *in, const char *out);

#endif
