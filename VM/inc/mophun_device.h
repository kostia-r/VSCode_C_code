#ifndef MOPHUN_DEVICE_H
#define MOPHUN_DEVICE_H

#include <stdint.h>

typedef struct MophunDeviceProfile
{
  uint16_t screen_width;
  uint16_t screen_height;
  uint16_t color_mode;
  uint16_t sound_flags;
  uint16_t system_flags;
  uint32_t device_id;
} MophunDeviceProfile;

#endif
