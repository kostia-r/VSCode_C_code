#include "FileApi.h"

#include <io.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#define FILE_API_PATH_SIZE                                     (512U)
#define FILE_API_STEM_SIZE                                     (32U)

static int FileApi_lBuildNamedPath(const char *image_path,
                                   const char *name,
                                   uint32_t candidate,
                                   char *path,
                                   size_t path_size);
static int FileApi_lOpen(void *context,
                         const char *image_path,
                         const char *name,
                         uint32_t flags,
                         MVM_FileHandle_t *handle,
                         size_t *size);
static int FileApi_lRead(void *context,
                         MVM_FileHandle_t handle,
                         size_t offset,
                         void *dst,
                         size_t size,
                         size_t *read_size);
static int FileApi_lWrite(void *context,
                          MVM_FileHandle_t handle,
                          size_t offset,
                          const void *src,
                          size_t size,
                          size_t *written_size);
static int FileApi_lResize(void *context, MVM_FileHandle_t handle, size_t size);
static int FileApi_lClose(void *context, MVM_FileHandle_t handle);
static int FileApi_lRemove(void *context, const char *image_path, const char *name);

const MVM_FileApi_t FileApi =
{
  .context = NULL,
  .open = FileApi_lOpen,
  .read = FileApi_lRead,
  .write = FileApi_lWrite,
  .resize = FileApi_lResize,
  .close = FileApi_lClose,
  .remove = FileApi_lRemove
};

static int FileApi_lBuildNamedPath(const char *image_path,
                                   const char *name,
                                   uint32_t candidate,
                                   char *path,
                                   size_t path_size)
{
  const char *cursor;
  const char *file_name;
  char root[FILE_API_PATH_SIZE];
  char stem[FILE_API_STEM_SIZE];
  size_t root_length;
  size_t stem_length;
  int written;

  if (!image_path || !name || !path || strchr(name, '/') || strchr(name, '\\') || strstr(name, ".."))
  {
    return -1;
  }

  root_length = 0U;
  for (cursor = image_path; *cursor != '\0'; ++cursor)
  {
    if (*cursor == '/' || *cursor == '\\')
    {
      root_length = (size_t)(cursor - image_path) + 1U;
    }
  }

  if (root_length >= sizeof(root))
  {
    return -1;
  }

  memcpy(root, image_path, root_length);
  root[root_length] = '\0';
  file_name = image_path + root_length;
  stem_length = 0U;
  while (stem_length + 1U < sizeof(stem) &&
         file_name[stem_length] != '\0' &&
         file_name[stem_length] != '_' &&
         file_name[stem_length] != '.')
  {
    stem[stem_length] = file_name[stem_length];
    ++stem_length;
  }
  while (stem_length > 0U && stem[stem_length - 1U] >= '0' && stem[stem_length - 1U] <= '9')
  {
    --stem_length;
  }
  stem[stem_length] = '\0';

  if (candidate == 0U)
  {
    written = snprintf(path, path_size, "%s%s", root, name);
  }
  else if (candidate == 1U)
  {
    written = snprintf(path, path_size, "%s%s.mpc", root, name);
  }
  else
  {
    written = snprintf(path, path_size, "%s%s_%s.mpc", root, stem, name);
  }

  return written >= 0 && (size_t)written < path_size ? 0 : -1;
}

static int FileApi_lOpen(void *context,
                         const char *image_path,
                         const char *name,
                         uint32_t flags,
                         MVM_FileHandle_t *handle,
                         size_t *size)
{
  char path[FILE_API_PATH_SIZE];
  const char *mode;
  FILE *file;
  long file_size;
  uint32_t candidate;

  (void)context;
  if (!image_path || !handle || !size)
  {
    return -1;
  }

  mode = (flags & MVM_FILE_OPEN_TRUNCATE) ? "wb+" :
         ((flags & MVM_FILE_OPEN_WRITE) ? "rb+" : "rb");
  file = NULL;

  if (!name)
  {
    file = fopen(image_path, mode);
    if (!file && (flags & MVM_FILE_OPEN_WRITE))
    {
      file = fopen(image_path, "rb");
    }
  }
  else
  {
    for (candidate = 0U; candidate < 3U && !file; ++candidate)
    {
      if (FileApi_lBuildNamedPath(image_path, name, candidate, path, sizeof(path)) == 0)
      {
        file = fopen(path, mode);
      }
    }
    if (!file && (flags & MVM_FILE_OPEN_CREATE) &&
        FileApi_lBuildNamedPath(image_path, name, 0U, path, sizeof(path)) == 0)
    {
      file = fopen(path, "wb+");
    }
  }

  if (!file || fseek(file, 0L, SEEK_END) != 0)
  {
    if (file)
    {
      fclose(file);
    }
    return -1;
  }

  file_size = ftell(file);
  if (file_size < 0)
  {
    fclose(file);
    return -1;
  }

  *handle = (MVM_FileHandle_t)(uintptr_t)file;
  *size = (size_t)file_size;
  return 0;
}

static int FileApi_lRead(void *context,
                         MVM_FileHandle_t handle,
                         size_t offset,
                         void *dst,
                         size_t size,
                         size_t *read_size)
{
  FILE *file;

  (void)context;
  file = (FILE *)(uintptr_t)handle;
  if (!file || !dst || !read_size || offset > (size_t)LONG_MAX ||
      fseek(file, (long)offset, SEEK_SET) != 0)
  {
    return -1;
  }

  *read_size = fread(dst, 1U, size, file);
  return ferror(file) ? -1 : 0;
}

static int FileApi_lWrite(void *context,
                          MVM_FileHandle_t handle,
                          size_t offset,
                          const void *src,
                          size_t size,
                          size_t *written_size)
{
  FILE *file;

  (void)context;
  file = (FILE *)(uintptr_t)handle;
  if (!file || !src || !written_size || offset > (size_t)LONG_MAX ||
      fseek(file, (long)offset, SEEK_SET) != 0)
  {
    return -1;
  }

  *written_size = fwrite(src, 1U, size, file);
  return *written_size == size && fflush(file) == 0 ? 0 : -1;
}

static int FileApi_lResize(void *context, MVM_FileHandle_t handle, size_t size)
{
  FILE *file;

  (void)context;
  file = (FILE *)(uintptr_t)handle;
  return file && size <= (size_t)INT64_MAX ? _chsize_s(_fileno(file), (__int64)size) : -1;
}

static int FileApi_lClose(void *context, MVM_FileHandle_t handle)
{
  (void)context;
  return handle != MVM_FILE_INVALID_HANDLE ? fclose((FILE *)(uintptr_t)handle) : -1;
}

static int FileApi_lRemove(void *context, const char *image_path, const char *name)
{
  char path[FILE_API_PATH_SIZE];

  (void)context;
  return FileApi_lBuildNamedPath(image_path, name, 0U, path, sizeof(path)) == 0 ? remove(path) : -1;
}
