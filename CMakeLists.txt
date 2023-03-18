cmake_minimum_required(VERSION 3.12)

# A list of all the different examples; each will build a uf2
set(EXAMPLES rain)

# Overall project name, used to hold all our examples.
set(NAME unicorn-cpp-examples)

# Configure some hardware specific bits
set(PICO_BOARD pico_w)
include(pimoroni_pico_import.cmake)
include(pico_sdk_import.cmake)

# Basic project setup
project(${NAME} C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the SDK
pico_sdk_init()

# Include required libraries
# This assumes `pimoroni-pico` is stored alongside your project
include(libraries/bitmap_fonts/bitmap_fonts)
include(libraries/pico_graphics/pico_graphics)
include(libraries/galactic_unicorn/galactic_unicorn)

# Assemble each example details
foreach(EXAMPLE IN LISTS EXAMPLES)

    # Add an executable target for each named file. 
    add_executable(${EXAMPLE} ${EXAMPLE}.cpp)

    # Link some suitable default libraries and headers to it.
    target_include_directories(${EXAMPLE} PRIVATE ${CMAKE_CURRENT_LIST_DIR})
    target_link_libraries(${EXAMPLE} 
                          pico_stdlib pico_cyw43_arch_lwip_threadsafe_background
                          hardware_pio hardware_adc hardware_dma
                          pico_graphics galactic_unicorn)

    # And some other Pico-related setup.
    pico_enable_stdio_usb(${EXAMPLE} 1)
    pico_add_extra_outputs(${EXAMPLE})

    # Install junk too.
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${EXAMPLE}.uf2 DESTINATION .)

endforeach()

# Add any other files needed for release
install(FILES
    ${CMAKE_CURRENT_LIST_DIR}/README.md
    ${CMAKE_CURRENT_LIST_DIR}/LICENSE
    DESTINATION .
)

set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
set(CPACK_GENERATOR "ZIP" "TGZ")
include(CPack)
