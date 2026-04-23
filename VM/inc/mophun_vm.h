#ifndef MOPHUN_VM_H
#define MOPHUN_VM_H

#include "mophun_platform.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct MophunVM MophunVM;

size_t mophun_vm_storage_size(void);
size_t mophun_vm_storage_align(void);
MophunVM *mophun_vm_from_storage(void *storage, size_t storage_size);
bool mophun_vm_init(MophunVM *vm, const uint8_t *image, size_t image_size);
bool mophun_vm_init_with_platform(MophunVM *vm,
                                  const uint8_t *image,
                                  size_t image_size,
                                  const MophunPlatform *platform);
void mophun_vm_free(MophunVM *vm);

#endif
