cmake_minimum_required(VERSION 3.16)
project(Ponedelnik2D LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT ANDROID)
    message(FATAL_ERROR
        "Ponedelnik 2D currently targets native Android/Vulkan. Configure with the Android NDK toolchain, "
        "for example -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-26.")
endif()

if(DEFINED CMAKE_ANDROID_NDK)
    set(PONA_ANDROID_NDK "${CMAKE_ANDROID_NDK}")
elseif(DEFINED ANDROID_NDK)
    set(PONA_ANDROID_NDK "${ANDROID_NDK}")
else()
    message(FATAL_ERROR "The Android NDK path is required to build android_native_app_glue.")
endif()

set(PONA_ANDROID_GLUE "${PONA_ANDROID_NDK}/sources/android/native_app_glue")
if(NOT EXISTS "${PONA_ANDROID_GLUE}/android_native_app_glue.c")
    message(FATAL_ERROR "Android native_app_glue not found at ${PONA_ANDROID_GLUE}")
endif()

find_package(Python3 3.8 COMPONENTS Interpreter REQUIRED)
set(PONA_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/examples/pond2d/main.pona" CACHE FILEPATH "Pona script compiled into the Android engine")
get_filename_component(PONA_SOURCE_ABSOLUTE "${PONA_SOURCE}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
if(NOT EXISTS "${PONA_SOURCE_ABSOLUTE}")
    message(FATAL_ERROR "Pona source script not found: ${PONA_SOURCE_ABSOLUTE}")
endif()
set(PONA_COMPILER "${CMAKE_CURRENT_SOURCE_DIR}/tools/ponac.py")
set(PONA_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(PONA_GENERATED_C "${PONA_GENERATED_DIR}/pona_script.c")
add_custom_command(
    OUTPUT "${PONA_GENERATED_C}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${PONA_GENERATED_DIR}"
    COMMAND "${Python3_EXECUTABLE}" "${PONA_COMPILER}" "${PONA_SOURCE_ABSOLUTE}" -o "${PONA_GENERATED_C}"
    DEPENDS "${PONA_COMPILER}" "${PONA_SOURCE_ABSOLUTE}"
    COMMENT "Compiling Pona source ahead of time to native C"
    VERBATIM
)

set(SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/src")
# Keep the NDK glue outside the project's strict warning policy. NDK revisions
# can add warnings to this upstream source that are unrelated to the engine.
add_library(pona_android_glue OBJECT
    "${PONA_ANDROID_GLUE}/android_native_app_glue.c"
)
set_target_properties(pona_android_glue PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(pona_android_glue PRIVATE "${PONA_ANDROID_GLUE}")
target_compile_options(pona_android_glue PRIVATE -O3)

add_library(ponedelnik_2d SHARED
    "${PONA_GENERATED_C}"
    "${SOURCE_ROOT}/engine2d/api.c"
    "${SOURCE_ROOT}/engine2d/app.c"
    "${SOURCE_ROOT}/engine2d/gc.c"
    "${SOURCE_ROOT}/engine2d/input.c"
    "${SOURCE_ROOT}/engine/runtime.c"
    "${SOURCE_ROOT}/engine/graphics.c"
    "${SOURCE_ROOT}/platform/android/pona_main.c"
    $<TARGET_OBJECTS:pona_android_glue>
)

target_include_directories(ponedelnik_2d PRIVATE
    "${SOURCE_ROOT}"
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PONA_ANDROID_GLUE}"
)
target_compile_options(ponedelnik_2d PRIVATE
    -Wall -Wextra -Werror -Wno-missing-field-initializers -Wno-unused-function -pedantic -O3
)
target_link_options(ponedelnik_2d PRIVATE -Wl,--no-undefined)
target_link_libraries(ponedelnik_2d PRIVATE android log dl m vulkan)
