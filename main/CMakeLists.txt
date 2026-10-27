file(GLOB_RECURSE APP_SOURCES
    "core/*.c"
    "ui/*.c"
    "platform/*.c")

idf_component_register(
    SRCS "main.c" ${APP_SOURCES}
    INCLUDE_DIRS "." "core" "ui" "platform"
    EMBED_TXTFILES "www/index.html")

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wno-unused-parameter)
