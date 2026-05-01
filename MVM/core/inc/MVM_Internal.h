/**********************************************************************************************************************
 *  FILE DESCRIPTION
 *  -------------------------------------------------------------------------------------------------------------------
 *          Project:  Mophun
 *             File:  MVM_Internal.h
 *           Module:  MVM_Core
 *           Target:  Portable C
 *      Description:  Internal VM context, shared constants, and cross-module helper declarations.
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  Header file guard
 *********************************************************************************************************************/

#ifndef MVM_INTERNAL_H
#define MVM_INTERNAL_H

/**********************************************************************************************************************
 *  INCLUDES
 *********************************************************************************************************************/

#include "MVM_Cfg.h"
#include "MVM_BuildCfg.h"
#include "MVM_Imports.h"
#include "MVM_VmgpDebug.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**********************************************************************************************************************
 *  GLOBAL MACROS
 *********************************************************************************************************************/

#define VMGP_MAX_REGS                                           (32U)
#define VMGP_MAX_STREAMS                                        (16U)
#define VMGP_MAX_SPRITE_SLOTS                                   (255U)
#define VMGP_MAX_DRAW_COMMANDS                                  (2048U)
#define VMGP_DRAW_TEXT_SNAPSHOT_BYTES                           (64U)
#define VM_STACK_EXTRA                                          (64U * 1024U)
#define VM_HEAP_EXTRA                                           (128U * 1024U)
#define MVM_U32_DEFAULT_WATCHDOG_LIMIT                          (0U)

/**********************************************************************************************************************
 *  GLOBAL DATA TYPES AND STRUCTURES
 *********************************************************************************************************************/

/**
 * @brief Defines the PIP2 register ABI indices.
 */
enum
{
  VM_REG_ZERO = 0,
  VM_REG_SP = 1,
  VM_REG_RA = 2,
  VM_REG_FP = 3,
  VM_REG_P0 = 12,
  VM_REG_P1 = 13,
  VM_REG_P2 = 14,
  VM_REG_P3 = 15,
  VM_REG_R0 = 30,
  VM_REG_R1 = 31,
};

/**
 * @brief Tracks one open VM resource stream.
 */
typedef struct VMGPStream
{
  bool used;                /**< Indicates whether the slot is active. */
  uint32_t handle;          /**< External stream handle. */
  size_t file_offset;       /**< Backing image offset of the stream data. */
  uint8_t *overlay_data;    /**< Optional writable session-local data overlay. */
  uint32_t size;            /**< Stream size in bytes. */
  uint32_t overlay_size;    /**< Capacity of the writable overlay in bytes. */
  uint32_t pos;             /**< Current read position in bytes. */
  uint32_t resource_id;     /**< Backing resource identifier. */
  uint32_t mode;            /**< Open mode flags passed by the guest. */
} VMGPStream;

/**
 * @brief Tracks one configured sprite slot.
 */
typedef struct VMGPSpriteSlot
{
  bool used;                /**< Indicates whether the slot currently references one sprite. */
  uint32_t sprite_addr;     /**< Guest pointer to one SPRITE header. */
  int16_t x;                /**< Horizontal sprite position in pixels. */
  int16_t y;                /**< Vertical sprite position in pixels. */
} VMGPSpriteSlot;

/**
 * @brief Stores one decoded tilemap state snapshot.
 */
typedef struct VMGPMapState
{
  bool valid;               /**< Indicates whether one tilemap is currently active. */
  uint8_t flags;            /**< Tilemap flags copied from the guest header. */
  uint8_t format;           /**< Tilemap format copied from the guest header. */
  uint8_t width;            /**< Tile count along the X axis. */
  uint8_t height;           /**< Tile count along the Y axis. */
  uint8_t animation_speed;  /**< Animation speed copied from the guest header. */
  uint8_t animation_count;  /**< Number of animation frames in the guest header. */
  uint8_t animation_active; /**< Active animation frame index. */
  int16_t x_pan;            /**< Horizontal tilemap pan value. */
  int16_t y_pan;            /**< Vertical tilemap pan value. */
  int16_t x_pos;            /**< Horizontal screen offset in pixels. */
  int16_t y_pos;            /**< Vertical screen offset in pixels. */
  uint32_t header_addr;     /**< Guest pointer to the original MAP_HEADER block. */
  uint32_t map_data_addr;   /**< Guest pointer to the tile data array. */
  uint32_t tile_data_addr;  /**< Guest pointer to the tile sprite atlas data. */
} VMGPMapState;

/**
 * @brief Identifies one deferred draw-command kind for the simple backend path.
 */
typedef enum MVM_DrawCommandType_t
{
  MVM_DRAW_FILL_RECT = 0,
  MVM_DRAW_LINE = 1,
  MVM_DRAW_SPRITE = 2,
  MVM_DRAW_TEXT = 3
} MVM_DrawCommandType_t;

/**
 * @brief Stores one deferred draw command emitted by graphics imports.
 */
typedef struct MVM_DrawCommand_t
{
  MVM_DrawCommandType_t type;  /**< Command kind to interpret. */
  int16_t x0;                  /**< Primary x coordinate. */
  int16_t y0;                  /**< Primary y coordinate. */
  int16_t x1;                  /**< Secondary x coordinate or width surrogate. */
  int16_t y1;                  /**< Secondary y coordinate or height surrogate. */
  uint16_t width;              /**< Width for sprite/text placeholder rendering. */
  uint16_t height;             /**< Height for sprite/text placeholder rendering. */
  uint32_t color;              /**< Guest-encoded foreground color. */
  uint32_t aux;                /**< Guest pointer or extra metadata. */
  uint32_t aux2;               /**< Secondary guest pointer or extra metadata. */
  uint16_t text_length;         /**< Captured text byte count for deferred text commands. */
  uint8_t text[VMGP_DRAW_TEXT_SNAPSHOT_BYTES]; /**< Captured text bytes for deferred text commands. */
} MVM_DrawCommand_t;

/**
 * @brief Stores the complete mutable VM execution context.
 */
struct MpnVM_t
{
  MpnPlatform_t platform;          /**< Host integration callbacks. */
  const MpnDevProfile_t *device_profile; /**< Selected device profile exposed to platform wrappers. */

  MpnImageSource_t image;           /**< Active VM image source descriptor. */
  MpnImageReadFn_t image_read;      /**< Configured image-backend range-read callback. */
  MpnImageMapFn_t image_map;        /**< Configured image-backend map callback. */
  MpnImageUnmapFn_t image_unmap;    /**< Configured image-backend unmap callback. */
  size_t size;                      /**< Total VM image size in bytes. */
  uint8_t *strtab;                  /**< Cached string table used for import lookup. */

  VMGPHeader header;                /**< Parsed VMGP file header. */
  bool header_valid;                /**< Indicates whether the header parsed successfully. */

  uint32_t code_offset;             /**< VM memory offset of the code section. */
  uint32_t code_file_offset;        /**< File offset of the code section. */
  uint32_t data_offset;             /**< VM memory offset of the data section. */
  uint32_t data_file_offset;        /**< File offset of the data section. */
  uint32_t bss_offset;              /**< VM memory offset of the BSS section. */
  uint32_t res_offset;              /**< VM memory offset of the resource section. */
  uint32_t res_file_offset;         /**< File offset of the resource section. */
  uint32_t pool_offset;             /**< File offset of the constant-pool section. */
  uint32_t strtab_offset;           /**< File offset of the string table. */
  uint32_t vm_end;                  /**< End offset of initialized VM memory. */

  VMGPPoolEntry *pool;              /**< Loaded constant-pool entries. */
  VMGPResource *resources;          /**< Loaded resource metadata table. */
  uint32_t resource_count;          /**< Number of loaded resources. */

  uint8_t *runtime_pool;            /**< Backing runtime arena used during init. */
  size_t runtime_pool_size;         /**< Total runtime arena size in bytes. */
  size_t runtime_pool_used;         /**< Number of runtime arena bytes consumed. */

  uint8_t *mem;                     /**< Backing VM memory buffer. */
  size_t mem_size;                  /**< Total VM memory size in bytes. */
  uint32_t heap_base;               /**< Start offset of the VM heap. */
  uint32_t heap_cur;                /**< Current heap allocation cursor. */
  uint32_t heap_limit;              /**< End offset of the VM heap. */
  uint32_t stack_top;               /**< Initial stack top offset. */

  VMGPStream streams[VMGP_MAX_STREAMS]; /**< Open stream table. */
  uint32_t next_stream_handle;      /**< Next stream handle to allocate. */
  uint32_t active_font;             /**< Guest pointer to the currently selected font. */
  uint32_t previous_font;           /**< Guest pointer to the previously selected font. */
  uint32_t fg_color;                /**< Current foreground color in guest encoding. */
  uint32_t bg_color;                /**< Current background color in guest encoding. */
  uint32_t transfer_mode;           /**< Current blit/text transfer mode. */
  uint16_t clip_x0;                 /**< Current clip window left edge. */
  uint16_t clip_y0;                 /**< Current clip window top edge. */
  uint16_t clip_x1;                 /**< Current clip window right edge. */
  uint16_t clip_y1;                 /**< Current clip window bottom edge. */
  uint32_t palette_entries[256];    /**< Current palette state in guest encoding. */
  uint32_t clear_color;             /**< Last clear-screen color. */
  MVM_DrawCommand_t draw_commands[VMGP_MAX_DRAW_COMMANDS]; /**< Deferred draw commands for the current frame. */
  uint32_t draw_command_count;      /**< Number of deferred draw commands currently stored. */
  uint32_t frame_serial;            /**< Monotonic frame-present counter bumped by vFlipScreen. */
  uint32_t button_state;            /**< Current polled button bit-mask. */
  VMGPSpriteSlot sprite_slots[VMGP_MAX_SPRITE_SLOTS]; /**< Current sprite-slot table. */
  uint32_t sprite_slot_count;       /**< Number of sprite slots configured by the guest. */
  VMGPMapState map_state;           /**< Current tilemap state used by map imports. */

  uint32_t regs[VMGP_MAX_REGS];     /**< Architectural register file. */
  uint32_t pc;                      /**< Program counter. */
  uint32_t steps;                   /**< Executed instruction count. */
  uint32_t logged_calls;            /**< Number of traced calls already logged. */
  uint32_t tick_count;              /**< Cached tick counter value. */
  uint32_t random_state;            /**< Internal pseudo-random state. */
  uint32_t last_pc;                 /**< Previous PC value used by the watchdog. */
  uint32_t no_progress_steps;       /**< Consecutive steps without PC progress. */
  uint32_t watchdog_limit;          /**< Allowed no-progress step budget. */
  bool halted;                      /**< Indicates that execution has stopped. */
  MVM_State_t state;              /**< Current VM execution state. */
  MVM_Err_t last_error;         /**< Last fatal execution error. */

  const MpnSyscall_t *syscalls;    /**< Registered host syscall table. */
  uint32_t syscall_count;           /**< Number of registered host syscalls. */
};

/**
 * @brief Aliases the public VM type as the internal runtime context.
 */
typedef MpnVM_t VMGPContext;

#include "MVM_Log.h"

/**********************************************************************************************************************
 *  GLOBAL FUNCTIONS PROTOTYPES
 *********************************************************************************************************************/

/**
 * @brief Reads a little-endian 16-bit value from VM memory.
 */
static inline uint16_t vm_read_u16_le(const uint8_t *p);

/**
 * @brief Reads a little-endian 32-bit value from VM memory.
 */
static inline uint32_t vm_read_u32_le(const uint8_t *p);

/**
 * @brief Writes a little-endian 32-bit value to VM memory.
 */
static inline void vm_write_u32_le(uint8_t *p, uint32_t v);

/**
 * @brief Writes a little-endian 16-bit value to VM memory.
 */
static inline void vm_write_u16_le(uint8_t *p, uint16_t v);

/**
 * @brief Decodes a PIP register index.
 */
static inline uint32_t vm_reg_index(uint8_t encoded);

/**
 * @brief Extracts an unsigned 24-bit immediate value.
 */
static inline uint32_t vm_imm24_u(uint32_t v);

/**
 * @brief Sign-extends a 24-bit immediate value.
 */
static inline int32_t vm_sext24(uint32_t v);

/**
 * @brief Interprets a VM register value as signed 32-bit.
 */
static inline int32_t vm_reg_s32(uint32_t v);

/**
 * @brief Aligns a value to a 4-byte boundary.
 */
static inline uint32_t vm_align4(uint32_t v);

/**
 * @brief Handles VMGP pool data.
 */
uint32_t MVM_ResolveVmgpPoolValue(const VMGPContext *ctx, const VMGPPoolEntry *entry);

/**
 * @brief Returns VMGP resource metadata.
 */
const VMGPResource *MVM_GetVmgpResource(const VMGPContext *ctx, uint32_t resource_id);

/**
 * @brief Parses one VMGP header through the internal loader path.
 */
bool MVM_ParseVmgpHeaderRaw(VMGPContext *ctx);

/**
 * @brief Loads one VMGP pool through the internal loader path.
 */
bool MVM_LoadVmgpPoolRaw(VMGPContext *ctx);

/**
 * @brief Reads one byte range from the active VM image source.
 */
bool MVM_ReadImageRange(const VMGPContext *ctx, size_t offset, void *dst, size_t size);

/**
 * @brief Traces one VM memory write.
 */
void MVM_WatchMemoryWrite(const VMGPContext *ctx, uint32_t addr, uint32_t size, const char *tag);

/**
 * @brief Handles a VM runtime import group.
 */
bool MVM_HandleRuntimeImportCall(VMGPContext *ctx, uint32_t pool_index);

/**
 * @brief Initializes VM state with one integration config object.
 */
bool MVM_InitRawWithConfig(VMGPContext *ctx, const MpnImageSource_t *image, const MVM_Config_t *config);

/**
 * @brief Releases VM resources.
 */
void MVM_FreeRaw(VMGPContext *ctx);

/**
 * @brief Executes one VM instruction.
 */
bool MVM_PipStep(VMGPContext *ctx);

/**
 * @brief Acquires one initialization buffer from the runtime arena.
 */
void *MVM_AcquireInitBuffer(VMGPContext *ctx, size_t required_size);

/**
 * @brief Executes up to one internal VM instruction budget.
 */
uint32_t MVM_RunStepsRaw(VMGPContext *ctx, uint32_t max_steps);

/**
 * @brief Emits one structured VM event through the platform hook.
 */
void MVM_EmitEvent(const VMGPContext *ctx, MVM_Event_t event, uint32_t arg0, uint32_t arg1);

/**
 * @brief Updates the current VM execution state.
 */
void MVM_SetStateRaw(VMGPContext *ctx, MVM_State_t state);

/**
 * @brief Records the last fatal execution error and moves the VM into error state.
 */
void MVM_SetErrorRaw(VMGPContext *ctx, MVM_Err_t error);

/**
 * @brief Requests immediate VM termination through the internal control path.
 */
void MVM_RequestExitRaw(VMGPContext *ctx);

/**
 * @brief Checks one VM memory range.
 */
bool MVM_RuntimeMemRangeOk(const VMGPContext *ctx, uint32_t addr, uint32_t size);

/**
 * @brief Measures one bounded VM string length.
 */
uint32_t MVM_RuntimeStrLen(const uint8_t *s, size_t max_len);

/**********************************************************************************************************************
 *  GLOBAL INLINE FUNCTIONS
 *********************************************************************************************************************/

/**
 * @brief Reads a little-endian 16-bit value from VM memory.
 */
static inline uint16_t vm_read_u16_le(const uint8_t *p)
{
  return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
} /* End of vm_read_u16_le */

/**
 * @brief Reads a little-endian 32-bit value from VM memory.
 */
static inline uint32_t vm_read_u32_le(const uint8_t *p)
{
  return (uint32_t)p[0] |
  ((uint32_t)p[1] << 8) |
  ((uint32_t)p[2] << 16) |
  ((uint32_t)p[3] << 24);
} /* End of vm_read_u32_le */

/**
 * @brief Writes a little-endian 32-bit value to VM memory.
 */
static inline void vm_write_u32_le(uint8_t *p, uint32_t v)
{
  p[0] = (uint8_t)(v & 0xFFu);
  p[1] = (uint8_t)((v >> 8) & 0xFFu);
  p[2] = (uint8_t)((v >> 16) & 0xFFu);
  p[3] = (uint8_t)((v >> 24) & 0xFFu);
} /* End of vm_write_u32_le */

/**
 * @brief Writes a little-endian 16-bit value to VM memory.
 */
static inline void vm_write_u16_le(uint8_t *p, uint16_t v)
{
  p[0] = (uint8_t)(v & 0xFFu);
  p[1] = (uint8_t)((v >> 8) & 0xFFu);
} /* End of vm_write_u16_le */

/**
 * @brief Decodes a PIP register index.
 */
static inline uint32_t vm_reg_index(uint8_t encoded)
{
  return (uint32_t)(encoded >> 2);
} /* End of vm_reg_index */

/**
 * @brief Extracts an unsigned 24-bit immediate value.
 */
static inline uint32_t vm_imm24_u(uint32_t v)
{
  return v & 0x00FFFFFFu;
} /* End of vm_imm24_u */

/**
 * @brief Sign-extends a 24-bit immediate value.
 */
static inline int32_t vm_sext24(uint32_t v)
{
  v &= 0x00FFFFFFu;

  if (v & 0x00800000u)
  {
    v |= 0xFF000000u;
  }
  return (int32_t)v;
} /* End of vm_sext24 */

/**
 * @brief Interprets a VM register value as signed 32-bit.
 */
static inline int32_t vm_reg_s32(uint32_t v)
{
  return (int32_t)v;
} /* End of vm_reg_s32 */

/**
 * @brief Aligns a value to a 4-byte boundary.
 */
static inline uint32_t vm_align4(uint32_t v)
{
  return (v + 3u) & ~3u;
} /* End of vm_align4 */

/**********************************************************************************************************************
 *  END of header file guard
 *********************************************************************************************************************/

#endif

/**********************************************************************************************************************
 *  END OF FILE MVM_Internal.h
 *********************************************************************************************************************/
