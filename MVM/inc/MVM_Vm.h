/**********************************************************************************************************************
 *  FILE DESCRIPTION
 *  -------------------------------------------------------------------------------------------------------------------
 *          Project:  Mophun
 *             File:  MVM_Vm.h
 *           Module:  MVM_Inc
 *           Target:  Portable C
 *      Description:  Public VM lifecycle, bounded execution, and state-query API.
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  Header file guard
 *********************************************************************************************************************/

#ifndef MVM_VM_H
#define MVM_VM_H

/**********************************************************************************************************************
 *  INCLUDES
 *********************************************************************************************************************/

#include "MVM_Platform.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**********************************************************************************************************************
 *  GLOBAL DATA TYPES AND STRUCTURES
 *********************************************************************************************************************/

/**
 * @brief Describes the current execution state of the VM.
 */
typedef enum MVM_tenuState
{
  MVM_TENU_STATE_READY = 0, /**< VM is initialized and ready to execute. */
  MVM_TENU_STATE_RUNNING,   /**< VM is currently executing instructions. */
  MVM_TENU_STATE_PAUSED,    /**< VM execution is paused by the host. */
  MVM_TENU_STATE_WAITING,   /**< VM is waiting for a host-driven external event. */
  MVM_TENU_STATE_EXITED,    /**< VM has exited normally. */
  MVM_TENU_STATE_ERROR,     /**< VM has stopped because of a fatal error. */
} MVM_tenuState;

/**
 * @brief Describes the last fatal execution error reported by the VM.
 */
typedef enum MVM_tenuError
{
  MVM_TENU_ERROR_NONE = 0,       /**< No fatal error has been reported. */
  MVM_TENU_ERROR_INVALID_ARG,    /**< Host code passed an invalid argument. */
  MVM_TENU_ERROR_INIT_FAILED,    /**< VM initialization failed. */
  MVM_TENU_ERROR_MEMORY,         /**< VM memory buffers are missing or undersized. */
  MVM_TENU_ERROR_EXECUTION,      /**< VM execution failed. */
  MVM_TENU_ERROR_WATCHDOG,       /**< Soft watchdog detected a stalled PC. */
} MVM_tenuError;

/**
 * @brief Describes host-supplied static buffers used by the VM.
 */
typedef struct MVM_tstMemoryConfig
{
  void *guest_memory;         /**< Backing VM memory buffer. */
  size_t guest_memory_size;   /**< Size of the guest memory buffer in bytes. */
  void *pool_entries;         /**< Constant-pool entry storage. */
  size_t pool_entries_size;   /**< Size of the pool entry storage in bytes. */
  void *resource_entries;     /**< Resource metadata storage. */
  size_t resource_entries_size; /**< Size of the resource metadata storage in bytes. */
} MVM_tstMemoryConfig;

/**
 * @brief Describes VM memory requirements for one VMGP image.
 */
typedef struct MVM_tstMemoryRequirements
{
  size_t guest_memory_bytes;    /**< Total guest memory buffer size required. */
  size_t pool_entries_bytes;    /**< Constant-pool storage size required. */
  size_t resource_entries_bytes; /**< Resource metadata storage size required. */
  uint32_t pool_entry_count;    /**< Number of constant-pool entries. */
  uint32_t resource_count;      /**< Number of resource metadata entries. */
  uint32_t static_data_bytes;   /**< Initial guest data section size. */
  uint32_t bss_bytes;           /**< Guest BSS section size. */
  uint32_t resource_bytes;      /**< Resource payload size copied into guest memory. */
  uint32_t heap_bytes;          /**< Heap budget included in guest memory. */
  uint32_t stack_bytes;         /**< Stack budget included in guest memory. */
} MVM_tstMemoryRequirements;

typedef struct MophunVM MophunVM;

/**********************************************************************************************************************
 *  GLOBAL FUNCTIONS PROTOTYPES
 *********************************************************************************************************************/

/**
 * @brief Provides MVM_udtGetStorageSize API.
 */
size_t MVM_udtGetStorageSize(void);

/**
 * @brief Provides MVM_udtGetStorageAlign API.
 */
size_t MVM_udtGetStorageAlign(void);

/**
 * @brief Provides MVM_pudtGetVmFromStorage API.
 */
MophunVM *MVM_pudtGetVmFromStorage(void *storage, size_t storage_size);

/**
 * @brief Initializes VM state.
 */
bool MVM_bInit(MophunVM *vm, const uint8_t *image, size_t image_size);

/**
 * @brief Initializes VM state.
 */
bool MVM_bInitWithPlatform(MophunVM *vm,
const uint8_t *image,
size_t image_size,
const MophunPlatform *platform);

/**
 * @brief Initializes VM state using host-supplied memory buffers.
 */
bool MVM_bInitWithConfig(MophunVM *vm,
const uint8_t *image,
size_t image_size,
const MVM_tstMemoryConfig *memory_config);

/**
 * @brief Initializes VM state using host platform callbacks and memory buffers.
 */
bool MVM_bInitWithPlatformAndMemory(MophunVM *vm,
const uint8_t *image,
size_t image_size,
const MophunPlatform *platform,
const MVM_tstMemoryConfig *memory_config);

/**
 * @brief Releases VM resources.
 */
void MVM_vidFree(MophunVM *vm);

/**
 * @brief Queries the static memory required for a VMGP image.
 */
bool MVM_bQueryMemoryRequirements(const uint8_t *image,
size_t image_size,
MVM_tstMemoryRequirements *requirements);

/**
 * @brief Executes at most one VM instruction without blocking.
 */
bool MVM_bStep(MophunVM *vm);

/**
 * @brief Executes up to the requested instruction budget.
 */
uint32_t MVM_u32RunSteps(MophunVM *vm, uint32_t max_steps);

/**
 * @brief Executes for up to the requested host time budget in milliseconds.
 */
uint32_t MVM_u32RunForTime(MophunVM *vm, uint32_t budget_ms);

/**
 * @brief Pauses VM execution.
 */
void MVM_vidPause(MophunVM *vm);

/**
 * @brief Moves the VM into a host-waiting state.
 */
void MVM_vidWait(MophunVM *vm);

/**
 * @brief Resumes VM execution after a pause or wait state.
 */
void MVM_vidResume(MophunVM *vm);

/**
 * @brief Requests immediate VM termination.
 */
void MVM_vidRequestExit(MophunVM *vm);

/**
 * @brief Returns the current execution state.
 */
MVM_tenuState MVM_tenuGetState(const MophunVM *vm);

/**
 * @brief Returns the last fatal execution error.
 */
MVM_tenuError MVM_tenuGetLastError(const MophunVM *vm);

/**
 * @brief Sets the no-progress step limit for the soft watchdog.
 */
void MVM_vidSetWatchdogLimit(MophunVM *vm, uint32_t no_progress_steps);

/**
 * @brief Returns the configured no-progress step limit for the soft watchdog.
 */
uint32_t MVM_u32GetWatchdogLimit(const MophunVM *vm);

/**
 * @brief Returns the total number of executed VM instructions.
 */
uint32_t MVM_u32GetExecutedSteps(const MophunVM *vm);

/**
 * @brief Returns the current VM program counter.
 */
uint32_t MVM_u32GetProgramCounter(const MophunVM *vm);

/**
 * @brief Returns the number of trace calls logged so far.
 */
uint32_t MVM_u32GetLoggedCalls(const MophunVM *vm);

/**********************************************************************************************************************
 *  END of header file guard
 *********************************************************************************************************************/

#endif

/**********************************************************************************************************************
 *  END OF FILE MVM_Vm.h
 *********************************************************************************************************************/
