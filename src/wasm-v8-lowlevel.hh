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

auto module_binary_size(v8::Local<v8::Object> module) -> size_t;
auto module_binary(v8::Local<v8::Object> module) -> const byte_t*;

auto extern_kind(v8::Local<v8::Object> external) -> ExternKind;

auto func_instance(v8::Local<v8::Function>) -> v8::Local<v8::Object>;

auto global_get(v8::Local<v8::Object> global) -> Val;
void global_set(v8::Local<v8::Object> global, const Val&);

auto table_get(v8::Local<v8::Object> table, size_t index) -> v8::MaybeLocal<v8::Function>;
auto table_set(v8::Local<v8::Object> table, size_t index, v8::MaybeLocal<v8::Function>) -> bool;
auto table_size(v8::Local<v8::Object> table) -> size_t;
auto table_grow(v8::Local<v8::Object> table, size_t delta) -> bool;

auto memory_data(v8::Local<v8::Object> memory) -> byte_t*;
auto memory_data_size(v8::Local<v8::Object> memory)-> size_t;
auto memory_size(v8::Local<v8::Object> memory) -> Memory::pages_t;
auto memory_grow(v8::Local<v8::Object> memory, Memory::pages_t delta) -> bool;

}  // namespace wasm_v8

#undef own

#endif  // #define __WASM_V8_LOWLEVEL_HH
