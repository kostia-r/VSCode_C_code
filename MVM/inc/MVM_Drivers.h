/**********************************************************************************************************************
 *  FILE DESCRIPTION
 *  -------------------------------------------------------------------------------------------------------------------
 *          Project:  Mophun
 *             File:  MVM_Drivers.h
 *           Module:  MVM_Inc
 *           Target:  Portable C
 *      Description:  Small hardware-oriented callback contract for MCU and parent-project ports.
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  Header file guard
 *********************************************************************************************************************/

#ifndef MVM_DRIVERS_H
#define MVM_DRIVERS_H

/**********************************************************************************************************************
 *  INCLUDES
 *********************************************************************************************************************/

#include "MVM_Types.h"

/**********************************************************************************************************************
 *  GLOBAL MACROS
 *********************************************************************************************************************/

#define MVM_BUTTON_UP_MASK                                      (0x00000001UL)
#define MVM_BUTTON_DOWN_MASK                                    (0x00000002UL)
#define MVM_BUTTON_LEFT_MASK                                    (0x00000004UL)
#define MVM_BUTTON_RIGHT_MASK                                   (0x00000008UL)
#define MVM_BUTTON_FIRE_MASK                                    (0x00000010UL)
#define MVM_BUTTON_SELECT_MASK                                  (0x00000020UL)
#define MVM_BUTTON_FIRE2_MASK                                   (0x00000100UL)

/**********************************************************************************************************************
 *  GLOBAL DATA TYPES AND STRUCTURES
 *********************************************************************************************************************/

/** @brief Describes pixels passed to a display flush callback. */
typedef enum MVM_PixelFormat_t
{
  MVM_PIXEL_FORMAT_RGB565 = 0, /**< Packed 16-bit RGB565 pixels in native byte order. */
  MVM_PIXEL_FORMAT_RGBA8888    /**< Packed 32-bit red, green, blue, alpha pixels. */
} MVM_PixelFormat_t;

/** @brief Describes one dirty display rectangle in pixels. */
typedef struct MVM_DirtyRect_t
{
  uint16_t x;      /**< Left edge in framebuffer coordinates. */
  uint16_t y;      /**< Top edge in framebuffer coordinates. */
  uint16_t width;  /**< Dirty width in pixels. */
  uint16_t height; /**< Dirty height in pixels. */
} MVM_DirtyRect_t;

/** @brief Describes one VM-owned framebuffer view valid only during the flush callback. */
typedef struct MVM_Framebuffer_t
{
  const void *pixels;              /**< Read-only pixel data; the driver must not retain this pointer. */
  uint16_t width;                  /**< Framebuffer width in pixels. */
  uint16_t height;                 /**< Framebuffer height in pixels. */
  uint32_t stride_bytes;           /**< Distance between adjacent rows in bytes. */
  MVM_PixelFormat_t pixel_format;  /**< Pixel encoding used by this framebuffer. */
  MVM_DirtyRect_t dirty_rect;      /**< Region requiring transfer; zero size means the complete frame. */
} MVM_Framebuffer_t;

/** @brief Describes normalized signed 16-bit PCM passed to an audio driver. */
typedef struct MVM_PcmBuffer_t
{
  const int16_t *samples; /**< Interleaved samples valid only during the callback. */
  uint32_t frame_count;   /**< Number of sample frames, not individual channel samples. */
  uint32_t sample_rate;   /**< Sample rate in Hz. */
  uint8_t channel_count;  /**< Interleaved channel count. */
} MVM_PcmBuffer_t;

/**
 * @brief Groups optional hardware-oriented drivers under one parent-owned context.
 *
 * Zero initialization is valid. All callbacks must be non-blocking unless the
 * parent integration explicitly schedules them outside the VM execution task.
 */
typedef struct MVM_Drivers_t
{
  void *user; /**< Opaque parent context passed to every callback below. */
  int (*display_flush)(void *user,
                       const MVM_Framebuffer_t *framebuffer); /**< Transfers one normalized framebuffer. */
  uint32_t (*input_get_buttons)(void *user); /**< Polls MVM_BUTTON_* values; NULL keeps host-injected state. */
  int (*audio_queue_pcm)(void *user, const MVM_PcmBuffer_t *pcm); /**< Queues normalized PCM; NULL disables audio. */
  void (*audio_stop)(void *user); /**< Stops queued audio; NULL is a valid no-op. */
  uint32_t (*get_ticks_ms)(void *user); /**< Returns monotonic milliseconds; NULL uses VM timing fallback. */
  uint32_t (*get_random)(void *user); /**< Returns one random value; NULL uses the deterministic VM PRNG. */
  int (*log)(void *user,
             MVM_LogLevel_t level,
             const char *module,
             const char *event,
             const char *message); /**< Receives diagnostics; NULL uses the configured/default logger. */
  void (*event)(void *user,
                MVM_Event_t event,
                uint32_t arg0,
                uint32_t arg1); /**< Receives structured events; NULL discards them. */
  MpnImageReadFn_t image_read;   /**< Reads the selected image; required for source mode if config read is NULL. */
  MpnImageWriteFn_t image_write; /**< Writes the selected image; NULL makes image storage read-only. */
  int (*persistent_read)(void *user,
                         const char *key,
                         size_t offset,
                         void *dst,
                         size_t size); /**< Reads an optional persistent record. */
  int (*persistent_write)(void *user,
                          const char *key,
                          size_t offset,
                          const void *src,
                          size_t size); /**< Optional record write; NULL disables external persistence. */
  uint32_t (*system_message)(void *user,
                             uint32_t flags,
                             const char *title,
                             const char *message); /**< Handles normalized UTF-8 message text; NULL acknowledges it. */
} MVM_Drivers_t;

/**********************************************************************************************************************
 *  END of header file guard
 *********************************************************************************************************************/

#endif

/**********************************************************************************************************************
 *  END OF FILE MVM_Drivers.h
 *********************************************************************************************************************/
