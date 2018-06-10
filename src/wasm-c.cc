#include "wasm.h"
#include "wasm.hh"

using namespace wasm;

extern "C" {

///////////////////////////////////////////////////////////////////////////////
// Auxiliaries

// Backing implementation

extern "C++" {

template<class T>
struct borrowed {
  own<T> it;
  borrowed(T x) : it(x) {}
  borrowed(borrowed<T>&& that) : it(std::move(that)) {}
  ~borrowed() { it.release(); }
};

template<class T>
struct borrowed_vec {
  vec<T> it;
  borrowed_vec(vec<T>&& v) : it(v.release()) {}
  borrowed_vec(borrowed_vec<T>&& that) : it(std::move(that)) {}
  ~borrowed_vec() { it.release(); }
};

}  // extern "C++"


#define WASM_DEFINE_OWN(name) \
  struct wasm_##name##_t : wasm::name {}; \
  \
  void wasm_##name##_delete(wasm_##name##_t* x) { \
    delete x; \
  } \
  \
  extern "C++" inline auto hide(wasm::name* x) -> wasm_##name##_t* { \
    return static_cast<wasm_##name##_t*>(x); \
  } \
  extern "C++" inline auto reveal(wasm_##name##_t* x) -> wasm::name* { \
    return x; \
  } \
  extern "C++" inline auto get(own<wasm::name*>& x) -> wasm_##name##_t* { \
    return hide(x.get()); \
  } \
  extern "C++" inline auto release(own<wasm::name*>&& x) -> wasm_##name##_t* { \
    return hide(x.release()); \
  } \
  extern "C++" inline auto adopt(wasm_##name##_t* x) -> own<wasm::name*> { \
    return make_own(x); \
  } \
  extern "C++" inline auto borrow(wasm_##name##_t* x) -> borrowed<name*> { \
    return borrowed<name*>(x); \
  }


// Vectors

#define WASM_DEFINE_VEC_BASE(name, ptr_or_none) \
  extern "C++" inline auto hide(name ptr_or_none* v) -> wasm_##name##_t ptr_or_none* { \
    return reinterpret_cast<wasm_##name##_t ptr_or_none*>(v); \
  } \
  extern "C++" inline auto reveal(wasm_##name##_t ptr_or_none* v) -> name ptr_or_none* { \
    return reinterpret_cast<name ptr_or_none*>(v); \
  } \
  extern "C++" inline auto get(wasm::vec<name ptr_or_none>& v) -> wasm_##name##_vec_t { \
    return wasm_##name##_vec(v.size(), hide(v.get())); \
  } \
  extern "C++" inline auto release(wasm::vec<name ptr_or_none>&& v) -> wasm_##name##_vec_t { \
    return wasm_##name##_vec(v.size(), hide(v.release())); \
  } \
  extern "C++" inline auto adopt(wasm_##name##_vec_t v) -> wasm::vec<name ptr_or_none> { \
    return wasm::vec<name ptr_or_none>::adopt(v.size, reveal(v.data)); \
  } \
  extern "C++" inline auto borrow_vec(wasm_##name##_vec_t v) -> borrowed_vec<name ptr_or_none> { \
    return borrowed_vec<name ptr_or_none>(wasm::vec<name ptr_or_none>::adopt(v.size, reveal(v.data))); \
  } \
  \
  wasm_##name##_vec_t wasm_##name##_vec_new_uninitialized(size_t size) { \
    return release(wasm::vec<name ptr_or_none>::make_uninitialized(size)); \
  } \
  wasm_##name##_vec_t wasm_##name##_vec_new_empty() { \
    return wasm_##name##_vec_new_uninitialized(0); \
  }

// Vectors with no ownership management of elements
#define WASM_DEFINE_VEC_PLAIN(name, ptr_or_none) \
  WASM_DEFINE_VEC_BASE(name, ptr_or_none) \
  \
  wasm_##name##_vec_t wasm_##name##_vec_new(size_t size, wasm_##name##_t ptr_or_none const data[]) { \
    auto v2 = wasm::vec<name ptr_or_none>::make_uninitialized(size); \
    if (v2.size() != 0) memcpy(v2.get(), data, size * sizeof(wasm_##name##_t ptr_or_none)); \
    return release(std::move(v2)); \
  } \
  \
  wasm_##name##_vec_t wasm_##name##_vec_clone(wasm_##name##_vec_t v) { \
    return wasm_##name##_vec_new(v.size, v.data); \
  } \
  \
  void wasm_##name##_vec_delete(wasm_##name##_vec_t v) { \
    if (v.data) delete[] v.data; \
  }

// Vectors who own their elements
#define WASM_DEFINE_VEC(name, ptr_or_none) \
  WASM_DEFINE_VEC_BASE(name, ptr_or_none) \
  \
  wasm_##name##_vec_t wasm_##name##_vec_new(size_t size, wasm_##name##_t ptr_or_none const data[]) { \
    auto v2 = wasm::vec<name ptr_or_none>::make_uninitialized(size); \
    for (size_t i = 0; i < v2.size(); ++i) { \
      v2[i] = adopt(data[i]); \
    } \
    return release(std::move(v2)); \
  } \
  \
  wasm_##name##_vec_t wasm_##name##_vec_clone(wasm_##name##_vec_t v) { \
    auto v2 = wasm::vec<name ptr_or_none>::make_uninitialized(v.size); \
    for (size_t i = 0; i < v2.size(); ++i) { \
      v2[i] = v.data[i]->clone(); \
    } \
    return release(std::move(v2)); \
  } \
  \
  void wasm_##name##_vec_delete(wasm_##name##_vec_t v) { \
    if (v.data) { \
      for (size_t i = 0; i < v.size; ++i) { \
        if (v.data[i]) wasm_##name##_delete(v.data[i]); \
      } \
      delete[] v.data; \
    } \
  }


// Byte vectors

using byte = byte_t;
WASM_DEFINE_VEC_PLAIN(byte, )


///////////////////////////////////////////////////////////////////////////////
// Runtime Environment

// Configuration

WASM_DEFINE_OWN(config)

wasm_config_t* wasm_config_new() {
  return release(config::make());
}


// Engine

WASM_DEFINE_OWN(engine)

wasm_engine_t* wasm_engine_new(int argc, const char *const argv[]) {
  return release(engine::make(argc, argv));
}

wasm_engine_t* wasm_engine_new_with_config(
  int argc, const char *const argv[], wasm_config_t* config
) {
  return release(engine::make(argc, argv, adopt(config)));
}


// Stores

WASM_DEFINE_OWN(store)

wasm_store_t* wasm_store_new(wasm_engine_t* engine) {
  auto engine_ = borrow(engine);
  return release(store::make(engine_.it));
};


///////////////////////////////////////////////////////////////////////////////
// Type Representations

// Type attributes

extern "C++" inline auto hide(wasm::mut mut) -> wasm_mut_t {
  return static_cast<wasm_mut_t>(mut);
}

extern "C++" inline auto reveal(wasm_mut_t mut) -> wasm::mut {
  return static_cast<wasm::mut>(mut);
}


extern "C++" inline auto hide(wasm::limits limits) -> wasm_limits_t {
  return wasm_limits(limits.min, limits.max);
}

extern "C++" inline auto reveal(wasm_limits_t limits) -> wasm::limits {
  return wasm::limits(limits.min, limits.max);
}


extern "C++" inline auto hide(wasm::valkind kind) -> wasm_valkind_t {
  return static_cast<wasm_valkind_t>(kind);
}

extern "C++" inline auto reveal(wasm_valkind_t kind) -> wasm::valkind {
  return static_cast<wasm::valkind>(kind);
}


extern "C++" inline auto hide(wasm::externkind kind) -> wasm_externkind_t {
  return static_cast<wasm_externkind_t>(kind);
}

extern "C++" inline auto reveal(wasm_externkind_t kind) -> wasm::externkind {
  return static_cast<wasm::externkind>(kind);
}



// Generic

#define WASM_DEFINE_TYPE(name) \
  WASM_DEFINE_OWN(name) \
  WASM_DEFINE_VEC(name, *) \
  \
  wasm_##name##_t* wasm_##name##_clone(wasm_##name##_t* t) { \
    return release(t->clone()); \
  }


// Value Types

WASM_DEFINE_TYPE(valtype)

wasm_valtype_t* wasm_valtype_new(wasm_valkind_t k) {
  return release(valtype::make(reveal(k)));
}

wasm_valkind_t wasm_valtype_kind(wasm_valtype_t* t) {
  return hide(t->kind());
}


// Function Types

WASM_DEFINE_TYPE(functype)

wasm_functype_t* wasm_functype_new(wasm_valtype_vec_t params, wasm_valtype_vec_t results) {
  return release(functype::make(adopt(params), adopt(results)));
}

wasm_valtype_vec_t wasm_functype_params(wasm_functype_t* ft) {
  return get(ft->params());
}

wasm_valtype_vec_t wasm_functype_results(wasm_functype_t* ft) {
  return get(ft->results());
}


// Global Types

WASM_DEFINE_TYPE(globaltype)

wasm_globaltype_t* wasm_globaltype_new(wasm_valtype_t* content, wasm_mut_t mut) {
  return release(globaltype::make(adopt(content), reveal(mut)));
}

wasm_valtype_t* wasm_globaltype_content(wasm_globaltype_t* gt) {
  return get(gt->content());
}

wasm_mut_t wasm_globaltype_mut(wasm_globaltype_t* gt) {
  return hide(gt->mut());
}


// Table Types

WASM_DEFINE_TYPE(tabletype)

wasm_tabletype_t* wasm_tabletype_new(wasm_valtype_t* elem, wasm_limits_t limits) {
  return release(tabletype::make(adopt(elem), reveal(limits)));
}

wasm_valtype_t* wasm_tabletype_elem(wasm_tabletype_t* tt) {
  return get(tt->element());
}

wasm_limits_t wasm_tabletype_limits(wasm_tabletype_t* tt) {
  return hide(tt->limits());
}


// Memory Types

WASM_DEFINE_TYPE(memtype)

wasm_memtype_t* wasm_memtype_new(wasm_limits_t limits) {
  return release(memtype::make(reveal(limits)));
}

wasm_limits_t wasm_memtype_limits(wasm_memtype_t* mt) {
  return hide(mt->limits());
}


// Extern Types

WASM_DEFINE_TYPE(externtype)

wasm_externtype_t* wasm_functype_as_externtype(wasm_functype_t* ft) {
  return hide(static_cast<externtype*>(ft));
}
wasm_externtype_t* wasm_globaltype_as_externtype(wasm_globaltype_t* gt) {
  return hide(static_cast<externtype*>(gt));
}
wasm_externtype_t* wasm_tabletype_as_externtype(wasm_tabletype_t* tt) {
  return hide(static_cast<externtype*>(tt));
}
wasm_externtype_t* wasm_memtype_as_externtype(wasm_memtype_t* mt) {
  return hide(static_cast<externtype*>(mt));
}

wasm_functype_t* wasm_externtype_as_functype(wasm_externtype_t* et) {
  return et->kind() == EXTERN_FUNC ? hide(static_cast<functype*>(reveal(et))) : nullptr;
}
wasm_globaltype_t* wasm_externtype_as_globaltype(wasm_externtype_t* et) {
  return et->kind() == EXTERN_GLOBAL ? hide(static_cast<globaltype*>(reveal(et))) : nullptr;
}
wasm_tabletype_t* wasm_externtype_as_tabletype(wasm_externtype_t* et) {
  return et->kind() == EXTERN_TABLE ? hide(static_cast<tabletype*>(reveal(et))) : nullptr;
}
wasm_memtype_t* wasm_externtype_as_memtype(wasm_externtype_t* et) {
  return et->kind() == EXTERN_MEMORY ? hide(static_cast<memtype*>(reveal(et))) : nullptr;
}

wasm_externkind_t wasm_externtype_kind(wasm_externtype_t* et) {
  return hide(et->kind());
}


// Import Types

WASM_DEFINE_TYPE(importtype)

wasm_importtype_t* wasm_importtype_new(wasm_name_t module, wasm_name_t name, wasm_externtype_t* type) {
  return release(importtype::make(adopt(module), adopt(name), adopt(type)));
}

wasm_name_t wasm_importtype_module(wasm_importtype_t* it) {
  return get(it->module());
}

wasm_name_t wasm_importtype_name(wasm_importtype_t* it) {
  return get(it->name());
}

wasm_externtype_t* wasm_importtype_type(wasm_importtype_t* it) {
  return get(it->type());
}


// Export Types

WASM_DEFINE_TYPE(exporttype)

wasm_exporttype_t* wasm_exporttype_new(wasm_name_t name, wasm_externtype_t* type) {
  return release(exporttype::make(adopt(name), adopt(type)));
}

wasm_name_t wasm_exporttype_name(wasm_exporttype_t* et) {
  return get(et->name());
}

wasm_externtype_t* wasm_exporttype_type(wasm_exporttype_t* et) {
  return get(et->type());
}


///////////////////////////////////////////////////////////////////////////////
// Runtime Values

// Values

WASM_DEFINE_VEC(val, )

void wasm_val_delete(wasm_val_t val) {
  delete_own(val);
}

own wasm_val_t wasm_val_clone(wasm_val_t val) {
  if (wasm_valkind_is_refkind(val.kind)) {
    val.ref = wasm_ref_clone(val.ref);
  }
  return val;
}

extern "C++" inline auto hide(wasm::val v) -> wasm_val_t {
  wasm_val_t v2 = { hide(v.kind()) };
  switch (v.kind()) {
    case I32: v2.i32 = v.i32(); break;
    case I64: v2.i64 = v.i64(); break;
    case F32: v2.f32 = v.f32(); break;
    case F64: v2.f64 = v.f64(); break;
    case ANYREF:
    case FUNCREF: v2.ref = v.release(); break;
    default: assert(false);
  }
  return v2;
}

extern "C++" inline auto reveal(wasm_val_t v) -> wasm::val {
  switch (reveal(v.kind)) {
    case I32: return val(v.i32);
    case I64: return val(v.i64);
    case F32: return val(v.f32);
    case F64: return val(v.f64);
    case ANYREF:
    case FUNCREF: return val(adopt(v.ref));
    default: assert(false);
  }
}

extern "C++" inline auto get(val v) -> wasm_##name##_t* {

  return hide(x.get());
  } \
  extern "C++" inline auto release(own<wasm::name*>&& x) -> wasm_##name##_t* { \
    return hide(x.release()); \
  } \
  extern "C++" inline auto adopt(wasm_##name##_t* x) -> own<wasm::name*> { \
    return make_own(x); \
  } \
  extern "C++" inline auto borrow(wasm_##name##_t* x) -> borrowed<name*> { \
    return borrowed<name*>(x); \
  }


// References

WASM_DEFINE_OWN(ref)

wasm_ref_t* wasm_ref_clone(wasm_ref_t* r) {
  return release(r->clone());
}


#define WASM_DEFINE_REF(name) \
  WASM_DEFINE_OWN(name) \
  \
  wasm_##name##_t* wasm_##name##_clone(wasm_##name##_t* t) { \
    return release(t->clone()); \
  } \
  \
  wasm_ref_t* wasm_##name##_as_ref(wasm_##name##_t* r) { \
    return hide(static_cast<ref*>(reveal(r))); \
  } \
  wasm_##name##_t* wasm_ref_as_##name(wasm_ref_t* r) { \
    return hide(static_cast<name*>(reveal(r))); \
  } \
  \
  void* wasm_##name##_get_host_info(wasm_##name##_t* r) { \
    return r->get_host_info(); \
  } \
  void wasm_##name##_set_host_info(wasm_##name##_t* r, void* info) { \
    r->set_host_info(info); \
  } \
  void wasm_##name##_set_host_info_with_finalizer( \
    wasm_##name##_t* r, void* info, void (*finalizer)(void*) \
  ) { \
    r->set_host_info(info, finalizer); \
  }


///////////////////////////////////////////////////////////////////////////////
// Runtime Objects

// Modules

struct wasm_module_t : wasm_ref_t {
  owned<wasm_importtype_vec_t> imports;
  owned<wasm_exporttype_vec_t> exports;

  wasm_module_t(wasm_store_t* store, v8::Local<v8::Object> obj,
    own wasm_importtype_vec_t imports, own wasm_exporttype_vec_t exports) :
    wasm_ref_t(store, obj), imports(imports), exports(exports) {}
};

WASM_DEFINE_REF(module)

own wasm_module_t* wasm_module_new(wasm_store_t* store, wasm_byte_vec_t binary) {
  auto isolate = store->isolate();
  auto context = store->context();
  v8::HandleScope handle_scope(isolate);

  auto array_buffer = v8::ArrayBuffer::New(isolate, binary.data, binary.size);

  v8::Local<v8::Value> args[] = {array_buffer};
  auto maybe_obj =
    store->v8_function(V8_F_MODULE)->NewInstance(context, 1, args);
  if (maybe_obj.IsEmpty()) return nullptr;
  auto obj = maybe_obj.ToLocalChecked();

  // TODO(wasm+): use JS API once available?
  auto imports_exports = wasm::bin::imports_exports(binary);
  // TODO store->cache_set(module_obj, module);
  return new wasm_module_t(store, obj, std::get<0>(imports_exports), std::get<1>(imports_exports));
}

bool wasm_module_validate(wasm_store_t* store, wasm_byte_vec_t binary) {
  v8::Isolate* isolate = store->isolate();
  v8::HandleScope handle_scope(isolate);

  auto array_buffer = v8::ArrayBuffer::New(isolate, binary.data, binary.size);

  v8::Local<v8::Value> args[] = {array_buffer};
  auto result = store->v8_function(V8_F_VALIDATE)->Call(
    store->context(), v8::Undefined(isolate), 1, args);
  if (result.IsEmpty()) return false;

  return result.ToLocalChecked()->IsTrue();
}


wasm_importtype_vec_t wasm_module_imports(wasm_module_t* module) {
  return module->imports.borrow();
/* OBSOLETE?
  auto store = module->store();
  auto isolate = store->isolate();
  auto context = store->context();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Value> args[] = { module->v8_object() };
  auto result = store->v8_function(V8_F_IMPORTS)->Call(
    context, v8::Undefined(isolate), 1, args);
  if (result.IsEmpty()) return wasm_importtype_vec_empty();
  auto array = v8::Local<v8::Array>::Cast(result.ToLocalChecked());
  size_t size = array->Length();

  wasm_importtype_vec_t imports = wasm_importtype_vec_new_uninitialized(size);
  for (size_t i = 0; i < size; ++i) {
    auto desc = v8::Local<v8::Object>::Cast(array->Get(i));
    auto module_str = v8::Local<v8::String>::Cast(
      desc->Get(context, store->v8_string(V8_S_MODULE)).ToLocalChecked());
    auto name_str = v8::Local<v8::String>::Cast(
      desc->Get(context, store->v8_string(V8_S_NAME)).ToLocalChecked());
    auto kind_str = v8::Local<v8::String>::Cast(
      desc->Get(context, store->v8_string(V8_S_KIND)).ToLocalChecked());

    auto type = wasm_externtype_new_from_v8_kind(store, kind_str);
    auto module = wasm_byte_vec_new_from_v8_string(module_str);
    auto name = wasm_byte_vec_new_from_v8_string(name_str);
    imports.data[i] = wasm_importtype_new(module, name, type);
  }

  return imports;
*/
}

wasm_exporttype_vec_t wasm_module_exports(wasm_module_t* module) {
  return module->exports.borrow();
/* OBSOLETE?
  auto store = module->store();
  auto isolate = store->isolate();
  auto context = store->context();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Value> args[] = { module->v8_object() };
  auto result = store->v8_function(V8_F_EXPORTS)->Call(
    context, v8::Undefined(isolate), 1, args);
  if (result.IsEmpty()) return wasm_exporttype_vec_empty();
  auto array = v8::Local<v8::Array>::Cast(result.ToLocalChecked());
  size_t size = array->Length();

  wasm_exporttype_vec_t exports = wasm_exporttype_vec_new_uninitialized(size);
  for (size_t i = 0; i < size; ++i) {
    auto desc = v8::Local<v8::Object>::Cast(array->Get(i));
    auto name_str = v8::Local<v8::String>::Cast(
      desc->Get(context, store->v8_string(V8_S_NAME)).ToLocalChecked());
    auto kind_str = v8::Local<v8::String>::Cast(
      desc->Get(context, store->v8_string(V8_S_KIND)).ToLocalChecked());

    auto type = wasm_externtype_new_from_v8_kind(store, kind_str);
    auto name = wasm_byte_vec_new_from_v8_string(name_str);
    exports.data[i] = wasm_exporttype_new(name, type);
  }

  return exports;
*/
}

own wasm_byte_vec_t wasm_module_serialize(wasm_module_t*) {
  UNIMPLEMENTED("wasm_module_serialize");
}

own wasm_module_t* wasm_module_deserialize(wasm_byte_vec_t) {
  UNIMPLEMENTED("wasm_module_deserialize");
}


// Host Objects

struct wasm_hostobj_t : wasm_ref_t {
  using wasm_ref_t::wasm_ref_t;
};

WASM_DEFINE_REF(hostobj)

own wasm_hostobj_t* wasm_hostobj_new(wasm_store_t* store) {
  auto isolate = store->isolate();
  v8::HandleScope handle_scope(isolate);

  auto obj = v8::Object::New(isolate);
  return new wasm_hostobj_t(store, obj);
}


// Function Instances

struct wasm_func_t : wasm_ref_t {
  owned<wasm_functype_t*> type;
  wasm_func_callback_t callback;

  wasm_func_t(wasm_store_t* store, v8::Local<v8::Function> obj, wasm_functype_t* type, wasm_func_callback_t callback = nullptr) :
    wasm_ref_t(store, obj), type(type), callback(callback) {}

  v8::Local<v8::Function> v8_function() const {
    return v8::Local<v8::Function>::Cast(v8_object());
  }
};

WASM_DEFINE_REF(func)


void wasm_callback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  auto data = v8::Local<v8::Object>::Cast(info.Data());
  auto func = reinterpret_cast<wasm_func_t*>(
    data->GetAlignedPointerFromInternalField(0));
  auto store = func->store();
  auto isolate = store->isolate();
  v8::HandleScope handle_scope(isolate);

  auto context = store->context();
  auto type = func->type.borrow();
  auto type_params = type->params.borrow();
  auto type_results = type->results.borrow();

  assert(type_params.size == info.Length());

  owned<wasm_val_vec_t> own_args = wasm_val_vec_new_uninitialized(type_params.size);
  auto args = own_args.borrow();
  for (size_t i = 0; i < type_params.size; ++i) {
    args.data[i] = wasm_v8_to_val(store, info[i], type_params.data[i]);
  }

  owned<wasm_val_vec_t> own_results = func->callback(args);
  auto results = own_results.borrow();

  assert(type_results.size == results.size);

  auto ret = info.GetReturnValue();
  if (type_results.size == 0) {
    ret.SetUndefined();
  } else if (type_results.size == 1) {
    assert(results.data[0].kind == wasm_valtype_kind(type_results.data[0]));
    ret.Set(wasm_val_to_v8(store, results.data[0]));
  } else {
    UNIMPLEMENTED("multiple results");
  }
}


own wasm_func_t* wasm_func_new(wasm_store_t* store, wasm_functype_t* type, wasm_func_callback_t callback) {
  auto isolate = store->isolate();
  v8::HandleScope handle_scope(isolate);
  auto context = store->context();

  // TODO(lowlevel): use V8 Foreign value
  auto data_template = store->callback_data_template();
  auto maybe_data = data_template->NewInstance(context);
  if (maybe_data.IsEmpty()) return nullptr;
  auto data = maybe_data.ToLocalChecked();

  auto function_template = v8::FunctionTemplate::New(isolate, &wasm_callback, data);
  auto maybe_function = function_template->GetFunction(context);
  if (maybe_function.IsEmpty()) return nullptr;
  auto function = maybe_function.ToLocalChecked();

  auto type_clone = wasm_functype_clone(type);
  if (type_clone == nullptr) return nullptr;
  auto func = new wasm_func_t(store, function, type_clone, callback);
  data->SetAlignedPointerInInternalField(0, func);
  return func;
}

wasm_func_t *wasm_func_new_with_env(wasm_store_t*, wasm_functype_t* type, wasm_func_callback_with_env_t callback, wasm_ref_t *env) {
  UNIMPLEMENTED("wasm_func_new_with_env");
}

own wasm_functype_t* wasm_func_type(wasm_func_t* func) {
  return wasm_functype_clone(func->type.borrow());
}

own wasm_val_vec_t wasm_func_call(wasm_func_t* func, wasm_val_vec_t args) {
  auto store = func->store();
  auto isolate = store->isolate();
  v8::HandleScope handle_scope(isolate);

  auto context = store->context();
  auto type = func->type.borrow();
  auto type_params = type->params.borrow();
  auto type_results = type->results.borrow();

  assert(type_params.size == args.size);

  auto v8_args = new v8::Local<v8::Value>[type_params.size];
  for (size_t i = 0; i < type_params.size; ++i) {
    assert(args.data[i].kind == wasm_valtype_kind(type_params.data[i]));
    v8_args[i] = wasm_val_to_v8(store, args.data[i]);
  }

  auto maybe_result =
    func->v8_function()->Call(context, v8::Undefined(isolate), args.size, v8_args);
  if (maybe_result.IsEmpty()) return wasm_val_vec_empty();
  auto result = maybe_result.ToLocalChecked();

  if (type_results.size == 0) {
    assert(result->IsUndefined());
    return wasm_val_vec_empty();
  } else if (type_results.size == 1) {
    assert(!result->IsUndefined());
    auto val = wasm_v8_to_val(store, result, type_results.data[0]);
    return wasm_val_vec_new(1, &val);
  } else {
    UNIMPLEMENTED("multiple results");
  }
}


// Global Instances

struct wasm_global_t : wasm_ref_t {
  owned<wasm_globaltype_t*> type;

  wasm_global_t(wasm_store_t* store, v8::Local<v8::Object> obj, own wasm_globaltype_t* type) :
    wasm_ref_t(store, obj), type(type) {}
};

WASM_DEFINE_REF(global)

own wasm_globaltype_t* wasm_global_type(wasm_global_t* global) {
  return wasm_globaltype_clone(global->type.borrow());
}

wasm_global_t* wasm_global_new(wasm_store_t* store, wasm_globaltype_t* type, wasm_val_t val) {
  auto isolate = store->isolate();
  v8::HandleScope handle_scope(isolate);
  auto context = store->context();

  assert(wasm_valtype_kind(wasm_globaltype_content(type)) == val.kind);

  // TODO(wasm+): remove
  if (store->v8_function(V8_F_GLOBAL).IsEmpty()) {
    UNIMPLEMENTED("wasm_global_new");
  }

  v8::Local<v8::Value> args[] = {
    wasm_globaltype_to_v8(store, type),
    wasm_val_to_v8(store, val)
  };
  auto maybe_obj =
    store->v8_function(V8_F_GLOBAL)->NewInstance(context, 2, args);
  if (maybe_obj.IsEmpty()) return nullptr;
  auto obj = maybe_obj.ToLocalChecked();

  auto type_clone = wasm_globaltype_clone(type);
  if (type_clone == nullptr) return nullptr;
  return new wasm_global_t(store, obj, type_clone);
}

own wasm_val_t wasm_global_get(wasm_global_t* global) {
  auto store = global->store();
  auto isolate = store->isolate();
  v8::HandleScope handle_scope(isolate);
  auto context = store->context();

  // TODO(wasm+): remove
  if (store->v8_function(V8_F_GLOBAL_GET).IsEmpty()) {
    UNIMPLEMENTED("wasm_global_get");
  }

  auto maybe_val =
    store->v8_function(V8_F_GLOBAL_GET)->Call(context, global->v8_object(), 0, nullptr);
  if (maybe_val.IsEmpty()) return wasm_null_val();
  auto val = maybe_val.ToLocalChecked();

  auto content_type = wasm_globaltype_content(global->type.borrow());
  return wasm_v8_to_val(store, val, content_type);
}

void wasm_global_set(wasm_global_t* global, wasm_val_t val) {
  auto store = global->store();
  auto isolate = store->isolate();
  v8::HandleScope handle_scope(isolate);
  auto context = store->context();

  auto content_type = wasm_globaltype_content(global->type.borrow());
  assert(val.kind == wasm_valtype_kind(content_type));

  // TODO(wasm+): remove
  if (store->v8_function(V8_F_GLOBAL_SET).IsEmpty()) {
    UNIMPLEMENTED("wasm_global_set");
  }

  v8::Local<v8::Value> args[] = { wasm_val_to_v8(store, val) };
  store->v8_function(V8_F_GLOBAL_SET)->Call(context, global->v8_object(), 1, args);
}


// Table Instances

struct wasm_table_t : wasm_ref_t {
  owned<wasm_tabletype_t*> type;

  wasm_table_t(wasm_store_t* store, v8::Local<v8::Object> obj, own wasm_tabletype_t* type) :
    wasm_ref_t(store, obj), type(type) {}
};

WASM_DEFINE_REF(table)

wasm_table_t* wasm_table_new(wasm_store_t* store, wasm_tabletype_t* type, wasm_ref_t*) {
  auto isolate = store->isolate();
  v8::HandleScope handle_scope(isolate);
  auto context = store->context();

  // TODO(wasm+): handle reference initialiser
  v8::Local<v8::Value> args[] = { wasm_tabletype_to_v8(store, type) };
  auto maybe_obj =
    store->v8_function(V8_F_TABLE)->NewInstance(context, 1, args);
  if (maybe_obj.IsEmpty()) return nullptr;
  auto obj = maybe_obj.ToLocalChecked();

  auto type_clone = wasm_tabletype_clone(type);
  if (type_clone == nullptr) return nullptr;
  return new wasm_table_t(store, obj, type_clone);
}

own wasm_tabletype_t* wasm_table_type(wasm_table_t* table) {
  // TODO: query and update min
  return wasm_tabletype_clone(table->type.borrow());
}


own wasm_ref_t* wasm_table_get(wasm_table_t*, wasm_table_size_t index) {
  UNIMPLEMENTED("wasm_table_get");
}

void wasm_table_set(wasm_table_t*, wasm_table_size_t index, wasm_ref_t*) {
  UNIMPLEMENTED("wasm_table_set");
}

wasm_table_size_t wasm_table_size(wasm_table_t*) {
  UNIMPLEMENTED("wasm_table_size");
}

wasm_table_size_t wasm_table_grow(wasm_table_t*, wasm_table_size_t delta) {
  UNIMPLEMENTED("wasm_table_grow");
}


// Memory Instances

struct wasm_memory_t : wasm_ref_t {
  owned<wasm_memtype_t*> type;

  wasm_memory_t(wasm_store_t* store, v8::Local<v8::Object> obj, own wasm_memtype_t* type) :
    wasm_ref_t(store, obj), type(type) {}
};

WASM_DEFINE_REF(memory)

wasm_memory_t* wasm_memory_new(wasm_store_t* store, wasm_memtype_t* type) {
  auto isolate = store->isolate();
  v8::HandleScope handle_scope(isolate);
  auto context = store->context();

  v8::Local<v8::Value> args[] = { wasm_memtype_to_v8(store, type) };
  auto maybe_obj =
    store->v8_function(V8_F_MEMORY)->NewInstance(context, 1, args);
  if (maybe_obj.IsEmpty()) return nullptr;
  auto obj = maybe_obj.ToLocalChecked();

  auto type_clone = wasm_memtype_clone(type);
  if (type_clone == nullptr) return nullptr;
  return new wasm_memory_t(store, obj, type_clone);
}

own wasm_memtype_t* wasm_memory_type(wasm_memory_t* memory) {
  // TODO: query and update min
  return wasm_memtype_clone(memory->type.borrow());
}

wasm_byte_t* wasm_memory_data(wasm_memory_t*) {
  UNIMPLEMENTED("wasm_memory_data");
}

size_t wasm_memory_data_size(wasm_memory_t*) {
  UNIMPLEMENTED("wasm_memory_data_size");
}

wasm_memory_pages_t wasm_memory_size(wasm_memory_t*) {
  UNIMPLEMENTED("wasm_memory_size");
}

wasm_memory_pages_t wasm_memory_grow(wasm_memory_t*, wasm_memory_pages_t delta) {
  UNIMPLEMENTED("wasm_memory_grow");
}


// Externals

WASM_DEFINE_VEC(extern, )

extern "C++"
void delete_own(own wasm_extern_t ex) {
  switch (ex.kind) {
    case WASM_EXTERN_FUNC: return wasm_func_delete(ex.func);
    case WASM_EXTERN_GLOBAL: return wasm_global_delete(ex.global);
    case WASM_EXTERN_TABLE: return wasm_table_delete(ex.table);
    case WASM_EXTERN_MEMORY: return wasm_memory_delete(ex.memory);
  }
}

void wasm_extern_delete(own wasm_extern_t ex) {
  delete_own(ex);
}

own wasm_extern_t wasm_extern_clone(wasm_extern_t ex) {
  switch (ex.kind) {
    case WASM_EXTERN_FUNC: return wasm_extern_func(wasm_func_clone(ex.func));
    case WASM_EXTERN_GLOBAL: return wasm_extern_global(wasm_global_clone(ex.global));
    case WASM_EXTERN_TABLE: return wasm_extern_table(wasm_table_clone(ex.table));
    case WASM_EXTERN_MEMORY: return wasm_extern_memory(wasm_memory_clone(ex.memory));
  }
}

extern "C++"
v8::Local<v8::Object> wasm_extern_to_v8(wasm_extern_t ex) {
  switch (ex.kind) {
    case WASM_EXTERN_FUNC: return ex.func->v8_object();
    case WASM_EXTERN_GLOBAL: return ex.global->v8_object();
    case WASM_EXTERN_TABLE: return ex.table->v8_object();
    case WASM_EXTERN_MEMORY: return ex.memory->v8_object();
  }
}


// Module Instances

struct wasm_instance_t : wasm_ref_t {
  owned<wasm_extern_vec_t> exports;

  wasm_instance_t(wasm_store_t* store, v8::Local<v8::Object> obj, own wasm_extern_vec_t exports) :
    wasm_ref_t(store, obj), exports(exports) {}
};

WASM_DEFINE_REF(instance)

own wasm_instance_t* wasm_instance_new(wasm_store_t* store, wasm_module_t* module, wasm_extern_vec_t imports) {
  auto isolate = store->isolate();
  auto context = store->context();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Value> imports_args[] = { module->v8_object() };
  auto imports_result = store->v8_function(V8_F_IMPORTS)->Call(
    context, v8::Undefined(isolate), 1, imports_args);
  if (imports_result.IsEmpty()) return nullptr;
  auto imports_array = v8::Local<v8::Array>::Cast(imports_result.ToLocalChecked());
  size_t imports_size = imports_array->Length();

  auto imports_obj = v8::Object::New(isolate);
  for (size_t i = 0; i < imports_size; ++i) {
    auto desc = v8::Local<v8::Object>::Cast(imports_array->Get(i));
    auto module_str = v8::Local<v8::String>::Cast(
      desc->Get(context, store->v8_string(V8_S_MODULE)).ToLocalChecked());
    auto name_str = v8::Local<v8::String>::Cast(
      desc->Get(context, store->v8_string(V8_S_NAME)).ToLocalChecked());

    v8::Local<v8::Object> module_obj;
    if (imports_obj->HasOwnProperty(context, module_str).ToChecked()) {
      module_obj = v8::Local<v8::Object>::Cast(
        imports_obj->Get(context, module_str).ToLocalChecked());
    } else {
      module_obj = v8::Object::New(isolate);
      imports_obj->DefineOwnProperty(context, module_str, module_obj);
    }

    module_obj->DefineOwnProperty(context, name_str, wasm_extern_to_v8(imports.data[i]));
  }

  v8::Local<v8::Value> instantiate_args[] = {module->v8_object(), imports_obj};
  auto instance_obj =
    store->v8_function(V8_F_INSTANCE)->NewInstance(context, 2, instantiate_args).ToLocalChecked();
  auto exports_obj = v8::Local<v8::Object>::Cast(
    instance_obj->Get(context, store->v8_string(V8_S_EXPORTS)).ToLocalChecked());
  assert(!exports_obj.IsEmpty() && exports_obj->IsObject());

  auto export_types = module->exports.borrow();
  owned<wasm_extern_vec_t> own_exports =
    wasm_extern_vec_new_uninitialized(export_types.size);
  auto exports = own_exports.borrow();
  for (size_t i = 0; i < export_types.size; ++i) {
    auto name = export_types.data[i]->name.borrow();
    auto maybe_name_obj = v8::String::NewFromUtf8(isolate, name.data,
      v8::NewStringType::kNormal, name.size);
    if (maybe_name_obj.IsEmpty()) return nullptr;
    auto name_obj = maybe_name_obj.ToLocalChecked();
    auto obj = v8::Local<v8::Function>::Cast(
      exports_obj->Get(context, name_obj).ToLocalChecked());

    auto type = export_types.data[i]->type.borrow();
    switch (wasm_externtype_kind(type)) {
      case WASM_EXTERN_FUNC: {
        auto func_obj = v8::Local<v8::Function>::Cast(obj);
        auto functype = wasm_functype_clone(wasm_externtype_as_functype(type));
        if (functype == nullptr) return nullptr;
        auto func = new wasm_func_t(store, func_obj, functype);
        exports.data[i] = wasm_extern_func(func);
      } break;
      case WASM_EXTERN_GLOBAL: {
        auto globaltype = wasm_globaltype_clone(wasm_externtype_as_globaltype(type));
        if (globaltype == nullptr) return nullptr;
        auto global = new wasm_global_t(store, obj, globaltype);
        exports.data[i] = wasm_extern_global(global);
      } break;
      case WASM_EXTERN_TABLE: {
        auto tabletype = wasm_tabletype_clone(wasm_externtype_as_tabletype(type));
        if (tabletype == nullptr) return nullptr;
        auto table = new wasm_table_t(store, obj, tabletype);
        exports.data[i] = wasm_extern_table(table);
      } break;
      case WASM_EXTERN_MEMORY: {
        auto memtype = wasm_memtype_clone(wasm_externtype_as_memtype(type));
        if (memtype == nullptr) return nullptr;
        auto memory = new wasm_memory_t(store, obj, memtype);
        exports.data[i] = wasm_extern_memory(memory);
      } break;
    }
  }

  return new wasm_instance_t(store, instance_obj, exports);
}

own wasm_extern_t wasm_instance_export(wasm_instance_t* instance, size_t index) {
  auto exports = instance->exports.borrow();
  if (index >= exports.size) return wasm_extern_func(nullptr);
  return exports.data[index];
}

own wasm_extern_vec_t wasm_instance_exports(wasm_instance_t* instance) {
  return wasm_extern_vec_clone(instance->exports.borrow());
}

}  // extern "C"
