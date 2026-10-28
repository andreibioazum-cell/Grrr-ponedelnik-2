cmake_minimum_required(VERSION 3.16)
project(PonedelnikMessenger LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT ANDROID)
    message(FATAL_ERROR
        "Ponedelnik Messenger targets native Android/Vulkan. Configure with the Android NDK toolchain, "
        "for example -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-26.")
endif()

if(DEFINED CMAKE_ANDROID_NDK)
    set(MESSENGER_ANDROID_NDK "${CMAKE_ANDROID_NDK}")
elseif(DEFINED ANDROID_NDK)
    set(MESSENGER_ANDROID_NDK "${ANDROID_NDK}")
else()
    message(FATAL_ERROR "The Android NDK path is required to build android_native_app_glue.")
endif()

set(ANDROID_GLUE "${MESSENGER_ANDROID_NDK}/sources/android/native_app_glue")
if(NOT EXISTS "${ANDROID_GLUE}/android_native_app_glue.c")
    message(FATAL_ERROR "Android native_app_glue not found at ${ANDROID_GLUE}")
endif()

set(SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/src")
# Keep upstream NDK glue outside the project's strict warning policy.
add_library(android_native_app_glue OBJECT
    "${ANDROID_GLUE}/android_native_app_glue.c"
)
set_target_properties(android_native_app_glue PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(android_native_app_glue PRIVATE "${ANDROID_GLUE}")
target_compile_options(android_native_app_glue PRIVATE -O3)

add_library(ponedelnik_messenger SHARED
    "${SOURCE_ROOT}/messenger/app.c"
    "${SOURCE_ROOT}/platform/android/messenger_bridge.c"
    "${SOURCE_ROOT}/engine/runtime.c"
    "${SOURCE_ROOT}/platform/android/android_main.c"
    $<TARGET_OBJECTS:android_native_app_glue>
)

target_include_directories(ponedelnik_messenger PRIVATE
    "${SOURCE_ROOT}"
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${ANDROID_GLUE}"
)
target_compile_options(ponedelnik_messenger PRIVATE
    -Wall -Wextra -Werror -Wno-missing-field-initializers -Wno-unused-function -pedantic -O3
)
target_link_options(ponedelnik_messenger PRIVATE -Wl,--no-undefined)
target_link_libraries(ponedelnik_messenger PRIVATE android log dl m vulkan)
