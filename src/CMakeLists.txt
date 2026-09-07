cmake_minimum_required(VERSION 3.22)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

set(CMAKE_PROJECT_NAME stairwayB)

include("cmake/gcc-arm-none-eabi.cmake")

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

enable_language(C ASM CXX)

add_executable(${CMAKE_PROJECT_NAME})

add_subdirectory(cmake/stm32cubemx)

# FreeRTOS kernel (static allocation only; no heap implementation compiled).
# Kept in a top-level FreeRTOS/ dir, NOT Middlewares/: CubeMX regeneration
# deletes Middlewares/ because the .ioc does not reference that middleware.
set(FREERTOS_SRC_DIR ${CMAKE_SOURCE_DIR}/FreeRTOS/Source)

add_library(freertos_kernel STATIC
    ${FREERTOS_SRC_DIR}/tasks.c
    ${FREERTOS_SRC_DIR}/queue.c
    ${FREERTOS_SRC_DIR}/list.c
    ${FREERTOS_SRC_DIR}/portable/GCC/ARM_CM3/port.c
)
target_include_directories(freertos_kernel PUBLIC
    ${FREERTOS_SRC_DIR}/include
    ${FREERTOS_SRC_DIR}/portable/GCC/ARM_CM3
    ${CMAKE_SOURCE_DIR}/App/config
)
target_link_libraries(freertos_kernel PUBLIC stm32cubemx)

target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE)
file(GLOB_RECURSE APP_Src "${CMAKE_SOURCE_DIR}/App/*.cpp")

target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${APP_Src})
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/App)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE)
target_link_libraries(${CMAKE_PROJECT_NAME} stm32cubemx m freertos_kernel)
