# robin

Robin aims to be a single-header MIDI-compliant frequency modulation synthesizer. This repository also contains configuration for it to play General MIDI audio, and a command-line interface for testing purposes.

## Usage

You can use Robin by placing `robin.h` in your project folder and including it. You need to include it once with the `RBN_IMPLEMENTATION` macro defined to actually define the functions, while - without the macro defined - it will only declare the robin's structures and functions.

```
#define RBN_IMPLEMENTATION
#include "robin.h"
```

## General MIDI configuration

To use the General MIDI configuration, include `robin_general.h` in place of `robin.h` (it depends on `robin.h` being in the same directory).

```
#define RBN_IMPLEMENTATION
#define RBN_GENERAL_IMPLEMENTATION
#include "robin_general.h"
```

## Command-Line Interface

### Building

You can use CMake to build the [cli/](cli) source folder.
