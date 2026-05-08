#ifndef INPUT_SCRIPT_H
#define INPUT_SCRIPT_H

#include <stdint.h>

typedef struct InputScript InputScript;

InputScript *InputScript_Load(const char *path);
void InputScript_Destroy(InputScript *script);
uint32_t InputScript_GetButtonMask(InputScript *script, uint32_t elapsed_ms);
const char *InputScript_GetLastError(const InputScript *script);

#endif
