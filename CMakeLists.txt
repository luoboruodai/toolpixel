cmake_minimum_required(VERSION 3.20)

project(ToonPixel VERSION 0.1.2 LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
if(NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures" FORCE)
endif()
if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "macOS deployment target" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

if(DEFINED ENV{AE_SDK_ROOT})
    set(AE_SDK_DIR "$ENV{AE_SDK_ROOT}" CACHE PATH "After Effects SDK directory")
else()
    set(AE_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/ae-sdk" CACHE PATH "After Effects SDK directory")
endif()
if(NOT EXISTS "${AE_SDK_DIR}/Headers/AE_Effect.h")
    message(FATAL_ERROR "Missing Adobe AE SDK headers: set AE_SDK_ROOT to a licensed SDK checkout")
endif()
set(TOONPIXEL_BUNDLE "${CMAKE_BINARY_DIR}/ToonPixel.plugin")
set(TOONPIXEL_CONTENTS "${TOONPIXEL_BUNDLE}/Contents")

find_library(CORE_SERVICES_FRAMEWORK CoreServices REQUIRED)
find_library(CORE_FOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
find_library(COCOA_FRAMEWORK Cocoa REQUIRED)
enable_language(OBJCXX)

enable_testing()
add_executable(toon_math_smoke src/core/ToonMathSmoke.cpp)
target_include_directories(toon_math_smoke PRIVATE src/core)
add_test(NAME toon_math_smoke COMMAND toon_math_smoke)

add_executable(toon_feature_smoke src/ae/ToonPixelFeatureSmoke.cpp)
target_include_directories(toon_feature_smoke PRIVATE src/core src/ae
    "${AE_SDK_DIR}/Headers" "${AE_SDK_DIR}/Headers/SP" "${AE_SDK_DIR}/Util")
target_compile_options(toon_feature_smoke PRIVATE -Wno-register)
target_link_libraries(toon_feature_smoke PRIVATE "${CORE_SERVICES_FRAMEWORK}")
add_test(NAME toon_feature_smoke COMMAND toon_feature_smoke)

add_library(ToonPixel MODULE src/ae/ToonPixel.cpp src/ae/ToonPixelColorPicker.mm)
target_include_directories(ToonPixel PRIVATE
    src/core
    "${AE_SDK_DIR}"
    "${AE_SDK_DIR}/Headers"
    "${AE_SDK_DIR}/Headers/SP"
    "${AE_SDK_DIR}/Util"
)
target_compile_options(ToonPixel PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-register)
target_compile_definitions(ToonPixel PRIVATE __MACH__ TOONPIXEL_HOST=1)
option(TOONPIXEL_DEBUG_LOG "Enable opt-in native diagnostic logging" OFF)
if(TOONPIXEL_DEBUG_LOG)
    target_compile_definitions(ToonPixel PRIVATE TOONPIXEL_DEBUG_LOG=1)
endif()
set_target_properties(ToonPixel PROPERTIES
    PREFIX ""
    SUFFIX ""
    OUTPUT_NAME "ToonPixel"
    LIBRARY_OUTPUT_DIRECTORY "${TOONPIXEL_CONTENTS}/MacOS"
)
target_link_libraries(ToonPixel PRIVATE "${CORE_SERVICES_FRAMEWORK}" "${CORE_FOUNDATION_FRAMEWORK}" "${COCOA_FRAMEWORK}")

add_executable(toon_native_entry_smoke src/ae/ToonPixelEntrySmoke.cpp)
target_include_directories(toon_native_entry_smoke PRIVATE
    src/core
    "${AE_SDK_DIR}/Headers"
    "${AE_SDK_DIR}/Headers/SP"
    "${AE_SDK_DIR}/Util"
)
target_compile_options(toon_native_entry_smoke PRIVATE -Wno-register)
target_link_libraries(toon_native_entry_smoke PRIVATE "${CMAKE_DL_LIBS}" "${CORE_SERVICES_FRAMEWORK}" "${CORE_FOUNDATION_FRAMEWORK}")
add_dependencies(toon_native_entry_smoke ToonPixel)
add_test(
    NAME toon_native_entry_smoke
    COMMAND toon_native_entry_smoke "$<TARGET_FILE:ToonPixel>"
)

add_executable(toon_registration_smoke src/ae/ToonPixelRegistrationSmoke.cpp)
target_include_directories(toon_registration_smoke PRIVATE
    "${AE_SDK_DIR}/Headers"
    "${AE_SDK_DIR}/Headers/SP"
    "${AE_SDK_DIR}/Util"
)
target_compile_options(toon_registration_smoke PRIVATE -Wno-register)
target_link_libraries(toon_registration_smoke PRIVATE "${CMAKE_DL_LIBS}")
add_dependencies(toon_registration_smoke ToonPixel)
add_test(
    NAME toon_registration_smoke
    COMMAND toon_registration_smoke "$<TARGET_FILE:ToonPixel>" "${TOONPIXEL_CONTENTS}/Resources/ToonPixel.rsrc"
)

file(MAKE_DIRECTORY "${TOONPIXEL_CONTENTS}/Resources")
configure_file(src/ae/Info.plist.in "${TOONPIXEL_CONTENTS}/Info.plist" @ONLY)
file(WRITE "${TOONPIXEL_CONTENTS}/PkgInfo" "eFKTFXTC")

find_program(REZ_EXECUTABLE Rez REQUIRED)
find_program(CODESIGN_EXECUTABLE codesign REQUIRED)
set(TOONPIXEL_RSRC "${TOONPIXEL_CONTENTS}/Resources/ToonPixel.rsrc")
add_custom_command(
    OUTPUT "${TOONPIXEL_RSRC}"
    COMMAND "${REZ_EXECUTABLE}" -useDF -d __MACH__ -d AE_OS_MAC
        -i "${AE_SDK_DIR}/Headers"
        -i "${AE_SDK_DIR}/Resources"
        -i "${CMAKE_CURRENT_SOURCE_DIR}/src/ae"
        -o "${TOONPIXEL_RSRC}"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/ae/ToonPixelPiPL.r"
    DEPENDS
        src/ae/ToonPixelPiPL.r
        "${AE_SDK_DIR}/Resources/AE_General.r"
    VERBATIM
)
add_custom_target(ToonPixelResources ALL DEPENDS "${TOONPIXEL_RSRC}")
add_dependencies(ToonPixel ToonPixelResources)
add_custom_target(ToonPixelSign ALL
    COMMAND "${CODESIGN_EXECUTABLE}" --sign - --force --timestamp=none "${TOONPIXEL_BUNDLE}"
    DEPENDS ToonPixel ToonPixelResources
    VERBATIM
)
