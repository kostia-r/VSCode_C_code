/**********************************************************************************************************************
 *  FILE DESCRIPTION
 *  -------------------------------------------------------------------------------------------------------------------
 *          Project:  Mophun
 *             File:  MVM_Render.h
 *           Module:  MVM_Runtime
 *           Target:  Portable C
 *      Description:  VMGP draw-command replay API using host-provided drawing primitives.
 *********************************************************************************************************************/

#ifndef MVM_RENDER_H
#define MVM_RENDER_H

#include "MVM_Types.h"
#include <stdint.h>

typedef struct MVM_RenderBackend_t
{
  void *user;
  void (*set_clip)(void *user, int enabled, int32_t x, int32_t y, int32_t width, int32_t height);
  void (*draw_point)(void *user, int32_t x, int32_t y, uint8_t red, uint8_t green, uint8_t blue);
  void (*draw_line)(void *user,
                    int32_t x0,
                    int32_t y0,
                    int32_t x1,
                    int32_t y1,
                    uint8_t red,
                    uint8_t green,
                    uint8_t blue);
  void (*fill_rect)(void *user,
                    int32_t x,
                    int32_t y,
                    int32_t width,
                    int32_t height,
                    uint8_t red,
                    uint8_t green,
                    uint8_t blue);
} MVM_RenderBackend_t;

typedef struct MVM_RenderFrameInfo_t
{
  uint32_t clear_color;
  uint32_t clear_serial;
  uint32_t draw_command_count;
  uint32_t frame_serial;
} MVM_RenderFrameInfo_t;

int MVM_RenderGetFrameInfo(const MpnVM_t *vm, MVM_RenderFrameInfo_t *info);
uint32_t MVM_RenderReplayCommands(MpnVM_t *vm, const MVM_RenderBackend_t *backend, uint32_t first_command);
void MVM_RenderConsumeCommands(MpnVM_t *vm);

#endif
