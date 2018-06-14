#include "wasm.hh"
#include "wasm-bin.hh"

#include "v8.h"
#include "libplatform/libplatform.h"

#include <iostream>


namespace wasm {

///////////////////////////////////////////////////////////////////////////////
// Auxiliaries

[[noreturn]] void UNIMPLEMENTED(const char* s) {
  std::cerr << "Wasm API: " << s << " not supported yet!\n";
  exit(1);
}


template<class C> struct implement;

template<class C>
auto impl(C* x) -> typename implement <C>::type* {
  return reinterpret_cast<typename implement<C>::type*>(x);
}

template<class C>
auto impl(const C* x) -> const typename implement<C>::type* {
  return reinterpret_cast<const typename implement<C>::type*>(x);
}

template<class C>
auto seal(typename implement <C>::type* x) -> C* {
  return reinterpret_cast<C*>(x);
}

template<class C>
auto seal(const typename implement <C>::type* x) -> const C* {
  return reinterpret_cast<const C*>(x);
}


///////////////////////////////////////////////////////////////////////////////
// Runtime Environment

// Initialisation

struct config_impl {};

template<> struct implement<config> { using type = config_impl; };

config::~config() {
  impl(this)->~config_impl();
}

void config::operator delete(void *p) {
  ::operator delete(p);
}

auto config::make() -> own<config*> {
  return own<config*>(seal<config>(new(std::nothrow) config_impl()));
}


struct engine_impl {
  ~engine_impl() {
    v8::V8::Dispose();
    v8::V8::ShutdownPlatform();
  }
};

template<> struct implement<engine> { using type = engine_impl; };

engine::~engine() {
  impl(this)->~engine_impl();
}

void engine::operator delete(void *p) {
  ::operator delete(p);
}

auto engine::make(int argc, const char *const argv[], own<config*>&& config) -> own<engine*> {
  auto engine = make_own(seal<wasm::engine>(new(std::nothrow) engine_impl));
  if (!engine) return engine;
  v8::V8::InitializeExternalStartupData(argv[0]);
  static std::unique_ptr<v8::Platform> platform =
    v8::platform::NewDefaultPlatform();
  v8::V8::InitializePlatform(platform.get());
  v8::V8::Initialize();
  return engine;
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

class store_impl {
  friend own<store*> store::make(own<engine*>&);

  v8::Isolate::CreateParams create_params_;
  v8::Isolate *isolate_;
  v8::Eternal<v8::Context> context_;
  v8::Eternal<v8::ObjectTemplate> callback_data_template_;
  v8::Eternal<v8::String> strings_[V8_S_COUNT];
  v8::Eternal<v8::Function> functions_[V8_F_COUNT];
  v8::Eternal<v8::Object> cache_;

public:
  ~store_impl() {
    context()->Exit();
    isolate_->Exit();
    isolate_->Dispose();
    delete create_params_.array_buffer_allocator;
  }

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
};

template<> struct implement<store> { using type = store_impl; };

store::~store() {
  impl(this)->~store_impl();
}

void store::operator delete(void *p) {
  ::operator delete(p);
}

auto store::make(own<engine*>&) -> own<store*> {
  auto store = make_own(new(std::nothrow) store_impl());
  if (!store) return own<wasm::store*>();
  store->create_params_.array_buffer_allocator =
    v8::ArrayBuffer::Allocator::NewDefaultAllocator();

  auto isolate = v8::Isolate::New(store->create_params_);
  if (!isolate) return own<wasm::store*>();
  {
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);

    auto context = v8::Context::New(isolate);
    if (context.IsEmpty()) return own<wasm::store*>();
    v8::Context::Scope context_scope(context);

    auto callback_data_template = v8::ObjectTemplate::New(isolate);
    if (callback_data_template.IsEmpty()) return own<wasm::store*>();
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
      if (maybe.IsEmpty()) return own<wasm::store*>();
      auto string = maybe.ToLocalChecked();
      store->strings_[i] = v8::Eternal<v8::String>(isolate, string);
    }

    auto global = context->Global();
    auto maybe_wasm_name = v8::String::NewFromUtf8(isolate, "WebAssembly",
        v8::NewStringType::kNormal);
    if (maybe_wasm_name.IsEmpty()) return own<wasm::store*>();
    auto wasm_name = maybe_wasm_name.ToLocalChecked();
    auto maybe_wasm = global->Get(context, wasm_name);
    if (maybe_wasm.IsEmpty()) return own<wasm::store*>();
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
      if (maybe_name.IsEmpty()) return own<wasm::store*>();
      auto name = maybe_name.ToLocalChecked();
      assert(!raw_functions[i].carrier->IsEmpty());
      // TODO(wasm+): remove
      if ((*raw_functions[i].carrier)->IsUndefined()) continue;
      auto maybe_obj = (*raw_functions[i].carrier)->Get(context, name);
      if (maybe_obj.IsEmpty()) return own<wasm::store*>();
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
    if (maybe_cache.IsEmpty()) return own<wasm::store*>();
    auto cache = v8::Local<v8::Object>::Cast(maybe_cache.ToLocalChecked());
    store->cache_ = v8::Eternal<v8::Object>(isolate, cache);
  }

  store->isolate()->Enter();
  store->context()->Enter();

  return make_own(seal<wasm::store>(store.release()));
};


///////////////////////////////////////////////////////////////////////////////
// Type Representations

// Value Types

struct valtype_impl {
  valkind kind;

  valtype_impl(valkind kind) : kind(kind) {}
};

template<> struct implement<valtype> { using type = valtype_impl; };

valtype_impl* valtypes[] = {
  new valtype_impl(I32),
  new valtype_impl(I64),
  new valtype_impl(F32),
  new valtype_impl(F64),
  new valtype_impl(ANYREF),
  new valtype_impl(FUNCREF),
};


valtype::~valtype() {}

void valtype::operator delete(void*) {}

auto valtype::make(valkind k) -> own<valtype*> {
  return own<valtype*>(seal<valtype>(valtypes[k]));
}

auto valtype::copy() const -> own<valtype*> {
  return make(kind());
}

auto valtype::kind() const -> valkind {
  return impl(this)->kind;
}


// Extern Types

struct externtype_impl {
  externkind kind;

  explicit externtype_impl(externkind kind) : kind(kind) {}
  virtual ~externtype_impl() {}
};

template<> struct implement<externtype> { using type = externtype_impl; };


externtype::~externtype() {
  impl(this)->~externtype_impl();
}

void externtype::operator delete(void *p) {
  ::operator delete(p);
}

auto externtype::copy() const -> own<externtype*> {
  switch (kind()) {
    case EXTERN_FUNC: return func()->copy();
    case EXTERN_GLOBAL: return global()->copy();
    case EXTERN_TABLE: return table()->copy();
    case EXTERN_MEMORY: return memory()->copy();
  }
}

auto externtype::kind() const -> externkind {
  return impl(this)->kind;
}


// Function Types

struct functype_impl : externtype_impl {
  vec<valtype*> params;
  vec<valtype*> results;

  functype_impl(vec<valtype*>& params, vec<valtype*>& results) :
    externtype_impl(EXTERN_FUNC), params(std::move(params)), results(std::move(results)) {}
};

template<> struct implement<functype> { using type = functype_impl; };


functype::~functype() {}

auto functype::make(vec<valtype*>&& params, vec<valtype*>&& results)
  -> own<functype*> {
  return params && results
    ? own<functype*>(seal<functype>(new(std::nothrow) functype_impl(params, results)))
    : own<functype*>();
}

auto functype::copy() const -> own<functype*> {
  return make(params().copy(), results().copy());
}

auto functype::params() const -> const vec<valtype*>& {
  return impl(this)->params;
}

auto functype::results() const -> const vec<valtype*>& {
  return impl(this)->results;
}


auto externtype::func() -> functype* {
  return kind() == EXTERN_FUNC ? seal<functype>(static_cast<functype_impl*>(impl(this))) : nullptr;
}

auto externtype::func() const -> const functype* {
  return kind() == EXTERN_FUNC ? seal<functype>(static_cast<const functype_impl*>(impl(this))) : nullptr;
}


// Global Types

struct globaltype_impl : externtype_impl {
  own<valtype*> content;
  wasm::mut mut;

  globaltype_impl(own<valtype*>& content, wasm::mut mut) :
    externtype_impl(EXTERN_GLOBAL), content(std::move(content)), mut(mut) {}
};

template<> struct implement<globaltype> { using type = globaltype_impl; };


globaltype::~globaltype() {}

auto globaltype::make(own<valtype*>&& content, wasm::mut mut) -> own<globaltype*> {
  return content
    ? own<globaltype*>(seal<globaltype>(new(std::nothrow) globaltype_impl(content, mut)))
    : own<globaltype*>();
}

auto globaltype::copy() const -> own<globaltype*> {
  return make(content()->copy(), mut());
}

auto globaltype::content() const -> const own<valtype*>& {
  return impl(this)->content;
}

auto globaltype::mut() const -> wasm::mut {
  return impl(this)->mut;
}


auto externtype::global() -> globaltype* {
  return kind() == EXTERN_GLOBAL ? seal<globaltype>(static_cast<globaltype_impl*>(impl(this))) : nullptr;
}

auto externtype::global() const -> const globaltype* {
  return kind() == EXTERN_GLOBAL ? seal<globaltype>(static_cast<const globaltype_impl*>(impl(this))) : nullptr;
}


// Table Types

struct tabletype_impl : externtype_impl {
  own<valtype*> element;
  wasm::limits limits;

  tabletype_impl(own<valtype*>& element, wasm::limits limits) :
    externtype_impl(EXTERN_TABLE), element(std::move(element)), limits(limits) {}
};

template<> struct implement<tabletype> { using type = tabletype_impl; };


tabletype::~tabletype() {}

auto tabletype::make(own<valtype*>&& element, wasm::limits limits) -> own<tabletype*> {
  return element
    ? own<tabletype*>(seal<tabletype>(new(std::nothrow) tabletype_impl(element, limits)))
    : own<tabletype*>();
}

auto tabletype::copy() const -> own<tabletype*> {
  return make(element()->copy(), limits());
}

auto tabletype::element() const -> const own<valtype*>& {
  return impl(this)->element;
}

auto tabletype::limits() const -> wasm::limits {
  return impl(this)->limits;
}


auto externtype::table() -> tabletype* {
  return kind() == EXTERN_TABLE ? seal<tabletype>(static_cast<tabletype_impl*>(impl(this))) : nullptr;
}

auto externtype::table() const -> const tabletype* {
  return kind() == EXTERN_TABLE ? seal<tabletype>(static_cast<const tabletype_impl*>(impl(this))) : nullptr;
}


// Memory Types

struct memtype_impl : externtype_impl {
  wasm::limits limits;

  memtype_impl(wasm::limits limits) :
    externtype_impl(EXTERN_MEMORY), limits(limits) {}
};

template<> struct implement<memtype> { using type = memtype_impl; };


memtype::~memtype() {}

auto memtype::make(wasm::limits limits) -> own<memtype*> {
  return own<memtype*>(seal<memtype>(new(std::nothrow) memtype_impl(limits)));
}

auto memtype::copy() const -> own<memtype*> {
  return memtype::make(limits());
}

auto memtype::limits() const -> wasm::limits {
  return impl(this)->limits;
}


auto externtype::memory() -> memtype* {
  return kind() == EXTERN_MEMORY ? seal<memtype>(static_cast<memtype_impl*>(impl(this))) : nullptr;
}

auto externtype::memory() const -> const memtype* {
  return kind() == EXTERN_MEMORY ? seal<memtype>(static_cast<const memtype_impl*>(impl(this))) : nullptr;
}


// Import Types

struct importtype_impl {
  wasm::name module;
  wasm::name name;
  own<externtype*> type;

  importtype_impl(wasm::name& module, wasm::name& name, own<externtype*>& type) :
    module(std::move(module)), name(std::move(name)), type(std::move(type)) {}
};

template<> struct implement<importtype> { using type = importtype_impl; };


importtype::~importtype() {
  impl(this)->~importtype_impl();
}

void importtype::operator delete(void *p) {
  ::operator delete(p);
}

auto importtype::make(wasm::name&& module, wasm::name&& name, own<externtype*>&& type) -> own<importtype*> {
  return module && name && type
    ? own<importtype*>(seal<importtype>(new(std::nothrow) importtype_impl(module, name, type)))
    : own<importtype*>();
}

auto importtype::copy() const -> own<importtype*> {
  return make(module().copy(), name().copy(), type()->copy());
}

auto importtype::module() const -> const wasm::name& {
  return impl(this)->module;
}

auto importtype::name() const -> const wasm::name& {
  return impl(this)->name;
}

auto importtype::type() const -> const own<externtype*>& {
  return impl(this)->type;
}


// Export Types

struct exporttype_impl {
  own<wasm::name> name;
  own<externtype*> type;

  exporttype_impl(own<wasm::name>& name, own<externtype*>& type) :
    name(std::move(name)), type(std::move(type)) {}
};

template<> struct implement<exporttype> { using type = exporttype_impl; };


exporttype::~exporttype() {
  impl(this)->~exporttype_impl();
}

void exporttype::operator delete(void *p) {
  ::operator delete(p);
}

auto exporttype::make(own<wasm::name>&& name, own<externtype*>&& type) -> own<exporttype*> {
  return name && type
    ? own<exporttype*>(seal<exporttype>(new(std::nothrow) exporttype_impl(name, type)))
    : own<exporttype*>();
}

auto exporttype::copy() const -> own<exporttype*> {
  return make(name().copy(), type()->copy());
}

auto exporttype::name() const -> const wasm::name& {
  return impl(this)->name;
}

auto exporttype::type() const -> const own<externtype*>& {
  return impl(this)->type;
}


///////////////////////////////////////////////////////////////////////////////
// Conversions of types from and to V8 objects

// Types

v8::Local<v8::Value> valtype_to_v8(store_impl* store, const own<valtype*>& type) {
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

void limits_to_v8(store_impl* store, limits limits, v8::Local<v8::Object> desc) {
  auto isolate = store->isolate();
  auto context = store->context();
  void(desc->DefineOwnProperty(context, store->v8_string(V8_S_MINIMUM),
    v8::Integer::NewFromUnsigned(isolate, limits.min)));
  if (limits.max != wasm::limits(0).max) {
    void(desc->DefineOwnProperty(context, store->v8_string(V8_S_MAXIMUM),
      v8::Integer::NewFromUnsigned(isolate, limits.max)));
  }
}

v8::Local<v8::Object> globaltype_to_v8(store_impl* store, const own<globaltype*>& type) {
  auto isolate = store->isolate();
  auto context = store->context();
  auto desc = v8::Object::New(isolate);
  void(desc->DefineOwnProperty(context, store->v8_string(V8_S_VALUE),
    valtype_to_v8(store, type->content())));
  void(desc->DefineOwnProperty(context, store->v8_string(V8_S_MUTABLE),
    mut_to_v8(store, type->mut())));
  return desc;
}

v8::Local<v8::Object> tabletype_to_v8(store_impl* store, const own<tabletype*>& type) {
  auto isolate = store->isolate();
  auto context = store->context();
  auto desc = v8::Object::New(isolate);
  void(desc->DefineOwnProperty(context, store->v8_string(V8_S_ELEMENT),
    valtype_to_v8(store, type->element())));
  limits_to_v8(store, type->limits(), desc);
  return desc;
}

v8::Local<v8::Object> memtype_to_v8(store_impl* store, const own<memtype*>& type) {
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

// Values

auto val_to_v8(store_impl* store, const val& v) -> v8::Local<v8::Value> {
  auto isolate = store->isolate();
  switch (v.kind()) {
    case I32: return v8::Integer::NewFromUnsigned(isolate, v.i32());
    case I64: UNIMPLEMENTED("i64 value");
    case F32: return v8::Number::New(isolate, v.f32());
    case F64: return v8::Number::New(isolate, v.f64());
    case ANYREF:
    case FUNCREF: {
      if (v.ref() == nullptr) {
        return v8::Null(isolate);
      } else {
        UNIMPLEMENTED("ref value");
      }
    }
    default: assert(false);
  }
}

auto v8_to_val(store_impl* store, v8::Local<v8::Value> value, const valtype* t) -> own<val> {
  auto context = store->context();
  switch (t->kind()) {
    case I32: return val(value->Int32Value(context).ToChecked());
    case I64: UNIMPLEMENTED("i64 value");
    case F32: {
      return val(static_cast<float32_t>(value->NumberValue(context).ToChecked()));
    }
    case F64: return val(value->NumberValue(context).ToChecked());
    case ANYREF:
    case FUNCREF: {
      if (value->IsNull()) {
        return val(nullptr);
      } else {
        UNIMPLEMENTED("ref value");
      }
    }
  }
}


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

class ref_data {
  template<class, class> friend struct ref_impl;

  int count_ = 1;
  store_impl* store_;
  v8::Persistent<v8::Object> obj_;
  void* host_info_ = nullptr;
  void (*host_finalizer_)(void*) = nullptr;

  void take() {
    if (count_++ == 0) {
      obj_.ClearWeak();
    }
  }
  bool drop() {
    if (--count_ == 0) {
      obj_.template SetWeak<ref_data>(this, &finalizer, v8::WeakCallbackType::kParameter);
    }
    return count_ == 0;
  }

  static void finalizer(const v8::WeakCallbackInfo<ref_data>& info) {
    auto data = info.GetParameter();
    assert(data->count_ == 0);
    if (data->host_finalizer_) (*data->host_finalizer_)(data->host_info_);
    delete data;
  }

public:
  ref_data(store_impl* store, v8::Local<v8::Object> obj) :
    store_(store), obj_(store->isolate(), obj) {}

  virtual ~ref_data() {}

  auto store() const -> store_impl* {
    return store_;
  }
};

template<class Ref, class Data>
struct ref_impl {
  Data* const data;

  static auto make(std::unique_ptr<Data>& data) -> own<Ref*> {
    return own<Ref*>(data ? seal<Ref>(new(std::nothrow) ref_impl(data.release())) : nullptr);
  }
  
  ~ref_impl() {
    if (data) data->drop();
  }

  auto copy() const -> own<Ref*> {
    if (data) data->take();
    return own<Ref*>(seal<Ref>(new(std::nothrow) ref_impl(data)));
  }

  auto store() const -> store_impl* {
    return data->store_;
  }

  auto v8_object() const -> v8::Local<v8::Object> {
    return data->obj_.Get(data->store_->isolate());
  }

  auto get_host_info() const -> void* {
    return data->host_info_;
  }

  void set_host_info(void* info, void (*finalizer)(void*)) {
    data->host_info_ = info;
    data->host_finalizer_ = finalizer;
  }

private:
  explicit ref_impl(Data* data) : data(data) {}
};

template<> struct implement<ref> { using type = ref_impl<ref, ref_data>; };


ref::~ref() {
  impl(this)->~ref_impl();
}

void ref::operator delete(void *p) {
  ::operator delete(p);
}

auto ref::copy() const -> own<ref*> {
  return impl(this)->copy();
}

auto ref::get_host_info() const -> void* {
  return impl(this)->get_host_info();
}

void ref::set_host_info(void* info, void (*finalizer)(void*)) {
  impl(this)->set_host_info(info, finalizer);
}


///////////////////////////////////////////////////////////////////////////////
// Runtime Objects

// Modules

struct module_data : ref_data {
  vec<importtype*> imports;
  vec<exporttype*> exports;

  module_data(store_impl* store, v8::Local<v8::Object> obj,
    vec<importtype*>& imports, vec<exporttype*>& exports) :
    ref_data(store, obj), imports(std::move(imports)), exports(std::move(exports)) {}
};

using module_impl = ref_impl<module, module_data>;
template<> struct implement<module> { using type = module_impl; };


module::~module() {}

auto module::copy() const -> own<module*> {
  return impl(this)->copy();
}

auto module::validate(own<store*>& store_abs, const vec<byte_t>& binary) -> bool {
  auto store = impl(store_abs.get());
  v8::Isolate* isolate = store->isolate();
  v8::HandleScope handle_scope(isolate);

  auto array_buffer =
    v8::ArrayBuffer::New(isolate, const_cast<byte_t*>(binary.get()), binary.size());

  v8::Local<v8::Value> args[] = {array_buffer};
  auto result = store->v8_function(V8_F_VALIDATE)->Call(
    store->context(), v8::Undefined(isolate), 1, args);
  if (result.IsEmpty()) return false;

  return result.ToLocalChecked()->IsTrue();
}

auto module::make(own<store*>& store_abs, const vec<byte_t>& binary) -> own<module*> {
  auto store = impl(store_abs.get());
  auto isolate = store->isolate();
  auto context = store->context();
  v8::HandleScope handle_scope(isolate);

  auto array_buffer =
    v8::ArrayBuffer::New(isolate, const_cast<byte_t*>(binary.get()), binary.size());

  v8::Local<v8::Value> args[] = {array_buffer};
  auto maybe_obj =
    store->v8_function(V8_F_MODULE)->NewInstance(context, 1, args);
  if (maybe_obj.IsEmpty()) return nullptr;
  auto obj = maybe_obj.ToLocalChecked();

  // TODO(wasm+): use JS API once available?
  auto imports_exports = wasm::bin::imports_exports(binary);
  // TODO store->cache_set(module_obj, module);
  auto& imports = std::get<0>(imports_exports);
  auto& exports = std::get<1>(imports_exports);
  if (!imports || !exports) return own<module*>();
  auto data = make_own(new(std::nothrow) module_data(store, obj, imports, exports));
  return data ? module_impl::make(data) : own<module*>();
}

auto module::imports() const -> vec<importtype*> {
  return impl(this)->data->imports.copy();
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

auto module::exports() const -> vec<exporttype*> {
  return impl(this)->data->exports.copy();
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

auto module::serialize() const -> vec<byte_t> {
  UNIMPLEMENTED("module::serialize");
}

auto module::deserialize(vec<byte_t>& serialized) -> own<module*> {
  UNIMPLEMENTED("module::deserialize");
}


// Host Objects

using hostobj_data = ref_data;
using hostobj_impl = ref_impl<hostobj, hostobj_data>;
template<> struct implement<hostobj> { using type = hostobj_impl; };


hostobj::~hostobj() {}

auto hostobj::copy() const -> own<hostobj*> {
  return impl(this)->copy();
}

auto hostobj::make(own<store*>& store_abs) -> own<hostobj*> {
  auto store = impl(store_abs.get());
  auto isolate = store->isolate();
  v8::HandleScope handle_scope(isolate);

  auto obj = v8::Object::New(isolate);
  auto data = make_own(new(std::nothrow) hostobj_data(store, obj));
  return data ? hostobj_impl::make(data) : own<hostobj*>();
}


// Externals

struct external_data : ref_data {
  externkind kind;

  external_data(store_impl* store, v8::Local<v8::Object> obj, externkind kind) :
    ref_data(store, obj), kind(kind) {}
};

using external_impl = ref_impl<external, external_data>;
template<> struct implement<external> { using type = external_impl; };


external::~external() {}

auto external::copy() const -> own<external*> {
  return impl(this)->copy();
}

auto external::kind() const -> externkind {
  return impl(this)->data->kind;
}

auto external::func() -> wasm::func* {
  return kind() == EXTERN_FUNC ? static_cast<wasm::func*>(this) : nullptr;
}

auto external::global() -> wasm::global* {
  return kind() == EXTERN_GLOBAL ? static_cast<wasm::global*>(this) : nullptr;
}

auto external::table() -> wasm::table* {
  return kind() == EXTERN_TABLE ? static_cast<wasm::table*>(this) : nullptr;
}

auto external::memory() -> wasm::memory* {
  return kind() == EXTERN_MEMORY ? static_cast<wasm::memory*>(this) : nullptr;
}

auto external::func() const -> const wasm::func* {
  return kind() == EXTERN_FUNC ? static_cast<const wasm::func*>(this) : nullptr;
}

auto external::global() const -> const wasm::global* {
  return kind() == EXTERN_GLOBAL ? static_cast<const wasm::global*>(this) : nullptr;
}

auto external::table() const -> const wasm::table* {
  return kind() == EXTERN_TABLE ? static_cast<const wasm::table*>(this) : nullptr;
}

auto external::memory() const -> const wasm::memory* {
  return kind() == EXTERN_MEMORY ? static_cast<const wasm::memory*>(this) : nullptr;
}

auto external_to_v8(const external* ex) -> v8::Local<v8::Object> {
  return impl(ex)->v8_object();
}


// Function Instances

struct func_data : external_data {
  own<functype*> type;
  enum { CALLBACK, CALLBACK_WITH_ENV } kind;
  union {
    func::callback callback;
    func::callback_with_env callback_with_env;
  };
  void* env;
  void (*finalizer)(void*);

  func_data(store_impl* store, v8::Local<v8::Function> obj, own<functype*>& type) :
    external_data(store, obj, EXTERN_FUNC), type(std::move(type)) {}

  ~func_data() {
    if (kind == CALLBACK_WITH_ENV && finalizer) finalizer(env);
  }

  static void v8_callback(const v8::FunctionCallbackInfo<v8::Value>&);
};

using func_impl = ref_impl<func, func_data>;
template<> struct implement<func> { using type = func_impl; };


func::~func() {}

auto func::copy() const -> own<func*> {
  return impl(this)->copy();
}

namespace {
auto make_func(own<store*>& store_abs, const own<functype*>& type) -> own<func*> {
  auto store = impl(store_abs.get());
  auto isolate = store->isolate();
  v8::HandleScope handle_scope(isolate);
  auto context = store->context();

  // TODO(lowlevel): use V8 Foreign value
  auto data_template = store->callback_data_template();
  auto maybe_data = data_template->NewInstance(context);
  if (maybe_data.IsEmpty()) return own<func*>();
  auto v8_data = maybe_data.ToLocalChecked();

  auto function_template = v8::FunctionTemplate::New(isolate, &func_data::v8_callback, v8_data);
  auto maybe_func_obj = function_template->GetFunction(context);
  if (maybe_func_obj.IsEmpty()) return own<func*>();
  auto func_obj = maybe_func_obj.ToLocalChecked();

  auto type_copy = type->copy();
  if (!type_copy) return own<func*>();
  auto data = make_own(new(std::nothrow) func_data(store, func_obj, type_copy));
  if (data) v8_data->SetAlignedPointerInInternalField(0, data.get());
  return func_impl::make(data);
}
}

auto func::make(own<store*>& store_abs, const own<functype*>& type, func::callback callback) -> own<func*> {
  auto func = make_func(store_abs, type);
  auto data = impl(func.get())->data;
  data->kind = func_data::CALLBACK;
  data->callback = callback;
  return func;
}

auto func::make(
  own<store*>& store_abs, const own<functype*>& type,
  callback_with_env callback, void* env, void (*finalizer)(void*)
) -> own<func*> {
  auto func = make_func(store_abs, type);
  auto data = impl(func.get())->data;
  data->kind = func_data::CALLBACK_WITH_ENV;
  data->callback_with_env = callback;
  data->env = env;
  data->finalizer = finalizer;
  return func;
}

auto func::type() const -> own<functype*> {
  return impl(this)->data->type->copy();
}

auto func::call(const vec<val>& args) const -> vec<val> {
  auto func = impl(this);
  auto store = func->store();
  auto isolate = store->isolate();
  v8::HandleScope handle_scope(isolate);

  auto context = store->context();
  auto& type = func->data->type;
  auto& type_params = type->params();
  auto& type_results = type->results();

  assert(type_params.size() == args.size());

  auto v8_args = std::unique_ptr<v8::Local<v8::Value>[]>(new(std::nothrow) v8::Local<v8::Value>[type_params.size()]);
  for (size_t i = 0; i < type_params.size(); ++i) {
    assert(args[i].kind() == type_params[i]->kind());
    v8_args[i] = val_to_v8(store, args[i]);
  }

  auto v8_function = v8::Local<v8::Function>::Cast(func->v8_object());
  auto maybe_result =
    v8_function->Call(context, v8::Undefined(isolate), args.size(), v8_args.get());
  if (maybe_result.IsEmpty()) return vec<val>::invalid();
  auto result = maybe_result.ToLocalChecked();

  if (type_results.size() == 0) {
    assert(result->IsUndefined());
    return vec<val>::make();
  } else if (type_results.size() == 1) {
    assert(!result->IsUndefined());
    return vec<val>::make(v8_to_val(store, result, type_results[0]));
  } else {
    UNIMPLEMENTED("multiple results");
  }
}

void func_data::v8_callback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  auto v8_data = v8::Local<v8::Object>::Cast(info.Data());
  auto self = reinterpret_cast<func_data*>(
    v8_data->GetAlignedPointerFromInternalField(0));
  auto store = self->store();
  auto isolate = store->isolate();
  v8::HandleScope handle_scope(isolate);

  auto context = store->context();
  auto& type = self->type;
  auto& type_params = type->params();
  auto& type_results = type->results();

  assert(type_params.size() == info.Length());

  auto args = vec<val>::make_uninitialized(type_params.size());
  for (size_t i = 0; i < type_params.size(); ++i) {
    args[i] = v8_to_val(store, info[i], type_params[i]);
  }

  auto results = vec<val>::invalid();
  if (self->kind == CALLBACK_WITH_ENV) {
    results = self->callback_with_env(self->env, args);
  } else {
    results = self->callback(args);
  }

  assert(type_results.size() == results.size());

  auto ret = info.GetReturnValue();
  if (type_results.size() == 0) {
    ret.SetUndefined();
  } else if (type_results.size() == 1) {
    assert(results[0].kind() == type_results[0]->kind());
    ret.Set(val_to_v8(store, results[0]));
  } else {
    UNIMPLEMENTED("multiple results");
  }
}


// Global Instances

struct global_data : external_data {
  own<globaltype*> type;

  global_data(store_impl* store, v8::Local<v8::Object> obj, own<globaltype*>& type) :
    external_data(store, obj, EXTERN_GLOBAL), type(std::move(type)) {}
};

using global_impl = ref_impl<global, global_data>;
template<> struct implement<global> { using type = global_impl; };


global::~global() {}

auto global::copy() const -> own<global*> {
  return impl(this)->copy();
}

auto global::make(own<store*>& store_abs, const own<globaltype*>& type, const val& val) -> own<global*> {
  auto store = impl(store_abs.get());
  auto isolate = store->isolate();
  v8::HandleScope handle_scope(isolate);
  auto context = store->context();

  assert(type->content()->kind() == val.kind());

  // TODO(wasm+): remove
  if (store->v8_function(V8_F_GLOBAL).IsEmpty()) {
    UNIMPLEMENTED("global::make");
  }

  v8::Local<v8::Value> args[] = {
    globaltype_to_v8(store, type),
    val_to_v8(store, val)
  };
  auto maybe_obj =
    store->v8_function(V8_F_GLOBAL)->NewInstance(context, 2, args);
  if (maybe_obj.IsEmpty()) return own<global*>();
  auto obj = maybe_obj.ToLocalChecked();

  auto type_copy = type->copy();
  if (!type_copy) return own<global*>();
  auto data = make_own(new(std::nothrow) global_data(store, obj, type_copy));
  return global_impl::make(data);
}

auto global::type() const -> own<globaltype*> {
  return impl(this)->data->type->copy();
}

auto global::get() const -> own<val> {
  auto global = impl(this);
  auto store = global->store();
  auto isolate = store->isolate();
  v8::HandleScope handle_scope(isolate);
  auto context = store->context();

  // TODO(wasm+): remove
  if (store->v8_function(V8_F_GLOBAL_GET).IsEmpty()) {
    UNIMPLEMENTED("global::get");
  }

  auto maybe_val =
    store->v8_function(V8_F_GLOBAL_GET)->Call(context, global->v8_object(), 0, nullptr);
  if (maybe_val.IsEmpty()) return val();
  auto val = maybe_val.ToLocalChecked();

  return v8_to_val(store, val, this->type()->content().get());
}

void global::set(const val& val) {
  auto global = impl(this);
  auto store = global->store();
  auto isolate = store->isolate();
  v8::HandleScope handle_scope(isolate);
  auto context = store->context();

  assert(val.kind() == this->type()->content()->kind());

  // TODO(wasm+): remove
  if (store->v8_function(V8_F_GLOBAL_SET).IsEmpty()) {
    UNIMPLEMENTED("global::set");
  }

  v8::Local<v8::Value> args[] = { val_to_v8(store, val) };
  void(store->v8_function(V8_F_GLOBAL_SET)->Call(
    context, global->v8_object(), 1, args));
}


// Table Instances

struct table_data : external_data {
  own<tabletype*> type;

  table_data(store_impl* store, v8::Local<v8::Object> obj, own<tabletype*>& type) :
    external_data(store, obj, EXTERN_TABLE), type(std::move(type)) {}
};

using table_impl = ref_impl<table, table_data>;
template<> struct implement<table> { using type = table_impl; };


table::~table() {}

auto table::copy() const -> own<table*> {
  return impl(this)->copy();
}

auto table::make(own<store*>& store_abs, const own<tabletype*>& type, const own<ref*>& ref) -> own<table*> {
  auto store = impl(store_abs.get());
  auto isolate = store->isolate();
  v8::HandleScope handle_scope(isolate);
  auto context = store->context();

  // TODO(wasm+): handle reference initialiser
  v8::Local<v8::Value> args[] = {
    tabletype_to_v8(store, type),
    impl(ref.get())->v8_object()
  };
  auto maybe_obj =
    store->v8_function(V8_F_TABLE)->NewInstance(context, 2, args);
  if (maybe_obj.IsEmpty()) return own<table*>();
  auto obj = maybe_obj.ToLocalChecked();

  auto type_copy = type->copy();
  if (!type_copy) return own<table*>();
  auto data = make_own(new(std::nothrow) table_data(store, obj, type_copy));
  return table_impl::make(data);
}

auto table::type() const -> own<tabletype*> {
  // TODO: query and update min
  return impl(this)->data->type->copy();
}

auto table::get(size_t index) const -> own<ref*> {
  UNIMPLEMENTED("table::get");
}

void table::set(size_t index, const own<ref*>& r) {
  UNIMPLEMENTED("table::set");
}

auto table::size() const -> size_t {
  UNIMPLEMENTED("table::size");
}

auto table::grow(size_t delta) -> size_t {
  UNIMPLEMENTED("table::grow");
}


// Memory Instances

struct memory_data : external_data {
  own<memtype*> type;

  memory_data(store_impl* store, v8::Local<v8::Object> obj, own<memtype*>& type) :
    external_data(store, obj, EXTERN_MEMORY), type(std::move(type)) {}
};

using memory_impl = ref_impl<memory, memory_data>;
template<> struct implement<memory> { using type = memory_impl; };


memory::~memory() {}

auto memory::copy() const -> own<memory*> {
  return impl(this)->copy();
}

auto memory::make(own<store*>& store_abs, const own<memtype*>& type) -> own<memory*> {
  auto store = impl(store_abs.get());
  auto isolate = store->isolate();
  v8::HandleScope handle_scope(isolate);
  auto context = store->context();

  v8::Local<v8::Value> args[] = { memtype_to_v8(store, type) };
  auto maybe_obj =
    store->v8_function(V8_F_MEMORY)->NewInstance(context, 1, args);
  if (maybe_obj.IsEmpty()) return own<memory*>();
  auto obj = maybe_obj.ToLocalChecked();

  auto type_copy = type->copy();
  if (!type_copy) return own<memory*>();
  auto data = make_own(new(std::nothrow) memory_data(store, obj, type_copy));
  return memory_impl::make(data);
}

auto memory::type() const -> own<memtype*> {
  // TODO: query and update min
  return impl(this)->data->type->copy();
}

auto memory::data() const -> byte_t* {
  UNIMPLEMENTED("memory::data");
}

auto memory::data_size() const -> size_t {
  UNIMPLEMENTED("memory::data_size");
}

auto memory::size() const -> pages_t {
  UNIMPLEMENTED("memory::size");
}

auto memory::grow(pages_t delta) -> pages_t {
  UNIMPLEMENTED("memory::grow");
}


// Module Instances

struct instance_data : ref_data {
  vec<external*> exports;

  instance_data(store_impl* store, v8::Local<v8::Object> obj, vec<external*>& exports) :
    ref_data(store, obj), exports(std::move(exports)) {}
};

using instance_impl = ref_impl<instance, instance_data>;
template<> struct implement<instance> { using type = instance_impl; };


instance::~instance() {}

auto instance::copy() const -> own<instance*> {
  return impl(this)->copy();
}

auto instance::make(own<store*>& store_abs, const own<module*>& module_abs, const vec<external*>& imports) -> own<instance*> {
  auto store = impl(store_abs.get());
  auto module = impl(module_abs.get());
  auto isolate = store->isolate();
  auto context = store->context();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Value> imports_args[] = { module->v8_object() };
  auto imports_result = store->v8_function(V8_F_IMPORTS)->Call(
    context, v8::Undefined(isolate), 1, imports_args);
  if (imports_result.IsEmpty()) return own<instance*>();
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
      void(imports_obj->DefineOwnProperty(context, module_str, module_obj));
    }

    void(module_obj->DefineOwnProperty(context, name_str, external_to_v8(imports[i])));
  }

  v8::Local<v8::Value> instantiate_args[] = {module->v8_object(), imports_obj};
  auto instance_obj =
    store->v8_function(V8_F_INSTANCE)->NewInstance(context, 2, instantiate_args).ToLocalChecked();
  auto exports_obj = v8::Local<v8::Object>::Cast(
    instance_obj->Get(context, store->v8_string(V8_S_EXPORTS)).ToLocalChecked());
  assert(!exports_obj.IsEmpty() && exports_obj->IsObject());

  auto export_types = module_abs->exports();
  auto exports = vec<external*>::make_uninitialized(export_types.size());
  if (!exports) return own<instance*>();
  for (size_t i = 0; i < export_types.size(); ++i) {
    auto& name = export_types[i]->name();
    auto maybe_name_obj = v8::String::NewFromUtf8(isolate, name.get(),
      v8::NewStringType::kNormal, name.size());
    if (maybe_name_obj.IsEmpty()) return own<instance*>();
    auto name_obj = maybe_name_obj.ToLocalChecked();
    auto obj = v8::Local<v8::Function>::Cast(
      exports_obj->Get(context, name_obj).ToLocalChecked());

    auto& type = export_types[i]->type();
    switch (type->kind()) {
      case EXTERN_FUNC: {
        auto func_obj = v8::Local<v8::Function>::Cast(obj);
        auto functype = type->func()->copy();
        if (!functype) return own<instance*>();
        auto data = make_own(new(std::nothrow) func_data(store, func_obj, functype));
        exports[i].reset(func_impl::make(data));
      } break;
      case EXTERN_GLOBAL: {
        auto globaltype = type->global()->copy();
        if (!globaltype) return own<instance*>();
        auto data = make_own(new(std::nothrow) global_data(store, obj, globaltype));
        exports[i].reset(global_impl::make(data));
      } break;
      case EXTERN_TABLE: {
        auto tabletype = type->table()->copy();
        if (!tabletype) return own<instance*>();
        auto data = make_own(new(std::nothrow) table_data(store, obj, tabletype));
        exports[i].reset(table_impl::make(data));
      } break;
      case EXTERN_MEMORY: {
        auto memtype = type->memory()->copy();
        if (!memtype) return own<instance*>();
        auto data = make_own(new(std::nothrow) memory_data(store, obj, memtype));
        exports[i].reset(memory_impl::make(data));
      } break;
    }
  }

  auto data = make_own(new(std::nothrow) instance_data(store, instance_obj, exports));
  return instance_impl::make(data);
}

auto instance::exports() const -> vec<external*> {
  return impl(this)->data->exports.copy();
}

///////////////////////////////////////////////////////////////////////////////

}  // namespace wasm
