#ifndef TRANSCODER_H
#define TRANSCODER_H

#include <libavformat/avformat.h>

#define TARGET_SIZE (8 * 1024 * 1024)

void compress_file(const char *in, const char *out, double factor);

#endif
