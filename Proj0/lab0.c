// NAME: Justin Yi
// EMAIL: joostinyi00@gmail.com
// ID: 905123893

#include <getopt.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef void (*sighandler_t)(int);
ssize_t ret = 0;
int i, ifd, ofd, pos = 0;
char buffer;
char *ptr, *infile, *outfile;
char order[4]; // structure to keep track of order of options

/* options descriptor */
const struct option args[] = 
{
    /*		option		has arg			flag		return value */
    {"input=", 1, NULL, 'i'},
    {"output=", 1, NULL, 'o'},
    {"catch", 0, NULL, 'c'},
    {"segfault", 0, NULL, 's'},
    {0, 0, 0, 0}
};

void sigsegv_handler()
{
  fprintf(stderr, "Caught segmentation fault\n");
  exit(4);
}

void segfault()
{
  ptr = NULL;
  (*ptr) = 0;
}

int main(int argc, char *argv[])
{
  // process arguments
  while ((i = getopt_long(argc, argv, "", args, NULL)) != -1)
  {
    if (i == 's')
    {
      order[3] = i;
      continue;
    }
    if (i == 'i')
      infile = optarg;
    if (i == 'o')
      outfile = optarg;
    order[pos] = i;
    pos++;
  }

  // action in order
  for (pos = 0; pos < 4; pos++)
  {
    switch (order[pos])
    {
    case 'i':
      ifd = open(infile, O_RDONLY);
      if (ifd == -1)
      {
        fprintf(stderr, "Error: --input=%s\n", infile);
        fprintf(stderr, "%s\n", strerror(errno));
        exit(2);
      }
      else
      {
        close(0); // closes stdin
        dup(ifd); // 0 maps to ifd
        close(ifd);

        if(pread(0,&buffer, 1, 0) < 0)
        {
          fprintf(stderr, "Error: --output=%s\n", infile);
          fprintf(stderr, "%s\n", strerror(errno));
          exit(2);
        }
      }
      break;
    case 'o':
      // create outfile with permissions
      ofd = creat(outfile, 0666);
      if (ofd == -1)
      {
        fprintf(stderr, "Error: --output=%s\n", outfile);
        fprintf(stderr, "%s\n", strerror(errno));
        exit(3);
      }
      else
      {
        close(1); // closes stdout
        dup(ofd); // 1 maps to ofd
        close(ofd);
      }
      break;
    case 'c':
      signal(SIGSEGV, sigsegv_handler);
      break;
    case 's':
      segfault();
      break;
    case '\0': // if not all options specified
      break;
    default:
      fprintf(stderr, "usage: %s [--input=input_file] [--output=output_file] [--catch] [--segfault]\n", argv[0]);
      exit(1);
      break;
    }
  }

  while (read(0, &buffer, 1) > 0)
  {
      ret = write(1, &buffer, 1);
      if(ret < 0)
      {
        fprintf(stderr, "Error: --output=%s\n", outfile);
        fprintf(stderr, "%s\n", strerror(errno));
        exit(3);
      }
  }

  close(1);
  close(0);
  exit(0);
}
