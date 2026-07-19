#ifndef SDL_BACKEND_H
#define SDL_BACKEND_H

#include "MVM.h"
#include "MVM_Device.h"
#include <stdint.h>

typedef struct SdlBackend SdlBackend;

SdlBackend *SdlBackend_Create(const MpnDevProfile_t *profile);
void SdlBackend_Destroy(SdlBackend *backend);
void SdlBackend_ConfigureDrivers(MVM_Config_t *config, SdlBackend *backend);
int SdlBackend_StartRecording(SdlBackend *backend, const char *record_dir);
void SdlBackend_StopRecording(SdlBackend *backend);
void SdlBackend_AttachVmTiming(MpnVM_t *vm, SdlBackend *backend);
void SdlBackend_SetSyntheticButtons(SdlBackend *backend, uint32_t button_mask);
void SdlBackend_SyncInputToVm(MpnVM_t *vm, SdlBackend *backend);
int SdlBackend_PumpEvents(MpnVM_t *vm, SdlBackend *backend);
void SdlBackend_Present(MpnVM_t *vm, SdlBackend *backend);
uint32_t SdlBackend_GetTicksMs(SdlBackend *backend);
void SdlBackend_Delay(uint32_t delay_ms);

#endif
