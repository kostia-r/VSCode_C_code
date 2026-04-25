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

#include "MVM_Types.h"
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
 * @brief Initializes VM state with the built-in integration config.
 */
MVM_RetCode_t MVM_Init(MpnVM_t *vm, const uint8_t *image, size_t image_size, const char *profile_name);

/**
 * @brief Releases VM resources.
 */
void MVM_Free(MpnVM_t *vm);

/**
 * @brief Queries the static memory required for a VMGP image.
 */
MVM_RetCode_t MVM_QueryMemReqs(const uint8_t *image, size_t image_size, MVM_MemReqs_t *requirements);

/**
 * @brief Returns the number of built-in device profiles.
 */
uint32_t MVM_GetDevProfileCount(void);

/**
 * @brief Returns one built-in device profile by zero-based index.
 */
const MpnDevProfile_t *MVM_GetDevProfile(uint32_t profile_index);

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
 * @brief Sets the no-progress step limit for the soft watchdog.
 */
MVM_RetCode_t MVM_SetWdgLimit(MpnVM_t *vm, uint32_t no_progress_steps);

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
