#ifndef EXEC_ARCH_SYSCALL_H
#define EXEC_ARCH_SYSCALL_H
#if defined(ARCH_ARM64)
#define EXEC_SC_NUMBER "x8"
#define EXEC_SC_ARG0 "x0"
#define EXEC_SC_ARG1 "x1"
#define EXEC_SC_ARG2 "x2"
#define EXEC_SC_ARG3 "x3"
#define EXEC_SC_ARG4 "x4"
#define EXEC_SC_TRAP "svc #0"
#else
#define EXEC_SC_NUMBER "a7"
#define EXEC_SC_ARG0 "a0"
#define EXEC_SC_ARG1 "a1"
#define EXEC_SC_ARG2 "a2"
#define EXEC_SC_ARG3 "a3"
#define EXEC_SC_ARG4 "a4"
#define EXEC_SC_TRAP "ecall"
#endif
#endif
