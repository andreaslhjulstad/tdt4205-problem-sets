cmake_minimum_required(VERSION 3.0)
project(AssemblyExamples)

enable_language(ASM)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -z noexecstack")

# Workaround to convince ARM macs to assemble and link x86
if(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "arch -x86_64")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK "arch -x86_64")
    set(CMAKE_ASM_COMPILER_TARGET "x86_64-apple-darwin")
endif()

file(GLOB ASSEMBLY_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.S")

foreach(ASSEMBLY_SOURCE ${ASSEMBLY_SOURCES})
    get_filename_component(ASSEMBLY_TARGET ${ASSEMBLY_SOURCE} NAME_WE)
    add_executable(${ASSEMBLY_TARGET} ${ASSEMBLY_SOURCE})
endforeach()
