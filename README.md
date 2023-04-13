# RayCO
- Ray's C Objects
- A collection of C objects for general use in C/C++ projects
- Written in (mostly) C99 by choice for constrained environments
- Builds as both a shared object (.so) and static (.a) library
- Integrated unit tests using MUT
- All code will be written in Allman Style
  - https://en.wikipedia.org/wiki/Indentation_style#Allman_style
  - Will use 'astyle --style=allman <file>' for formatting

# Goals
- Remain MIT and/or BSD license-compatible.
  - No GPL of any version shall be linked to this project.
- Fundamental design goal is to have as few dependencies and inter-dependencies as possible
  - Will depend (at least initially) on a subset of libc calls
  - May provide (or use) an optional limited implementation of libc subset later.  musl?
- To produce a set of objects that can be used in highly constrained, embedded environments
  - For example, ability to port to C64 via CC65 compiler (later versions TBD)
  - Heap allocation (malloc, realloc, free) WILL be used.
    - For platforms that don't have these there will be implementations imported or provided.

# Objects
- **chain_t** A doubly-linked-list implementation using 'chain' and 'link' nomenclature
  - Handles arbitrary (homogeneous) payloads, using void * callbacks to handle payload operations
  - Automatic garbage collection of payloads.  Payloads may be managed or unmanaged
- **collect_t** Similar to chain_t but can handle heterogeneous payloads (e.g. a set)
  - Type determination of payloads is left entirely to the user.

- **bytes_t** Yet another managed string/byte-array implementation
  - Nothing super fancy: Assumes ASCII, No UTF-8 support
  - Can be used either as an arbitrary byte array buffer or a null-terminated C string
  - Main focus is just on being usable without needing to declare static buffers: 80% rule in effect here
- **chronom_t** A chronometer for tracking elapsed time
  - Depends on libc struct timespec, breaking strict C99 requirement
- **scallop_t** A simple and flexible Command Line Interface (CLI)
  - Somewhat declarative interface: Nested keyword and callback registration
  - Optionally uses 'linenoise' submodule for tab completion, argument hints, and command history.
  - Intended to be re-used by multiple other planned projects

# Future Plans
- **bits_t** A C-only re-implementation of BitDeque
  - Essentially an arbitrary-precision unsigned integer
  - Can be used to handle dynamic (runtime-defined) bit fields
  - Useful for compression, encoding, serial comms

