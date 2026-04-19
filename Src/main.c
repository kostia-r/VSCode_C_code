#include "vmgp_parser.h"

#include <stdio.h>
#include <stdlib.h>

static uint8_t *load_file(const char *path, size_t *out_size)
{
  FILE *f = fopen(path, "rb");
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

  long sz = ftell(f);
  if (sz < 0)
  {
    fclose(f);
    return NULL;
  }

  rewind(f);

  uint8_t *buf = (uint8_t *)malloc((size_t)sz);
  if (!buf)
  {
    fclose(f);
    return NULL;
  }

  size_t read_sz = fread(buf, 1, (size_t)sz, f);
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
  if (argc != 2)
  {
    fprintf(stderr, "Usage: %s <file.mpn>\n", argv[0]);
    return 1;
  }

  size_t file_size = 0;
  uint8_t *file_data = load_file(argv[1], &file_size);
  if (!file_data)
  {
    fprintf(stderr, "Could not load file.\n");
    return 1;
  }

  VMGPContext ctx;
  if (!vmgp_init(&ctx, file_data, file_size))
  {
    fprintf(stderr, "vmgp_init failed.\n");
    free(file_data);
    return 1;
  }

  if (!vmgp_parse_header(&ctx))
  {
    fprintf(stderr, "Not a valid VMGP file.\n");
    free(file_data);
    return 1;
  }

  vmgp_find_section_tags(&ctx);
  vmgp_find_imports_and_strtab(&ctx);
  vmgp_dump_summary(&ctx);

  free(file_data);
  return 0;
}
