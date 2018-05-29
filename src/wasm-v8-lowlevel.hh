#ifndef __WASM_V8_LOWLEVEL_H
#define __WASM_V8_LOWLEVEL_H

#include "wasm.h"
#include "v8.h"

#define own

namespace wasm_v8 {

v8::Local<v8::Value> v8_foreign_new(v8::Isolate*, void*);
void* v8_foreign_get(v8::Local<v8::Value>);

v8::Local<v8::Object> v8_function_instance(v8::Local<v8::Function>);
own wasm_globaltype_t* v8_global_type(v8::Local<v8::Object> global);
own wasm_tabletype_t* v8_table_type(v8::Local<v8::Object> table);
own wasm_memtype_t* v8_memory_type(v8::Local<v8::Object> memory);

}  // namespace wasm_v8

#undef own

#endif  // #define __WASM_V8_LOWLEVEL_H
