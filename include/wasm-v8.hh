// V8 vendor-specific extensions to WebAssembly C++ API

#ifndef __WASM_V8_HH
#define __WASM_V8_HH

#include "wasm.hh"


namespace wasm {
namespace v8 {

///////////////////////////////////////////////////////////////////////////////

namespace Memory {
  using grow_callback_t = auto (*)(void*, byte_t*, size_t, size_t) -> byte_t*;
  using free_callback_t = void (*)(void*, byte_t*, size_t);

  // Create a Memory backed by an external storage.
  // For a memory type with limits.min = S, It is the callers responsibility to
  // * provide a readable and writable, zeroed byte array of size S
  // * install an inaccessible redzone address range of size redzone_size_lo(S)
  //   right before the byte vector's address range
  // * install an inaccessible redzone address range of size redzone_size_hi(S)
  //   right after the byte vector's address range
  // * optionally, provide a `grow_callback` that is invoked by the engine when
  //   the Memory needs to grow; it receives the current byte vector, its
  //   current size, and the new size requested (it is an invariant that
  //   new size > old size when invoked); it needs to return the address
  //   of a new byte vector with redzones installed as before, or `nullptr`to
  //   reject the request; the new byte vector can be the same as the old if the
  //   host is able to grow it in place; if not, it is the host's responsibility
  //   to copy the contents from the old to the new vector; the additional
  //   bytes must be zeroed; if no `grow_callback` is provided, all grow
  //   requests for the Memory will be rejected except if the delta is zero
  // * optionally, provide a `free_callback` that is invoked by the engine when
  //   the Memory is no longer needed; it receives the current byte vector and
  //   the current size; when invoked, the host can free the bytee vector and
  //   associated redzones
  // * optionally, provide an additional parameter that is stored by the engine
  //   and passed on to the callbacks as their first argument; the host should
  //   free any associated allocation in thee `free_callback`
  auto make_external(
    Store*, const MemoryType*, byte_t*,
    grow_callback_t = nullptr, free_callback_t = nullptr, void* = nullptr
  ) -> own<wasm::Memory>;

  auto redzone_size_lo(size_t) -> size_t;
  auto redzone_size_hi(size_t) -> size_t;
}

///////////////////////////////////////////////////////////////////////////////

}  // namespace v8
}  // namespace wasm

#endif  // #ifdef __WASM_V8_HH
