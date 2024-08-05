#include "transcoder.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

const char *infile = "jp.mkv";
const char *outfile = "term.mkv";

static off_t get_file_size(const char *filename) {
  int fd = open(filename, O_RDONLY);
  if (fd < 0)
    return INT_MAX;

  struct stat buf;
  fstat(fd, &buf);
  close(fd);
  return buf.st_size;
}

static double get_next_factor(const char *filename) {
  off_t filesize = get_file_size(filename);
  double percent_of_target = (100.f / TARGET_SIZE) * filesize;
  double factor = 100.f / percent_of_target;
  printf("RESULT=%f (%f)\n", percent_of_target, factor);
  return factor;
}

int main() {
  double factor = 1;
  int attemps = 0;

  char in_filename[16] = {0};
  char out_filename[16] = {0};
  snprintf(in_filename, 16, "%s", infile);

  do {
    snprintf(out_filename, 16, "%d-%s", attemps, outfile);
    compress_file(in_filename, out_filename, factor);

    factor = get_next_factor(out_filename);
    attemps++;
  } while (factor > 1.1);

  rename(out_filename, outfile);
  for (int i = 0; i < attemps - 1; i++) {
    char remove_name[16];
    snprintf(remove_name, 16, "%d-%s", i, outfile);
    printf("Removing '%s'\n", remove_name);
    remove(remove_name);
  }

  return 0;
}
