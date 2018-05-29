#ifndef __WASM_BIN_H
#define __WASM_BIN_H

#include "wasm.h"

#define own

namespace wasm {
namespace bin {

own wasm_importtype_vec_t imports(wasm_byte_vec_t binary);
own wasm_exporttype_vec_t exports(wasm_byte_vec_t binary);

}  // namespace bin
}  // namespace wasm

#endif  // #ifdef __WASM_BIN_H
