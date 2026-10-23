cmake_minimum_required(VERSION 3.16)
project(Geometrium LANGUAGES C CXX)

if(NOT CMAKE_C_COMPILER_ID MATCHES "^(Apple)?Clang$")
    message(FATAL_ERROR
        "Geometrium builds with Clang only. Reconfigure with CC=clang (or use "
        "the Android NDK toolchain, which provides Clang).")
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT ANDROID)
    message(FATAL_ERROR
        "Geometrium builds natively for Android. Configure CMake with the Android "
        "NDK toolchain, for example: cmake -B build "
        "-DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake "
        "-DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-26")
endif()

set(GEO_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(GEO_ANDROID_GLUE "${ANDROID_NDK}/sources/android/native_app_glue")

add_library(geometrium SHARED
    "${GEO_SOURCE_ROOT}/game/game.c"
    "${GEO_SOURCE_ROOT}/game/cpp/ui_fx.cpp"
    "${GEO_SOURCE_ROOT}/engine/runtime.c"
    "${GEO_SOURCE_ROOT}/platform/android/android_main.c"
    "${GEO_ANDROID_GLUE}/android_native_app_glue.c"
)

target_include_directories(geometrium PRIVATE
    "${GEO_SOURCE_ROOT}"
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${GEO_ANDROID_GLUE}"
)
target_link_libraries(geometrium PRIVATE android log dl m vulkan)
