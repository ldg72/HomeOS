#ifndef EXEC_SYSCALLS_H
#define EXEC_SYSCALLS_H

/*
 * Canonical Exec64 syscall number table.
 *
 * Kernel dispatchers, U-Mode trampolines and compatibility headers must include
 * this file instead of maintaining private copies. Existing numbers are ABI and
 * must not be changed or reused.
 */
#define SYS_YIELD       0
#define SYS_KPUTS       1
#define SYS_KGETC       2
#define SYS_ALLOC_MEM   3
#define SYS_FREE_MEM    4
#define SYS_GET_TICK    5

/* --- DOS Syscalls --- */
#define SYS_DOS_OPEN         6
#define SYS_DOS_CLOSE        7
#define SYS_DOS_READ         8
#define SYS_DOS_WRITE        9
#define SYS_DOS_READENTRIES  10
#define SYS_DOS_RESOLVE      11
#define SYS_DOS_LOADSEG      12
#define SYS_DOS_GETCWD       13
#define SYS_DOS_SETCWD       14
#define SYS_DOS_EXAMINE      15
#define SYS_DOS_DELETE       16
#define SYS_DOS_CD           17
#define SYS_DOS_ASSIGN       18
#define SYS_DOS_UNLOADSEG    19
#define SYS_DOS_MAKEDIR      23

#define SYS_EXIT             20
#define SYS_AVAIL_MEM        21
#define SYS_HALT             22

#define SYS_ADD_TASK         24
#define SYS_PREPARE_STACK    25
#define SYS_REM_TASK         26
#define SYS_GET_CURRENT_TASK 27

/* --- Messaging (IPC) Syscalls --- */
#define SYS_PUT_MSG          28
#define SYS_GET_MSG          29
#define SYS_REPLY_MSG        30
#define SYS_WAIT_PORT        31
#define SYS_CREATE_MSG_PORT  32
#define SYS_DELETE_MSG_PORT  33
#define SYS_MAKE_EXECUTABLE  34
#define SYS_SET_EXCEPT       35
#define SYS_EXCEPT_RETURN    36
#define SYS_OPEN_DEVICE      37
#define SYS_CLOSE_DEVICE     38
#define SYS_OPEN_LIBRARY     39
#define SYS_CLOSE_LIBRARY    40
#define SYS_GET_INTERFACE    41
#define SYS_UMODE_RETURN     42
#define SYS_MMIO_READ32      43
#define SYS_MMIO_WRITE32     44
#define SYS_DOS_GETVOLUMEINFO 45
#define SYS_DMA_SYNC         46
#define SYS_BOOT_RESOURCE_INFO  47
#define SYS_BOOT_RESOURCE_READ  48
#define SYS_BOOT_RESOURCE_WRITE 49
#define SYS_IMAGE_LOAD           50
#define SYS_IMAGE_UNLOAD         51
#define SYS_DO_IO                52
#define SYS_DELAY_MS             53
#define SYS_ADD_SUPERVISED_TASK  54
#define SYS_TAKE_TASK_EXIT       55
#define SYS_WAIT_SIGNAL          56
#define SYS_MODULE_OPEN          57
#define SYS_MODULE_CALL          58
#define SYS_MODULE_CLOSE         59
#define SYS_MARSFB_DIAG          60

#define EXEC_DMA_SYNC_TO_DEVICE   0
#define EXEC_DMA_SYNC_FROM_DEVICE 1
#define EXEC_DMA_SYNC_BIDIRECTIONAL 2

#endif
