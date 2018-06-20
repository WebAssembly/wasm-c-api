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


namespace wasm_v8 {

// Foreign pointers

auto foreign_new(v8::Isolate* isolate, void* ptr) -> v8::Local<v8::Value> {
  auto foreign = v8::FromCData(
    reinterpret_cast<v8::internal::Isolate*>(isolate),
    reinterpret_cast<v8::internal::Address>(ptr)
  );
  return v8::Utils::ToLocal(foreign);
}

auto foreign_get(v8::Local<v8::Value> val) -> void* {
  auto addr = v8::ToCData<v8::internal::Address>(*v8::Utils::OpenHandle(*val));
  return reinterpret_cast<void*>(addr);
}


// Types

auto v8_mutability_to_wasm(bool is_mutable) -> Mutability {
  return is_mutable ? VAR : CONST;
}

auto v8_valtype_to_wasm(v8::internal::wasm::ValueType v8_valtype) -> own<ValType*> {
  switch (v8_valtype) {
    case v8::internal::wasm::kWasmI32: return wasm::ValType::make(wasm::I32);
    case v8::internal::wasm::kWasmI64: return wasm::ValType::make(wasm::I64);
    case v8::internal::wasm::kWasmF32: return wasm::ValType::make(wasm::F32);
    case v8::internal::wasm::kWasmF64: return wasm::ValType::make(wasm::F64);
    case v8::internal::wasm::kWasmAnyRef: return wasm::ValType::make(wasm::ANYREF);
    default:
      // TODO(wasm+): support new value types
      assert(false);
  }
}

auto v8_functype_to_wasm(const v8::internal::wasm::FunctionSig* v8_funcsig) -> own<FuncType*> {
  auto params = vec<ValType*>::make_uninitialized(v8_funcsig->parameter_count());
  auto results = vec<ValType*>::make_uninitialized(v8_funcsig->return_count());

  for (size_t i = 0; i < params.size(); ++i) {
    params[i] = v8_valtype_to_wasm(v8_funcsig->GetParam(i));
  }
  for (size_t i = 0; i < results.size(); ++i) {
    results[i] = v8_valtype_to_wasm(v8_funcsig->GetReturn(i));
  }

  return FuncType::make(std::move(params), std::move(results));
}


auto func_type(v8::Local<v8::Object> function) -> own<FuncType*> {
  auto v8_object = v8::Utils::OpenHandle<v8::Object, v8::internal::JSReceiver>(function);
  auto v8_function = v8::internal::Handle<v8::internal::WasmExportedFunction>::cast(v8_object);

  v8::internal::wasm::FunctionSig* sig =
    v8_function->instance()->module()->functions[v8_function->function_index()].sig;

  return v8_functype_to_wasm(sig);
}

auto global_type(v8::Local<v8::Object> global) -> own<GlobalType*> {
  auto v8_object = v8::Utils::OpenHandle<v8::Object, v8::internal::JSReceiver>(global);
  auto v8_global = v8::internal::Handle<v8::internal::WasmGlobalObject>::cast(v8_object);

  auto is_mutable = v8_global->is_mutable();
  auto v8_valtype = v8_global->type();

  return GlobalType::make(v8_valtype_to_wasm(v8_valtype), v8_mutability_to_wasm(is_mutable));
}

auto table_type(v8::Local<v8::Object> table) -> own<TableType*> {
  auto v8_object = v8::Utils::OpenHandle<v8::Object, v8::internal::JSReceiver>(table);
  auto v8_table = v8::internal::Handle<v8::internal::WasmTableObject>::cast(v8_object);

  uint32_t min = v8_table->current_length();
  uint32_t max;
  auto v8_max_obj = v8_table->maximum_length();
  Limits limits = v8_max_obj->ToUint32(&max) ? Limits(min, max) : Limits(min);

  // TODO(wasm+): support new element types.
  return TableType::make(ValType::make(FUNCREF), limits);
}

auto memory_type(v8::Local<v8::Object> memory) -> own<MemoryType*> {
  auto v8_object = v8::Utils::OpenHandle<v8::Object, v8::internal::JSReceiver>(memory);
  auto v8_memory = v8::internal::Handle<v8::internal::WasmMemoryObject>::cast(v8_object);

  uint32_t min = v8_memory->current_pages();
  Limits limits = v8_memory->has_maximum_pages()
    ? Limits(min, v8_memory->maximum_pages()) : Limits(min);

  return MemoryType::make(limits);
}


// Modules

auto module_binary_size(v8::Local<v8::Object> module) -> size_t {
  auto v8_object = v8::Utils::OpenHandle<v8::Object, v8::internal::JSReceiver>(module);
  auto v8_module = v8::internal::Handle<v8::internal::WasmModuleObject>::cast(v8_object);
  return v8_module->shared()->module_bytes()->length();
}

auto module_binary(v8::Local<v8::Object> module) -> const byte_t* {
  auto v8_object = v8::Utils::OpenHandle<v8::Object, v8::internal::JSReceiver>(module);
  auto v8_module = v8::internal::Handle<v8::internal::WasmModuleObject>::cast(v8_object);
  return reinterpret_cast<byte_t*>(v8_module->shared()->module_bytes()->GetChars());
}


// Instance

auto instance_exports(v8::Local<v8::Object> instance) -> v8::Local<v8::Object> {
  auto v8_object = v8::Utils::OpenHandle<v8::Object, v8::internal::JSReceiver>(instance);
  auto v8_instance = v8::internal::Handle<v8::internal::WasmInstanceObject>::cast(v8_object);
  auto v8_exports = handle(v8_instance->exports_object());
  return v8::Utils::ToLocal(v8_exports);
}


// Externals

auto extern_kind(v8::Local<v8::Object> external) -> ExternKind {
  auto v8_object = v8::Utils::OpenHandle<v8::Object, v8::internal::JSReceiver>(external);

  if (v8::internal::WasmExportedFunction::IsWasmExportedFunction(*v8_object)) return EXTERN_FUNC;
  if (v8_object->IsWasmGlobalObject()) return EXTERN_GLOBAL;
  if (v8_object->IsWasmTableObject()) return EXTERN_TABLE;
  if (v8_object->IsWasmMemoryObject()) return EXTERN_MEMORY;
  assert(false);
}


// Functions

/*
auto func_make(
  v8::Local<v8::Function> function, const own<FuncType*>& type
) -> own<Func*> {
  auto v8_function = v8::Utils::OpenHandle<v8::Object, v8::internal::JSFunction>(function);
  auto isolate = v8_function->GetIsolate();
  auto factory = isolate->factory();

  // Create a module as in CompileToModuleObjectInternal
  auto managed = v8::internal::Handle<v8::internal::Foreign>();  // TODO?
  auto bytes = v8::internal::Handle<v8::internal::SeqOneByteString>();
  auto script = v8::internal::Handle<v8::internal::Script>();
  auto offsets = v8::internal::Handle<v8::internal::ByteArray>();
  auto shared = wasm::internal::WasmSharedModuleData::New(
      isolate, managed, bytes, script, offsets);

  // Create instance as in InstanceBuilder::Build
  CodeSpaceMemoryModificationScope modification_scope(isolate->heap());

  auto compiled = nullptr;
  auto shared = nullptr;
  auto exports = v8::FixedArray
  auto module = v8::WasmModuleObject::New(
    isolate, compiled,
    Handle<FixedArray> export_wrappers, Handle<WasmSharedModuleData> shared) {

  auto instance =
    v8::WasmInstanceObject::New(isolate, module, compiled);
  auto weak_instance = factory->NewWeakCell(instance);


  auto wrapper = v8::wasm::compiler::CompileWasmToJSWrapper(
              isolate_, js_receiver, expected_sig, func_index,
              module_->origin(), use_trap_handler());
          RecordStats(*wrapper_code, counters());

          WasmCode* wasm_code = native_module->AddCodeCopy(
              wrapper_code, wasm::WasmCode::kWasmToJsWrapper, func_index);
          ImportedFunctionEntry entry(instance, func_index);
          entry.set_wasm_to_js(*js_receiver, wasm_code);

}
*/

auto func_instance(v8::Local<v8::Function> function) -> v8::Local<v8::Object> {
  auto v8_function = v8::Utils::OpenHandle(*function);
  auto v8_func = v8::internal::Handle<v8::internal::WasmExportedFunction>::cast(v8_function);
  auto index = v8_func->function_index();
  v8::internal::Handle<v8::internal::JSObject> v8_instance(v8_func->instance());
  return v8::Utils::ToLocal(v8_instance);
}


// Globals

auto global_get(v8::Local<v8::Object> global) -> Val {
  auto v8_object = v8::Utils::OpenHandle<v8::Object, v8::internal::JSReceiver>(global);
  auto v8_global = v8::internal::Handle<v8::internal::WasmGlobalObject>::cast(v8_object);
  switch (v8_global->type()) {
    case v8::internal::wasm::kWasmI32: return Val(v8_global->GetI32());
    case v8::internal::wasm::kWasmI64: return Val(v8_global->GetI64());
    case v8::internal::wasm::kWasmF32: return Val(v8_global->GetF32());
    case v8::internal::wasm::kWasmF64: return Val(v8_global->GetF64());
    default:
      // TODO(wasm+): support new value types
      assert(false);
  }
}

void global_set(v8::Local<v8::Object> global, const Val& val) {
  auto v8_object = v8::Utils::OpenHandle<v8::Object, v8::internal::JSReceiver>(global);
  auto v8_global = v8::internal::Handle<v8::internal::WasmGlobalObject>::cast(v8_object);
  switch (val.kind()) {
    case I32: return v8_global->SetI32(val.i32());
    case I64: return v8_global->SetI64(val.i64());
    case F32: return v8_global->SetF32(val.f32());
    case F64: return v8_global->SetF64(val.f64());
    default:
      // TODO(wasm+): support new value types
      assert(false);
  }
}


// Tables

auto table_get(v8::Local<v8::Object> table, size_t index) -> v8::MaybeLocal<v8::Function> {
  auto v8_object = v8::Utils::OpenHandle<v8::Object, v8::internal::JSReceiver>(table);
  auto v8_table = v8::internal::Handle<v8::internal::WasmTableObject>::cast(v8_object);
  if (index > v8_table->current_length()) return v8::Local<v8::Function>();
  v8::internal::Handle<v8::internal::Object> v8_value(
    v8_table->functions()->get(static_cast<int>(index)), v8_table->GetIsolate());
  return v8_value->IsNull(v8_table->GetIsolate())
    ? v8::MaybeLocal<v8::Function>()
    : v8::MaybeLocal<v8::Function>(
        v8::Utils::ToLocal(v8::internal::Handle<v8::internal::JSFunction>::cast(v8_value)));
}

auto table_set(v8::Local<v8::Object> table, size_t index, v8::MaybeLocal<v8::Function> maybe) -> bool {
  auto v8_object = v8::Utils::OpenHandle<v8::Object, v8::internal::JSReceiver>(table);
  auto v8_table = v8::internal::Handle<v8::internal::WasmTableObject>::cast(v8_object);
  auto v8_function = maybe.IsEmpty()
    ? v8::internal::Handle<v8::internal::JSFunction>::null()
    : v8::internal::Handle<v8::internal::JSFunction>::cast(
        v8::Utils::OpenHandle<v8::Function, v8::internal::JSReceiver>(maybe.ToLocalChecked()));
  if (index > v8_table->current_length()) return false;
  // TODO: failure?
  v8::internal::WasmTableObject::Set(v8_table->GetIsolate(), v8_table,
    static_cast<uint32_t>(index), v8_function);
  return true;
}

auto table_size(v8::Local<v8::Object> table) -> size_t {
  auto v8_object = v8::Utils::OpenHandle<v8::Object, v8::internal::JSReceiver>(table);
  auto v8_table = v8::internal::Handle<v8::internal::WasmTableObject>::cast(v8_object);
  return v8_table->current_length();
}

auto table_grow(v8::Local<v8::Object> table, size_t delta) -> bool {
  auto v8_object = v8::Utils::OpenHandle<v8::Object, v8::internal::JSReceiver>(table);
  auto v8_table = v8::internal::Handle<v8::internal::WasmTableObject>::cast(v8_object);
  if (delta > 0xffffffffu) return false;
  // TODO: failure?
  v8_table->Grow(v8_table->GetIsolate(), static_cast<uint32_t>(delta));
  return true;
}


// Memory

auto memory_data(v8::Local<v8::Object> memory) -> byte_t* {
  auto v8_object = v8::Utils::OpenHandle<v8::Object, v8::internal::JSReceiver>(memory);
  auto v8_memory = v8::internal::Handle<v8::internal::WasmMemoryObject>::cast(v8_object);
  return reinterpret_cast<byte_t*>(v8_memory->array_buffer()->allocation_base());
}

auto memory_data_size(v8::Local<v8::Object> memory)-> size_t {
  auto v8_object = v8::Utils::OpenHandle<v8::Object, v8::internal::JSReceiver>(memory);
  auto v8_memory = v8::internal::Handle<v8::internal::WasmMemoryObject>::cast(v8_object);
  return v8_memory->array_buffer()->allocation_length();
}

auto memory_size(v8::Local<v8::Object> memory) -> Memory::pages_t {
  auto v8_object = v8::Utils::OpenHandle<v8::Object, v8::internal::JSReceiver>(memory);
  auto v8_memory = v8::internal::Handle<v8::internal::WasmMemoryObject>::cast(v8_object);
  return v8_memory->current_pages();
}

auto memory_grow(v8::Local<v8::Object> memory, Memory::pages_t delta) -> bool {
  auto v8_object = v8::Utils::OpenHandle<v8::Object, v8::internal::JSReceiver>(memory);
  auto v8_memory = v8::internal::Handle<v8::internal::WasmMemoryObject>::cast(v8_object);
  auto old = v8::internal::WasmMemoryObject::Grow(
    v8_memory->GetIsolate(), v8_memory, delta);
  return old != -1;
}

}  // namespace wasm_v8
