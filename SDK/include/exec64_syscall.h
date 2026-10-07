/*
 * sdk/include/exec64_syscall.h
 *
 * Legacy compatibility header.
 *
 * Syscall numbers live exclusively in <exec/syscalls.h>. Keep this wrapper for
 * existing source code, but do not add another numeric syscall table here.
 */

#ifndef EXEC64_SYSCALL_H
#define EXEC64_SYSCALL_H

#include <stdint.h>
#include <exec/syscalls.h>

/* Source-level aliases retained for users of the original header. */
#define SYS_CD     SYS_DOS_CD
#define SYS_ASSIGN SYS_DOS_ASSIGN

/* 
 * Legacy wrapper prototypes.
 * Ogni kernel (RISC-V Micro, ARM Monolitico) deve implementare queste firme.
 */

void      Sys_Yield(void);
void      Sys_KPutS(const char *msg);
int32_t   Sys_KGetC(void);
void*     Sys_AllocMem(uint32_t size, uint32_t flags);
void      Sys_FreeMem(void *ptr);
uint64_t  Sys_GetTick(void);

/* DOS Prototypes (Wrappers for ecall) */
typedef void* BPTR;
BPTR      Dos_Open(const char *name, int32_t mode);
void      Dos_Close(BPTR lock);
int64_t   Dos_Read(BPTR lock, void *buffer, uint64_t len);
int64_t   Dos_Write(BPTR lock, const void *buffer, uint64_t len);
int32_t   Dos_Resolve(const char *path, char *buffer, uint32_t len);
BPTR      Dos_LoadSeg(const char *name);
int32_t   Dos_CD(const char *path);

void      Sys_Exit(int32_t status);

#endif /* EXEC64_SYSCALL_H */
