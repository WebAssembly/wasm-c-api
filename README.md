# WebAssembly C and C++ API 

Work in progress. No docs yet.

* C API:

  * See `example/hello.c` for example usage.

  * See `include/wasm.h` for interface.

* C++ API:

  * See `example/hello.cc` for example usage.

  * See `include/wasm.hh` for interface.

* A half-complete implementation based on V8 is in `src`.

* C API is build on top of C++ API.

* See `Makefile` for build recipe.

* TODO in V8 implementation:

  * Replace use of JS API with V8 internal

  * Implement missing functionality through V8 internals

    * Global::get, Global::set
    * Table::get, Table::set, Table::size, Table::grow
    * Memory::data, Memory::data_size, Memory::size, Memory::grow
    * Module::serialize, Module::deserialize
    * multiple return values

  * Simplify reference wrappers to be plain persistent handles

    * Move host information to V8 object (func callback & env)
    * Compute reflection on demand

  * Use reference counting and caching for types

  * Add a memory debug mode?

* Possible API tweaks:

  * Handle constness of vectors properly in C API

  * Find a way to perform C callbacks through C++ without extra wrapper?

  * Add iterators to `vec` class?
