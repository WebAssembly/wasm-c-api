#include "wasm.h"
#include "wasm-bin.hh"

#include "v8.h"
#include "libplatform/libplatform.h"

#include <stdio.h>


void UNIMPLEMENTED(const char* s) {
  printf("Wasm C API: %s not supported yet!\n", s);
  exit(1);
}


extern "C" {

///////////////////////////////////////////////////////////////////////////////
// Auxiliaries

#define own


// Vectors

#define WASM_DEFINE_VEC(name, ptr_or_none) \
  own name##_vec_t name##_vec_new_uninitialized(size_t size) { \
    typedef name##_t ptr_or_none name##_elem_t; \
    return name##_vec(size, size == 0 ? nullptr : new name##_elem_t[size]); \
  } \
  \
  own own name##_vec_t name##_vec_new(size_t size, own name##_t ptr_or_none const input[]) { \
    auto v = name##_vec_new_uninitialized(size); \
    if (size != 0) memcpy(v.data, input, size * sizeof(name##_t ptr_or_none)); \
    return v; \
  } \
  \
  own own name##_vec_t name##_vec_clone(name##_vec_t v) { \
    return name##_vec_new(v.size, v.data); \
  } \
  \
  void name##_vec_delete(own own name##_vec_t v) { \
    if (v.size != 0) { \
      for (size_t i = 0; i < v.size; ++i) name##_delete(v.data[i]); \
      delete[] v.data; \
    } \
  }


// Byte vectors

inline void wasm_byte_delete(wasm_byte_t) {}

WASM_DEFINE_VEC(wasm_byte, )


///////////////////////////////////////////////////////////////////////////////
// Type Representations

// Value Types

struct wasm_valtype_t {};

own wasm_valtype_t* wasm_valtype_new(wasm_valkind_t k) {
  return reinterpret_cast<wasm_valtype_t*>(static_cast<intptr_t>(k));
}

own wasm_valtype_t* wasm_valtype_clone(wasm_valtype_t* t) {
  return t;
}

void wasm_valtype_delete(wasm_valtype_t*) {}

wasm_valkind_t wasm_valtype_kind(wasm_valtype_t* t) {
  return static_cast<wasm_valkind_t>(reinterpret_cast<intptr_t>(t));
}

WASM_DEFINE_VEC(wasm_valtype, *)


// Extern Types

struct wasm_externtype_t {
  wasm_externkind_t kind;
};


// Function Types

struct wasm_functype_t : wasm_externtype_t {
  own wasm_valtype_vec_t params;
  own wasm_valtype_vec_t results;
};

own wasm_functype_t* wasm_functype_new(own wasm_valtype_vec_t params, own wasm_valtype_vec_t results) {
  auto ft = new wasm_functype_t;
  ft->kind = WASM_EXTERN_FUNC;
  ft->params = params;
  ft->results = results;
  return ft;
}

own wasm_functype_t* wasm_functype_clone(wasm_functype_t* ft) {
  return wasm_functype_new(wasm_valtype_vec_clone(ft->params), wasm_valtype_vec_clone(ft->results));
}

void wasm_functype_delete(own wasm_functype_t* ft) {
  wasm_valtype_vec_delete(ft->params);
  wasm_valtype_vec_delete(ft->results);
  delete ft;
}

wasm_valtype_vec_t wasm_functype_params(wasm_functype_t* ft) {
  return ft->params;
}

wasm_valtype_vec_t wasm_functype_results(wasm_functype_t* ft) {
  return ft->results;
}

WASM_DEFINE_VEC(wasm_functype, *)


// Global Types

struct wasm_globaltype_t : wasm_externtype_t {
  wasm_valtype_t* content;
  wasm_mut_t mut;
};

own wasm_globaltype_t* wasm_globaltype_new(own wasm_valtype_t* content, wasm_mut_t mut) {
  auto gt = new wasm_globaltype_t;
  gt->kind = WASM_EXTERN_GLOBAL;
  gt->content = content;
  gt->mut = mut;
  return gt;
}

own wasm_globaltype_t* wasm_globaltype_clone(wasm_globaltype_t* gt) {
  return wasm_globaltype_new(wasm_valtype_clone(gt->content), gt->mut);
}

void wasm_globaltype_delete(own wasm_globaltype_t* gt) {
  wasm_valtype_delete(gt->content);
  delete gt;
}

wasm_valtype_t *wasm_globaltype_content(wasm_globaltype_t* gt) {
  return gt->content;
}

wasm_mut_t wasm_globaltype_mut(wasm_globaltype_t* gt) {
  return gt->mut;
}

WASM_DEFINE_VEC(wasm_globaltype, *)


// Table Types

struct wasm_tabletype_t : wasm_externtype_t {
  wasm_reftype_t* elem;
  wasm_limits_t limits;
};

own wasm_tabletype_t* wasm_tabletype_new(own wasm_reftype_t* elem, wasm_limits_t limits) {
  auto tt = new wasm_tabletype_t;
  tt->kind = WASM_EXTERN_TABLE;
  tt->elem = elem;
  tt->limits = limits;
  return tt;
}

own wasm_tabletype_t* wasm_tabletype_clone(wasm_tabletype_t* tt) {
  return wasm_tabletype_new(wasm_valtype_clone(tt->elem), tt->limits);
}

void wasm_tabletype_delete(own wasm_tabletype_t* tt) {
  wasm_valtype_delete(tt->elem);
  delete tt;
}

wasm_reftype_t* wasm_tabletype_elem(wasm_tabletype_t* tt) {
  return tt->elem;
}

wasm_limits_t wasm_tabletype_limits(wasm_tabletype_t* tt) {
  return tt->limits;
}

WASM_DEFINE_VEC(wasm_tabletype, *)


// Memory Types

struct wasm_memtype_t : wasm_externtype_t {
  wasm_limits_t limits;
};

own wasm_memtype_t* wasm_memtype_new(wasm_limits_t limits) {
  auto mt = new wasm_memtype_t;
  mt->kind = WASM_EXTERN_MEMORY;
  mt->limits = limits;
  return mt;
}

own wasm_memtype_t* wasm_memtype_clone(wasm_memtype_t* mt) {
  return wasm_memtype_new(mt->limits);
}

void wasm_memtype_delete(own wasm_memtype_t* mt) {
  delete mt;
}

wasm_limits_t wasm_memtype_limits(wasm_memtype_t* mt) {
  return mt->limits;
}

WASM_DEFINE_VEC(wasm_memtype, *)


// Extern Types

wasm_externtype_t* wasm_functype_as_externtype(wasm_functype_t* ft) {
  return ft;
}
wasm_externtype_t* wasm_globaltype_as_externtype(wasm_globaltype_t* gt) {
  return gt;
}
wasm_externtype_t* wasm_tabletype_as_externtype(wasm_tabletype_t* tt) {
  return tt;
}
wasm_externtype_t* wasm_memtype_as_externtype(wasm_memtype_t* mt) {
  return mt;
}

wasm_functype_t* wasm_externtype_as_functype(wasm_externtype_t* et) {
  return et->kind == WASM_EXTERN_FUNC ? static_cast<wasm_functype_t*>(et) : nullptr;
}
wasm_globaltype_t* wasm_externtype_as_globaltype(wasm_externtype_t* et) {
  return et->kind == WASM_EXTERN_GLOBAL ? static_cast<wasm_globaltype_t*>(et) : nullptr;
}
wasm_tabletype_t* wasm_externtype_as_tabletype(wasm_externtype_t* et) {
  return et->kind == WASM_EXTERN_TABLE ? static_cast<wasm_tabletype_t*>(et) : nullptr;
}
wasm_memtype_t* wasm_externtype_as_memtype(wasm_externtype_t* et) {
  return et->kind == WASM_EXTERN_MEMORY ? static_cast<wasm_memtype_t*>(et) : nullptr;
}

own wasm_externtype_t* wasm_externtype_clone(wasm_externtype_t* et) {
  switch (et->kind) {
    case WASM_EXTERN_FUNC: return wasm_functype_clone(wasm_externtype_as_functype(et));
    case WASM_EXTERN_GLOBAL: return wasm_globaltype_clone(wasm_externtype_as_globaltype(et));
    case WASM_EXTERN_TABLE: return wasm_tabletype_clone(wasm_externtype_as_tabletype(et));
    case WASM_EXTERN_MEMORY: return wasm_memtype_clone(wasm_externtype_as_memtype(et));
  }
}

void wasm_externtype_delete(own wasm_externtype_t* et) {
  switch (et->kind) {
    case WASM_EXTERN_FUNC: return wasm_functype_delete(wasm_externtype_as_functype(et));
    case WASM_EXTERN_GLOBAL: return wasm_globaltype_delete(wasm_externtype_as_globaltype(et));
    case WASM_EXTERN_TABLE: return wasm_tabletype_delete(wasm_externtype_as_tabletype(et));
    case WASM_EXTERN_MEMORY: return wasm_memtype_delete(wasm_externtype_as_memtype(et));
  }
}

wasm_externkind_t wasm_externtype_kind(wasm_externtype_t* et) {
  return et->kind;
}

WASM_DEFINE_VEC(wasm_externtype, *)


// Import Types

struct wasm_importtype_t {
  own wasm_name_t module;
  own wasm_name_t name;
  own wasm_externtype_t* type;
};

own wasm_importtype_t* wasm_importtype_new(own wasm_name_t module, own wasm_name_t name, own wasm_externtype_t* type) {
  auto it = new wasm_importtype_t;
  it->module = module;
  it->name = name;
  it->type = type;
  return it;
}

own wasm_importtype_t* wasm_importtype_clone(wasm_importtype_t* it) {
  return wasm_importtype_new(wasm_name_clone(it->module), wasm_name_clone(it->name), wasm_externtype_clone(it->type));
}

void wasm_importtype_delete(own wasm_importtype_t* it) {
  wasm_name_delete(it->module);
  wasm_name_delete(it->name);
  wasm_externtype_delete(it->type);
  delete it;
}

wasm_name_t wasm_importtype_module(wasm_importtype_t* it) {
  return it->module;
}

wasm_name_t wasm_importtype_name(wasm_importtype_t* it) {
  return it->name;
}

wasm_externtype_t* wasm_importtype_type(wasm_importtype_t* it) {
  return it->type;
}

WASM_DEFINE_VEC(wasm_importtype, *)


// Export Types

struct wasm_exporttype_t {
  own wasm_name_t name;
  own wasm_externtype_t* type;
};

own wasm_exporttype_t* wasm_exporttype_new(own wasm_name_t name, own wasm_externtype_t* type) {
  auto et = new wasm_exporttype_t;
  et->name = name;
  et->type = type;
  return et;
}

own wasm_exporttype_t* wasm_exporttype_clone(wasm_exporttype_t* et) {
  return wasm_exporttype_new(wasm_name_clone(et->name), wasm_externtype_clone(et->type));
}

void wasm_exporttype_delete(own wasm_exporttype_t* et) {
  wasm_name_delete(et->name);
  wasm_externtype_delete(et->type);
  delete et;
}

wasm_name_t wasm_exporttype_name(wasm_exporttype_t* et) {
  return et->name;
}

wasm_externtype_t* wasm_exporttype_type(wasm_exporttype_t* et) {
  return et->type;
}

WASM_DEFINE_VEC(wasm_exporttype, *)


///////////////////////////////////////////////////////////////////////////////
// Runtime Environment

// Initialisation

struct wasm_config_t {};

wasm_config_t* wasm_config_new() {
  return new wasm_config_t;
}

void wasm_config_delete(own wasm_config_t *config) {
  delete config;
}


void wasm_init_with_config(int argc, const char *const argv[], wasm_config_t* config) {
  v8::V8::InitializeExternalStartupData(argv[0]);
  static std::unique_ptr<v8::Platform> platform =
    v8::platform::NewDefaultPlatform();
  v8::V8::InitializePlatform(platform.get());
  v8::V8::Initialize();
}

void wasm_init(int argc, const char *const argv[]) {
  wasm_init_with_config(argc, argv, nullptr);
}

void wasm_deinit() {
  v8::V8::Dispose();
  v8::V8::ShutdownPlatform();
}


// Stores

enum v8_string_t {
  V8_S_FUNCTION, V8_S_GLOBAL, V8_S_TABLE, V8_S_MEMORY,
  V8_S_MODULE, V8_S_NAME, V8_S_KIND, V8_S_EXPORTS,
  V8_S_I32, V8_S_I64, V8_S_F32, V8_S_F64, V8_S_ANYREF, V8_S_ANYFUNC,
  V8_S_VALUE, V8_S_MUTABLE, V8_S_ELEMENT, V8_S_MINIMUM, V8_S_MAXIMUM,
  V8_s_BUFFER,
  V8_S_COUNT
};

enum v8_function_t {
  V8_F_WEAKMAP, V8_F_WEAK_GET, V8_F_WEAK_SET,
  V8_F_MODULE, V8_F_IMPORTS, V8_F_EXPORTS,
  V8_F_GLOBAL, V8_F_GLOBAL_GET, V8_F_GLOBAL_SET,
  V8_F_TABLE, V8_F_TABLE_GET, V8_F_TABLE_SET, V8_F_TABLE_GROW,
  V8_F_MEMORY, V8_F_MEMORY_GROW,
  V8_F_INSTANCE, V8_F_VALIDATE,
  V8_F_COUNT,
};

class wasm_store_t {
public:
  v8::Isolate* isolate() const {
    return isolate_;
  }

  v8::Local<v8::Context> context() const {
    return context_.Get(isolate_);
  }

  v8::Local<v8::ObjectTemplate> callback_data_template() const {
    return callback_data_template_.Get(isolate_);
  }

  v8::Local<v8::String> v8_string(v8_string_t i) const {
    return strings_[i].Get(isolate_);
  }

  v8::Local<v8::Function> v8_function(v8_function_t i) const {
    return functions_[i].Get(isolate_);
  }

/* TODO
  void cache_set(v8::Local<v8::Object> obj, void* val) {
    v8::Local<v8::Value> cache_args[] = {???};
    store->v8_function(V8_F_WEAKSET)->Call(context, cache_, 1, args);
  }
*/

private:
  friend own wasm_store_t* wasm_store_new();
  friend void wasm_store_delete(own wasm_store_t*);

  v8::Isolate::CreateParams create_params_;
  v8::Isolate *isolate_;
  v8::Eternal<v8::Context> context_;
  v8::Eternal<v8::ObjectTemplate> callback_data_template_;
  v8::Eternal<v8::String> strings_[V8_S_COUNT];
  v8::Eternal<v8::Function> functions_[V8_F_COUNT];
  v8::Eternal<v8::Object> cache_;
};


own wasm_store_t* wasm_store_new() {
  std::unique_ptr<wasm_store_t> store(new wasm_store_t);
  if (store.get() == nullptr) return nullptr;
  store->create_params_.array_buffer_allocator =
    v8::ArrayBuffer::Allocator::NewDefaultAllocator();

  auto isolate = v8::Isolate::New(store->create_params_);
  if (isolate == nullptr) return nullptr;
  {
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);

    auto context = v8::Context::New(isolate);
    if (context.IsEmpty()) return nullptr;
    v8::Context::Scope context_scope(context);

    auto callback_data_template = v8::ObjectTemplate::New(isolate);
    if (callback_data_template.IsEmpty()) return nullptr;
    callback_data_template->SetInternalFieldCount(1);

    store->isolate_ = isolate;
    store->context_ = v8::Eternal<v8::Context>(isolate, context);
    store->callback_data_template_ =
      v8::Eternal<v8::ObjectTemplate>(isolate, callback_data_template);

    static const char* const raw_strings[V8_S_COUNT] = {
      "function", "global", "table", "memory",
      "module", "name", "kind", "exports",
      "i32", "i64", "f32", "f64", "anyref", "anyfunc", 
      "value", "mutable", "element", "initial", "maximum",
      "buffer"
    };
    for (int i = 0; i < V8_S_COUNT; ++i) {
      auto maybe = v8::String::NewFromUtf8(isolate, raw_strings[i],
        v8::NewStringType::kNormal);
      if (maybe.IsEmpty()) return nullptr;
      auto string = maybe.ToLocalChecked();
      store->strings_[i] = v8::Eternal<v8::String>(isolate, string);
    }

    auto global = context->Global();
    auto maybe_wasm_name = v8::String::NewFromUtf8(isolate, "WebAssembly",
        v8::NewStringType::kNormal);
    if (maybe_wasm_name.IsEmpty()) return nullptr;
    auto wasm_name = maybe_wasm_name.ToLocalChecked();
    auto maybe_wasm = global->Get(context, wasm_name);
    if (maybe_wasm.IsEmpty()) return nullptr;
    auto wasm = v8::Local<v8::Object>::Cast(maybe_wasm.ToLocalChecked());
    v8::Local<v8::Object> wasm_module;
    v8::Local<v8::Object> wasm_global;
    v8::Local<v8::Object> wasm_table;
    v8::Local<v8::Object> wasm_memory;
    v8::Local<v8::Object> weakmap;

    struct {
      const char* name;
      v8::Local<v8::Object>* carrier;
    } raw_functions[V8_F_COUNT] = {
      {"WeakMap", &global}, {"get", &weakmap}, {"set", &weakmap},
      {"Module", &wasm}, {"imports", &wasm_module}, {"exports", &wasm_module},
      {"Global", &wasm}, {"get", &wasm_global}, {"set", &wasm_global},
      {"Table", &wasm}, {"get", &wasm_table}, {"set", &wasm_table}, {"grow", &wasm_table},
      {"Memory", &wasm}, {"grow", &wasm_memory},
      {"Instance", &wasm}, {"validate", &wasm},
    };
    for (int i = 0; i < V8_F_COUNT; ++i) {
      auto maybe_name = v8::String::NewFromUtf8(isolate, raw_functions[i].name,
        v8::NewStringType::kNormal);
      if (maybe_name.IsEmpty()) return nullptr;
      auto name = maybe_name.ToLocalChecked();
      assert(!raw_functions[i].carrier->IsEmpty());
      // TODO(wasm+): remove
      if ((*raw_functions[i].carrier)->IsUndefined()) continue;
      auto maybe_obj = (*raw_functions[i].carrier)->Get(context, name);
      if (maybe_obj.IsEmpty()) return nullptr;
      auto function = v8::Local<v8::Function>::Cast(maybe_obj.ToLocalChecked());
      store->functions_[i] = v8::Eternal<v8::Function>(isolate, function);
      if (i == V8_F_WEAKMAP) weakmap = function;
      else if (i == V8_F_MODULE) wasm_module = function;
      else if (i == V8_F_GLOBAL) wasm_global = function;
      else if (i == V8_F_TABLE) wasm_table = function;
      else if (i == V8_F_MEMORY) wasm_memory = function;
    }

    v8::Local<v8::Value> empty_args[] = {};
    auto maybe_cache =
      store->v8_function(V8_F_WEAKMAP)->NewInstance(context, 0, empty_args);
    if (maybe_cache.IsEmpty()) return nullptr;
    auto cache = v8::Local<v8::Object>::Cast(maybe_cache.ToLocalChecked());
    store->cache_ = v8::Eternal<v8::Object>(isolate, cache);
  }

  store->isolate()->Enter();
  store->context()->Enter();
  return store.release();
};

void wasm_store_delete(own wasm_store_t* store) {
  store->context()->Exit();
  store->isolate_->Exit();
  store->isolate_->Dispose();
  delete store->create_params_.array_buffer_allocator;
  delete store;
}


///////////////////////////////////////////////////////////////////////////////
// Conversions from and to simple V8 objects

extern "C++" {

// Types

v8::Local<v8::Value> wasm_valtype_to_v8(wasm_store_t* store, wasm_valtype_t* type) {
  v8_string_t string;
  switch (wasm_valtype_kind(type)) {
    case WASM_I32_VAL: string = V8_S_I32; break;
    case WASM_I64_VAL: string = V8_S_I64; break;
    case WASM_F32_VAL: string = V8_S_F32; break;
    case WASM_F64_VAL: string = V8_S_F64; break;
    case WASM_ANYREF_VAL: string = V8_S_ANYREF; break;
    case WASM_FUNCREF_VAL: string = V8_S_ANYFUNC; break;
    default:
      // TODO(wasm+): support new value types
      assert(false);
  }
  return store->v8_string(string);
}

v8::Local<v8::Boolean> wasm_mut_to_v8(wasm_store_t* store, wasm_mut_t mut) {
  return v8::Boolean::New(store->isolate(), mut == WASM_VAR);
}

void wasm_limits_to_v8(wasm_store_t* store, wasm_limits_t limits, v8::Local<v8::Object> desc) {
  auto isolate = store->isolate();
  auto context = store->context();
  desc->DefineOwnProperty(context, store->v8_string(V8_S_MINIMUM),
    v8::Integer::NewFromUnsigned(isolate, limits.min));
  if (limits.max != wasm_limits_no_max(0).max) {
    desc->DefineOwnProperty(context, store->v8_string(V8_S_MAXIMUM),
      v8::Integer::NewFromUnsigned(isolate, limits.max));
  }
}

v8::Local<v8::Object> wasm_globaltype_to_v8(wasm_store_t* store, wasm_globaltype_t* type) {
  // TODO: define templates
  auto isolate = store->isolate();
  auto context = store->context();
  auto desc = v8::Object::New(isolate);
  desc->DefineOwnProperty(context, store->v8_string(V8_S_VALUE),
    wasm_valtype_to_v8(store, wasm_globaltype_content(type)));
  desc->DefineOwnProperty(context, store->v8_string(V8_S_MUTABLE),
    wasm_mut_to_v8(store, wasm_globaltype_mut(type)));
  return desc;
}

v8::Local<v8::Object> wasm_tabletype_to_v8(wasm_store_t* store, wasm_tabletype_t* type) {
  auto isolate = store->isolate();
  auto context = store->context();
  auto desc = v8::Object::New(isolate);
  desc->DefineOwnProperty(context, store->v8_string(V8_S_ELEMENT),
    wasm_valtype_to_v8(store, wasm_tabletype_elem(type)));
  wasm_limits_to_v8(store, wasm_tabletype_limits(type), desc);
  return desc;
}

v8::Local<v8::Object> wasm_memtype_to_v8(wasm_store_t* store, wasm_memtype_t* type) {
  auto isolate = store->isolate();
  auto context = store->context();
  auto desc = v8::Object::New(isolate);
  wasm_limits_to_v8(store, wasm_memtype_limits(type), desc);
  return desc;
}

/* OBSOLETE?
wasm_externkind_t wasm_externkind_from_v8_kind(wasm_store_t* store, v8::Local<v8::String> kind) {
  if (kind->SameValue(store->v8_string(V8_S_FUNCTION))) {
    return WASM_EXTERN_FUNC;
  } else if (kind->SameValue(store->v8_string(V8_S_GLOBAL))) {
    return WASM_EXTERN_GLOBAL;
  } else if (kind->SameValue(store->v8_string(V8_S_TABLE))) {
    return WASM_EXTERN_TABLE;
  } else if (kind->SameValue(store->v8_string(V8_S_MEMORY))) {
    return WASM_EXTERN_MEMORY;
  } else {
    assert(false);
  }
}

own wasm_externtype_t* wasm_externtype_new_from_v8_kind(wasm_store_t* store, v8::Local<v8::String> kind) {
  // TODO: proper types
  switch (wasm_externkind_from_v8_kind(store, kind)) {
    case WASM_EXTERN_FUNC:
      return wasm_functype_as_externtype(wasm_functype_new_0_0());
    case WASM_EXTERN_GLOBAL:
      return wasm_globaltype_as_externtype(wasm_globaltype_new(wasm_valtype_new_anyref(), WASM_CONST));
    case WASM_EXTERN_TABLE:
      return wasm_tabletype_as_externtype(wasm_tabletype_new(wasm_valtype_new_funcref(), wasm_limits(0, 0)));
    case WASM_EXTERN_MEMORY:
      return wasm_memtype_as_externtype(wasm_memtype_new(wasm_limits(0, 0)));
  }
}
*/



// Strings

/* OBSOLETE?
own wasm_byte_vec_t wasm_v8_to_byte_vec(v8::Local<v8::String> string) {
  size_t len = string->Utf8Length();
  auto v = wasm_byte_vec_new_uninitialized(len);
  if (v.data != nullptr) string->WriteUtf8(v.data);
  return v;
}
*/


// Values

v8::Local<v8::Value> wasm_val_to_v8(wasm_store_t* store, wasm_val_t v) {
  auto isolate = store->isolate();
  switch (v.kind) {
    case WASM_I32_VAL: return v8::Integer::NewFromUnsigned(isolate, v.i32);
    case WASM_I64_VAL: UNIMPLEMENTED("i64 value");
    case WASM_F32_VAL: return v8::Number::New(isolate, v.f32);
    case WASM_F64_VAL: return v8::Number::New(isolate, v.f64);
    case WASM_ANYREF_VAL:
    case WASM_FUNCREF_VAL: {
      if (wasm_ref_is_null(v.ref)) {
        return v8::Null(isolate);
      } else {
        UNIMPLEMENTED("ref value");
      }
    }
    default: assert(false);
  }
}

wasm_val_t wasm_v8_to_val(wasm_store_t* store, v8::Local<v8::Value> value, wasm_valtype_t* t) {
  auto context = store->context();
  wasm_val_t v;
  v.kind = wasm_valtype_kind(t);
  switch (v.kind) {
    case WASM_I32_VAL: {
      v.i32 = static_cast<uint32_t>(value->Int32Value(context).ToChecked());
    } break;
    case WASM_I64_VAL: {
      UNIMPLEMENTED("i64 value");
    }
    case WASM_F32_VAL: {
      v.f32 = static_cast<float32_t>(value->NumberValue(context).ToChecked());
    } break;
    case WASM_F64_VAL: {
      v.f64 = value->NumberValue(context).ToChecked();
    } break;
    case WASM_ANYREF_VAL:
    case WASM_FUNCREF_VAL: {
      if (value->IsNull()) {
        v.ref = wasm_ref_null();
      } else {
        UNIMPLEMENTED("ref value");
      }
    } break;
  }
  return v;
}

}  // extern "C++"


///////////////////////////////////////////////////////////////////////////////
// Auxiliaries for V8 interaction

// Wrappers for V8 heap objects

// - each API wrapper has C side reference count
// - when refcount goes to 0 (drop), SetWeak on persistent handle
// - finalizer deletes wrapper
// - store has weakmap for each V8 object category, mapping to API wrapper (TODO)
// - when returning V8 object to C, looks up wrapper or create fresh (TODO)
// - when wrapper was found in weakmap, bump refcnt (take)
// - if refcnt was 0, ClearWeak on persistent handle

extern "C++" {

template<class T, class Self>
class wasm_wrapper_t {
  int count = 1;
  wasm_store_t* store_;
  v8::Persistent<T> obj_;
  void* host_info = nullptr;
  void (*host_finalizer)(void*) = nullptr;

public:
  wasm_wrapper_t(wasm_store_t* store, v8::Local<T> obj) :
    store_(store), obj_(store->isolate(), obj) {}

  void take() {
    if (count++ == 0) {
      obj_.ClearWeak();
    }
  }
  void drop() {
    if (--count == 0) {
      obj_.template SetWeak<Self>(static_cast<Self*>(this), &callback, v8::WeakCallbackType::kParameter);
    }
  }

  wasm_store_t* store() const {
    return store_;
  }

  v8::Local<T> obj() const {
    return obj_.Get(store_->isolate());
  }

  void* get_host_info() {
    return host_info;
  }
  void set_host_info(void* info, void (*finalizer)(void*) = nullptr) {
    host_info = info;
    host_finalizer = finalizer;
  }

private:
  static void callback(const v8::WeakCallbackInfo<Self>& info) {
    Self* self = info.GetParameter();
    assert(self->count == 0);
    if (self->host_finalizer) (*self->host_finalizer)(self->host_info);
    self->finalize();
  }
};

}  // extern "C++"


///////////////////////////////////////////////////////////////////////////////
// Runtime Values

// References

// TODO
struct wasm_ref_t {};

void wasm_ref_delete(own wasm_ref_t* r) {
  if (r) delete r;
}

own wasm_ref_t* wasm_ref_null() {
  return nullptr;
}

bool wasm_ref_is_null(wasm_ref_t* r) {
  return r == nullptr;
} 


// Values

WASM_DEFINE_VEC(wasm_val, )


///////////////////////////////////////////////////////////////////////////////
// Runtime Objects

// Generic functionality

#define WASM_DEFINE_OBJECT(name) \
  void name##_delete(own name##_t* name) { \
    name->drop(); \
  } \
  \
  /* TODO \
  wasm_ref_t *name##_as_ref(name##_t*); \
  name##_t *wasm_ref_as_##name(wasm_ref_t*); \
  */ \
  \
  void* name##_get_host_info(name##_t* name) { \
    return name->get_host_info(); \
  } \
  \
  void name##_set_host_info(name##_t* name, void* info) { \
    name->set_host_info(info); \
  } \
  \
  void name##_set_host_info_with_finalizer(name##_t* name, void* info, void (*finalizer)(void*)) { \
    name->set_host_info(info, finalizer); \
  }


// Modules

struct wasm_module_t : wasm_wrapper_t<v8::Object, wasm_module_t> {
  wasm_importtype_vec_t imports;
  wasm_exporttype_vec_t exports;

  using wasm_wrapper_t::wasm_wrapper_t;

  void finalize() {
    wasm_importtype_vec_delete(imports);
    wasm_exporttype_vec_delete(exports);
    delete this;
  }
};

WASM_DEFINE_OBJECT(wasm_module)

own wasm_module_t* wasm_module_new(wasm_store_t* store, wasm_byte_vec_t binary) {
  auto isolate = store->isolate();
  auto context = store->context();
  v8::HandleScope handle_scope(isolate);

  auto array_buffer = v8::ArrayBuffer::New(isolate, binary.data, binary.size);

  v8::Local<v8::Value> args[] = {array_buffer};
  auto maybe_module_obj =
    store->v8_function(V8_F_MODULE)->NewInstance(context, 1, args);
  if (maybe_module_obj.IsEmpty()) return nullptr;
  auto module_obj = maybe_module_obj.ToLocalChecked();

  auto module = new wasm_module_t(store, module_obj);

  // TODO(wasm+): use JS API once available?
  auto imports_exports = wasm::bin::imports_exports(binary);
  module->imports = std::get<0>(imports_exports);
  module->exports = std::get<1>(imports_exports);

  // TODO store->cache_set(module_obj, module);
  return module;
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
  return module->imports;
/* OBSOLETE?
  auto store = module->store();
  auto isolate = store->isolate();
  auto context = store->context();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Value> args[] = { module->obj() };
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
  return module->exports;
/* OBSOLETE?
  auto store = module->store();
  auto isolate = store->isolate();
  auto context = store->context();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Value> args[] = { module->obj() };
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

struct wasm_hostobj_t : wasm_wrapper_t<v8::Object, wasm_hostobj_t> {
  using wasm_wrapper_t::wasm_wrapper_t;

  void finalize() {
    delete this;
  }
};

WASM_DEFINE_OBJECT(wasm_hostobj)

own wasm_hostobj_t* wasm_hostobj_new(wasm_store_t* store) {
  auto isolate = store->isolate();
  v8::HandleScope handle_scope(isolate);

  auto obj = v8::Object::New(isolate);
  return new wasm_hostobj_t(store, obj);
}


// Function Instances

struct wasm_func_t : wasm_wrapper_t<v8::Function, wasm_func_t> {
  own wasm_functype_t* type;
  wasm_func_callback_t callback;

  wasm_func_t(wasm_store_t* store, v8::Local<v8::Function> obj, wasm_functype_t* type, wasm_func_callback_t callback = nullptr) :
    wasm_wrapper_t(store, obj), type(type), callback(callback) {}

  void finalize() {
    wasm_functype_delete(type);
    delete this;
  }
};

WASM_DEFINE_OBJECT(wasm_func)


void wasm_callback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  auto data = v8::Local<v8::Object>::Cast(info.Data());
  auto func = reinterpret_cast<wasm_func_t*>(
    data->GetAlignedPointerFromInternalField(0));
  auto store = func->store();
  auto isolate = store->isolate();
  v8::HandleScope handle_scope(isolate);

  auto context = store->context();
  auto type = func->type;

  assert(type->params.size == info.Length());

  wasm_val_vec_t args = wasm_val_vec_new_uninitialized(type->params.size);
  for (size_t i = 0; i < type->params.size; ++i) {
    args.data[i] = wasm_v8_to_val(store, info[i], type->params.data[i]);
  }

  auto results = func->callback(args);

  assert(type->results.size == results.size);

  auto ret = info.GetReturnValue();
  if (type->results.size == 0) {
    ret.SetUndefined();
  } else if (type->results.size == 1) {
    assert(results.data[0].kind == wasm_valtype_kind(type->results.data[0]));
    ret.Set(wasm_val_to_v8(store, results.data[0]));
  } else {
    UNIMPLEMENTED("multiple results");
  }

  wasm_val_vec_delete(args);
  wasm_val_vec_delete(results);
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

wasm_functype_t* wasm_func_type(wasm_func_t* func) {
  return func->type;
}

own wasm_val_vec_t wasm_func_call(wasm_func_t* func, wasm_val_vec_t vs) {
  auto store = func->store();
  auto isolate = store->isolate();
  v8::HandleScope handle_scope(isolate);

  auto context = store->context();
  auto type = func->type;

  assert(vs.size == type->params.size);

  auto args = new v8::Local<v8::Value>[type->params.size];
  for (size_t i = 0; i < type->params.size; ++i) {
    assert(vs.data[i].kind == wasm_valtype_kind(type->params.data[i]));
    args[i] = wasm_val_to_v8(store, vs.data[i]);
  }

  auto maybe_result =
    func->obj()->Call(context, v8::Undefined(isolate), vs.size, args);
  if (maybe_result.IsEmpty()) return wasm_val_vec_empty();
  auto result = maybe_result.ToLocalChecked();

  own wasm_val_vec_t results = wasm_val_vec_empty();
  if (type->results.size == 0) {
    assert(result->IsUndefined());
  } else if (type->results.size == 1) {
    assert(!result->IsUndefined());
    auto v = wasm_v8_to_val(store, result, type->results.data[0]);
    results = wasm_val_vec_new(1, &v);
  } else {
    UNIMPLEMENTED("multiple results");
  }

  delete[] args;
  return results;
}


// Global Instances

struct wasm_global_t : wasm_wrapper_t<v8::Object, wasm_global_t> {
  own wasm_globaltype_t* type;

  wasm_global_t(wasm_store_t* store, v8::Local<v8::Object> obj, wasm_globaltype_t* type) :
    wasm_wrapper_t(store, obj), type(type) {}

  void finalize() {
    wasm_globaltype_delete(type);
    delete this;
  }
};

WASM_DEFINE_OBJECT(wasm_global)

wasm_globaltype_t* wasm_global_type(wasm_global_t* global) {
  return global->type;
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

wasm_val_t wasm_global_get(wasm_global_t* global) {
  auto store = global->store();
  auto isolate = store->isolate();
  v8::HandleScope handle_scope(isolate);
  auto context = store->context();

  // TODO(wasm+): remove
  if (store->v8_function(V8_F_GLOBAL_GET).IsEmpty()) {
    UNIMPLEMENTED("wasm_global_get");
  }

  auto maybe_val =
    store->v8_function(V8_F_GLOBAL_GET)->Call(context, global->obj(), 0, nullptr);
  if (maybe_val.IsEmpty()) return wasm_null_val();
  auto val = maybe_val.ToLocalChecked();

  return wasm_v8_to_val(store, val, global->type->content);
}

void wasm_global_set(wasm_global_t* global, wasm_val_t val) {
  auto store = global->store();
  auto isolate = store->isolate();
  v8::HandleScope handle_scope(isolate);
  auto context = store->context();

  assert(val.kind == wasm_valtype_kind(global->type->content));

  // TODO(wasm+): remove
  if (store->v8_function(V8_F_GLOBAL_SET).IsEmpty()) {
    UNIMPLEMENTED("wasm_global_set");
  }

  v8::Local<v8::Value> args[] = { wasm_val_to_v8(store, val) };
  store->v8_function(V8_F_GLOBAL_SET)->Call(context, global->obj(), 1, args);
}


// Table Instances

struct wasm_table_t : wasm_wrapper_t<v8::Object, wasm_table_t> {
  own wasm_tabletype_t* type;

  wasm_table_t(wasm_store_t* store, v8::Local<v8::Object> obj, wasm_tabletype_t* type) :
    wasm_wrapper_t(store, obj), type(type) {}

  void finalize() {
    wasm_tabletype_delete(type);
    delete this;
  }
};

WASM_DEFINE_OBJECT(wasm_table)

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

wasm_tabletype_t* wasm_table_type(wasm_table_t* table) {
  return table->type;
}


wasm_ref_t* wasm_table_get(wasm_table_t*, wasm_table_size_t index) {
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

struct wasm_memory_t : wasm_wrapper_t<v8::Object, wasm_memory_t> {
  own wasm_memtype_t* type;

  wasm_memory_t(wasm_store_t* store, v8::Local<v8::Object> obj, wasm_memtype_t* type) :
    wasm_wrapper_t(store, obj), type(type) {}

  void finalize() {
    wasm_memtype_delete(type);
    delete this;
  }
};

WASM_DEFINE_OBJECT(wasm_memory)

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

wasm_memtype_t* wasm_memory_type(wasm_memory_t* memory) {
  return memory->type;
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

WASM_DEFINE_VEC(wasm_extern, )

extern "C++"
v8::Local<v8::Object> wasm_extern_to_v8(wasm_extern_t ex) {
  switch (ex.kind) {
    case WASM_EXTERN_FUNC: return ex.func->obj();
    case WASM_EXTERN_GLOBAL: return ex.global->obj();
    case WASM_EXTERN_TABLE: return ex.table->obj();
    case WASM_EXTERN_MEMORY: return ex.memory->obj();
  }
}


// Module Instances

struct wasm_instance_t : wasm_wrapper_t<v8::Object, wasm_instance_t> {
  wasm_extern_vec_t exports;

  using wasm_wrapper_t::wasm_wrapper_t;

  void finalize() {
    wasm_extern_vec_delete(exports);
    delete this;
  }
};

WASM_DEFINE_OBJECT(wasm_instance)

own wasm_instance_t* wasm_instance_new(wasm_store_t* store, wasm_module_t* module, wasm_extern_vec_t imports) {
  auto isolate = store->isolate();
  auto context = store->context();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Value> imports_args[] = { module->obj() };
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

  v8::Local<v8::Value> instantiate_args[] = {module->obj(), imports_obj};
  auto instance_obj =
    store->v8_function(V8_F_INSTANCE)->NewInstance(context, 2, instantiate_args).ToLocalChecked();
  auto exports_obj = v8::Local<v8::Object>::Cast(
    instance_obj->Get(context, store->v8_string(V8_S_EXPORTS)).ToLocalChecked());
  assert(!exports_obj.IsEmpty() && exports_obj->IsObject());

  auto instance = new wasm_instance_t(store, exports_obj);

  instance->exports = wasm_extern_vec_new_uninitialized(module->exports.size);
  for (size_t i = 0; i < module->exports.size; ++i) {
    auto name = module->exports.data[i]->name;
    auto maybe_name_obj = v8::String::NewFromUtf8(isolate, name.data,
      v8::NewStringType::kNormal, name.size);
    if (maybe_name_obj.IsEmpty()) return nullptr;
    auto name_obj = maybe_name_obj.ToLocalChecked();
    auto obj = v8::Local<v8::Function>::Cast(
      exports_obj->Get(context, name_obj).ToLocalChecked());

    switch (wasm_externtype_kind(module->exports.data[i]->type)) {
      case WASM_EXTERN_FUNC: {
        auto type = wasm_externtype_as_functype(module->exports.data[i]->type);
        auto func_obj = v8::Local<v8::Function>::Cast(obj);
        auto type_clone = wasm_functype_clone(type);
        if (type_clone == nullptr) return nullptr;
        auto func = new wasm_func_t(store, func_obj, type_clone);
        instance->exports.data[i] = wasm_extern_func(func);
      } break;
      case WASM_EXTERN_GLOBAL: {
        auto type = wasm_externtype_as_globaltype(module->exports.data[i]->type);
        auto type_clone = wasm_globaltype_clone(type);
        if (type_clone == nullptr) return nullptr;
        auto global = new wasm_global_t(store, obj, type_clone);
        instance->exports.data[i] = wasm_extern_global(global);
      } break;
      case WASM_EXTERN_TABLE: {
        auto type = wasm_externtype_as_tabletype(module->exports.data[i]->type);
        auto type_clone = wasm_tabletype_clone(type);
        if (type_clone == nullptr) return nullptr;
        auto table = new wasm_table_t(store, obj, type_clone);
        instance->exports.data[i] = wasm_extern_table(table);
      } break;
      case WASM_EXTERN_MEMORY: {
        auto type = wasm_externtype_as_memtype(module->exports.data[i]->type);
        auto type_clone = wasm_memtype_clone(type);
        if (type_clone == nullptr) return nullptr;
        auto memory = new wasm_memory_t(store, obj, type_clone);
        instance->exports.data[i] = wasm_extern_memory(memory);
      } break;
    }
  }

  return instance;
}

wasm_extern_t wasm_instance_export(wasm_instance_t* instance, size_t index) {
  if (index >= instance->exports.size) return wasm_extern_func(nullptr);
  return instance->exports.data[index];
}

own own wasm_extern_vec_t wasm_instance_exports(wasm_instance_t* instance) {
  return wasm_extern_vec_clone(instance->exports);
}

}  // extern "C"
