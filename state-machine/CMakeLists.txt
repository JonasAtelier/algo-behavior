# Makes this repo usable as an ESP-IDF component directly, instead of needing a
# wrapper in the consuming project. Ignored by the Makefile build, which stays
# the way to build and test this library on a host.
idf_component_register(SRCS "src/fsm.c"
                       INCLUDE_DIRS "include")
