#include "wasm-v8-lowlevel.hh"

// TODO(v8): if we don't include these, api.h does not compile
#include "objects.h"
#include "objects/bigint.h"
#include "objects/module.h"
#include "objects/shared-function-info.h"
#include "objects/templates.h"
#include "objects/fixed-array.h"
#include "objects/ordered-hash-table.h"
#include "objects/js-promise.h"
#include "objects/js-collection.h"

#include "api.h"
#include "wasm/wasm-objects.h"
#include "wasm/wasm-objects-inl.h"


#define own

namespace wasm_v8 {

// Foreign pointers

v8::Local<v8::Value> v8_foreign_new(v8::Isolate* isolate, void* ptr) {
  auto foreign = v8::FromCData(
    reinterpret_cast<v8::internal::Isolate*>(isolate),
    reinterpret_cast<v8::internal::Address>(ptr)
  );
  return v8::Utils::ToLocal(foreign);
}

void* v8_foreign_get(v8::Local<v8::Value> val) {
  auto addr = v8::ToCData<v8::internal::Address>(*v8::Utils::OpenHandle(*val));
  return reinterpret_cast<void*>(addr);
}


// Types

wasm_mut_t v8_mut_to_wasm(bool is_mutable) {
  return is_mutable ? WASM_VAR : WASM_CONST;
}

own wasm_valtype_t* v8_valtype_to_wasm(v8::internal::wasm::ValueType v8_valtype) {
  switch (v8_valtype) {
    case v8::internal::wasm::kWasmI32: return wasm_valtype_new_i32();
    case v8::internal::wasm::kWasmI64: return wasm_valtype_new_i64();
    case v8::internal::wasm::kWasmF32: return wasm_valtype_new_f32();
    case v8::internal::wasm::kWasmF64: return wasm_valtype_new_f64();
    case v8::internal::wasm::kWasmAnyRef: return wasm_valtype_new_anyref();
    default:
      // TODO(wasm+): support new value types
      assert(false);
  }
}

v8::Local<v8::Object> v8_function_instance(v8::Local<v8::Function> function) {
  auto v8_function = v8::Utils::OpenHandle(*function);
  auto v8_func = v8::internal::Handle<v8::internal::WasmExportedFunction>::cast(v8_function);
  auto index = v8_func->function_index();
  v8::internal::Handle<v8::internal::JSObject> v8_instance(v8_func->instance());
  return v8::Utils::ToLocal(v8_instance);
}

own wasm_globaltype_t* v8_global_type(v8::Local<v8::Object> global) {
  auto v8_object = v8::Utils::OpenHandle<v8::Object, v8::internal::JSReceiver>(global);
  auto v8_global = v8::internal::Handle<v8::internal::WasmGlobalObject>::cast(v8_object);

  auto is_mutable = v8_global->is_mutable();
  auto v8_valtype = v8_global->type();

  return wasm_globaltype_new(v8_valtype_to_wasm(v8_valtype), v8_mut_to_wasm(is_mutable));
}

own wasm_tabletype_t* v8_table_type(v8::Local<v8::Object> table) {
  auto v8_object = v8::Utils::OpenHandle<v8::Object, v8::internal::JSReceiver>(table);
  auto v8_table = v8::internal::Handle<v8::internal::WasmTableObject>::cast(v8_object);

  uint32_t min = v8_table->current_length();
  uint32_t max;
  auto v8_max_obj = v8_table->maximum_length();
  wasm_limits_t limits =
    v8_max_obj->ToUint32(&max) ? wasm_limits(min, max) : wasm_limits_no_max(min);

  // TODO(wasm+): support new element types.
  return wasm_tabletype_new(wasm_valtype_new_funcref(), limits);
}

own wasm_memtype_t* v8_memory_type(v8::Local<v8::Object> memory) {
  auto v8_object = v8::Utils::OpenHandle<v8::Object, v8::internal::JSReceiver>(memory);
  auto v8_memory = v8::internal::Handle<v8::internal::WasmMemoryObject>::cast(v8_object);

  uint32_t min = v8_memory->current_pages();
  wasm_limits_t limits = v8_memory->has_maximum_pages()
    ? wasm_limits(min, v8_memory->maximum_pages()) : wasm_limits_no_max(min);

  return wasm_memtype_new(limits);
}

}  // namespace wasm_v8
