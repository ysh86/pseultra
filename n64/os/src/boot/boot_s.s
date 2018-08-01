#
# pseultra/n64/os/src/boot/boot_s.s
# OS boot code
# 
# (C) pseudophpt 2018 
#

.include "n64.s.h"

.extern __osBoot
.global _boot

# Boot process
_boot:

# Use OS boot stack
lui $sp, %hi(__osBootStack + 0x1000)
addiu $sp, %lo(__osBootStack + 0x1000)

# Jump to OS boot process
lui $t0, %hi(__osBoot)
addiu $t0, %lo(__osBoot)

jr $t0

# OS Boot Stack
.lcomm __osBootStack, 0x1000