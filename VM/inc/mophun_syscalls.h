#ifndef MOPHUN_SYSCALLS_H
#define MOPHUN_SYSCALLS_H

#include <stdint.h>

struct MophunVM;

typedef uint32_t (*MophunSyscallFn)(struct MophunVM *vm, void *user);

typedef struct MophunSyscall
{
  const char *name;
  MophunSyscallFn fn;
  void *user;
} MophunSyscall;

void mophun_vm_set_syscalls(struct MophunVM *vm, const MophunSyscall *syscalls, uint32_t count);

#endif
