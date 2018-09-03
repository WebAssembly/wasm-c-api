# WebAssembly C and C++ API 

Work in progress! No docs yet.


### Design Goals

* Provide a "black box" API for embedding a Wasm engine in other applications.

  * Be completely agnostic to VM specifics.

  * "White box" interoperability with environment (such as combined GC) is not a current goal (and *much* more difficult to achieve).

* Allow creation of bindings for other languages through their foreign function interfaces.

  * Support a pure C API.

  * Mostly manual memory management.

* Avoid language features raising barriers to use.

  * No exceptions or post-C++11 features in C++ API.

  * No passing of structs by-value or post-C99 features in C API.

* Achieve link-time compatibility between different implementations.

  * All classes can be instantiated through factory methods only and have completely abstract implementations.


### Interfaces

* C++ API:

  * See `include/wasm.hh` for interface.

  * See `example/*.cc` for example usages.

* C API:

  * See `include/wasm.h` for interface.

  * See `example/*.c` for example usages.

Some random explanations:

* The VM must be initialised by creating an instance of an *engine* (`wasm::Engine`/`wasm_engine_t`) and is shut down by deleting it. An instance may only be created once per a process.

* All runtime objects are tied to a specific *store* (`wasm::Store`/`wasm_store_t`). Multiple stores can be created, but their objects cannot interact. Every store and its objects must only be accessed in a single thread.

* To exchange module objects between threads, create a *shared* module (`wasm::Shared<Module>`/`wasm_shared_module_t`). Other objects cannot be shared in current Wasm.

* *Vector* structures (`wasm::vec<X>`/`wasm_x_vec_t`) are lightweight abstractions of a pair of a plain array and its length. The C++ API does not use `std::vector` because that does not support adopting pre-existing arrays.

* *References* may be implemented by indirections, which may or may not be cached. Thus, pointer equality cannot be used to check reference equality. However, `nullptr`/`NULL` represents null references.

* The API already encompasses current proposals like [multiple return values](https://github.com/WebAssembly/multi-value/blob/master/proposals/multi-value/Overview.md) and [reference types](https://github.com/WebAssembly/reference-types/blob/master/proposals/reference-types/Overview.md), but not [threads](https://github.com/WebAssembly/threads/blob/master/proposals/threads/Overview.md).


### Implementation

* A prototype implementation based on V8 is in `src`.

  * Note that this requires adding a module to V8, so it patches V8's build file.

* The C API is implemented on top of the C++ API.

* See `Makefile` for build recipe. Canonical steps to run examples:

  1. `make v8-checkout`
  2. `make v8`
  3. `make all`


#### Limitations

V8 implementation:

* Currently requires patching V8 by adding a module.

* Host functions (`Func::make`) create a JavaScript function internally, since V8 cannot handle raw C imports yet.

* As a consequence, does not support i64 in external calls or host functions.

* Also cannot handle multiple values in external calls or host functions.

* Host functions and host globals are created through auxiliary modules constructed on the fly, to work around limitations in JS API.

* `Shared<Module>` is currently implemented via serialisation, since V8 does not currently have direct support for cross-isolate sharing.


### TODO

Possible API tweaks:

  * Add `Ref::eq` (or better, a subclass `EqRef::eq`) for reference equality?

  * Avoid allocation for `Result` objects by making them into out parameters?

  * Use `restrict` in C API?

  * Distinguish vec and own_vec in C++ API?

  * Handle constness of vectors properly in C API?

  * Find a way to perform C callbacks through C++ without extra wrapper?

  * Add iterators to `vec` class?

V8 implementation:

  * Find a way to avoid external calls through JS?

  * Use reference counting and caching for types?
