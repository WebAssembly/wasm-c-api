# WebAssembly C and C++ API 

Work in progress! No docs yet.


### Interfaces

* C API:

  * See `example/*.c` for example usages.

  * See `include/wasm.h` for interface.

* C++ API:

  * See `example/*.cc` for example usages.

  * See `include/wasm.hh` for interface.


### Implementation

* A prototype implementation based on V8 is in `src`.

  * Note that this requires adding a module to V8, so it patches V8's build file.

* C API is implemented on top of C++ API.

* See `Makefile` for build recipe. Canonical steps to run examples:

  1. `make v8-checkout`
  2. `make v8`
  3. `make all`


### Limitations

V8 implementation:

* Currently requires patching V8 by adding a module.

* Host functions (Func::make) create a JavaScript function internally, since V8 cannot handle raw C imports yet.

* As a consequence, does not support i64 in external calls or host functions.

* Also cannot handle multiple values in external calls or host functions.

* Host functions and host globals are created through auxiliary modules constructed on the fly, to work around limitations in JS API.


### TODO

V8 implementation:

  * Use reference counting and caching for types?

Possible API tweaks:

  * Distinguish vec and own_vec in C++ API

  * Handle constness of vectors properly in C API

  * Find a way to perform C callbacks through C++ without extra wrapper?

  * Add iterators to `vec` class?
