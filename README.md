# algo-behavior

Choose what a robot does next with two small C99 libraries.

## Try the example

From this repository’s root, use Make and a C99 compiler. No robot hardware is needed.

```sh
make -C state-machine run
```

The example models a turnstile. Watch how inserting a coin unlocks it and how pushing it changes the state. [Example source](state-machine/examples/turnstile.c)

## Choose a library

| Library | What it does |
| --- | --- |
| [state-machine](state-machine/) | Describe modes such as locked and unlocked, and the events that switch between them. |
| [behavior-tree](behavior-tree/) | Check conditions in priority order, such as checking the battery before continuing a patrol. |

## Add it to your project

For plain C, compile the source files in the library’s `src/` directory and add its `include/` directory to your compiler’s include path. For a CMake application named `app`, place this checkout inside your project and use the following lines.

```cmake
add_subdirectory(algo-behavior/state-machine)
target_link_libraries(app PRIVATE algo::state-machine)
```

For ESP-IDF, add the library directory to `EXTRA_COMPONENT_DIRS` and its name to the consuming component’s `REQUIRES`. Select either the individual libraries or the entire repository to avoid duplicate symbols.

<details>
<summary>Fetch just this library from GitHub with CMake</summary>

```cmake
include(FetchContent)
FetchContent_Declare(algo_state_machine
    GIT_REPOSITORY https://github.com/JonasAtelier/algo-behavior.git
    GIT_TAG        stable
    SOURCE_SUBDIR  state-machine)
FetchContent_MakeAvailable(algo_state_machine)
target_link_libraries(app PRIVATE algo::state-machine)
```

</details>

Run `make check` from the repository root to test the libraries. [MIT license](LICENSE)
