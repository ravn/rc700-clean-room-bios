function(add_bios_test name)
    add_executable(${name} ${name}.c hal_mock.c)
    target_link_libraries(${name} PRIVATE bios_logic)
    target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_bios_test(test_iobyte)
