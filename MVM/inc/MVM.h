/**********************************************************************************************************************
 *  FILE DESCRIPTION
 *  -------------------------------------------------------------------------------------------------------------------
 *          Project:  Mophun
 *             File:  MVM.h
 *           Module:  MVM_Inc
 *           Target:  Portable C
 *      Description:  Public VM lifecycle, bounded execution, device-profile, and state-query API.
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  Header file guard
 *********************************************************************************************************************/

#ifndef MVM_VM_H
#define MVM_VM_H

/**********************************************************************************************************************
 *  INCLUDES
 *********************************************************************************************************************/

#include "MVM_Config.h"
#include <stddef.h>
#include <stdint.h>

/**********************************************************************************************************************
 *  GLOBAL FUNCTIONS PROTOTYPES
 *********************************************************************************************************************/

/**
 * @brief Provides MVM_GetStorageSize API.
 */
size_t MVM_GetStorageSize(void);

/**
 * @brief Provides MVM_GetStorageAlign API.
 */
size_t MVM_GetStorageAlign(void);

/**
 * @brief Provides MVM_GetVmFromStorage API.
 */
MpnVM_t *MVM_GetVmFromStorage(void *storage, size_t storage_size);

/**
 * @brief Initializes VM state from one image source using a parent-owned integration config.
 */
MVM_RetCode_t MVM_InitFromSourceWithConfig(MpnVM_t *vm,
                                           const MpnImageSource_t *image,
                                           const char *profile_name,
                                           const MVM_Config_t *config);

/**
 * @brief Initializes VM state from one image source using the built-in integration config.
 */
MVM_RetCode_t MVM_InitFromSource(MpnVM_t *vm, const MpnImageSource_t *image, const char *profile_name);

/**
 * @brief Initializes VM state from a memory image using a parent-owned integration config.
 */
MVM_RetCode_t MVM_InitWithConfig(MpnVM_t *vm,
                                 const uint8_t *image,
                                 size_t image_size,
                                 const char *profile_name,
                                 const MVM_Config_t *config);

/**
 * @brief Initializes VM state with the built-in integration config.
 */
MVM_RetCode_t MVM_Init(MpnVM_t *vm, const uint8_t *image, size_t image_size, const char *profile_name);

/**
 * @brief Releases VM resources.
 */
void MVM_Free(MpnVM_t *vm);

/**
 * @brief Queries memory required for a source-backed image using a parent-owned integration config.
 */
MVM_RetCode_t MVM_QueryMemReqsFromSourceWithConfig(const MpnImageSource_t *image,
                                                   const MVM_Config_t *config,
                                                   MVM_MemReqs_t *requirements);

/**
 * @brief Queries the static memory required for a source-backed VMGP image.
 */
MVM_RetCode_t MVM_QueryMemReqsFromSource(const MpnImageSource_t *image, MVM_MemReqs_t *requirements);

/**
 * @brief Queries memory required for a memory image using a parent-owned integration config.
 */
MVM_RetCode_t MVM_QueryMemReqsWithConfig(const uint8_t *image,
                                         size_t image_size,
                                         const MVM_Config_t *config,
                                         MVM_MemReqs_t *requirements);

/**
 * @brief Queries the static memory required for a VMGP image.
 */
MVM_RetCode_t MVM_QueryMemReqs(const uint8_t *image, size_t image_size, MVM_MemReqs_t *requirements);

/**
 * @brief Returns the number of profiles in a parent-owned integration config.
 */
uint32_t MVM_GetDevProfileCountWithConfig(const MVM_Config_t *config);

/**
 * @brief Returns the number of built-in device profiles.
 */
uint32_t MVM_GetDevProfileCount(void);

/**
 * @brief Returns one profile from a parent-owned integration config by zero-based index.
 */
const MpnDevProfile_t *MVM_GetDevProfileWithConfig(const MVM_Config_t *config, uint32_t profile_index);

/**
 * @brief Returns one built-in device profile by zero-based index.
 */
const MpnDevProfile_t *MVM_GetDevProfile(uint32_t profile_index);

/**
 * @brief Finds one named profile in a parent-owned integration config.
 */
const MpnDevProfile_t *MVM_FindDevProfileByNameWithConfig(const MVM_Config_t *config, const char *profile_name);

/**
 * @brief Finds one built-in device profile by name.
 */
const MpnDevProfile_t *MVM_FindDevProfileByName(const char *profile_name);

/**
 * @brief Executes at most one VM instruction without blocking.
 */
MVM_RetCode_t MVM_RunStep(MpnVM_t *vm);

/**
 * @brief Executes up to the requested instruction budget.
 */
MVM_RetCode_t MVM_RunSteps(MpnVM_t *vm, uint32_t max_steps, uint32_t *executed_steps);

/**
 * @brief Executes for up to the requested host time budget in milliseconds.
 */
MVM_RetCode_t MVM_RunForTime(MpnVM_t *vm, uint32_t budget_ms, uint32_t *executed_steps);

/**
 * @brief Pauses VM execution.
 */
MVM_RetCode_t MVM_Pause(MpnVM_t *vm);

/**
 * @brief Moves the VM into a host-waiting state.
 */
MVM_RetCode_t MVM_Wait(MpnVM_t *vm);

/**
 * @brief Resumes VM execution after a pause or wait state.
 */
MVM_RetCode_t MVM_Resume(MpnVM_t *vm);

/**
 * @brief Requests immediate VM termination.
 */
MVM_RetCode_t MVM_RequestExit(MpnVM_t *vm);

/**
 * @brief Returns the current execution state.
 */
MVM_State_t MVM_GetState(const MpnVM_t *vm);

/**
 * @brief Returns the last fatal execution error.
 */
MVM_Err_t MVM_GetLastError(const MpnVM_t *vm);

/**
 * @brief Copies current guest-heap diagnostics into caller-owned storage.
 */
MVM_RetCode_t MVM_GetHeapStats(const MpnVM_t *vm, MVM_HeapStats_t *stats);

/**
 * @brief Sets the no-progress step limit for the soft watchdog.
 */
MVM_RetCode_t MVM_SetWdgLimit(MpnVM_t *vm, uint32_t no_progress_steps);

/**
 * @brief Sets the host tick provider used by time-based imports and bounded execution.
 */
MVM_RetCode_t MVM_SetTickProvider(MpnVM_t *vm, void *user, uint32_t (*get_ticks_ms)(void *user));

/**
 * @brief Sets a deterministic date/time returned by VM date imports.
 *
 * Pass year zero to disable the override and use the host clock again.
 */
MVM_RetCode_t MVM_SetFixedDateTime(MpnVM_t *vm,
                                   uint16_t year,
                                   uint8_t month,
                                   uint8_t day,
                                   uint8_t hour,
                                   uint8_t minute,
                                   uint8_t second);

/**
 * @brief Updates the current VM button-state bitmask.
 */
MVM_RetCode_t MVM_SetButtonState(MpnVM_t *vm, uint32_t button_state);

/**
 * @brief Returns and removes the oldest pending sound request, if any.
 */
int MVM_PollSoundRequest(MpnVM_t *vm, MVM_SoundRequest_t *request);

/**
 * @brief Copies one range from guest-visible VM memory.
 */
int MVM_ReadGuestMemory(const MpnVM_t *vm, uint32_t address, void *dst, size_t size);

/**
 * @brief Returns the configured no-progress step limit for the soft watchdog.
 */
uint32_t MVM_GetWdgLimit(const MpnVM_t *vm);

/**
 * @brief Returns the total number of executed VM instructions.
 */
uint32_t MVM_GetExecutedSteps(const MpnVM_t *vm);

/**
 * @brief Returns the current VM program counter.
 */
uint32_t MVM_GetProgramCounter(const MpnVM_t *vm);

/**
 * @brief Returns the number of trace calls logged so far.
 */
uint32_t MVM_GetLoggedCalls(const MpnVM_t *vm);

/**********************************************************************************************************************
 *  END of header file guard
 *********************************************************************************************************************/

#endif

/**********************************************************************************************************************
 *  END OF FILE MVM.h
 *********************************************************************************************************************/
