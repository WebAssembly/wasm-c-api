#include "wasm.hh"
#include "wasm-bin.hh"

#include "v8.h"
#include "libplatform/libplatform.h"

#include <stdio.h>


///////////////////////////////////////////////////////////////////////////////
// Auxiliaries

void UNIMPLEMENTED(const char* s) {
  printf("Wasm API: %s not supported yet!\n", s);
  exit(1);
}


///////////////////////////////////////////////////////////////////////////////
// Type Representations

// Value Types

valtype::~valtype() {}

auto valtype::make(valkind k) -> own<valtype*> {
  return own<valtype*>(reinterpret_cast<valtype*>(static_cast<intptr_t>(k)));
}

auto valtype::clone() const -> own<valtype*> {
  return make(kind());
}

auto valtype::kind() const -> valkind {
  return static_cast<valkind>(reinterpret_cast<intptr_t>(this));
}


// Extern Types

struct externtype_impl : externtype {
  externkind kind;

  explicit externtype(externtype kind) : kind(kind) {}
  virtual ~externtype() {}
};


// Function Types

struct functype_impl : functype, externtype_impl {
  own<vec<valtype*>> params;
  own<vec<valtype*>> results;

  functype_impl(own<vec<valtype*>> params, own<vec<valtype*>> results) :
    externtype_impl(EXTERN_FUNC), params(params), results(results) {}
};

functype::~functype() {
  static_cast<functype_impl*>(this)->~functype_impl();
}

auto functype::make(own<vec<valtype*>> params, own<vec<valtype*>> results)
  -> own<functype*> {
  return own<functype*>(new functype_impl(params, results));
}

auto functype::clone() const -> own<functype*> {
  return make(params()->clone(), results()->clone());
}

auto functype::params() const -> vec<valtype*> {
  return static_cast<functype_impl*>(this)->params.get();
}

auto functype::results() const -> vec<valtype*> {
  return static_cast<functype_impl*>(this)->results.get();
}


// Global Types

struct globaltype_impl : globaltype, externtype_impl {
  own<valtype*> content;
  mut mut;

  globaltype_impl(own<valtype*> content, mut mut) :
    externtype_impl(EXTERN_GLOBAL), content(content), mut(mut) {}
};

globaltype::~globaltype() {
  static_cast<globaltype_impl*>(this)->~globaltype_impl();
}

auto globaltype::make(own<valtype*> content, mut mut) -> own<globaltype*> {
  return own<globaltype*>(new globaltype_impl(content, mut));
}

auto globaltype::clone() const -> own<functype*> {
  return make(content(), mut());
}

auto globaltype::content() const -> valtype* {
  return static_cast<globaltype_impl*>(this)->content.get();
}

auto globaltype::mut() const -> mut {
  return static_cast<globaltype_impl*>(this)->mut;
}


// Table Types

struct tabletype_impl : tabletype, externtype_impl {
  own<reftype*> elem;
  limits limits;

  tabletype_impl(own<reftype*> elem, limits limits) :
    externtype_impl(EXTERN_TABLE), elem(elem), limits(limits) {}
};

tabletype::~tabletype() {
  static_cast<tabletype_impl*>(this)->~tabletype_impl();
}

auto tabletype::make(own<valtype*> elem, limits limits) -> own<tabletype*> {
  return own<tabletype*>(new tabletype_impl(elem, limits));
}

auto tabletype::clone() const -> own<tabletype*> {
  return make(element()->clone(), limits());
}

auto tabletype::element() const -> valtype* {
  return static_cast<tabletype_impl*>(this)->elem.get();
}

auto tabletype::limits() const -> limits {
  return static_cast<tabletype_impl*>(this)->limits;
}


// Memory Types

struct memtype_impl : memtype, externtype_impl {
  limits limits;

  memtype(limits limits) :
    externtype_impl(EXTERN_MEMORY), limits(limits) {}
};

memtype::~memtype() {
  static_cast<memtype_impl*>(this)->~memtype_impl();
}

auto memtype::make(limits limits) -> own<memtype*> {
  return own<memtype*>(new memtype_impl(limits));
}

auto memtype::clone() const -> own<memtype*> {
  return memtype::make(limits());
}

auto memtype::limits() const -> limits {
  return static_cast<memtype_impl*>(this)->limits;
}


// Extern Types

externtype::~externtype() {
  static_cast<externtype_impl*>(this)->~externtype_impl();
}

auto externtype::make(own<functype*> ft) -> own<externtype*> {
  return static_cast<functype_impl*>(ft.release());
}

auto externtype::make(own<globaltype*> gt) -> own<externtype*> {
  return static_cast<globaltype_impl*>(gt.release());
}

auto externtype::make(own<tabletype*> tt) -> own<externtype*> {
  return static_cast<tabletype_impl*>(tt.release());
}

auto externtype::make(own<memtype*> mt) -> own<externtype*> {
  return static_cast<memtype_impl*>(mt.release());
}

auto externtype::clone() const -> own<externtype*> {
  switch (kind()) {
    case EXTERN_FUNC: return make(func()->clone());
    case EXTERN_GLOBAL: return make(global()->clone());
    case EXTERN_TABLE: return make(table()->clone());
    case EXTERN_MEMORY: return make(memory()->clone());
  }
}

auto externtype::kind() const -> externkind {
  return static_cast<externtype_impl*>(this)->kind;
}

auto externtype::func() const -> functype* {
  return kind() == EXTERN_FUNC ? static_cast<functype*>(this) : nullptr;
}

auto externtype::global() const -> globaltype* {
  return kind() == EXTERN_GLOBAL ? static_cast<globaltype*>(this) : nullptr;
}

auto externtype::table() const -> tabletype* {
  return kind() == EXTERN_TABLE ? static_cast<tabletype*>(this) : nullptr;
}

auto externtype::memory() const -> memtype* {
  return kind() == EXTERN_MEMORY ? static_cast<memtype*>(this) : nullptr;
}


// Import Types

struct importtype_impl : importtype {
  own<name> module;
  own<name> name;
  own<externtype*> type;

  importtype_impl(own<name> module, own<name> name, own<externtype*> type) :
    module(module), name(name), type(type) {}
};

importtype::~importtype() {
  static_cast<importtype_impl*>(this)->~importtype_impl();
}

auto importtype::make(own<name> module, own<name> name, own<externtype*> type) -> own<importtype*> {
  return own<importtype*>(new importtype_impl(module, name, type));
}

auto importtype::clone() const -> own<importtype*> {
  return make(module()->clone(), name()->clone(), type()->clone());
}

auto importtype::module() const -> name {
  return static_cast<importtype_impl*>(this)->module.get();
}

auto importtype::name() const -> name {
  return static_cast<importtype_impl*>(this)->name.get();
}

auto importtype::type() const -> externtype* {
  return static_cast<importtype_impl*>(this)->type.get();
}


// Export Types

struct exporttype_impl : exporttype {
  own<name> name;
  own<externtype*> type;

  exporttype_impl(own<name> name, own<externtype*> type) :
    name(name), type(type) {}
};

exporttype::~exporttype() {
  static_cast<exporttype_impl*>(this)->~exporttype_impl();
}

auto exporttype::make(own<name> name, own<externtype*> type) -> own<exporttype*> {
  return own<exporttype*>(new exporttype_impl(name, type));
}

auto exporttype::clone() const -> own<exporttype*> {
  return make(name()->clone(), type()->clone());
}

auto exporttype::name() const -> name {
  return static_cast<exporttype_impl*>(this)->name.get();
}

auto exporttype::type() const -> externtype* {
  return static_cast<exporttype_impl*>(this)->type.get();
}


///////////////////////////////////////////////////////////////////////////////
// Runtime Environment

// Initialisation

struct config_impl : config {};

config::~config() {
  static_cast<config_impl*>(this)->~config_impl();
}

auto config::make() -> own<config*> {
  return own<config*>(new config_impl());
}


void init(int argc, const char *const argv[], own<config*> config) {
  v8::V8::InitializeExternalStartupData(argv[0]);
  static std::unique_ptr<v8::Platform> platform =
    v8::platform::NewDefaultPlatform();
  v8::V8::InitializePlatform(platform.get());
  v8::V8::Initialize();
}

void deinit() {
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

class store_impl : public store {
  friend own<store*> store::make();

  v8::Isolate::CreateParams create_params_;
  v8::Isolate *isolate_;
  v8::Eternal<v8::Context> context_;
  v8::Eternal<v8::ObjectTemplate> callback_data_template_;
  v8::Eternal<v8::String> strings_[V8_S_COUNT];
  v8::Eternal<v8::Function> functions_[V8_F_COUNT];
  v8::Eternal<v8::Object> cache_;

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
};

auto store::make() -> own<store*> {
  std::unique_ptr<store_impl> store = new store_impl();
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

  return own<store*>(store.release());
};

store::~store() {
  context()->Exit();
  isolate_->Exit();
  isolate_->Dispose();
  delete create_params_.array_buffer_allocator;
  static_cast<store_impl*>(this)->~store_impl();
}


///////////////////////////////////////////////////////////////////////////////
// Conversions of types from and to V8 objects

// Types

v8::Local<v8::Value> valtype_to_v8(store_impl* store, valtype* type) {
  v8_string_t string;
  switch (type->kind()) {
    case I32: string = V8_S_I32; break;
    case I64: string = V8_S_I64; break;
    case F32: string = V8_S_F32; break;
    case F64: string = V8_S_F64; break;
    case ANYREF: string = V8_S_ANYREF; break;
    case FUNCREF: string = V8_S_ANYFUNC; break;
    default:
      // TODO(wasm+): support new value types
      assert(false);
  }
  return store->v8_string(string);
}

v8::Local<v8::Boolean> mut_to_v8(store_impl* store, mut mut) {
  return v8::Boolean::New(store->isolate(), mut == VAR);
}

void limits_to_v8(store* store, limits limits, v8::Local<v8::Object> desc) {
  auto isolate = store->isolate();
  auto context = store->context();
  desc->DefineOwnProperty(context, store->v8_string(V8_S_MINIMUM),
    v8::Integer::NewFromUnsigned(isolate, limits.min));
  if (limits.max != limits(0).max) {
    desc->DefineOwnProperty(context, store->v8_string(V8_S_MAXIMUM),
      v8::Integer::NewFromUnsigned(isolate, limits.max));
  }
}

v8::Local<v8::Object> globaltype_to_v8(store_impl* store, globaltype* type) {
  // TODO: define templates
  auto isolate = store->isolate();
  auto context = store->context();
  auto desc = v8::Object::New(isolate);
  desc->DefineOwnProperty(context, store->v8_string(V8_S_VALUE),
    valtype_to_v8(store, type->content()));
  desc->DefineOwnProperty(context, store->v8_string(V8_S_MUTABLE),
    mut_to_v8(store, type->mut()));
  return desc;
}

v8::Local<v8::Object> tabletype_to_v8(store_impl* store, tabletype* type) {
  auto isolate = store->isolate();
  auto context = store->context();
  auto desc = v8::Object::New(isolate);
  desc->DefineOwnProperty(context, store->v8_string(V8_S_ELEMENT),
    valtype_to_v8(store, type->element()));
  limits_to_v8(store, type->limits(), desc);
  return desc;
}

v8::Local<v8::Object> memtype_to_v8(store_impl* store, memtype* type) {
  auto isolate = store->isolate();
  auto context = store->context();
  auto desc = v8::Object::New(isolate);
  limits_to_v8(store, type->limits(), desc);
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


///////////////////////////////////////////////////////////////////////////////
// Runtime Values

// References

// - each API wrapper has C side reference count
// - when refcount goes to 0 (drop), SetWeak on persistent handle
// - finalizer deletes wrapper
// - store has weakmap for each V8 object category, mapping to API wrapper (TODO)
// - when returning V8 object to C, looks up wrapper or create fresh (TODO)
// - when wrapper was found in weakmap, bump refcnt (take)
// - if refcnt was 0, ClearWeak on persistent handle

// TODO: make refs casted persistent handles directly, 
// and put extra info on object, with fallback to a weakmap when frozen

class ref_impl : public ref {
  struct data {
    int count_ = 1;
    store_impl* store_;
    v8::Persistent<v8::Object> obj_;
    void* host_info_ = nullptr;
    void (*host_finalizer_)(void*) = nullptr;

    data(store_impl* store, v8::Local<v8::Object> obj) :
      store_(store), obj_(store->isolate(), obj) {}

    virtual ~data() {}

    void take() {
      if (count_++ == 0) {
        obj_.ClearWeak();
      }
    }
    bool drop() {
      if (--count_ == 0) {
        obj_.template SetWeak<data>(static_cast<data*>(this), &finalizer, v8::WeakCallbackType::kParameter);
      }
      return count_ == 0;
    }
  };

  data* data_;

public:
  ref_impl(data* data) : data_(data) {}

  store_impl* store() const {
    return data_->store_;
  }

  v8::Local<v8::Object> v8_object() const {
    return data_->obj_.Get(data_->store_->isolate());
  }

  void* get_host_info() const {
    return data_->host_info_;
  }
  void set_host_info(void* info, void (*finalizer)(void*) = nullptr) {
    data_->host_info_ = info;
    data_->host_finalizer_ = finalizer;
  }

private:
  static void finalizer(const v8::WeakCallbackInfo<data>& info) {
    auto data = info.GetParameter();
    assert(data->count_ == 0);
    if (data->host_finalizer_) (*data->host_finalizer_)(data->host_info_);
    delete data;
  }
};

ref::~ref() {
  auto data = static_cast<ref_impl*>(this)->~ref_impl()->data;
  if (data) data->drop();
}

auto ref::clone() -> own<ref*> {
  auto data = static_cast<ref_impl*>(this)->~ref_impl()->data;
  if (data) data->take();
  return own<ref*>(ref_impl(data));
}


// Values

auto val::clone() const -> own<val> {
  auto v = own<val>(*this);
  if (is_refkind(kind_) && v.ref != nullptr) v.ref = v.ref->clone();
  return v;
}


v8::Local<v8::Value> val_to_v8(store_impl* store, val v) {
  auto isolate = store_impl->isolate();
  switch (v.kind) {
    case I32: return v8::Integer::NewFromUnsigned(isolate, v.i32);
    case I64: UNIMPLEMENTED("i64 value");
    case F32: return v8::Number::New(isolate, v.f32);
    case F64: return v8::Number::New(isolate, v.f64);
    case ANYREF:
    case FUNCREF: {
      if (v.ref == nullptr) {
        return v8::Null(isolate);
      } else {
        UNIMPLEMENTED("ref value");
      }
    }
    default: assert(false);
  }
}

val v8_to_val(store_impl* store, v8::Local<v8::Value> value, valtype* t) {
  auto context = store->context();
  switch (value.kind()) {
    case I32: return value->Int32Value(context).ToChecked();
    case I64: UNIMPLEMENTED("i64 value");
    case F32:
      return static_cast<float32_t>(value->NumberValue(context).ToChecked());
    case F64: return value->NumberValue(context).ToChecked();
    case ANYREF:
    case FUNCREF: {
      if (value->IsNull()) {
        return nullptr;
      } else {
        UNIMPLEMENTED("ref value");
      }
    }
  }
}


///////////////////////////////////////////////////////////////////////////////
// Runtime Objects

// Modules

struct module_impl : module, ref_impl {
  own<vec<importtype>> imports;
  own<vec<exporttype>> exports;

  module_impl(store_impl* store, v8::Local<v8::Object> obj,
    own<vec<importtype>> imports, own<vec<exporttype>> exports) :
    ref_impl(new data(store, obj)), imports(imports), exports(exports) {}
};

module::~module() {}

auto module::make(store* store_abs, vec<byte_t> binary) -> own<module*> {
  auto store = static_cast<store_impl*>(store_abs);
  auto isolate = store->isolate();
  auto context = store->context();
  v8::HandleScope handle_scope(isolate);

  auto array_buffer = v8::ArrayBuffer::New(isolate, binary.data(), binary.size());

  v8::Local<v8::Value> args[] = {array_buffer};
  auto maybe_obj =
    store_impl->v8_function(V8_F_MODULE)->NewInstance(context, 1, args);
  if (maybe_obj.IsEmpty()) return nullptr;
  auto obj = maybe_obj.ToLocalChecked();

  // TODO(wasm+): use JS API once available?
  auto imports_exports = wasm::bin::imports_exports(binary);
  // TODO store->cache_set(module_obj, module);
  return own<module*>(new module_impl(store, obj, std::get<0>(imports_exports), std::get<1>(imports_exports)));
}

auto module::validate(store* store_abs, vec<byte_t> binary) -> bool {
  auto store = static_cast<store_impl*>(store_abs);
  v8::Isolate* isolate = store->isolate();
  v8::HandleScope handle_scope(isolate);

  auto array_buffer = v8::ArrayBuffer::New(isolate, binary.data, binary.size);

  v8::Local<v8::Value> args[] = {array_buffer};
  auto result = store->v8_function(V8_F_VALIDATE)->Call(
    store->context(), v8::Undefined(isolate), 1, args);
  if (result.IsEmpty()) return false;

  return result.ToLocalChecked()->IsTrue();
}

auto module::imports() -> own<vec<importtype>> {
  return static_cast<module_impl*>(this)->imports.clone();
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

auto module::exports() -> own<vec<exporttype>> {
  return static_cast<module_impl*>(this)->exports.clone();
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

auto module::serialize() -> own<vec<byte_t>> {
  UNIMPLEMENTED("wasm_module_serialize");
}

auto module::deserialize(vec<byte_t> serialized) -> own<module*> {
  UNIMPLEMENTED("wasm_module_deserialize");
}


// Host Objects

struct hostobj_impl : hostobj, ref_impl {
  using ref_impl::ref_impl;
};

hostobj::~hostobj() {}

auto hostobj::make(store* store_abs) -> own<hostobj*> {
  auto store = static_cast<store_impl*>(store_abs);
  auto isolate = store->isolate();
  v8::HandleScope handle_scope(isolate);

  auto obj = v8::Object::New(isolate);
  return own<hostobj*>(new hostobj_impl(store, obj));
}


// Function Instances

struct func_impl : func, ref_impl {
  own<functype*> type;
  callback callback;

  func_impl(store_impl* store, v8::Local<v8::Function> obj, own<functype*> type, callback callback = nullptr) :
    ref_impl(store, obj), type(type), callback(callback) {}

  v8::Local<v8::Function> v8_function() const {
    return v8::Local<v8::Function>::Cast(v8_object());
  }

private:
  void callback(const v8::FunctionCallbackInfo<v8::Value>&);
};

void func_impl::callback(const v8::FunctionCallbackInfo<v8::Value>& info) {
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

  own<vec<val>> own_args = vec<val>::make(type_params.size);
  auto args = own_args.borrow();
  for (size_t i = 0; i < type_params.size; ++i) {
    args.data[i] = v8_to_val(store, info[i], type_params.data[i]);
  }

  own<vec<val>> own_results = func->callback(args);
  auto results = own_results.borrow();

  assert(type_results.size == results.size);

  auto ret = info.GetReturnValue();
  if (type_results.size == 0) {
    ret.SetUndefined();
  } else if (type_results.size == 1) {
    assert(results.data[0].kind == type_results.data[0]->kind());
    ret.Set(val_to_v8(store, results.data[0]));
  } else {
    UNIMPLEMENTED("multiple results");
  }
}


func::~func() {}

auto func::make(store* store_abs, functype* type, callback callback) -> own<func*> {
  auto store = static_cast<store_impl*>(store_abs);
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

auto func::make(store* store_abs, functype* type, callback_with_env callback, wasm_ref_t *env) -> own<func*> {
  UNIMPLEMENTED("func::callback_with_env");
}

auto func::type() -> own<functype*> {
  return static_cast<func_impl*>(this)->type->clone();
}

auto func::call(func* func_abs, vec<val> args) -> own<vec<val>> {
  auto func = static_cast<func_impl*>(func_abs);
  auto store = func->store();
  auto isolate = store->isolate();
  v8::HandleScope handle_scope(isolate);

  auto context = store->context();
  auto type = func->type.get();
  auto type_params = type->params;
  auto type_results = type->results;

  assert(type_params.size == args.size);

  auto v8_args = new v8::Local<v8::Value>[type_params.size];
  for (size_t i = 0; i < type_params.size; ++i) {
    assert(args.data[i].kind == type_params.data[i]->kind());
    v8_args[i] = val_to_v8(store, args.data[i]);
  }

  auto maybe_result =
    func->v8_function()->Call(context, v8::Undefined(isolate), args.size, v8_args);
  if (maybe_result.IsEmpty()) return own<vec<val>>();
  auto result = maybe_result.ToLocalChecked();

  if (type_results.size == 0) {
    assert(result->IsUndefined());
    return own<vec<val>>();
  } else if (type_results.size == 1) {
    assert(!result->IsUndefined());
    auto val = v8_to_val(store, result, type_results.data[0]);
    return own<vec<val>>(vec<val>::make(1, &val));
  } else {
    UNIMPLEMENTED("multiple results");
  }
}


// Global Instances

struct global_impl : global, ref_impl {
  own<globaltype*> type;

  global_impl(store_impl* store, v8::Local<v8::Object> obj, own<globaltype*> type) :
    wasm_ref_t(store, obj), type(type) {}
};

global::~global() {}

auto func::make(store* store_abs, globaltype* type, val val) -> own<global*> {
  auto store = static_cast<store_impl*>(store_abs);
  auto isolate = store->isolate();
  v8::HandleScope handle_scope(isolate);
  auto context = store->context();

  assert(type->content()->kind() == val.kind);

  // TODO(wasm+): remove
  if (store->v8_function(V8_F_GLOBAL).IsEmpty()) {
    UNIMPLEMENTED("wasm_global_new");
  }

  v8::Local<v8::Value> args[] = {
    globaltype_to_v8(store, type),
    val_to_v8(store, val)
  };
  auto maybe_obj =
    store->v8_function(V8_F_GLOBAL)->NewInstance(context, 2, args);
  if (maybe_obj.IsEmpty()) return nullptr;
  auto obj = maybe_obj.ToLocalChecked();

  auto type_clone = type->clone();
  if (type_clone == nullptr) return nullptr;
  return own<global*>(new global_impl(store, obj, type_clone));
}

auto func::type() const -> own<globaltype*> {
  return static_cast<global_impl*>(global)->type->clone();
}

auto func::get() const -> own<val> {
  auto global* = static_cast<global_impl*>(this);
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
  if (maybe_val.IsEmpty()) return val();
  auto val = maybe_val.ToLocalChecked();

  return v8_to_val(store, val, global->type->content());
}

void func::set(val val) {
  auto global* = static_cast<global_impl*>(this);
  auto store = global->store();
  auto isolate = store->isolate();
  v8::HandleScope handle_scope(isolate);
  auto context = store->context();

  assert(val.kind == global->type->content());

  // TODO(wasm+): remove
  if (store->v8_function(V8_F_GLOBAL_SET).IsEmpty()) {
    UNIMPLEMENTED("wasm_global_set");
  }

  v8::Local<v8::Value> args[] = { val_to_v8(store, val) };
  store->v8_function(V8_F_GLOBAL_SET)->Call(context, global->v8_object(), 1, args);
}


// Table Instances

struct table_impl : table, ref_imple {
  own<tabletype*> type;

  table_impl(store_impl* store, v8::Local<v8::Object> obj, tabletype* type) :
    ref_impl(store, obj), type(type) {}
};

table::~table() {}

auto make(store* store_abs, tabletype* type, ref* ref) -> own<table*> {
  auto store = static_cast<store_impl*>(store_abs);
  auto isolate = store->isolate();
  v8::HandleScope handle_scope(isolate);
  auto context = store->context();

  // TODO(wasm+): handle reference initialiser
  v8::Local<v8::Value> args[] = { tabletype_to_v8(store, type) };
  auto maybe_obj =
    store->v8_function(V8_F_TABLE)->NewInstance(context, 1, args);
  if (maybe_obj.IsEmpty()) return nullptr;
  auto obj = maybe_obj.ToLocalChecked();

  auto type_clone = type->clone();
  if (type_clone == nullptr) return nullptr;
  return own<table*>(new wasm_table_t(store, obj, type_clone));
}

  ~table();

  using size_t = uint32_t;

  static auto make(store*, tabletype*, ref*) -> own<table*>;

  auto kind() const -> externkind override { return EXTERN_TABLE; };
  auto type() const -> own<tabletype*>;
  auto get(size_t index) const -> own<ref*>;
  void set(size_t index, ref*);
  auto size() const -> size_t;
  auto grow(size_t delta) -> size_t;
own wasm_tabletype_t* wasm_table_type(wasm_table_t* table) {
  // TODO: query and update min
  return wasm_tabletype_clone(table->type.borrow());
}


  ~table();

  using size_t = uint32_t;

  static auto make(store*, tabletype*, ref*) -> own<table*>;

  auto kind() const -> externkind override { return EXTERN_TABLE; };
  auto type() const -> own<tabletype*>;
  auto get(size_t index) const -> own<ref*>;
  void set(size_t index, ref*);
  auto size() const -> size_t;
  auto grow(size_t delta) -> size_t;
own wasm_ref_t* wasm_table_get(wasm_table_t*, wasm_table_size_t index) {
  UNIMPLEMENTED("wasm_table_get");
}

  ~table();

  using size_t = uint32_t;

  static auto make(store*, tabletype*, ref*) -> own<table*>;

  auto kind() const -> externkind override { return EXTERN_TABLE; };
  auto type() const -> own<tabletype*>;
  auto get(size_t index) const -> own<ref*>;
  void set(size_t index, ref*);
  auto size() const -> size_t;
  auto grow(size_t delta) -> size_t;
void wasm_table_set(wasm_table_t*, wasm_table_size_t index, wasm_ref_t*) {
  UNIMPLEMENTED("wasm_table_set");
}

  ~table();

  using size_t = uint32_t;

  static auto make(store*, tabletype*, ref*) -> own<table*>;

  auto kind() const -> externkind override { return EXTERN_TABLE; };
  auto type() const -> own<tabletype*>;
  auto get(size_t index) const -> own<ref*>;
  void set(size_t index, ref*);
  auto size() const -> size_t;
  auto grow(size_t delta) -> size_t;
wasm_table_size_t wasm_table_size(wasm_table_t*) {
  UNIMPLEMENTED("wasm_table_size");
}

  ~table();

  using size_t = uint32_t;

  static auto make(store*, tabletype*, ref*) -> own<table*>;

  auto kind() const -> externkind override { return EXTERN_TABLE; };
  auto type() const -> own<tabletype*>;
  auto get(size_t index) const -> own<ref*>;
  void set(size_t index, ref*);
  auto size() const -> size_t;
  auto grow(size_t delta) -> size_t;
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

wasm_memory_t* wasm_memory_new(wasm_store_t* store_abs, wasm_memtype_t* type) {
  auto store = static_cast<store_impl*>(store_abs);
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

own wasm_instance_t* wasm_instance_new(wasm_store_t* store_abs, wasm_module_t* module, wasm_extern_vec_t imports) {
  auto store = static_cast<store_impl*>(store_abs);
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
