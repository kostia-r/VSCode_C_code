/**********************************************************************************************************************
 *  FILE DESCRIPTION
 *  -------------------------------------------------------------------------------------------------------------------
 *          Project:  OpenMophun
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

/**
 * @brief Stores a platform-defined open file handle.
 */
typedef uintptr_t MVM_FileHandle_t;

/** @brief Identifies an invalid or unopened file handle. */
#define MVM_FILE_INVALID_HANDLE ((MVM_FileHandle_t)0U)

/**********************************************************************************************************************
 *  GLOBAL DATA TYPES AND STRUCTURES
 *********************************************************************************************************************/

/**
 * @brief Identifies the severity of a diagnostic log message.
 */
typedef enum MVM_LogLevel_t
{
  MVM_LOG_LEVEL_OFF = 0,     /**< Disables all diagnostic logging for the instance. */
  MVM_LOG_LEVEL_ERROR,       /**< Fatal or user-visible failure. */
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
  MVM_EVENT_DATA_CERT_CHECKED,   /**< Guest requested one data-certificate verification. */
  MVM_EVENT_LICENSE_EXPIRED,     /**< Guest reported an expired game/license path. */
  MVM_EVENT_DEVICE_UNSUPPORTED,  /**< Guest reported that the current device/profile is unsupported. */
  MVM_EVENT_SIDECAR_MISSING,     /**< Guest requested one read-only sidecar file that is absent on the host. */
  MVM_EVENT_SYSTEM_MESSAGE,      /**< Guest requested one normalized platform system message. */
} MVM_Event_t;

/**
 * @brief Identifies the source used for one data-certificate verification event.
 */
typedef enum MVM_DataCertSource_t
{
  MVM_DATA_CERT_SOURCE_MEMORY = 0, /**< Certificate bytes were provided from guest memory. */
  MVM_DATA_CERT_SOURCE_STREAM = 1, /**< Certificate bytes were provided through one guest stream. */
} MVM_DataCertSource_t;

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
  size_t guest_memory_bytes;     /**< Physically backed guest RAM stored in the runtime pool. */
  size_t logical_guest_address_space_bytes; /**< Highest valid guest address plus one; not an SRAM allocation size. */
  size_t framebuffer_bytes;      /**< Optional RGB565 framebuffer contribution to the runtime pool. */
  size_t allocator_metadata_bytes; /**< Guest allocator tracking contribution to the runtime pool. */
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

/**
 * @brief Declares the opaque descriptor used by every instance API.
 */
typedef struct MVM_Instance MVM_Instance_t;

/**
 * @brief Reports deterministic guest-heap usage without exposing VM internals.
 */
typedef struct MVM_HeapStats_t
{
  uint32_t capacity_bytes;          /**< Physical heap capacity available to the guest. */
  uint32_t soft_limit_bytes;        /**< Image-requested heap limit before compatibility fallback. */
  uint32_t high_water_bytes;        /**< Highest physical heap offset ever committed. */
  uint32_t allocation_requests;     /**< Total allocation requests. */
  uint32_t free_requests;           /**< Total free requests. */
  uint32_t allocation_failures;     /**< Allocation requests that could not be satisfied. */
  uint32_t invalid_free_requests;   /**< Free requests that did not identify a tracked allocation. */
  uint32_t double_free_requests;    /**< Repeated free requests detected for released allocations. */
  uint32_t live_bytes;              /**< Bytes held by currently live allocations. */
  uint32_t peak_live_bytes;         /**< Maximum simultaneously live allocation size. */
  uint32_t quarantine_bytes;        /**< Released bytes not currently reused. */
  uint32_t tracker_overflows;       /**< Allocation records lost because tracking capacity was exhausted. */
  uint32_t reuse_count;             /**< Allocations satisfied from released blocks. */
  uint32_t soft_limit_fallbacks;    /**< Allocations permitted beyond the image-requested soft limit. */
} MVM_HeapStats_t;

/** @brief Indicates that the selected profile provides video output. */
#define MVM_DEVICE_CAP_VIDEO                                    (1UL << 0U)
/** @brief Indicates that the selected profile provides button input. */
#define MVM_DEVICE_CAP_INPUT                                    (1UL << 1U)
/** @brief Indicates that the selected profile provides sound output. */
#define MVM_DEVICE_CAP_SOUND                                    (1UL << 2U)
/** @brief Indicates that the selected profile provides communication services. */
#define MVM_DEVICE_CAP_COMM                                     (1UL << 3U)
/** @brief Indicates that the selected profile provides system services. */
#define MVM_DEVICE_CAP_SYSTEM                                   (1UL << 4U)

/** @brief Guest-visible Up button bit. */
#define MVM_BUTTON_UP_MASK                                      (0x00000001UL)
/** @brief Guest-visible Down button bit. */
#define MVM_BUTTON_DOWN_MASK                                    (0x00000002UL)
/** @brief Guest-visible Left button bit. */
#define MVM_BUTTON_LEFT_MASK                                    (0x00000004UL)
/** @brief Guest-visible Right button bit. */
#define MVM_BUTTON_RIGHT_MASK                                   (0x00000008UL)
/** @brief Guest-visible primary Fire button bit. */
#define MVM_BUTTON_FIRE_MASK                                    (0x00000010UL)
/** @brief Guest-visible Select button bit. */
#define MVM_BUTTON_SELECT_MASK                                  (0x00000020UL)
/** @brief Guest-visible secondary Fire button bit. */
#define MVM_BUTTON_FIRE2_MASK                                   (0x00000100UL)

/**
 * @brief Describes the guest-visible capabilities of one emulated device.
 */
typedef struct MVM_DeviceProfile_t
{
  const char *name;            /**< Human-readable profile name. */
  uint16_t screen_width;       /**< Display width in pixels. */
  uint16_t screen_height;      /**< Display height in pixels. */
  uint16_t color_mode;         /**< Guest-visible display color mode. */
  uint16_t sound_flags;        /**< Guest-visible sound capability flags. */
  uint16_t system_flags;       /**< Guest-visible system capability flags. */
  uint16_t key_layout;         /**< Guest-visible key-layout identifier. */
  uint16_t frame_interval_ms;  /**< Default frame interval in milliseconds. */
  uint32_t device_id;          /**< Stable guest-visible device identifier. */
  uint32_t memory_limit_bytes; /**< Guest-visible working-memory limit. */
  uint32_t supported_caps;     /**< Combination of MVM_DEVICE_CAP_* values. */
} MVM_DeviceProfile_t;

/**
 * @brief Identifies the pixel encoding of a framebuffer.
 */
typedef enum MVM_PixelFormat_t
{
  MVM_PIXEL_FORMAT_RGB565 = 0 /**< Packed 16-bit RGB565 pixels in native byte order. */
} MVM_PixelFormat_t;

/**
 * @brief Describes one dirty framebuffer rectangle in pixels.
 */
typedef struct MVM_DirtyRect_t
{
  uint16_t x;      /**< Left edge in framebuffer coordinates. */
  uint16_t y;      /**< Top edge in framebuffer coordinates. */
  uint16_t width;  /**< Dirty width in pixels. */
  uint16_t height; /**< Dirty height in pixels. */
} MVM_DirtyRect_t;

/**
 * @brief Describes a read-only framebuffer view valid during a display callback.
 */
typedef struct MVM_Framebuffer_t
{
  const void *pixels;             /**< Pixel storage that must not be retained by the parent. */
  uint16_t width;                 /**< Framebuffer width in pixels. */
  uint16_t height;                /**< Framebuffer height in pixels. */
  uint32_t stride_bytes;          /**< Distance between adjacent rows in bytes. */
  MVM_PixelFormat_t pixel_format; /**< Pixel encoding used by the framebuffer. */
  MVM_DirtyRect_t dirty_rect;     /**< Region requiring transfer; zero size means the complete frame. */
} MVM_Framebuffer_t;

/**
 * @brief Identifies the encoded audio format requested by the guest.
 */
typedef enum MVM_AudioFormat_t
{
  MVM_AUDIO_FORMAT_BEEP = 0, /**< Mophun beep-sequence data. */
  MVM_AUDIO_FORMAT_MIDI = 2, /**< Standard MIDI file data. */
  MVM_AUDIO_FORMAT_AMR = 3,  /**< Adaptive Multi-Rate audio data. */
  MVM_AUDIO_FORMAT_UNKNOWN = 255 /**< Unsupported or unidentified encoded audio. */
} MVM_AudioFormat_t;

/** @brief Requests repeated playback. */
#define MVM_AUDIO_FLAG_LOOP                                     (0x00000100UL)
/** @brief Indicates that the source value identifies a guest stream. */
#define MVM_AUDIO_FLAG_STREAM                                   (0x00000200UL)

/**
 * @brief Describes one synchronous encoded-audio playback request.
 */
typedef struct MVM_AudioRequest_t
{
  const void *data;         /**< Borrowed encoded bytes, or NULL for a stream request. */
  uint32_t length;          /**< Encoded byte length reported by the guest. */
  uint32_t flags;           /**< Original guest playback flags. */
  MVM_AudioFormat_t format; /**< Normalized encoded-audio format. */
} MVM_AudioRequest_t;

/**
 * @brief Groups optional platform services for one isolated VM instance.
 *
 * Zero initialization is valid. Callback buffers are borrowed only for the
 * duration of the call and must not be retained by the platform.
 */
typedef struct MVM_HostServices_t
{
  void *context; /**< Parent-defined context passed to every service callback. */
  /** Transfers a normalized framebuffer to the display backend. */
  int (*display_flush)(void *context, const MVM_Framebuffer_t *framebuffer);
  /** Returns the current combination of MVM_BUTTON_* values. */
  uint32_t (*input_get_buttons)(void *context);
  /** Starts encoded audio playback; request data is valid only during the callback. */
  int (*audio_play)(void *context, const MVM_AudioRequest_t *request);
  /** Stops audio previously started by this instance. */
  void (*audio_stop)(void *context);
  /** Returns a monotonic millisecond counter. */
  uint32_t (*get_ticks_ms)(void *context);
  /** Returns one platform random value. */
  uint32_t (*get_random)(void *context);
  /** Prints one complete preformatted diagnostic line. */
  int (*log)(void *context, const char *message);
  /** Receives one structured VM event. */
  void (*event)(void *context, MVM_Event_t event, uint32_t arg0, uint32_t arg1);
  /** Presents a normalized system message and returns the selected response. */
  uint32_t (*system_message)(void *context, uint32_t flags, const char *title, const char *message);
} MVM_HostServices_t;

/**
 * @brief Defines flags accepted by MVM_FileApi_t::open.
 */
typedef enum MVM_FileOpenFlags_t
{
  MVM_FILE_OPEN_READ = (1U << 0),     /**< Open the file for reading. */
  MVM_FILE_OPEN_WRITE = (1U << 1),    /**< Open the file for writing. */
  MVM_FILE_OPEN_CREATE = (1U << 2),   /**< Create the file when it does not exist. */
  MVM_FILE_OPEN_TRUNCATE = (1U << 3)  /**< Truncate the file when it is opened. */
} MVM_FileOpenFlags_t;

/**
 * @brief Defines random-access filesystem operations supplied by the parent.
 *
 * The image_path identifies the selected game image. A NULL name refers to
 * that image; a non-NULL name identifies a guest sidecar or save file.
 */
typedef struct MVM_FileApi_t
{
  void *context; /**< Parent-defined context passed to every filesystem callback. */
  /** Opens the game image or one named file and returns its handle and current size. */
  int (*open)(void *context,
              const char *image_path,
              const char *name,
              uint32_t flags,
              MVM_FileHandle_t *handle,
              size_t *size);
  /** Reads up to size bytes from an absolute file offset. */
  int (*read)(void *context,
              MVM_FileHandle_t handle,
              size_t offset,
              void *dst,
              size_t size,
              size_t *read_size);
  /** Writes up to size bytes at an absolute file offset. */
  int (*write)(void *context,
               MVM_FileHandle_t handle,
               size_t offset,
               const void *src,
               size_t size,
               size_t *written_size);
  /** Changes the size of an open file. */
  int (*resize)(void *context, MVM_FileHandle_t handle, size_t size);
  /** Closes one previously opened file handle. */
  int (*close)(void *context, MVM_FileHandle_t handle);
  /** Removes one named sidecar or save file. */
  int (*remove)(void *context, const char *image_path, const char *name);
} MVM_FileApi_t;

/**
 * @brief Describes all parent-owned configuration required by one VM instance.
 *
 * MVM_Init() copies this structure. Referenced memory and callback contexts
 * must remain valid until MVM_Deinit().
 */
typedef struct MVM_InitConfig_t
{
  const char *image_path;        /**< Parent-owned path of the selected decrypted VMGP image. */
  const MVM_FileApi_t *file_api; /**< Parent-owned filesystem callback table. */
  MVM_DeviceProfile_t profile;   /**< Device profile copied into the instance. */
  MVM_HostServices_t services;   /**< Platform services copied into the instance. */
  void *runtime_pool;            /**< Parent-owned aligned runtime arena. */
  size_t runtime_pool_size;      /**< Available runtime arena capacity in bytes. */
  uint32_t watchdog_limit;       /**< No-progress step limit; zero disables the watchdog. */
  MVM_LogLevel_t log_level;      /**< Highest diagnostic level emitted by this instance; zero disables logging. */
} MVM_InitConfig_t;

/**********************************************************************************************************************
 *  END of header file guard
 *********************************************************************************************************************/

#endif

/**********************************************************************************************************************
 *  END OF FILE MVM_Types.h
 *********************************************************************************************************************/
