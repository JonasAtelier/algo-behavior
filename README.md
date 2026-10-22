# algo-behavior

<p align="center">
  <img alt="platform: portable C99" src="https://img.shields.io/badge/platform-portable%20C99-70e1b2?style=flat-square&labelColor=0d2238">
  <img alt="language: C" src="https://img.shields.io/badge/language-C-70e1b2?style=flat-square&labelColor=0d2238">
  <img alt="license: MIT" src="https://img.shields.io/badge/license-MIT-f2bd68?style=flat-square&labelColor=0d2238">
</p>

Decision-making for robots — two small libraries in portable C99, each one
header and one `.c` file, no `malloc`. Fine on a microcontroller.

## Libraries

| Library | What it does | Pick it when |
|---|---|---|
| [behavior-tree](behavior-tree) | A behaviour tree: selectors, sequences, conditions, actions | priorities apply in every state — a low-battery check that must always win |
| [state-machine](state-machine) | A table-driven finite state machine | a handful of clear states and the events that move between them |

## Use one library

Every library stands on its own: no shared headers, nothing from one library
needed by another. Take the one you need and leave the rest. Three ways in.

**Copy the files.** `state-machine/include/fsm.h` and `state-machine/src/fsm.c`
into your project, built with your own sources. That is the whole install.

**CMake.** Every library is its own target, `algo::<folder>`. Add just the
folder you need:

```cmake
add_subdirectory(algo-behavior/state-machine)
target_link_libraries(app PRIVATE algo::state-machine)
```

or, without a copy in your tree:

```cmake
include(FetchContent)
FetchContent_Declare(algo_state_machine
    GIT_REPOSITORY https://github.com/JonasAtelier/algo-behavior.git
    GIT_TAG        stable
    SOURCE_SUBDIR  state-machine)
FetchContent_MakeAvailable(algo_state_machine)
target_link_libraries(app PRIVATE algo::state-machine)
```

`add_subdirectory(algo-behavior)`, or FetchContent without `SOURCE_SUBDIR`, adds
every library's target instead; link the ones you use. Examples and
tests only build when this repo is the top-level project.

**ESP-IDF.** Point `EXTRA_COMPONENT_DIRS` at one library's folder. The
component is named after the folder:

```cmake
# top-level CMakeLists.txt of your IDF project, before project()
list(APPEND EXTRA_COMPONENT_DIRS path/to/algo-behavior/state-machine)
```

then `REQUIRES state-machine` in your component's `idf_component_register()`.

Pointing it at the repo root instead gives you **one** component holding
every library, named after the folder (`algo-behavior`): IDF does not look
inside a folder that has its own `CMakeLists.txt`. Use one or the other,
never both — every symbol would be defined twice.

## Build and test

```sh
make check    # every library's own tests, stopping at the first failure
make clean
```

Or with CMake and CTest. CTest runs the C tests only; the C++ linkage tests
run under `make check`.

```sh
cmake -B build && cmake --build build -j2 && ctest --test-dir build
```

## License

MIT. See [LICENSE](LICENSE).
