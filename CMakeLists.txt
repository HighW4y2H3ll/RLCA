
###############################################################
##      Compile DynamoRIO Version
##

#set(CMAKE_C_COMPILER "../../Android/android-ndk-r12/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64/bin/arm-linux-androideabi-gcc")
#set(CMAKE_C_FLAGS "--sysroot=../../Android/android-ndk-r12/platforms/android-19/arch-arm -w")
set (CMAKE_C_FLAGS "-w")
cmake_minimum_required(VERSION 3.0)

if (NOT DEFINED DynamoRIO_DIR)
    set(DynamoRIO_DIR "dynamorio/cmake" CACHE PATH
    #set(DynamoRIO_DIR "../../test/dynamorio/cmake" CACHE PATH
    "DynamoRIO installation's cmake directory")
endif (NOT DEFINED DynamoRIO_DIR)

function (add_client name source_file_list extension_list)
    add_library(${name} SHARED ${source_file_list})
    configure_DynamoRIO_client(${name})
    foreach (ext ${extension_list})
        use_DynamoRIO_extension(${name} ${ext})
    endforeach (ext)
endfunction (add_client)

find_package(DynamoRIO)
if (NOT DynamoRIO_FOUND)
    message(FATAL_ERROR "DynamoRIO package required to build")
endif(NOT DynamoRIO_FOUND)



add_definitions(-DDEBUG=ON)
add_definitions(-DBRK_ON)
#add_definitions(-DNUL_RIO)
add_definitions(-DRAND_OFF)

add_definitions(-DX86_64)
#add_definitions(-DX86)
add_definitions(-DLINUX)
add_library(utils.o OBJECT utils.c)
add_library(bitmap.o OBJECT bitmap.c)
add_library(base.o OBJECT base.c utils)
add_library(parsemap.o OBJECT parsemap.c base utils)
add_library(memtrace.o OBJECT memtrace.c parsemap base utils)
add_library(mmap.o OBJECT mmap.c parsemap base utils)
add_library(brk.o OBJECT brk.c memtrace mmap base utils)
add_library(threadlocal.o OBJECT threadlocal.c bitmap)
add_library(drmain SHARED drmain.c mmap brk memtrace threadlocal bitmap parsemap base utils)
configure_DynamoRIO_client(drmain)
use_DynamoRIO_extension(drmain drmgr)
use_DynamoRIO_extension(drmain drutil)
#add_client(drmain "drmain.c;mmap" "drmgr")
#add_dependencies(mmap base utils)

###############################################################
##      Compile Ptrace Version
##
#add_library(mmap.o OBJECT mmap.c)
#add_library(ptraceutil.o OBJECT ptraceutil.c)
#add_executable(main ptracemain.c ptraceutil mmap)
