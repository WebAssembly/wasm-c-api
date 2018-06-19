#ifndef __WASM_V8_LOWLEVEL_HH
#define __WASM_V8_LOWLEVEL_HH

#include "wasm.hh"
#include "v8.h"

using namespace wasm;

namespace wasm_v8 {

auto foreign_new(v8::Isolate*, void*) -> v8::Local<v8::Value>;
auto foreign_get(v8::Local<v8::Value>) -> void*;

auto func_type(v8::Local<v8::Object> global) -> own<FuncType*>;
auto global_type(v8::Local<v8::Object> global) -> own<GlobalType*>;
auto table_type(v8::Local<v8::Object> table) -> own<TableType*>;
auto memory_type(v8::Local<v8::Object> memory) -> own<MemoryType*>;

auto func_instance(v8::Local<v8::Function>) -> v8::Local<v8::Object>;

}  // namespace wasm_v8

#undef own

#endif  // #define __WASM_V8_LOWLEVEL_HH
