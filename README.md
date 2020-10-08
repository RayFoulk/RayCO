# RayCO
- Ray's C Objects
- A collection of C objects for general use in C/C++ projects
- Written in C99 by choice for environments where C++ is not available or not preferred.
- Builds as both a shared object (.so) and static (.a) library
- Integrated unit tests
- All code will be written in Allman Style
  - https://en.wikipedia.org/wiki/Indentation_style#Allman_style
  - Will use 'astyle --style=allman <file>' for formatting imported code

# Goals
- Fundamental design goal is to have as few dependencies and inter-dependencies as possible
- Will depend (at least initially) on a subset of libc calls
- To produce a set of objects that can be used in highly constrained, embedded environments

# Objects
- **chain_t** A doubly-linked-list implementation using 'chain' and 'link' nomenclature
  - Handles arbitrary (homogeneous) payloads, using void * callbacks to handle payload operations
  - Automatic garbage collection of payloads.  Payloads may be managed or unmanaged
- **bytes_t** Yet another managed string/byte array implementation
  - Nothing super fancy: Assumes ASCII, No UTF-8 support
  - Can be used either as an arbitrary byte array buffer or a null-terminated C string
  - Main focus is just on being usable without needing to declare static buffers: 80% rule in effect here
