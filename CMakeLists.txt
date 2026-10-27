cmake_minimum_required(VERSION 3.16)
project(PonedelnikMessenger LANGUAGES C)

if(NOT CMAKE_C_COMPILER_ID MATCHES "^(Apple)?Clang$")
    message(FATAL_ERROR
        "Ponedelnik Messenger requires Clang. Reconfigure with CC=clang or use the Android NDK toolchain.")
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT ANDROID)
    message(FATAL_ERROR
        "Ponedelnik Messenger currently targets Android. Configure with the Android NDK toolchain, "
        "for example -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-26.")
endif()

set(SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(ANDROID_GLUE "${ANDROID_NDK}/sources/android/native_app_glue")

add_library(ponedelnik_messenger SHARED
    "${SOURCE_ROOT}/messenger/app.c"
    "${SOURCE_ROOT}/platform/android/messenger_bridge.c"
    "${SOURCE_ROOT}/engine/runtime.c"
    "${SOURCE_ROOT}/platform/android/android_main.c"
    "${ANDROID_GLUE}/android_native_app_glue.c"
)

target_include_directories(ponedelnik_messenger PRIVATE
    "${SOURCE_ROOT}"
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${ANDROID_GLUE}"
)
target_compile_options(ponedelnik_messenger PRIVATE -Wall -Wextra -Werror -pedantic)
target_link_options(ponedelnik_messenger PRIVATE -Wl,--no-undefined)
target_link_libraries(ponedelnik_messenger PRIVATE android log dl m vulkan)
