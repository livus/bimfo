# Host build of the device-independent parts: parsers, config, board logic and
# the LVGL UI. Produces
#   bimfo_tests   unit tests against captured API responses (host/fixtures)
#   bimfo_render  renders every screen/theme to PNG files in host/out
#   bimfo_fuzz    feeds truncated/mutated payloads to the parsers
# See host/README.md.
cmake_minimum_required(VERSION 3.16)
project(bimfo_host C)

set(CMAKE_C_STANDARD 11)
get_filename_component(REPO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(MAIN_DIR "${REPO_DIR}/main")

include(FetchContent)

# --- LVGL 8.3 (same version as the firmware) ---------------------------------
set(LVGL_DIR "" CACHE PATH "LVGL 8.3 source tree (defaults to managed_components or a download)")
if(NOT LVGL_DIR AND EXISTS "${REPO_DIR}/managed_components/lvgl__lvgl/lvgl.h")
    set(LVGL_DIR "${REPO_DIR}/managed_components/lvgl__lvgl")
endif()
if(NOT LVGL_DIR)
    FetchContent_Declare(lvgl GIT_REPOSITORY https://github.com/lvgl/lvgl.git GIT_TAG v8.3.11 GIT_SHALLOW TRUE)
    FetchContent_GetProperties(lvgl)
    if(NOT lvgl_POPULATED)
        FetchContent_Populate(lvgl)
    endif()
    set(LVGL_DIR "${lvgl_SOURCE_DIR}")
endif()
message(STATUS "LVGL: ${LVGL_DIR}")

file(GLOB_RECURSE LVGL_SOURCES "${LVGL_DIR}/src/*.c")
add_library(lvgl STATIC ${LVGL_SOURCES})
target_include_directories(lvgl PUBLIC "${LVGL_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(lvgl PUBLIC LV_CONF_INCLUDE_SIMPLE LV_LVGL_H_INCLUDE_SIMPLE)
target_compile_options(lvgl PRIVATE -w)

# --- cJSON (ESP-IDF's copy when available) -----------------------------------
set(CJSON_DIR "" CACHE PATH "Directory containing cJSON.c/cJSON.h")
if(NOT CJSON_DIR AND DEFINED ENV{IDF_PATH} AND EXISTS "$ENV{IDF_PATH}/components/json/cJSON/cJSON.c")
    set(CJSON_DIR "$ENV{IDF_PATH}/components/json/cJSON")
endif()
if(NOT CJSON_DIR)
    FetchContent_Declare(cjson GIT_REPOSITORY https://github.com/DaveGamble/cJSON.git GIT_TAG v1.7.18 GIT_SHALLOW TRUE)
    FetchContent_GetProperties(cjson)
    if(NOT cjson_POPULATED)
        FetchContent_Populate(cjson)
    endif()
    set(CJSON_DIR "${cjson_SOURCE_DIR}")
endif()
message(STATUS "cJSON: ${CJSON_DIR}")

# --- Device-independent application code -------------------------------------
file(GLOB CORE_SOURCES "${MAIN_DIR}/core/*.c")
add_library(core STATIC ${CORE_SOURCES} "${CJSON_DIR}/cJSON.c")
target_include_directories(core PUBLIC "${MAIN_DIR}/core" "${CJSON_DIR}")
target_compile_options(core PRIVATE -Wall -Wextra -Wno-unused-parameter)

file(GLOB UI_SOURCES "${MAIN_DIR}/ui/*.c" "${MAIN_DIR}/ui/fonts/*.c")
add_library(ui STATIC ${UI_SOURCES})
target_include_directories(ui PUBLIC "${MAIN_DIR}/ui")
target_link_libraries(ui PUBLIC core lvgl)
target_compile_options(ui PRIVATE -Wall -Wno-unused-parameter)

add_compile_definitions(FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
                        OUT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/out")

add_executable(bimfo_tests tests.c fixture.c)
target_link_libraries(bimfo_tests core)

add_executable(bimfo_fuzz fuzz.c fixture.c)
target_link_libraries(bimfo_fuzz core)

add_executable(bimfo_render render.c fixture.c png.c)
target_link_libraries(bimfo_render ui)

enable_testing()
add_test(NAME unit COMMAND bimfo_tests)
add_test(NAME fuzz COMMAND bimfo_fuzz)
add_test(NAME render COMMAND bimfo_render)

add_executable(bimfo_parse parse_cli.c)
target_link_libraries(bimfo_parse core)
