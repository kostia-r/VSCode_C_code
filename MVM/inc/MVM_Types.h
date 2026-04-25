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

typedef struct MpnDevProfile_t MpnDevProfile_t;
typedef struct MpnImageSource_t MpnImageSource_t;

/**********************************************************************************************************************
 *  GLOBAL DATA TYPES AND STRUCTURES
 *********************************************************************************************************************/

/**
 * @brief Describes host services exposed to the VM core.
 */
typedef enum MVM_LogLevel_t
{
  MVM_LOG_LEVEL_ERROR = 0,   /**< Fatal or user-visible failure. */
  MVM_LOG_LEVEL_WARNING,     /**< Recoverable problem or unsupported path. */
  MVM_LOG_LEVEL_INFO,        /**< High-level lifecycle information. */
  MVM_LOG_LEVEL_DEBUG,       /**< Detailed debug diagnostics. */
  MVM_LOG_LEVEL_TRACE,       /**< Very chatty step/trace diagnostics. */
} MVM_LogLevel_t;

/**
 * @brief Describes VM events emitted through the host event callback.
 */
typedef enum MVM_Event_t
{
  MVM_EVENT_IMPORT_CALL = 0,     /**< Guest import dispatch started. */
  MVM_EVENT_MISSING_SYSCALL,     /**< No runtime handler was found for one import. */
  MVM_EVENT_INVALID_OPCODE,      /**< The interpreter hit one unsupported opcode. */
  MVM_EVENT_MEMORY_OOB,          /**< One guest memory access went out of bounds. */
  MVM_EVENT_RESOURCE_OPENED,     /**< One resource stream was opened. */
  MVM_EVENT_RESOURCE_READ,       /**< One resource stream read completed. */
  MVM_EVENT_FRAME_READY,         /**< One frame became ready for presentation. */
  MVM_EVENT_SOUND_REQUESTED,     /**< The guest requested one platform sound action. */
  MVM_EVENT_VM_PAUSED,           /**< VM moved into the paused state. */
  MVM_EVENT_VM_RESUMED,          /**< VM resumed execution after pause/wait. */
  MVM_EVENT_VM_WAITING,          /**< VM moved into the waiting state. */
  MVM_EVENT_VM_EXITED,           /**< VM exited normally. */
  MVM_EVENT_VM_ERROR,            /**< VM entered the fatal error state. */
} MVM_Event_t;

typedef struct MpnPlatform_t
{
  void *user;  /**< Opaque host context passed to all platform callbacks. */
  uint32_t (*get_ticks_ms)(void *user);  /**< Returns a monotonic host tick value in milliseconds. */
  uint32_t (*get_random)(void *user);    /**< Returns one host-provided random value. */
  int (*log)(void *user,
             MVM_LogLevel_t level,
             const char *module,
             const char *event,
             const char *message);       /**< Writes one diagnostic message with metadata through the host logger. */
  void (*event)(void *user,
                MVM_Event_t event,
                uint32_t arg0,
                uint32_t arg1);          /**< Delivers one structured VM event to the host. */
} MpnPlatform_t;

/**
 * @brief Reads one byte range from a VM image source.
 */
typedef int (*MpnImageReadFn_t)(void *user, size_t offset, void *dst, size_t size);

/**
 * @brief Optionally maps one byte range from a VM image source.
 */
typedef const uint8_t *(*MpnImageMapFn_t)(void *user, size_t offset, size_t size);

/**
 * @brief Optionally unmaps one previously mapped VM image range.
 */
typedef void (*MpnImageUnmapFn_t)(void *user, const uint8_t *view, size_t size);

/**
 * @brief Describes one VM image source selected for the current run.
 */
struct MpnImageSource_t
{
  void *user;                      /**< Opaque host context passed to the configured image backend. */
  size_t image_size;               /**< Total size of the VM image in bytes. */
};

/**
 * @brief Describes the current execution state of the VM.
 */
typedef enum MVM_State_t
{
  MVM_STATE_READY = 0, /**< VM is initialized and ready to execute. */
  MVM_STATE_RUNNING,   /**< VM is currently executing instructions. */
  MVM_STATE_PAUSED,    /**< VM execution is paused by the host. */
  MVM_STATE_WAITING,   /**< VM is waiting for a host-driven external event. */
  MVM_STATE_EXITED,    /**< VM has exited normally. */
  MVM_STATE_ERROR,     /**< VM has stopped because of a fatal error. */
} MVM_State_t;

/**
 * @brief Describes public API return codes.
 */
typedef enum MVM_RetCode_t
{
  MVM_OK = 0,          /**< Operation completed successfully. */
  MVM_INVALID_ARG,     /**< One or more API arguments are invalid. */
  MVM_BAD_STATE,       /**< The requested API cannot run in the current VM state. */
  MVM_NOT_FOUND,       /**< A requested named resource or profile was not found. */
  MVM_INIT_FAILED,     /**< VM initialization failed. */
  MVM_MEMORY_ERROR,    /**< Runtime memory configuration is missing or too small. */
  MVM_EXECUTION_ERROR, /**< VM execution failed. */
  MVM_WATCHDOG_ERROR,  /**< The VM watchdog detected stalled execution. */
} MVM_RetCode_t;

/**
 * @brief Describes the last fatal execution error reported by the VM.
 */
typedef enum MVM_Err_t
{
  MVM_E_NONE = 0,       /**< No fatal error has been reported. */
  MVM_E_INVALID_ARG,    /**< Host code passed an invalid argument. */
  MVM_E_INIT_FAILED,    /**< VM initialization failed. */
  MVM_E_MEMORY,         /**< VM runtime pool is missing or undersized. */
  MVM_E_EXECUTION,      /**< VM execution failed. */
  MVM_E_WDG,            /**< Soft watchdog detected a stalled PC. */
} MVM_Err_t;

/**
 * @brief Describes runtime memory requirements for one loaded VMGP image.
 */
typedef struct MVM_MemReqs_t
{
  size_t runtime_pool_bytes;     /**< Total arena capacity required by the VM runtime. */
  size_t guest_memory_bytes;     /**< Total guest-visible RAM requirement. */
  size_t pool_entries_bytes;     /**< Storage required for decoded constant-pool metadata. */
  size_t resource_entries_bytes; /**< Storage required for decoded resource metadata. */
  uint32_t pool_entry_count;     /**< Number of constant-pool records described by the image. */
  uint32_t resource_count;       /**< Number of resource records described by the image. */
  uint32_t static_data_bytes;    /**< Size of the initialized guest data section. */
  uint32_t bss_bytes;            /**< Size of the zero-initialized guest BSS section. */
  uint32_t resource_bytes;       /**< Guest RAM budget reserved for mirrored resource payloads, if any. */
  uint32_t heap_bytes;           /**< Guest heap budget included in the RAM requirement. */
  uint32_t stack_bytes;          /**< Guest stack budget included in the RAM requirement. */
} MVM_MemReqs_t;

typedef struct MpnVM_t MpnVM_t;

/**
 * @brief Represents a host syscall callback.
 *
 * The callback may inspect or update VM execution state through the public VM
 * control APIs when host-driven waiting, pause, resume, or exit handling is
 * required.
 */
typedef uint32_t (*MpnSyscallFn_t)(MpnVM_t *vm, void *user);

/**
 * @brief Describes one named host syscall binding.
 */
typedef struct MpnSyscall_t
{
  const char *name;   /**< Exported syscall name visible to the guest runtime. */
  MpnSyscallFn_t fn;  /**< Host callback that implements the named syscall. */
  void *user;         /**< Opaque host context passed to the syscall callback. */
} MpnSyscall_t;

/**********************************************************************************************************************
 *  END of header file guard
 *********************************************************************************************************************/

#endif

/**********************************************************************************************************************
 *  END OF FILE MVM_Types.h
 *********************************************************************************************************************/
