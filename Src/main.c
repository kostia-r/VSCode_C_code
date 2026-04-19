#include "vmgp_parser.h"

#include <stdio.h>
#include <stdlib.h>

static uint8_t *load_file(const char *path, size_t *out_size)
{
  FILE *f = fopen(path, "rb");
  long sz;
  uint8_t *buf;
  size_t read_sz;

  if (!f)
  {
    fprintf(stderr, "Failed to open: %s\n", path);
    return NULL;
  }

  if (fseek(f, 0, SEEK_END) != 0)
  {
    fclose(f);
    return NULL;
  }

  sz = ftell(f);
  if (sz < 0)
  {
    fclose(f);
    return NULL;
  }

  rewind(f);
  buf = (uint8_t *)malloc((size_t)sz);
  if (!buf)
  {
    fclose(f);
    return NULL;
  }

  read_sz = fread(buf, 1, (size_t)sz, f);
  fclose(f);

  if (read_sz != (size_t)sz)
  {
    free(buf);
    return NULL;
  }

  *out_size = (size_t)sz;
  return buf;
}

int main(int argc, char **argv)
{
  size_t file_size = 0;
  uint8_t *file_data;
  VMGPContext ctx;
  uint32_t max_steps = 1000000;
  uint32_t max_logged_calls = 80;

  if (argc < 2 || argc > 4)
  {
    fprintf(stderr, "Usage: %s <decrypted.mpn> [max_steps] [max_logged_calls]\n", argv[0]);
    return 1;
  }

  if (argc >= 3)
  {
    max_steps = (uint32_t)strtoul(argv[2], NULL, 0);
  }
  if (argc >= 4)
  {
    max_logged_calls = (uint32_t)strtoul(argv[3], NULL, 0);
  }

  file_data = load_file(argv[1], &file_size);
  if (!file_data)
  {
    fprintf(stderr, "Could not load file.\n");
    return 1;
  }

  if (!vmgp_init(&ctx, file_data, file_size) ||
      !vmgp_parse_header(&ctx) ||
      !vmgp_load_pool(&ctx))
  {
    fprintf(stderr, "Failed to initialize VMGP context.\n");
    free(file_data);
    return 1;
  }

  vmgp_dump_summary(&ctx);
  vmgp_dump_imports(&ctx, 64);
  vmgp_run_trace(&ctx, max_steps, max_logged_calls);

  vmgp_free(&ctx);
  free(file_data);
  return 0;
}
