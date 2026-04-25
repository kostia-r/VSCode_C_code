/**********************************************************************************************************************
 *  FILE DESCRIPTION
 *  -------------------------------------------------------------------------------------------------------------------
 *          Project:  Mophun
 *             File:  MVM_Types.h
 *           Module:  MVM_Inc
 *           Target:  Portable C
 *      Description:  Shared public VM, platform, syscall, and integration type declarations.
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  Header file guard
 *********************************************************************************************************************/

#ifndef MVM_TYPES_H
#define MVM_TYPES_H

/**********************************************************************************************************************
 *  INCLUDES
 *********************************************************************************************************************/

#include <stddef.h>
#include <stdint.h>

typedef struct MophunDeviceProfile MophunDeviceProfile;

/**********************************************************************************************************************
 *  GLOBAL DATA TYPES AND STRUCTURES
 *********************************************************************************************************************/

/**
 * @brief Describes host services exposed to the VM core.
 */
typedef struct MophunPlatform
{
  void *user;                                   /**< Opaque host context passed to all platform callbacks. */
  uint32_t (*get_ticks_ms)(void *user);         /**< Returns a monotonic host tick value in milliseconds. */
  uint32_t (*get_random)(void *user);           /**< Returns one host-provided random value. */
  int (*log)(void *user, const char *message);  /**< Writes one diagnostic message through the host logger. */
} MophunPlatform;

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
  MVM_TENU_ERROR_MEMORY,         /**< VM runtime pool is missing or undersized. */
  MVM_TENU_ERROR_EXECUTION,      /**< VM execution failed. */
  MVM_TENU_ERROR_WATCHDOG,       /**< Soft watchdog detected a stalled PC. */
} MVM_tenuError;

/**
 * @brief Describes runtime memory requirements for one loaded VMGP image.
 */
typedef struct MVM_tstMemoryRequirements
{
  size_t runtime_pool_bytes;     /**< Total arena capacity required by the VM runtime. */
  size_t guest_memory_bytes;     /**< Total guest-visible RAM requirement. */
  size_t pool_entries_bytes;     /**< Storage required for decoded constant-pool metadata. */
  size_t resource_entries_bytes; /**< Storage required for decoded resource metadata. */
  uint32_t pool_entry_count;     /**< Number of constant-pool records described by the image. */
  uint32_t resource_count;       /**< Number of resource records described by the image. */
  uint32_t static_data_bytes;    /**< Size of the initialized guest data section. */
  uint32_t bss_bytes;            /**< Size of the zero-initialized guest BSS section. */
  uint32_t resource_bytes;       /**< Guest RAM budget reserved for loaded resource payloads. */
  uint32_t heap_bytes;           /**< Guest heap budget included in the RAM requirement. */
  uint32_t stack_bytes;          /**< Guest stack budget included in the RAM requirement. */
} MVM_tstMemoryRequirements;

typedef struct MophunVM MophunVM;

/**
 * @brief Represents a host syscall callback.
 *
 * The callback may inspect or update VM execution state through the public VM
 * control APIs when host-driven waiting, pause, resume, or exit handling is
 * required.
 */
typedef uint32_t (*MophunSyscallFn)(MophunVM *vm, void *user);

/**
 * @brief Describes one named host syscall binding.
 */
typedef struct MophunSyscall
{
  const char *name;   /**< Exported syscall name visible to the guest runtime. */
  MophunSyscallFn fn; /**< Host callback that implements the named syscall. */
  void *user;         /**< Opaque host context passed to the syscall callback. */
} MophunSyscall;

/**
 * @brief Describes one complete host integration instance for the VM.
 *
 * This object gathers every platform-specific integration point in one place:
 * host callbacks, device profiles, syscall bindings, runtime arena, and common
 * execution policy defaults.
 */
typedef struct MVM_tstConfig
{
  MophunPlatform platform;                   /**< Host callback table used by the VM core. */
  const MophunDeviceProfile *device_profiles; /**< Catalog of device profiles offered by this integration. */
  uint32_t device_profile_count;             /**< Number of entries in the device profile catalog. */
  const MophunDeviceProfile *device_profile; /**< Active device profile exposed to guest imports. */
  const MophunSyscall *syscalls;             /**< Host syscall table visible to the runtime dispatcher. */
  uint32_t syscall_count;                    /**< Number of entries in the syscall table. */
  void *runtime_pool;                        /**< Backing arena used for VM state, RAM, and decoded metadata. */
  size_t runtime_pool_size;                  /**< Total size of the runtime arena in bytes. */
  uint32_t watchdog_limit;                   /**< Default no-progress watchdog budget in VM steps. */
} MVM_tstConfig;

/**********************************************************************************************************************
 *  END of header file guard
 *********************************************************************************************************************/

#endif

/**********************************************************************************************************************
 *  END OF FILE MVM_Types.h
 *********************************************************************************************************************/
