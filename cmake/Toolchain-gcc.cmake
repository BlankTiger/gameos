set(CMAKE_SYSTEM_NAME Linux)

set(CMAKE_C_COMPILER gcc)
set(CMAKE_CXX_COMPILER g++)

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    set(CMAKE_EXE_LINKER_FLAGS_INIT    "-fuse-ld=mold")
    set(CMAKE_SHARED_LINKER_FLAGS_INIT "-fuse-ld=mold")
    set(CMAKE_MODULE_LINKER_FLAGS_INIT "-fuse-ld=mold")
endif()
