# Makes this library usable directly, instead of needing a wrapper in the
# consuming project: as an ESP-IDF component, or in plain CMake through
# add_subdirectory() or FetchContent as the target algo::state-machine.
# Ignored by the Makefile build, which stays the way to build and test this
# library on a host.
if(COMMAND idf_component_register)
    idf_component_register(SRCS "src/fsm.c"
                           INCLUDE_DIRS "include")
    return()
endif()

add_library(algo-state-machine src/fsm.c)
add_library(algo::state-machine ALIAS algo-state-machine)
target_include_directories(algo-state-machine PUBLIC include)
set_target_properties(algo-state-machine PROPERTIES C_STANDARD 99)
