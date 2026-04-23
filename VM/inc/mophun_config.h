#ifndef MOPHUN_CONFIG_H
#define MOPHUN_CONFIG_H

/*
 * Integration-time configuration belongs here. Keep defaults portable; platform
 * backends may override these from their build system before including VM/vm.mk.
 */

#ifndef MOPHUN_VM_DEFAULT_MAX_STREAMS
#define MOPHUN_VM_DEFAULT_MAX_STREAMS 16
#endif

#ifndef MOPHUN_VM_ENABLE_DEFAULT_ALLOCATOR
#define MOPHUN_VM_ENABLE_DEFAULT_ALLOCATOR 1
#endif

#ifndef MOPHUN_VM_ENABLE_DEFAULT_LOGGER
#define MOPHUN_VM_ENABLE_DEFAULT_LOGGER 1
#endif

#ifndef MOPHUN_VM_LOG_BUFFER_SIZE
#define MOPHUN_VM_LOG_BUFFER_SIZE 256
#endif

#endif
