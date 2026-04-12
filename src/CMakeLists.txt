set(BIOS_LOGIC_SOURCES
    iobyte.c
    sector.c
    dpb.c
    config.c
    deblock.c
    console.c
    chartab.c
    ringbuf.c
    serial.c
)

if(TARGET_Z80)
    add_executable(bios
        ${BIOS_LOGIC_SOURCES}
        hal_z80.c
        main.c
    )
else()
    add_library(bios_logic STATIC ${BIOS_LOGIC_SOURCES})
    target_include_directories(bios_logic PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
endif()
