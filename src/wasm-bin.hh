#ifndef __WASM_BIN_H
#define __WASM_BIN_H

#include "wasm.h"
#include <tuple>

#define own

namespace wasm {
namespace bin {

std::tuple<own wasm_importtype_vec_t, own wasm_exporttype_vec_t>
imports_exports(wasm_byte_vec_t binary);

}  // namespace bin
}  // namespace wasm

#endif  // #ifdef __WASM_BIN_H
