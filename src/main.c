#include "transcoder.h"
#include <stdio.h>

const char *infile = "copy.mkv";
const char *outfile = "out.mkv";

int compute_bit_rate(AVFormatContext *ctx) {
  int targetSizeKilobytes = 8192;
  int targetSizeBytes = targetSizeKilobytes * 1024;
  uint64_t duration = ctx->duration / AV_TIME_BASE;
  uint64_t result = targetSizeBytes / duration;
  printf("Output bitrate = %lu\n", result);
  return result;
}

int main() {
  compress_file("in.mkv", "out.mkv");
  return 0;
}
