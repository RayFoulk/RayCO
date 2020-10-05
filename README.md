# RayCO
- Ray's C Objects
- A collection of C objects for general use in C/C++ projects
- Written in C99 by choice for environments where C++ is not available or preferred.

# Goals
- Fundamental design goal is to have as few dependencies as possible
- Produce objects that can be used in highly constrained, embedded environments
- Build as both a shared object (.so) and static (.a) library

# Objects
- *chain_t* A doubly-linked-list implementation with 'chain' and 'link' nomenclature
-- Handles arbitrary (homogeneous) payloads, using callbacks to handle payload operations.
-- Automatic garbage collection of payloads
- *bytes_t* Yet another managed string/byte array implementation
-- Nothing fancy, assumes ASCII.  No UTF-8 support.
-- Focus is on just being usable without needing to declare static buffers.
