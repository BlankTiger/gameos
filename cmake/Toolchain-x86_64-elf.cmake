set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_C_COMPILER   x86_64-elf-gcc)
set(CMAKE_CXX_COMPILER x86_64-elf-g++)
set(CMAKE_ASM_COMPILER x86_64-elf-gcc)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_C_FLAGS_INIT   "-ffreestanding -Wall -Wextra -Wno-sign-compare -Wno-narrowing -mno-red-zone")
set(CMAKE_CXX_FLAGS_INIT "-ffreestanding -Wall -Wextra -Wno-sign-compare -Wno-narrowing -fno-exceptions -fno-rtti -mno-red-zone -D_GLIBCXX_NO_ASSERTIONS")
