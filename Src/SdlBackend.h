#ifndef SDL_BACKEND_H
#define SDL_BACKEND_H

#include "MVM.h"
#include <stdint.h>

typedef struct SdlBackend SdlBackend;

SdlBackend *SdlBackend_Create(const MVM_DeviceProfile_t *profile);
void SdlBackend_Destroy(SdlBackend *backend);
int SdlBackend_DisplayFlush(void *context, const MVM_Framebuffer_t *framebuffer);
uint32_t SdlBackend_InputGetButtons(void *context);
int SdlBackend_AudioPlay(void *context, const MVM_AudioRequest_t *request);
void SdlBackend_AudioStop(void *context);
uint32_t SdlBackend_GetTicks(void *context);
void SdlBackend_SetLogLevel(SdlBackend *backend, MVM_LogLevel_t level);
int SdlBackend_StartRecording(SdlBackend *backend, const char *record_dir);
void SdlBackend_StopRecording(SdlBackend *backend);
void SdlBackend_SetSyntheticButtons(SdlBackend *backend, uint32_t button_mask);
int SdlBackend_PumpEvents(MVM_Instance_t *vm, SdlBackend *backend);
void SdlBackend_Present(MVM_Instance_t *vm, SdlBackend *backend);
uint32_t SdlBackend_GetTicksMs(SdlBackend *backend);
void SdlBackend_Delay(uint32_t delay_ms);

#endif
