cmake_minimum_required(VERSION 3.16)

# S2 uses the ROM USB-OTG CDC driver; keep its pinned compatibility fix
# scoped to this target so other chips retain their SDK components.
if(IDF_TARGET STREQUAL "esp32s2")
    list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/platform/esp32s2/esp_usb_cdc_rom_console")
endif()

option(ESP_SDR_STREAMING "Build the S31 Ethernet/high-speed USB receiver" OFF)
if(ESP_SDR_STREAMING)
    if(NOT IDF_TARGET STREQUAL "esp32s31")
        message(FATAL_ERROR "ESP_SDR_STREAMING requires ESP32-S31")
    endif()
    list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/main/targets/esp32s31/streaming")
else()
    set(ENV{IDF_COMPONENT_MANAGER} "0")
endif()
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
idf_build_set_property(MINIMAL_BUILD ON)
idf_build_set_property(ESP_SDR_STREAMING "${ESP_SDR_STREAMING}")
set(scout_version_header "${CMAKE_CURRENT_SOURCE_DIR}/main/families/c5_c6_c61/scout_version.h")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${scout_version_header}")
file(STRINGS "${scout_version_header}" scout_version_line
    REGEX "^#define SCOUT_BUILD_VERSION \"[0-9.]+\"$")
string(REGEX MATCH "[0-9.]+" PROJECT_VER "${scout_version_line}")
project(fpv_scout)

if(IDF_TARGET STREQUAL "esp32c5" AND CONFIG_ESPSOUP_CYD_UI)
    # ESP-IDF's FatFs config disables exFAT. Use the same SDK config with that
    # flag enabled for the CYD, leaving the installed toolchain untouched.
    idf_component_get_property(fatfs_dir fatfs COMPONENT_DIR)
    idf_component_get_property(fatfs_lib fatfs COMPONENT_LIB)
    set(fatfs_config_path "${fatfs_dir}/src/ffconf.h")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${fatfs_config_path}")
    file(READ "${fatfs_config_path}" fatfs_config)
    string(REGEX REPLACE "#define[ \t]+FF_FS_EXFAT[ \t]+0"
        "#define FF_FS_EXFAT 1" fatfs_exfat_config "${fatfs_config}")
    set(fatfs_header "${CMAKE_BINARY_DIR}/scout_ffconf.h")
    # The exFAT directory reader also evaluates FF_USE_LABEL in a C expression.
    string(APPEND fatfs_exfat_config "\n#ifndef CONFIG_FATFS_USE_LABEL\n"
        "#undef FF_USE_LABEL\n#define FF_USE_LABEL 0\n#endif\n")
    file(CONFIGURE OUTPUT "${fatfs_header}" CONTENT "${fatfs_exfat_config}" @ONLY)
    # Every caller must see the same FatFs struct layout as the library.
    target_compile_options(${fatfs_lib} PUBLIC "SHELL:-include \"${fatfs_header}\"")
endif()
