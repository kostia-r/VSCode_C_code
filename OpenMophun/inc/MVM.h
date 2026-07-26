/**********************************************************************************************************************
 *  FILE DESCRIPTION
 *  -------------------------------------------------------------------------------------------------------------------
 *          Project:  OpenMophun
 *             File:  MVM.h
 *           Module:  MVM_Inc
 *           Target:  Portable C
 *      Description:  Public lifecycle and bounded-execution API for isolated VM instances.
 *********************************************************************************************************************/

#ifndef MVM_VM_H
#define MVM_VM_H

#include "MVM_Types.h"
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Returns the storage size required for one opaque VM instance.
 */
size_t MVM_GetInstanceStorageSize(void);

/**
 * @brief Returns the alignment required for an opaque VM instance storage block.
 */
size_t MVM_GetInstanceStorageAlign(void);

/**
 * @brief Calculates the runtime-pool requirements for the configured image.
 */
MVM_RetCode_t MVM_QueryMemory(const MVM_InitConfig_t *config, MVM_MemReqs_t *requirements);

/**
 * @brief Initializes one isolated instance in parent-owned storage.
 *
 * On success, out_instance receives the only runtime descriptor used by all
 * subsequent public APIs. The input config itself may go out of scope after
 * this call, but its referenced pool, image state, and service context must
 * remain valid until MVM_Deinit().
 */
MVM_RetCode_t MVM_Init(void *instance_storage,
                       size_t instance_storage_size,
                       const MVM_InitConfig_t *config,
                       MVM_Instance_t **out_instance);

/**
 * @brief Deinitializes an instance without freeing parent-owned memory.
 */
void MVM_Deinit(MVM_Instance_t *instance);

/**
 * @brief Executes at most one guest instruction.
 */
MVM_RetCode_t MVM_RunStep(MVM_Instance_t *instance);

/**
 * @brief Executes up to the requested number of guest instructions.
 */
MVM_RetCode_t MVM_RunSteps(MVM_Instance_t *instance, uint32_t max_steps, uint32_t *executed_steps);

/**
 * @brief Executes until one frame is ready or the instruction budget is exhausted.
 */
MVM_RetCode_t MVM_RunFrame(MVM_Instance_t *instance, uint32_t max_steps, uint32_t *executed_steps);

/**
 * @brief Executes until the requested host-time budget expires.
 */
MVM_RetCode_t MVM_RunForTime(MVM_Instance_t *instance, uint32_t budget_ms, uint32_t *executed_steps);

/**
 * @brief Pauses a runnable VM instance.
 */
MVM_RetCode_t MVM_Pause(MVM_Instance_t *instance);

/**
 * @brief Places a runnable VM instance into the waiting state.
 */
MVM_RetCode_t MVM_Wait(MVM_Instance_t *instance);

/**
 * @brief Resumes a paused or waiting VM instance.
 */
MVM_RetCode_t MVM_Resume(MVM_Instance_t *instance);

/**
 * @brief Requests a normal exit from the VM instance.
 */
MVM_RetCode_t MVM_RequestExit(MVM_Instance_t *instance);

/**
 * @brief Returns the current execution state of the VM instance.
 */
MVM_State_t MVM_GetState(const MVM_Instance_t *instance);

/**
 * @brief Returns the last fatal error recorded by the VM instance.
 */
MVM_Err_t MVM_GetLastError(const MVM_Instance_t *instance);

/**
 * @brief Copies the current guest-heap statistics.
 */
MVM_RetCode_t MVM_GetHeapStats(const MVM_Instance_t *instance, MVM_HeapStats_t *stats);

/**
 * @brief Sets the maximum number of steps allowed without program-counter progress.
 */
MVM_RetCode_t MVM_SetWdgLimit(MVM_Instance_t *instance, uint32_t no_progress_steps);

/**
 * @brief Sets the deterministic date and time reported to the guest.
 */
MVM_RetCode_t MVM_SetFixedDateTime(MVM_Instance_t *instance,
                                   uint16_t year,
                                   uint8_t month,
                                   uint8_t day,
                                   uint8_t hour,
                                   uint8_t minute,
                                   uint8_t second);

/**
 * @brief Returns the configured no-progress watchdog limit.
 */
uint32_t MVM_GetWdgLimit(const MVM_Instance_t *instance);

/**
 * @brief Returns the total number of guest instructions executed by the instance.
 */
uint32_t MVM_GetExecutedSteps(const MVM_Instance_t *instance);

/**
 * @brief Returns the current guest program counter.
 */
uint32_t MVM_GetProgramCounter(const MVM_Instance_t *instance);

/**
 * @brief Returns the number of guest import calls recorded by the instance.
 */
uint32_t MVM_GetLoggedCalls(const MVM_Instance_t *instance);

/**
 * @brief Writes a decoded VMGP image summary through the configured logger.
 */
void MVM_DumpVmgpSummary(const MVM_Instance_t *vm);

/**
 * @brief Writes decoded VMGP imports through the configured logger.
 */
void MVM_DumpVmgpImports(const MVM_Instance_t *vm, uint32_t max_count);

#endif
