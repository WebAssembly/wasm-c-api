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

    * global::get, global::set
    * table::get, table::set, table::size, table::grow
    * memory::data, memory::data_size, memory::size, memory::grow
    * module::serialize, module::deserialize
    * multiple return values

  * Simplify reference wrappers to be plain persistent handles

    * Move host information to V8 object (func callback & env)
    * Compute reflection on demand

* Possible API tweaks:

  * Find a way to perform C callbacks through C++ without extra wrapper?

  * Possible renamings?

    * `externkind`, `externtype` to `externalkind`, `externaltype`?
    * `memtype` to `memorytype`?
    * CamlCase class names in C++ API?

  * Add iterators to `vec` class?
