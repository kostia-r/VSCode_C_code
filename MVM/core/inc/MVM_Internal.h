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

#include "MVM_Vm.h"
#include "MVM_Syscalls.h"
#include "MVM_VmgpDebug.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**********************************************************************************************************************
 *  GLOBAL MACROS
 *********************************************************************************************************************/

#define VMGP_MAX_REGS                                           (32U)
#define VMGP_MAX_STREAMS                                        (16U)
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
  uint32_t base;            /**< VM memory base address of the stream data. */
  uint32_t size;            /**< Stream size in bytes. */
  uint32_t pos;             /**< Current read position in bytes. */
  uint32_t resource_id;     /**< Backing resource identifier. */
} VMGPStream;

/**
 * @brief Stores the complete mutable VM execution context.
 */
struct MophunVM
{
  MophunPlatform platform;           /**< Host integration callbacks. */

  const uint8_t *data;              /**< Pointer to the loaded VM image. */
  size_t size;                      /**< Loaded VM image size in bytes. */

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

  uint8_t *mem;                     /**< Backing VM memory buffer. */
  size_t mem_size;                  /**< Total VM memory size in bytes. */
  uint32_t heap_base;               /**< Start offset of the VM heap. */
  uint32_t heap_cur;                /**< Current heap allocation cursor. */
  uint32_t heap_limit;              /**< End offset of the VM heap. */
  uint32_t stack_top;               /**< Initial stack top offset. */

  VMGPStream streams[VMGP_MAX_STREAMS]; /**< Open stream table. */
  uint32_t next_stream_handle;      /**< Next stream handle to allocate. */

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
  MVM_tenuState state;              /**< Current VM execution state. */
  MVM_tenuError last_error;         /**< Last fatal execution error. */

  const MophunSyscall *syscalls;    /**< Registered host syscall table. */
  uint32_t syscall_count;           /**< Number of registered host syscalls. */
};

/**
 * @brief Aliases the public VM type as the internal runtime context.
 */
typedef MophunVM VMGPContext;

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
uint32_t MVM_u32VmgpResolvePoolValue(const VMGPContext *ctx, const VMGPPoolEntry *entry);

/**
 * @brief Returns VMGP resource metadata.
 */
const VMGPResource *MVM_pudtVmgpGetResource(const VMGPContext *ctx, uint32_t resource_id);

/**
 * @brief Provides MVM_vidMemoryWriteWatch API.
 */
void MVM_vidMemoryWriteWatch(const VMGPContext *ctx, uint32_t addr, uint32_t size, const char *tag);

/**
 * @brief Handles a VM runtime import group.
 */
bool MVM_bRuntimeHandleImportCall(VMGPContext *ctx, uint32_t pool_index);

/**
 * @brief Initializes VM state.
 */
bool MVM_LbInitRaw(VMGPContext *ctx, const uint8_t *data, size_t size);

/**
 * @brief Initializes VM state.
 */
bool MVM_LbInitRawWithPlatform(VMGPContext *ctx,
const uint8_t *data,
size_t size,
const MophunPlatform *platform);

/**
 * @brief Releases VM resources.
 */
void MVM_LvidFreeRaw(VMGPContext *ctx);

/**
 * @brief Runs traced VM execution.
 */
bool MVM_LbRunTrace(VMGPContext *ctx, uint32_t max_steps, uint32_t max_logged_calls);

/**
 * @brief Provides MVM_LbPipStep API.
 */
bool MVM_LbPipStep(VMGPContext *ctx);

/**
 * @brief Provides MVM_LpudtCalloc API.
 */
void *MVM_LpudtCalloc(VMGPContext *ctx, size_t count, size_t size);

/**
 * @brief Releases VM resources.
 */
void MVM_LvidFreeMem(VMGPContext *ctx, void *ptr);

/**
 * @brief Provides MVM_LvidLogf API.
 */
void MVM_LvidLogf(const VMGPContext *ctx, const char *fmt, ...);

/**
 * @brief Updates the current VM execution state.
 */
void MVM_LvidSetState(VMGPContext *ctx, MVM_tenuState state);

/**
 * @brief Records the last fatal execution error and moves the VM into error state.
 */
void MVM_LvidSetError(VMGPContext *ctx, MVM_tenuError error);

/**
 * @brief Provides MVM_LbRuntimeMemRangeOk API.
 */
bool MVM_LbRuntimeMemRangeOk(const VMGPContext *ctx, uint32_t addr, uint32_t size);

/**
 * @brief Provides MVM_Lu32RuntimeStrLen API.
 */
uint32_t MVM_Lu32RuntimeStrLen(const uint8_t *s, size_t max_len);

/**
 * @brief Handles a VM runtime import group.
 */
bool MVM_bRuntimeHandleStream(VMGPContext *ctx, const char *name);

/**
 * @brief Handles a VM runtime import group.
 */
bool MVM_bRuntimeHandleCaps(VMGPContext *ctx, const char *name);

/**
 * @brief Handles a VM runtime import group.
 */
bool MVM_bRuntimeHandleDecompress(VMGPContext *ctx, const char *name);

/**
 * @brief Handles a VM runtime import group.
 */
bool MVM_bRuntimeHandleHeap(VMGPContext *ctx, const char *name);

/**
 * @brief Handles a VM runtime import group.
 */
bool MVM_bRuntimeHandleTimeRandom(VMGPContext *ctx, const char *name);

/**
 * @brief Handles a VM runtime import group.
 */
bool MVM_bRuntimeHandleStrings(VMGPContext *ctx, const char *name);

/**
 * @brief Handles a VM runtime import group.
 */
bool MVM_bRuntimeHandleMisc(VMGPContext *ctx, const char *name);

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
