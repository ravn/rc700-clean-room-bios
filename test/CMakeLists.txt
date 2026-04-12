function(add_bios_test name)
    add_executable(${name} ${name}.c hal_mock.c)
    target_link_libraries(${name} PRIVATE bios_logic)
    target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_bios_test(test_iobyte)
add_bios_test(test_sector)
add_bios_test(test_dpb)
add_bios_test(test_config)
add_bios_test(test_deblock)
add_bios_test(test_console)
add_bios_test(test_chartab)
add_bios_test(test_ringbuf)
add_bios_test(test_serial)

# Hardware init test — provides its own HAL to capture port writes
add_executable(test_hwinit test_hwinit.c)
target_link_libraries(test_hwinit PRIVATE bios_logic)
target_include_directories(test_hwinit PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME test_hwinit COMMAND test_hwinit)

# Interrupt ISR test — provides its own HAL to capture port writes
add_executable(test_interrupt test_interrupt.c)
target_link_libraries(test_interrupt PRIVATE bios_logic)
target_include_directories(test_interrupt PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME test_interrupt COMMAND test_interrupt)

add_bios_test(test_jtvars)

# FDC simulator test — includes fdc_sim.c directly
add_executable(test_fdc_sim test_fdc_sim.c fdc_sim.c hal_mock.c)
target_link_libraries(test_fdc_sim PRIVATE bios_logic)
target_include_directories(test_fdc_sim PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME test_fdc_sim COMMAND test_fdc_sim)

# IMD disk image reader test
add_executable(test_imd test_imd.c imd.c hal_mock.c)
target_link_libraries(test_imd PRIVATE bios_logic)
target_include_directories(test_imd PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(test_imd PRIVATE TEST_IMAGE="${CMAKE_SOURCE_DIR}/disks/SW1711-I8.imd")
add_test(NAME test_imd COMMAND test_imd)

# Floppy driver integration test — provides its own HAL, links FDC IMD adapter
add_executable(test_floppy test_floppy.c fdc_imd_adapter.c imd.c)
target_link_libraries(test_floppy PRIVATE bios_logic)
target_include_directories(test_floppy PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(test_floppy PRIVATE TEST_IMAGE="${CMAKE_SOURCE_DIR}/disks/SW1711-I8.imd")
add_test(NAME test_floppy COMMAND test_floppy)

# Warm boot integration test — full disk I/O path with real IMD image
add_executable(test_warmboot test_warmboot.c fdc_imd_adapter.c imd.c)
target_link_libraries(test_warmboot PRIVATE bios_logic)
target_include_directories(test_warmboot PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(test_warmboot PRIVATE TEST_IMAGE="${CMAKE_SOURCE_DIR}/disks/SW1711-I8.imd")
add_test(NAME test_warmboot COMMAND test_warmboot)
