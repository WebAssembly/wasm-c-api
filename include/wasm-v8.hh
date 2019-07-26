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

  auto make_external(
    Store*, const MemoryType*, byte_t*, void*, grow_callback_t, free_callback_t
  ) -> own<wasm::Memory>;

  auto redzone_size_lo(size_t) -> size_t;
  auto redzone_size_hi(size_t) -> size_t;
}

///////////////////////////////////////////////////////////////////////////////

}  // namespace v8
}  // namespace wasm

#endif  // #ifdef __WASM_V8_HH
