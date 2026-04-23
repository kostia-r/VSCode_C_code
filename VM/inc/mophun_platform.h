#ifndef MOPHUN_PLATFORM_H
#define MOPHUN_PLATFORM_H

#include <stddef.h>
#include <stdint.h>

typedef struct MophunPlatform
{
  void *user;
  void *(*calloc)(void *user, size_t count, size_t size);
  void (*free)(void *user, void *ptr);
  uint32_t (*get_ticks_ms)(void *user);
  uint32_t (*get_random)(void *user);
  int (*log)(void *user, const char *message);
} MophunPlatform;

#endif
