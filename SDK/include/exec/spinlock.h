/* Architecture binding; implementations preserved from the source trees. */
#if defined(ARCH_ARM64) || defined(EXEC64_BINDING_ARM64)
#include "../../arch/arm64/include/exec/spinlock.h"
#elif defined(ARCH_RISCV64) || defined(EXEC64_BINDING_RISCV64)
#include "../../arch/riscv64/include/exec/spinlock.h"
#else
#error "Select ARCH_ARM64 or ARCH_RISCV64"
#endif
