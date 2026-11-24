idf_build_get_property(streaming ESP_SDR_STREAMING)
if(streaming)
    idf_component_register(REQUIRES streaming)
    return()
endif()
set(esp_sdr_supported_targets esp32 esp32c2 esp32c3 esp32c5 esp32c6 esp32c61 esp32h2 esp32s2 esp32s3 esp32s31)
if(NOT IDF_TARGET IN_LIST esp_sdr_supported_targets)
    message(FATAL_ERROR "No ESP-SDR RF backend for ${IDF_TARGET}. Supported targets: ${esp_sdr_supported_targets}")
endif()

option(SAMPLE_RATE_PROBE "Build volatile hardware sample-rate diagnostics (never package)" OFF)

set(target_dir "targets/${IDF_TARGET}")
set(target_includes "${target_dir}")
if(IDF_TARGET STREQUAL "esp32s31")
    set(target_dir "${target_dir}/burst")
    list(APPEND target_includes "${target_dir}")
endif()
set(receiver "${target_dir}/receiver.c")
set(sources "common/burst_serial.c" "common/spectrum_stats.c")
set(dependencies esp_phy esp_wifi nvs_flash esp_timer esp_driver_uart esp-dsp)
option(RING_PROBE "Build bounded experimental ring diagnostics (never package)" OFF)
if(IDF_TARGET STREQUAL "esp32c61" OR IDF_TARGET STREQUAL "esp32c6" OR IDF_TARGET STREQUAL "esp32c3")
    list(APPEND sources "common/ring_capture.c")
endif()
if(IDF_TARGET STREQUAL "esp32c2")
    list(APPEND sources "${target_dir}/heap_guard.c")
endif()
set(probes RING_PROBE)
set(fragments)
if(NOT IDF_TARGET STREQUAL "esp32s3")
    list(APPEND sources "common/spectrum.c")
endif()

if(IDF_TARGET STREQUAL "esp32s2")
    list(APPEND dependencies esp_usb_cdc_rom_console)
    list(APPEND probes S2_RF_PROBE)
elseif(NOT IDF_TARGET STREQUAL "esp32" AND NOT IDF_TARGET STREQUAL "esp32c2")
    list(APPEND dependencies esp_driver_usb_serial_jtag)
endif()

if(IDF_TARGET STREQUAL "esp32s3")
    list(APPEND sources "common/ring_capture.c" "${target_dir}/s3_fft_rnd.S" "${target_dir}/s3_unpack.S" "${target_dir}/s3_core1.S" "${target_dir}/s3_firdot.S")
    # SPEC-mode FFT; pinned submodule, the component manager stays disabled.
    list(APPEND dependencies esp-dsp)
    list(APPEND fragments "${target_dir}/linker.lf")
    list(APPEND probes SAMPLE_RATE_PROBE FILTER_REGISTER_PROBE S3_RF_PROBE)
elseif(IDF_TARGET STREQUAL "esp32c5" OR IDF_TARGET STREQUAL "esp32c6" OR IDF_TARGET STREQUAL "esp32c61")
    set(receiver "families/c5_c6_c61/receiver.c")
    if(IDF_TARGET STREQUAL "esp32c6" AND FILTER_REGISTER_PROBE)
        message(FATAL_ERROR "Filter register diagnostics are not characterized for C6")
    endif()
    list(APPEND probes SAMPLE_RATE_PROBE FILTER_REGISTER_PROBE C5_TUNE_PROBE)
    if(IDF_TARGET STREQUAL "esp32c5")
        if(NOT DEFINED SCOUT_RUST_CORE AND CONFIG_ESPSOUP_CYD_UI)
            set(SCOUT_RUST_CORE ON)
        endif()
        # ESPsoup extensions: on-chip temperature sensor, PARLIO RX + GPIO for the
        # gap-free capture path, GDMA (async memcpy) for the gap diagnostics.
        list(APPEND dependencies esp_driver_tsens esp_driver_parlio esp_driver_gpio esp_driver_dma)
        # RockBase NM-CYD-C5 touch-screen UI (CONFIG_ESPSOUP_CYD_UI, sdkconfig.defaults.cyd-c5).
        list(APPEND dependencies cyd_c5 bt)
        list(APPEND sources "families/c5_c6_c61/composite/bench_hw.c")
        if(NOT SCOUT_RUST_CORE)
            list(APPEND sources "families/c5_c6_c61/composite/bench.c"
                 "families/c5_c6_c61/composite/cvbs.c" "families/c5_c6_c61/composite/patterns.c")
        endif()
        option(C5_GAP_PROBE "Build C5 dump-bank gap diagnostics (never package)" OFF)
        option(SCOUT_VIDEO_PROBE "Enable wired video frame readback for bench testing" OFF)
        list(APPEND probes C5_GAP_PROBE SCOUT_VIDEO_PROBE)
    endif()
endif()

if(IDF_TARGET STREQUAL "esp32s31")
    list(APPEND sources "${target_dir}/s31_fft_rnd.S" "${target_dir}/s31_unpack.S")
    list(APPEND fragments "${target_dir}/linker.lf")
endif()

if(IDF_TARGET STREQUAL "esp32c61" OR IDF_TARGET STREQUAL "esp32s31")
    list(APPEND sources "common/burst_gain_table.c")
endif()

idf_component_register(SRCS "${receiver}" ${sources}
    LDFRAGMENTS ${fragments}
    PRIV_INCLUDE_DIRS "common" ${target_includes} "diagnostics"
    REQUIRES ${dependencies})

# Keep the linker guard beside the backend whose SRAM aperture it protects.
target_link_options(${COMPONENT_LIB} INTERFACE
    "-T${CMAKE_CURRENT_LIST_DIR}/${target_dir}/sram_guard.ld")
if(IDF_TARGET STREQUAL "esp32c61" OR IDF_TARGET STREQUAL "esp32s31")
    target_link_options(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=phy_write_gain_mem")
endif()
if(IDF_TARGET STREQUAL "esp32c2")
    target_link_options(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=soc_get_available_memory_regions")
endif()
foreach(probe IN LISTS probes)
    if(${probe})
        target_compile_definitions(${COMPONENT_LIB} PRIVATE "${probe}=1")
    endif()
endforeach()

if(SCOUT_RUST_CORE)
    if(NOT IDF_TARGET STREQUAL "esp32c5")
        message(FATAL_ERROR "SCOUT_RUST_CORE requires ESP32-C5")
    endif()
    find_program(CARGO_EXECUTABLE cargo REQUIRED)
    set(rust_dir "${CMAKE_CURRENT_LIST_DIR}/../rust-scout")
    set(rust_out "${CMAKE_BINARY_DIR}/rust-target")
    set(rust_lib "${rust_out}/riscv32imac-unknown-none-elf/release/libfpv_scout_core.a")
    add_custom_command(OUTPUT "${rust_lib}"
        COMMAND ${CMAKE_COMMAND} -E env "CARGO_TARGET_DIR=${rust_out}"
            "${CARGO_EXECUTABLE}" build --manifest-path "${rust_dir}/Cargo.toml"
            --target riscv32imac-unknown-none-elf --features device-bench --release --locked --offline
        DEPENDS "${rust_dir}/Cargo.toml" "${rust_dir}/Cargo.lock" "${rust_dir}/src/lib.rs"
                "${rust_dir}/src/composite.rs" "${rust_dir}/src/bench.rs" "${rust_dir}/src/digital.rs"
                "${rust_dir}/src/remote_id.rs"
        VERBATIM)
    add_custom_target(scout_rust_build DEPENDS "${rust_lib}")
    add_dependencies(${COMPONENT_LIB} scout_rust_build)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE SCOUT_RUST_CORE=1)
    # The Rust archive follows libmain. Include the bridge object when libmain is scanned.
    target_link_options(${COMPONENT_LIB} INTERFACE "LINKER:--undefined=scout_bench_hw_open")
    target_link_libraries(${COMPONENT_LIB} INTERFACE "${rust_lib}")
endif()
