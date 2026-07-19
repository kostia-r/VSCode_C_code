#include "MVM.h"
#include "MVM_Cfg.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct SmokeContext
{
  FILE *file;
  uint32_t flushCount;
} SmokeContext;

static int read_image(void *user, size_t offset, void *dst, size_t size)
{
  SmokeContext *context = (SmokeContext *)user;

  if (!context || !context->file || !dst || fseek(context->file, (long)offset, SEEK_SET) != 0)
  {
    return -1;
  }

  return (fread(dst, 1U, size, context->file) == size) ? 0 : -1;
}

static int flush_display(void *user, const MVM_Framebuffer_t *framebuffer)
{
  SmokeContext *context = (SmokeContext *)user;

  if (!context || !framebuffer || !framebuffer->pixels || framebuffer->pixel_format != MVM_PIXEL_FORMAT_RGB565 ||
      framebuffer->width != 96U || framebuffer->height != 64U ||
      framebuffer->dirty_rect.width == 0U || framebuffer->dirty_rect.height == 0U)
  {
    return -1;
  }

  ++context->flushCount;
  return 0;
}

int main(int argc, char **argv)
{
  FILE *file = NULL;
  uint8_t *image = NULL;
  void *storage = NULL;
  MpnVM_t *vm = NULL;
  MVM_MemReqs_t requirements;
  MpnImageSource_t source;
  SmokeContext context;
  MVM_Config_t parentConfig = MVM_Config;
  MpnDevProfile_t parentProfile = *MVM_Config.device_profile;
  long imageSize = 0;
  uint32_t executedSteps = 0U;
  int result = 1;

  if (argc != 2)
  {
    return 2;
  }

  parentProfile.name = "PARENT_OWNED";
  parentProfile.screen_width = 96U;
  parentConfig.device_profiles = &parentProfile;
  parentConfig.device_profile = &parentProfile;
  parentConfig.image_read = NULL;
  parentConfig.drivers.image_read = read_image;
  parentConfig.drivers.display_flush = flush_display;
  parentConfig.drivers.user = &context;
  context.file = NULL;
  context.flushCount = 0U;

  if (MVM_GetDevProfileCountWithConfig(&parentConfig) != 1U ||
      MVM_GetDevProfileWithConfig(&parentConfig, 0U) != &parentProfile ||
      MVM_FindDevProfileByNameWithConfig(&parentConfig, "PARENT_OWNED") != &parentProfile ||
      MVM_GetDevProfileCount() != 1U || MVM_GetDevProfile(0U)->screen_width != 64U)
  {
    return 3;
  }

  file = fopen(argv[1], "rb");
  if (!file || fseek(file, 0L, SEEK_END) != 0)
  {
    goto cleanup;
  }

  imageSize = ftell(file);
  if (imageSize <= 0L || fseek(file, 0L, SEEK_SET) != 0)
  {
    goto cleanup;
  }

  image = (uint8_t *)malloc((size_t)imageSize);
  storage = malloc(MVM_GetStorageSize());
  if (!image || !storage || fread(image, 1U, (size_t)imageSize, file) != (size_t)imageSize)
  {
    goto cleanup;
  }

  vm = MVM_GetVmFromStorage(storage, MVM_GetStorageSize());
  context.file = file;
  source.user = &context;
  source.image_size = (size_t)imageSize;
  source.path = argv[1];

  if (!vm || MVM_QueryMemReqsWithConfig(image, (size_t)imageSize, &parentConfig, &requirements) != MVM_OK ||
      MVM_QueryMemReqsFromSourceWithConfig(&source, &parentConfig, &requirements) != MVM_OK ||
      requirements.runtime_pool_bytes > parentConfig.runtime_pool_size ||
      MVM_InitFromSourceWithConfig(vm, &source, "PARENT_OWNED", &parentConfig) != MVM_OK)
  {
    goto cleanup;
  }

  if (MVM_RunSteps(vm, 2000000U, &executedSteps) != MVM_OK || executedSteps == 0U || context.flushCount == 0U)
  {
    goto cleanup;
  }

  MVM_Free(vm);
  vm = NULL;
  result = 0;

cleanup:
  if (vm)
  {
    MVM_Free(vm);
  }

  if (file)
  {
    fclose(file);
  }

  free(storage);
  free(image);
  return result;
}
