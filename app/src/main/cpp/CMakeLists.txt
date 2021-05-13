cmake_minimum_required(VERSION 3.4.1)

enable_language(ASM)

# build cpufeatures as a static lib
add_library(cpufeatures STATIC
            ${ANDROID_NDK}/sources/android/cpufeatures/cpu-features.c)

# build app's shared lib
set(distribution_DIR ${CMAKE_SOURCE_DIR}/../../../libs/armeabi-v7a)
include_directories(${CMAKE_SOURCE_DIR}/../../../libs/include)


add_library(minicap SHARED IMPORTED)
set_target_properties(minicap PROPERTIES IMPORTED_LOCATION
        ${distribution_DIR}/minicap.so)

add_library(avcodec SHARED IMPORTED)
set_target_properties(avcodec PROPERTIES IMPORTED_LOCATION
        ${distribution_DIR}/libavcodec.so)

add_library(avdevice SHARED IMPORTED)
set_target_properties(avdevice PROPERTIES IMPORTED_LOCATION
        ${distribution_DIR}/libavdevice.so)

add_library(avfilter SHARED IMPORTED)
set_target_properties(avfilter PROPERTIES IMPORTED_LOCATION
        ${distribution_DIR}/libavfilter.so)

add_library(avformat SHARED IMPORTED)
set_target_properties(avformat PROPERTIES IMPORTED_LOCATION
        ${distribution_DIR}/libavformat.so)

add_library(avutil SHARED IMPORTED)
set_target_properties(avutil PROPERTIES IMPORTED_LOCATION
        ${distribution_DIR}/libavutil.so)

add_library(swresample SHARED IMPORTED)
set_target_properties(swresample PROPERTIES IMPORTED_LOCATION
        ${distribution_DIR}/libswresample.so)

add_library(swscale SHARED IMPORTED)
set_target_properties(swscale PROPERTIES IMPORTED_LOCATION
        ${distribution_DIR}/libswscale.so)

add_library(x264 SHARED IMPORTED)
set_target_properties(x264 PROPERTIES IMPORTED_LOCATION
        ${distribution_DIR}/libx264.so)

add_library(utils SHARED IMPORTED)
set_target_properties(utils PROPERTIES IMPORTED_LOCATION
        ${distribution_DIR}/libutils.so)

add_library(binder SHARED IMPORTED)
set_target_properties(binder PROPERTIES IMPORTED_LOCATION
        ${distribution_DIR}/libbinder.so)

add_library(cutils SHARED IMPORTED)
set_target_properties(cutils PROPERTIES IMPORTED_LOCATION
        ${distribution_DIR}/libcutils.so)

add_library(c++ SHARED IMPORTED)
set_target_properties(c++ PROPERTIES IMPORTED_LOCATION
        ${distribution_DIR}/libc++.so)

add_library(backtrace SHARED IMPORTED)
set_target_properties(backtrace PROPERTIES IMPORTED_LOCATION
        ${distribution_DIR}/libbacktrace.so)

add_library(base SHARED IMPORTED)
set_target_properties(base PROPERTIES IMPORTED_LOCATION
        ${distribution_DIR}/libbase.so)

add_library(unwind SHARED IMPORTED)
set_target_properties(unwind PROPERTIES IMPORTED_LOCATION
        ${distribution_DIR}/libunwind.so)

add_library(lzma SHARED IMPORTED)
set_target_properties(lzma PROPERTIES IMPORTED_LOCATION
        ${distribution_DIR}/liblzma.so)

add_library(postproc SHARED IMPORTED)
set_target_properties(postproc PROPERTIES IMPORTED_LOCATION
        ${distribution_DIR}/libpostproc.so)




# set up neon build flag for file using intrinsics
# name: helloneon-intrinsics.c (It is named EXACTLY as this on disk,
#                              just like a normal source file)
# then set up neon flag for neon files
# [This example only build for armeabi-v7a, x86 could be done the same way]
#
if (${ANDROID_ABI} STREQUAL "armeabi-v7a")
  # make a list of neon files and add neon compiling flags to them
  set(neon_SRCS helloneon-intrinsics.c Neon.S Test.S)

  set_property(SOURCE ${neon_SRCS}
               APPEND_STRING PROPERTY COMPILE_FLAGS " -mcpu=cortex-a7 -mfpu=neon -mfloat-abi=softfp -ffast-math")
  add_definitions("-DHAVE_NEON=1")
elseif (${ANDROID_ABI} STREQUAL "x86")
    set(neon_SRCS helloneon-intrinsics.c)
    set_property(SOURCE ${neon_SRCS} APPEND_STRING PROPERTY COMPILE_FLAGS
        " -mssse3  -Wno-unknown-attributes \
                   -Wno-deprecated-declarations \
                   -Wno-constant-conversion \
                   -Wno-static-in-inline")
    add_definitions(-DHAVE_NEON_X86=1 -DHAVE_NEON=1)
else ()
  set(neon_SRCS)
endif ()

add_library(hello-neon SHARED helloneon.cpp rgb2gray.cpp rgb2yuv.cpp ${neon_SRCS})
target_include_directories(hello-neon PRIVATE
    ${ANDROID_NDK}/sources/android/cpufeatures)

target_link_libraries(
        hello-neon
        android
        cpufeatures
        log
        avcodec
        avdevice
        avfilter
        avformat
        avutil
        swresample
        swscale
        x264
        postproc
        cutils
        c++
        backtrace
        base
        unwind
        lzma
        )

