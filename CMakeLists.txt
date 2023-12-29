# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

# try to find existing tinygo installation
execute_process(COMMAND tinygo env TINYGOROOT OUTPUT_VARIABLE TINYGO_ROOT ERROR_VARIABLE err)
if(err)
    message(FATAL_ERROR ${err})
endif()

string(STRIP "${TINYGO_ROOT}" TINYGO_ROOT)
string(REPLACE "\\" "/" TINYGO_ROOT "${TINYGO_ROOT}")
message("Found tinygo at ${TINYGO_ROOT}")

# copy tinygo executable to the local TINYGOROOT
file(COPY "${TINYGO_ROOT}/bin" DESTINATION "${CMAKE_SOURCE_DIR}/hexowl/tinygo/")

# set TINYGOROOT and check
set(TINYGO_ROOT "${CMAKE_SOURCE_DIR}/hexowl/tinygo")
message("TINYGOROOT: ${TINYGO_ROOT}")
execute_process(COMMAND ${CMAKE_COMMAND} -E env TINYGOROOT=${TINYGO_ROOT} tinygo env TINYGOROOT ERROR_VARIABLE err)
if(err)
    message(FATAL_ERROR ${err})
endif()

# create output dir
set(HEXOWL_BINPATH "${CMAKE_CURRENT_BINARY_DIR}/hexowl")
add_custom_command(OUTPUT "${HEXOWL_BINPATH}/.exists" DEPENDS ${TINYGO_ROOT}
    COMMAND touch "${HEXOWL_BINPATH}/.exists")

# build hexowl
set(HEXOWL_OBJNAME goesp.o gostack.o hexowl.o)
add_custom_command(OUTPUT hexowl.o DEPENDS "${HEXOWL_BINPATH}/.exists"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/hexowl/sources"
    COMMAND ${CMAKE_COMMAND} -E env TINYGOROOT=${TINYGO_ROOT}
    tinygo build -target=esp32-lib -o "${HEXOWL_BINPATH}/hexowl.o"
    COMMENT "Building hexowl library")
add_custom_command(OUTPUT goesp.o DEPENDS "${HEXOWL_BINPATH}/.exists"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/hexowl/sources"
    COMMAND xtensa-esp32-elf-gcc -c -o "${HEXOWL_BINPATH}/goesp.o" "${TINYGO_ROOT}/src/device/esp/esp32lib.S"
    COMMENT "Building hexowl library")
add_custom_command(OUTPUT gostack.o DEPENDS "${HEXOWL_BINPATH}/.exists"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/hexowl/sources"
    COMMAND xtensa-esp32-elf-gcc -c -o "${HEXOWL_BINPATH}/gostack.o" "${TINYGO_ROOT}/src/internal/task/task_stack_esp32.S"
    COMMENT "Building hexowl library")

# create static library
set(HEXOWL_LIBNAME libhexowl.a)
add_custom_command(OUTPUT ${HEXOWL_LIBNAME} DEPENDS ${HEXOWL_OBJNAME}
    WORKING_DIRECTORY "${HEXOWL_BINPATH}"
    COMMAND xtensa-esp32-elf-ar.exe -rcv libhexowl.a ${HEXOWL_OBJNAME}
    COMMENT "Archiving hexowl library")

add_custom_target(hexowl DEPENDS ${HEXOWL_LIBNAME})

# link hexowl library
include_directories("${CMAKE_SOURCE_DIR}/hexowl/include")
link_directories(${HEXOWL_BINPATH})
link_libraries(hexowl)

set(PROJECT_VER "v1.0.0-14")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(hard-hexowl)
