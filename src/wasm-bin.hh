#ifndef __WASM_BIN_H
#define __WASM_BIN_H

#include "wasm.h"

#define own

namespace wasm {
namespace bin {

own wasm_functype_vec_t types(wasm_byte_vec_t binary);
own wasm_functype_vec_t funcs(wasm_byte_vec_t binary);
own wasm_globaltype_vec_t globals(wasm_byte_vec_t binary);
own wasm_tabletype_vec_t tables(wasm_byte_vec_t binary);
own wasm_memtype_vec_t memories(wasm_byte_vec_t binary);

}  // namespace bin
}  // namespace wasm

#endif  // #ifdef __WASM_BIN_H
