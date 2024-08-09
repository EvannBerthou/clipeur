#include "transcoder.h"
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <libgen.h>

#define MAX_INPUT_LENGTH 64

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

int main(int argc, char **argv) {
  if (argc != 5) {
    fprintf(stderr, "Usage: %s <input> <output> <start_time> <end_time>",
            argv[0]);
    exit(1);
  }
  char *infile = argv[1];
  char *outfile = argv[2];

  char *filename = basename(outfile);
  char *dirpath = dirname(outfile);
  
  // TODO: Add error handling
  int start_time = strtol(argv[3], NULL, 10);
  int end_time = strtol(argv[4], NULL, 10);

  double factor = 1;
  int attemps = 0;

  char out_filename[MAX_INPUT_LENGTH] = {0};
  do {
    snprintf(out_filename, MAX_INPUT_LENGTH, "%s/%d-%s", dirpath, attemps, filename);
    printf("X: %s\n", out_filename);
    compress_file(infile, out_filename, start_time, end_time, factor);

    factor = get_next_factor(out_filename);
    attemps++;
  } while (factor > 1.1);

  rename(out_filename, outfile);
  for (int i = 0; i < attemps - 1; i++) {
    char remove_name[MAX_INPUT_LENGTH];
    snprintf(remove_name, MAX_INPUT_LENGTH, "%d-%s", i, outfile);
    printf("Removing '%s'\n", remove_name);
    remove(remove_name);
  }

  return 0;
}
