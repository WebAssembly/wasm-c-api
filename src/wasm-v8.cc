#include "wasm.hh"
#include "wasm-bin.hh"
#include "wasm-v8-lowlevel.hh"

#include "v8.h"
#include "libplatform/libplatform.h"

#include <iostream>


namespace wasm_v8 {
  using namespace v8::wasm;
}

namespace v8 {
  namespace internal {
    extern bool FLAG_experimental_wasm_mut_global;
  }
}

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

// Configuration

struct ConfigImpl {};

template<> struct implement<Config> { using type = ConfigImpl; };


Config::~Config() {
  impl(this)->~ConfigImpl();
}

void Config::operator delete(void *p) {
  ::operator delete(p);
}

auto Config::make() -> own<Config*> {
  return own<Config*>(seal<Config>(new(std::nothrow) ConfigImpl()));
}


// Engine

struct EngineImpl {
  static bool created;

  std::unique_ptr<v8::Platform> platform;

  EngineImpl() {
    assert(!created);
    created = true;
  }

  ~EngineImpl() {
    v8::V8::Dispose();
    v8::V8::ShutdownPlatform();
  }
};

bool EngineImpl::created = false;

template<> struct implement<Engine> { using type = EngineImpl; };


Engine::~Engine() {
  impl(this)->~EngineImpl();
}

void Engine::operator delete(void *p) {
  ::operator delete(p);
}

auto Engine::make(
  int argc, const char *const argv[], own<Config*>&& config
) -> own<Engine*> {
  v8::internal::FLAG_experimental_wasm_mut_global = true;
  v8::V8::SetFlagsFromCommandLine(&argc, const_cast<char**>(argv), false);
  auto engine = new(std::nothrow) EngineImpl;
  if (!engine) return own<Engine*>();
  // v8::V8::InitializeICUDefaultLocation(argv[0]);
  v8::V8::InitializeExternalStartupData(argv[0]);
  engine->platform = v8::platform::NewDefaultPlatform();
  v8::V8::InitializePlatform(engine->platform.get());
  v8::V8::Initialize();
  return make_own(seal<Engine>(engine));
}


// Stores

enum v8_string_t {
  V8_S_EMPTY,
  V8_S_EXPORTS,
  V8_S_I32, V8_S_I64, V8_S_F32, V8_S_F64, V8_S_ANYREF, V8_S_ANYFUNC,
  V8_S_VALUE, V8_S_MUTABLE, V8_S_ELEMENT, V8_S_MINIMUM, V8_S_MAXIMUM,
  V8_S_COUNT
};

enum v8_function_t {
  V8_F_WEAKMAP, V8_F_WEAK_GET, V8_F_WEAK_SET,
  V8_F_MODULE, V8_F_GLOBAL, V8_F_TABLE, V8_F_MEMORY,
  V8_F_INSTANCE, V8_F_VALIDATE,
  V8_F_COUNT,
};

class StoreImpl {
  friend own<Store*> Store::make(Engine*);

  v8::Isolate::CreateParams create_params_;
  v8::Isolate *isolate_;
  v8::Eternal<v8::Context> context_;
  v8::Eternal<v8::ObjectTemplate> callbackData_template_;
  v8::Eternal<v8::String> strings_[V8_S_COUNT];
  v8::Eternal<v8::Function> functions_[V8_F_COUNT];
  v8::Eternal<v8::Object> cache_;

public:
  ~StoreImpl() {
    context()->Exit();
    isolate_->Exit();
    isolate_->Dispose();
    delete create_params_.array_buffer_allocator;
  }

  auto isolate() const -> v8::Isolate* {
    return isolate_;
  }

  auto context() const -> v8::Local<v8::Context> {
    return context_.Get(isolate_);
  }

  auto callbackData_template() const -> v8::Local<v8::ObjectTemplate> {
    return callbackData_template_.Get(isolate_);
  }

  auto v8_string(v8_string_t i) const -> v8::Local<v8::String> {
    return strings_[i].Get(isolate_);
  }

  auto v8_function(v8_function_t i) const -> v8::Local<v8::Function> {
    return functions_[i].Get(isolate_);
  }
};

template<> struct implement<Store> { using type = StoreImpl; };


Store::~Store() {
  impl(this)->~StoreImpl();
}

void Store::operator delete(void *p) {
  ::operator delete(p);
}

auto Store::make(Engine*) -> own<Store*> {
  auto store = make_own(new(std::nothrow) StoreImpl());
  if (!store) return own<Store*>();
  store->create_params_.array_buffer_allocator =
    v8::ArrayBuffer::Allocator::NewDefaultAllocator();

  auto isolate = v8::Isolate::New(store->create_params_);
  if (!isolate) return own<Store*>();
  {
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);

    auto context = v8::Context::New(isolate);
    if (context.IsEmpty()) return own<Store*>();
    v8::Context::Scope context_scope(context);

    auto callbackData_template = v8::ObjectTemplate::New(isolate);
    if (callbackData_template.IsEmpty()) return own<Store*>();
    callbackData_template->SetInternalFieldCount(1);

    store->isolate_ = isolate;
    store->context_ = v8::Eternal<v8::Context>(isolate, context);
    store->callbackData_template_ =
      v8::Eternal<v8::ObjectTemplate>(isolate, callbackData_template);

    static const char* const raw_strings[V8_S_COUNT] = {
      "",
      "exports",
      "i32", "i64", "f32", "f64", "anyref", "anyfunc", 
      "value", "mutable", "element", "initial", "maximum",
    };
    for (int i = 0; i < V8_S_COUNT; ++i) {
      auto maybe = v8::String::NewFromUtf8(isolate, raw_strings[i],
        v8::NewStringType::kNormal);
      if (maybe.IsEmpty()) return own<Store*>();
      auto string = maybe.ToLocalChecked();
      store->strings_[i] = v8::Eternal<v8::String>(isolate, string);
    }

    auto global = context->Global();
    auto maybe_wasm_name = v8::String::NewFromUtf8(isolate, "WebAssembly",
        v8::NewStringType::kNormal);
    if (maybe_wasm_name.IsEmpty()) return own<Store*>();
    auto wasm_name = maybe_wasm_name.ToLocalChecked();
    auto maybe_wasm = global->Get(context, wasm_name);
    if (maybe_wasm.IsEmpty()) return own<Store*>();
    auto wasm = v8::Local<v8::Object>::Cast(maybe_wasm.ToLocalChecked());
    v8::Local<v8::Object> weakmap;

    struct {
      const char* name;
      v8::Local<v8::Object>* carrier;
    } raw_functions[V8_F_COUNT] = {
      {"WeakMap", &global}, {"get", &weakmap}, {"set", &weakmap},
      {"Module", &wasm}, {"Global", &wasm}, {"Table", &wasm}, {"Memory", &wasm},
      {"Instance", &wasm}, {"validate", &wasm},
    };
    for (int i = 0; i < V8_F_COUNT; ++i) {
      auto maybe_name = v8::String::NewFromUtf8(isolate, raw_functions[i].name,
        v8::NewStringType::kNormal);
      if (maybe_name.IsEmpty()) return own<Store*>();
      auto name = maybe_name.ToLocalChecked();
      assert(!raw_functions[i].carrier->IsEmpty());
      // TODO(wasm+): remove
      if ((*raw_functions[i].carrier)->IsUndefined()) continue;
      auto maybe_obj = (*raw_functions[i].carrier)->Get(context, name);
      if (maybe_obj.IsEmpty()) return own<Store*>();
      auto function = v8::Local<v8::Function>::Cast(maybe_obj.ToLocalChecked());
      store->functions_[i] = v8::Eternal<v8::Function>(isolate, function);
      if (i == V8_F_WEAKMAP) weakmap = function;
    }

    v8::Local<v8::Value> empty_args[] = {};
    auto maybe_cache =
      store->v8_function(V8_F_WEAKMAP)->NewInstance(context, 0, empty_args);
    if (maybe_cache.IsEmpty()) return own<Store*>();
    auto cache = v8::Local<v8::Object>::Cast(maybe_cache.ToLocalChecked());
    store->cache_ = v8::Eternal<v8::Object>(isolate, cache);
  }

  store->isolate()->Enter();
  store->context()->Enter();

  return make_own(seal<Store>(store.release()));
};


///////////////////////////////////////////////////////////////////////////////
// Type Representations

// Value Types

struct ValTypeImpl {
  ValKind kind;

  ValTypeImpl(ValKind kind) : kind(kind) {}
};

template<> struct implement<ValType> { using type = ValTypeImpl; };

ValTypeImpl* valtypes[] = {
  new ValTypeImpl(I32),
  new ValTypeImpl(I64),
  new ValTypeImpl(F32),
  new ValTypeImpl(F64),
  new ValTypeImpl(ANYREF),
  new ValTypeImpl(FUNCREF),
};


ValType::~ValType() {}

void ValType::operator delete(void*) {}

auto ValType::make(ValKind k) -> own<ValType*> {
  return own<ValType*>(seal<ValType>(valtypes[k]));
}

auto ValType::copy() const -> own<ValType*> {
  return make(kind());
}

auto ValType::kind() const -> ValKind {
  return impl(this)->kind;
}


// Extern Types

struct ExternTypeImpl {
  ExternKind kind;

  explicit ExternTypeImpl(ExternKind kind) : kind(kind) {}
  virtual ~ExternTypeImpl() {}
};

template<> struct implement<ExternType> { using type = ExternTypeImpl; };


ExternType::~ExternType() {
  impl(this)->~ExternTypeImpl();
}

void ExternType::operator delete(void *p) {
  ::operator delete(p);
}

auto ExternType::copy() const -> own<ExternType*> {
  switch (kind()) {
    case EXTERN_FUNC: return func()->copy();
    case EXTERN_GLOBAL: return global()->copy();
    case EXTERN_TABLE: return table()->copy();
    case EXTERN_MEMORY: return memory()->copy();
  }
}

auto ExternType::kind() const -> ExternKind {
  return impl(this)->kind;
}


// Function Types

struct FuncTypeImpl : ExternTypeImpl {
  vec<ValType*> params;
  vec<ValType*> results;

  FuncTypeImpl(vec<ValType*>& params, vec<ValType*>& results) :
    ExternTypeImpl(EXTERN_FUNC),
    params(std::move(params)), results(std::move(results)) {}
};

template<> struct implement<FuncType> { using type = FuncTypeImpl; };


FuncType::~FuncType() {}

auto FuncType::make(vec<ValType*>&& params, vec<ValType*>&& results)
  -> own<FuncType*> {
  return params && results
    ? own<FuncType*>(
        seal<FuncType>(new(std::nothrow) FuncTypeImpl(params, results)))
    : own<FuncType*>();
}

auto FuncType::copy() const -> own<FuncType*> {
  return make(params().copy(), results().copy());
}

auto FuncType::params() const -> const vec<ValType*>& {
  return impl(this)->params;
}

auto FuncType::results() const -> const vec<ValType*>& {
  return impl(this)->results;
}


auto ExternType::func() -> FuncType* {
  return kind() == EXTERN_FUNC
    ? seal<FuncType>(static_cast<FuncTypeImpl*>(impl(this)))
    : nullptr;
}

auto ExternType::func() const -> const FuncType* {
  return kind() == EXTERN_FUNC
    ? seal<FuncType>(static_cast<const FuncTypeImpl*>(impl(this)))
    : nullptr;
}


// Global Types

struct GlobalTypeImpl : ExternTypeImpl {
  own<ValType*> content;
  Mutability mutability;

  GlobalTypeImpl(own<ValType*>& content, Mutability mutability) :
    ExternTypeImpl(EXTERN_GLOBAL),
    content(std::move(content)), mutability(mutability) {}
};

template<> struct implement<GlobalType> { using type = GlobalTypeImpl; };


GlobalType::~GlobalType() {}

auto GlobalType::make(
  own<ValType*>&& content, Mutability mutability
) -> own<GlobalType*> {
  return content
    ? own<GlobalType*>(
        seal<GlobalType>(new(std::nothrow) GlobalTypeImpl(content, mutability)))
    : own<GlobalType*>();
}

auto GlobalType::copy() const -> own<GlobalType*> {
  return make(content()->copy(), mutability());
}

auto GlobalType::content() const -> const ValType* {
  return impl(this)->content.get();
}

auto GlobalType::mutability() const -> Mutability {
  return impl(this)->mutability;
}


auto ExternType::global() -> GlobalType* {
  return kind() == EXTERN_GLOBAL
    ? seal<GlobalType>(static_cast<GlobalTypeImpl*>(impl(this)))
    : nullptr;
}

auto ExternType::global() const -> const GlobalType* {
  return kind() == EXTERN_GLOBAL
    ? seal<GlobalType>(static_cast<const GlobalTypeImpl*>(impl(this)))
    : nullptr;
}


// Table Types

struct TableTypeImpl : ExternTypeImpl {
  own<ValType*> element;
  Limits limits;

  TableTypeImpl(own<ValType*>& element, Limits limits) :
    ExternTypeImpl(EXTERN_TABLE), element(std::move(element)), limits(limits) {}
};

template<> struct implement<TableType> { using type = TableTypeImpl; };


TableType::~TableType() {}

auto TableType::make(own<ValType*>&& element, Limits limits) -> own<TableType*> {
  return element
    ? own<TableType*>(
        seal<TableType>(new(std::nothrow) TableTypeImpl(element, limits)))
    : own<TableType*>();
}

auto TableType::copy() const -> own<TableType*> {
  return make(element()->copy(), limits());
}

auto TableType::element() const -> const ValType* {
  return impl(this)->element.get();
}

auto TableType::limits() const -> const Limits& {
  return impl(this)->limits;
}


auto ExternType::table() -> TableType* {
  return kind() == EXTERN_TABLE
    ? seal<TableType>(static_cast<TableTypeImpl*>(impl(this)))
    : nullptr;
}

auto ExternType::table() const -> const TableType* {
  return kind() == EXTERN_TABLE
    ? seal<TableType>(static_cast<const TableTypeImpl*>(impl(this)))
    : nullptr;
}


// Memory Types

struct MemoryTypeImpl : ExternTypeImpl {
  Limits limits;

  MemoryTypeImpl(Limits limits) :
    ExternTypeImpl(EXTERN_MEMORY), limits(limits) {}
};

template<> struct implement<MemoryType> { using type = MemoryTypeImpl; };


MemoryType::~MemoryType() {}

auto MemoryType::make(Limits limits) -> own<MemoryType*> {
  return own<MemoryType*>(
    seal<MemoryType>(new(std::nothrow) MemoryTypeImpl(limits)));
}

auto MemoryType::copy() const -> own<MemoryType*> {
  return MemoryType::make(limits());
}

auto MemoryType::limits() const -> const Limits& {
  return impl(this)->limits;
}


auto ExternType::memory() -> MemoryType* {
  return kind() == EXTERN_MEMORY
    ? seal<MemoryType>(static_cast<MemoryTypeImpl*>(impl(this)))
    : nullptr;
}

auto ExternType::memory() const -> const MemoryType* {
  return kind() == EXTERN_MEMORY
    ? seal<MemoryType>(static_cast<const MemoryTypeImpl*>(impl(this)))
    : nullptr;
}


// Import Types

struct ImportTypeImpl {
  Name module;
  Name name;
  own<ExternType*> type;

  ImportTypeImpl(Name& module, Name& name, own<ExternType*>& type) :
    module(std::move(module)), name(std::move(name)), type(std::move(type)) {}
};

template<> struct implement<ImportType> { using type = ImportTypeImpl; };


ImportType::~ImportType() {
  impl(this)->~ImportTypeImpl();
}

void ImportType::operator delete(void *p) {
  ::operator delete(p);
}

auto ImportType::make(
  Name&& module, Name&& name, own<ExternType*>&& type
) -> own<ImportType*> {
  return module && name && type
    ? own<ImportType*>(
        seal<ImportType>(new(std::nothrow) ImportTypeImpl(module, name, type)))
    : own<ImportType*>();
}

auto ImportType::copy() const -> own<ImportType*> {
  return make(module().copy(), name().copy(), type()->copy());
}

auto ImportType::module() const -> const Name& {
  return impl(this)->module;
}

auto ImportType::name() const -> const Name& {
  return impl(this)->name;
}

auto ImportType::type() const -> const ExternType* {
  return impl(this)->type.get();
}


// Export Types

struct ExportTypeImpl {
  Name name;
  own<ExternType*> type;

  ExportTypeImpl(Name& name, own<ExternType*>& type) :
    name(std::move(name)), type(std::move(type)) {}
};

template<> struct implement<ExportType> { using type = ExportTypeImpl; };


ExportType::~ExportType() {
  impl(this)->~ExportTypeImpl();
}

void ExportType::operator delete(void *p) {
  ::operator delete(p);
}

auto ExportType::make(
  Name&& name, own<ExternType*>&& type
) -> own<ExportType*> {
  return name && type
    ? own<ExportType*>(
        seal<ExportType>(new(std::nothrow) ExportTypeImpl(name, type)))
    : own<ExportType*>();
}

auto ExportType::copy() const -> own<ExportType*> {
  return make(name().copy(), type()->copy());
}

auto ExportType::name() const -> const Name& {
  return impl(this)->name;
}

auto ExportType::type() const -> const ExternType* {
  return impl(this)->type.get();
}


///////////////////////////////////////////////////////////////////////////////
// Conversions of types from and to V8 objects

// Types

auto valtype_to_v8(
  StoreImpl* store, const ValType* type
) -> v8::Local<v8::Value> {
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

auto mutability_to_v8(
  StoreImpl* store, Mutability mutability
) -> v8::Local<v8::Boolean> {
  return v8::Boolean::New(store->isolate(), mutability == VAR);
}

void limits_to_v8(StoreImpl* store, Limits limits, v8::Local<v8::Object> desc) {
  auto isolate = store->isolate();
  auto context = store->context();
  void(desc->DefineOwnProperty(context, store->v8_string(V8_S_MINIMUM),
    v8::Integer::NewFromUnsigned(isolate, limits.min)));
  if (limits.max != Limits(0).max) {
    void(desc->DefineOwnProperty(context, store->v8_string(V8_S_MAXIMUM),
      v8::Integer::NewFromUnsigned(isolate, limits.max)));
  }
}

auto globaltype_to_v8(
  StoreImpl* store, const GlobalType* type
) -> v8::Local<v8::Object> {
  auto isolate = store->isolate();
  auto context = store->context();
  auto desc = v8::Object::New(isolate);
  void(desc->DefineOwnProperty(context, store->v8_string(V8_S_VALUE),
    valtype_to_v8(store, type->content())));
  void(desc->DefineOwnProperty(context, store->v8_string(V8_S_MUTABLE),
    mutability_to_v8(store, type->mutability())));
  return desc;
}

auto tabletype_to_v8(
  StoreImpl* store, const TableType* type
) -> v8::Local<v8::Object> {
  auto isolate = store->isolate();
  auto context = store->context();
  auto desc = v8::Object::New(isolate);
  void(desc->DefineOwnProperty(context, store->v8_string(V8_S_ELEMENT),
    valtype_to_v8(store, type->element())));
  limits_to_v8(store, type->limits(), desc);
  return desc;
}

auto memorytype_to_v8(
  StoreImpl* store, const MemoryType* type
) -> v8::Local<v8::Object> {
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

own wasm_ExternType_t* wasm_externtype_new_from_v8_kind(wasm_store_t* store, v8::Local<v8::String> kind) {
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

auto val_to_v8(StoreImpl* store, const Val& v) -> v8::Local<v8::Value> {
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

auto v8_to_val(
  StoreImpl* store, v8::Local<v8::Value> value, const ValType* t
) -> own<Val> {
  auto context = store->context();
  switch (t->kind()) {
    case I32: return Val(value->Int32Value(context).ToChecked());
    case I64: UNIMPLEMENTED("i64 value");
    case F32: {
      auto number = value->NumberValue(context).ToChecked();
      return Val(static_cast<float32_t>(number));
    }
    case F64: return Val(value->NumberValue(context).ToChecked());
    case ANYREF:
    case FUNCREF: {
      if (value->IsNull()) {
        return Val(nullptr);
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

class RefData {
  template<class, class> friend struct RefImpl;

  int count_ = 1;
  StoreImpl* store_;
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
      obj_.template SetWeak<RefData>(
        this, &finalizer, v8::WeakCallbackType::kParameter);
    }
    return count_ == 0;
  }

  static void finalizer(const v8::WeakCallbackInfo<RefData>& info) {
    auto data = info.GetParameter();
    assert(data->count_ == 0);
    if (data->host_finalizer_) (*data->host_finalizer_)(data->host_info_);
    delete data;
  }

public:
  RefData(StoreImpl* store, v8::Local<v8::Object> obj) :
    store_(store), obj_(store->isolate(), obj) {}

  virtual ~RefData() {}

  auto store() const -> StoreImpl* {
    return store_;
  }

  auto v8_object() const -> v8::Local<v8::Object> {
    return obj_.Get(store_->isolate());
  }
};

template<class Ref, class Data>
struct RefImpl {
  Data* const data;

  static auto make(std::unique_ptr<Data>& data) -> own<Ref*> {
    return own<Ref*>(
      data ? seal<Ref>(new(std::nothrow) RefImpl(data.release())) : nullptr);
  }
  
  ~RefImpl() {
    if (data) data->drop();
  }

  auto copy() const -> own<Ref*> {
    if (data) data->take();
    return own<Ref*>(seal<Ref>(new(std::nothrow) RefImpl(data)));
  }

  auto store() const -> StoreImpl* {
    return data->store_;
  }

  auto v8_object() const -> v8::Local<v8::Object> {
    return data->v8_object();
  }

  auto get_host_info() const -> void* {
    return data->host_info_;
  }

  void set_host_info(void* info, void (*finalizer)(void*)) {
    data->host_info_ = info;
    data->host_finalizer_ = finalizer;
  }

private:
  explicit RefImpl(Data* data) : data(data) {}
};

template<> struct implement<Ref> { using type = RefImpl<Ref, RefData>; };


Ref::~Ref() {
  impl(this)->~RefImpl();
}

void Ref::operator delete(void *p) {
  ::operator delete(p);
}

auto Ref::copy() const -> own<Ref*> {
  return impl(this)->copy();
}

auto Ref::get_host_info() const -> void* {
  return impl(this)->get_host_info();
}

void Ref::set_host_info(void* info, void (*finalizer)(void*)) {
  impl(this)->set_host_info(info, finalizer);
}


///////////////////////////////////////////////////////////////////////////////
// Runtime Objects

// Modules

using ModuleData = RefData;
using ModuleImpl = RefImpl<Module, RefData>;
template<> struct implement<Module> { using type = ModuleImpl; };


Module::~Module() {}

auto Module::copy() const -> own<Module*> {
  return impl(this)->copy();
}

auto Module::validate(Store* store_abs, const vec<byte_t>& binary) -> bool {
  auto store = impl(store_abs);
  v8::Isolate* isolate = store->isolate();
  v8::HandleScope handle_scope(isolate);

  auto array_buffer = v8::ArrayBuffer::New(
    isolate, const_cast<byte_t*>(binary.get()), binary.size());

  v8::Local<v8::Value> args[] = {array_buffer};
  auto result = store->v8_function(V8_F_VALIDATE)->Call(
    store->context(), v8::Undefined(isolate), 1, args);
  if (result.IsEmpty()) return false;

  return result.ToLocalChecked()->IsTrue();
}

auto Module::make(Store* store_abs, const vec<byte_t>& binary) -> own<Module*> {
  auto store = impl(store_abs);
  auto isolate = store->isolate();
  auto context = store->context();
  v8::HandleScope handle_scope(isolate);

  auto array_buffer = v8::ArrayBuffer::New(
    isolate, const_cast<byte_t*>(binary.get()), binary.size());

  v8::Local<v8::Value> args[] = {array_buffer};
  auto maybe_obj =
    store->v8_function(V8_F_MODULE)->NewInstance(context, 1, args);
  if (maybe_obj.IsEmpty()) return nullptr;
  auto obj = maybe_obj.ToLocalChecked();

  // TODO store->cache_set(obj, module);
  auto data = make_own(new(std::nothrow) ModuleData(store, obj));
  return data ? ModuleImpl::make(data) : own<Module*>();
}

auto Module::imports() const -> vec<ImportType*> {
  v8::HandleScope handle_scope(impl(this)->store()->isolate());
  auto module = impl(this)->v8_object();
  auto binary = vec<byte_t>::adopt(
    wasm_v8::module_binary_size(module),
    const_cast<byte_t*>(wasm_v8::module_binary(module))
  );
  auto imports = wasm::bin::imports(binary);
  binary.release();
  return imports;
  // return impl(this)->data->imports.copy();
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

auto Module::exports() const -> vec<ExportType*> {
  v8::HandleScope handle_scope(impl(this)->store()->isolate());
  auto module = impl(this)->v8_object();
  auto binary = vec<byte_t>::adopt(
    wasm_v8::module_binary_size(module),
    const_cast<byte_t*>(wasm_v8::module_binary(module))
  );
  auto exports = wasm::bin::exports(binary);
  binary.release();
  return exports;
  // return impl(this)->data->exports.copy();
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

auto Module::serialize() const -> vec<byte_t> {
  UNIMPLEMENTED("Module::serialize");
}

auto Module::deserialize(vec<byte_t>& serialized) -> own<Module*> {
  UNIMPLEMENTED("Module::deserialize");
}


// Foreign Objects

using ForeignData = RefData;
using ForeignImpl = RefImpl<Foreign, ForeignData>;
template<> struct implement<Foreign> { using type = ForeignImpl; };


Foreign::~Foreign() {}

auto Foreign::copy() const -> own<Foreign*> {
  return impl(this)->copy();
}

auto Foreign::make(Store* store_abs) -> own<Foreign*> {
  auto store = impl(store_abs);
  auto isolate = store->isolate();
  v8::HandleScope handle_scope(isolate);

  auto obj = v8::Object::New(isolate);
  auto data = make_own(new(std::nothrow) ForeignData(store, obj));
  return data ? ForeignImpl::make(data) : own<Foreign*>();
}


// Externals

struct ExternData : RefData {
  ExternKind kind;

  ExternData(StoreImpl* store, v8::Local<v8::Object> obj, ExternKind kind) :
    RefData(store, obj), kind(kind) {}
};

using ExternImpl = RefImpl<Extern, ExternData>;
template<> struct implement<Extern> { using type = ExternImpl; };


Extern::~Extern() {}

auto Extern::copy() const -> own<Extern*> {
  return impl(this)->copy();
}

auto Extern::kind() const -> ExternKind {
  return impl(this)->data->kind;
  // TODO: WTF?
  v8::HandleScope handle_scope(impl(this)->store()->isolate());
  return static_cast<ExternKind>(wasm_v8::extern_kind(impl(this)->v8_object()));
}

auto Extern::type() const -> own<ExternType*> {
  switch (kind()) {
    case EXTERN_FUNC: return func()->type();
    case EXTERN_GLOBAL: return global()->type();
    case EXTERN_TABLE: return table()->type();
    case EXTERN_MEMORY: return memory()->type();
  }
}

auto Extern::func() -> Func* {
  return kind() == EXTERN_FUNC ? static_cast<Func*>(this) : nullptr;
}

auto Extern::global() -> Global* {
  return kind() == EXTERN_GLOBAL ? static_cast<Global*>(this) : nullptr;
}

auto Extern::table() -> Table* {
  return kind() == EXTERN_TABLE ? static_cast<Table*>(this) : nullptr;
}

auto Extern::memory() -> Memory* {
  return kind() == EXTERN_MEMORY ? static_cast<Memory*>(this) : nullptr;
}

auto Extern::func() const -> const Func* {
  return kind() == EXTERN_FUNC ? static_cast<const Func*>(this) : nullptr;
}

auto Extern::global() const -> const Global* {
  return kind() == EXTERN_GLOBAL ? static_cast<const Global*>(this) : nullptr;
}

auto Extern::table() const -> const Table* {
  return kind() == EXTERN_TABLE ? static_cast<const Table*>(this) : nullptr;
}

auto Extern::memory() const -> const Memory* {
  return kind() == EXTERN_MEMORY ? static_cast<const Memory*>(this) : nullptr;
}

auto extern_to_v8(const Extern* ex) -> v8::Local<v8::Object> {
  return impl(ex)->v8_object();
}


// Function Instances

struct FuncData : ExternData {
  enum { CALLBACK, CALLBACK_WITH_ENV } kind;
  union {
    Func::callback callback;
    Func::callback_with_env callback_with_env;
  };
  void* env;
  void (*finalizer)(void*);

  FuncData(StoreImpl* store, v8::Local<v8::Object> obj) :
    ExternData(store, obj, EXTERN_FUNC) {}

  ~FuncData() {
    if (kind == CALLBACK_WITH_ENV && finalizer) finalizer(env);
  }

  static void v8_callback(const v8::FunctionCallbackInfo<v8::Value>&);
};

using FuncImpl = RefImpl<Func, FuncData>;
template<> struct implement<Func> { using type = FuncImpl; };


Func::~Func() {}

auto Func::copy() const -> own<Func*> {
  return impl(this)->copy();
}

namespace {

auto make_func(Store* store_abs, const FuncType* type) -> own<Func*> {
  auto store = impl(store_abs);
  auto isolate = store->isolate();
  v8::HandleScope handle_scope(isolate);
  auto context = store->context();

  // Create V8 function
  // TODO(lowlevel): use V8 Foreign value
  auto data_template = store->callbackData_template();
  auto maybeData = data_template->NewInstance(context);
  if (maybeData.IsEmpty()) return own<Func*>();
  auto v8Data = maybeData.ToLocalChecked();

  auto function_template = v8::FunctionTemplate::New(
    isolate, &FuncData::v8_callback, v8Data);
  auto maybe_func_obj = function_template->GetFunction(context);
  if (maybe_func_obj.IsEmpty()) return own<Func*>();
  auto func_obj = maybe_func_obj.ToLocalChecked();

  // Create wrapper instance
  auto binary = wasm::bin::wrapper(type);
  auto module = Module::make(store_abs, binary);

  auto imports_obj = v8::Object::New(isolate);
  auto module_obj = v8::Object::New(isolate);
  auto str = store->v8_string(V8_S_EMPTY);
  void(imports_obj->DefineOwnProperty(context, str, module_obj));
  void(module_obj->DefineOwnProperty(context, str, func_obj));

  v8::Local<v8::Value> instantiate_args[] = {
    impl(module.get())->v8_object(), imports_obj
  };
  auto instance_obj = store->v8_function(V8_F_INSTANCE)->NewInstance(
    context, 2, instantiate_args).ToLocalChecked();
  // TODO: crashes, why?
  // auto exports_obj = wasm_v8::instance_exports(instance_obj);
  auto exports_obj = v8::Local<v8::Object>::Cast(
    instance_obj->Get(context, store->v8_string(V8_S_EXPORTS)).ToLocalChecked()
  );
  auto wrapped_func_obj = v8::Local<v8::Function>::Cast(
    exports_obj->Get(context, str).ToLocalChecked());

  auto data = make_own(new(std::nothrow) FuncData(store, wrapped_func_obj));
  if (data) v8Data->SetAlignedPointerInInternalField(0, data.get());

  return FuncImpl::make(data);
}

auto func_type(v8::Handle<v8::Object> v8_func) -> own<FuncType*> {
  // return impl(this)->data->type->copy();
  auto param_arity = wasm_v8::func_type_param_arity(v8_func);
  auto result_arity = wasm_v8::func_type_result_arity(v8_func);
  auto params = vec<ValType*>::make_uninitialized(param_arity);
  auto results = vec<ValType*>::make_uninitialized(result_arity);

  for (size_t i = 0; i < params.size(); ++i) {
    auto kind = static_cast<ValKind>(wasm_v8::func_type_param(v8_func, i));
    params[i] = ValType::make(kind);
  }
  for (size_t i = 0; i < results.size(); ++i) {
    auto kind = static_cast<ValKind>(wasm_v8::func_type_result(v8_func, i));
    results[i] = ValType::make(kind);
  }

  return FuncType::make(std::move(params), std::move(results));
}

}  // namespace

auto Func::make(
  Store* store_abs, const FuncType* type, Func::callback callback
) -> own<Func*> {
  auto func = make_func(store_abs, type);
  auto data = impl(func.get())->data;
  data->kind = FuncData::CALLBACK;
  data->callback = callback;
  return func;
}

auto Func::make(
  Store* store_abs, const FuncType* type,
  callback_with_env callback, void* env, void (*finalizer)(void*)
) -> own<Func*> {
  auto func = make_func(store_abs, type);
  auto data = impl(func.get())->data;
  data->kind = FuncData::CALLBACK_WITH_ENV;
  data->callback_with_env = callback;
  data->env = env;
  data->finalizer = finalizer;
  return func;
}

auto Func::type() const -> own<FuncType*> {
  // return impl(this)->data->type->copy();
  v8::HandleScope handle_scope(impl(this)->store()->isolate());
  return func_type(impl(this)->v8_object());
}

auto Func::call(const vec<Val>& args) const -> Result {
  auto func = impl(this);
  auto store = func->store();
  auto isolate = store->isolate();
  v8::HandleScope handle_scope(isolate);

  auto context = store->context();
  auto type = this->type();
  auto& type_params = type->params();
  auto& type_results = type->results();

  assert(type_params.size() == args.size());

  auto v8_args = std::unique_ptr<v8::Local<v8::Value>[]>(
    new(std::nothrow) v8::Local<v8::Value>[type_params.size()]);
  for (size_t i = 0; i < type_params.size(); ++i) {
    assert(args[i].kind() == type_params[i]->kind());
    v8_args[i] = val_to_v8(store, args[i]);
  }

  v8::TryCatch handler(isolate);
  auto v8_function = v8::Local<v8::Function>::Cast(func->v8_object());
  auto maybe_result = v8_function->Call(
    context, v8::Undefined(isolate), args.size(), v8_args.get());

  if (handler.HasCaught()) {
    v8::String::Utf8Value message(isolate, handler.Exception());
    return Result(vec<byte_t>::make(std::string(*message)));
  }

  auto result = maybe_result.ToLocalChecked();
  if (type_results.size() == 0) {
    assert(result->IsUndefined());
    return Result();
  } else if (type_results.size() == 1) {
    assert(!result->IsUndefined());
    return Result(v8_to_val(store, result, type_results[0]));
  } else {
    UNIMPLEMENTED("multiple results");
  }
}

void FuncData::v8_callback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  auto v8Data = v8::Local<v8::Object>::Cast(info.Data());
  auto self = reinterpret_cast<FuncData*>(
    v8Data->GetAlignedPointerFromInternalField(0));
  auto store = self->store();
  auto isolate = store->isolate();
  v8::HandleScope handle_scope(isolate);

  auto context = store->context();
  auto type = func_type(self->v8_object());
  auto& type_params = type->params();
  auto& type_results = type->results();

  assert(type_params.size() == info.Length());

  auto args = vec<Val>::make_uninitialized(type_params.size());
  for (size_t i = 0; i < type_params.size(); ++i) {
    args[i] = v8_to_val(store, info[i], type_params[i]);
  }

  auto result = self->kind == CALLBACK_WITH_ENV
    ? self->callback_with_env(self->env, args)
    : self->callback(args);

  if (result.kind() == Result::TRAP) {
    v8::Local<v8::Value> exn = v8::Undefined(isolate);
    if (result.trap()) {
      auto maybe = v8::String::NewFromUtf8(
        isolate, result.trap().get(), v8::NewStringType::kNormal);
      if (!maybe.IsEmpty()) exn = maybe.ToLocalChecked();
    }
    isolate->ThrowException(exn);
    return;
  }

  assert(type_results.size() == result.vals().size());

  auto ret = info.GetReturnValue();
  if (type_results.size() == 0) {
    ret.SetUndefined();
  } else if (type_results.size() == 1) {
    assert(result[0].kind() == type_results[0]->kind());
    ret.Set(val_to_v8(store, result[0]));
  } else {
    UNIMPLEMENTED("multiple results");
  }
}


// Global Instances

struct GlobalData : ExternData {
  GlobalData(StoreImpl* store, v8::Local<v8::Object> obj) :
    ExternData(store, obj, EXTERN_GLOBAL) {}
};

using GlobalImpl = RefImpl<Global, GlobalData>;
template<> struct implement<Global> { using type = GlobalImpl; };


Global::~Global() {}

auto Global::copy() const -> own<Global*> {
  return impl(this)->copy();
}

auto Global::make(
  Store* store_abs, const GlobalType* type, const Val& val
) -> own<Global*> {
  auto store = impl(store_abs);
  auto isolate = store->isolate();
  v8::HandleScope handle_scope(isolate);
  auto context = store->context();

  assert(type->content()->kind() == val.kind());

  // Create wrapper instance
  auto binary = wasm::bin::wrapper(type);
  auto module = Module::make(store_abs, binary);

  v8::Local<v8::Value> instantiate_args[] = { impl(module.get())->v8_object() };
  auto instance_obj = store->v8_function(V8_F_INSTANCE)->NewInstance(
    context, 1, instantiate_args).ToLocalChecked();
  // TODO: crashes, why?
  // auto exports_obj = wasm_v8::instance_exports(instance_obj);
  auto exports_obj = v8::Local<v8::Object>::Cast(
    instance_obj->Get(context, store->v8_string(V8_S_EXPORTS)).ToLocalChecked()
  );
  auto global_obj = v8::Local<v8::Object>::Cast(
    exports_obj->Get(context, store->v8_string(V8_S_EMPTY)).ToLocalChecked());
  assert(!global_obj.IsEmpty() && global_obj->IsObject());

  auto data = make_own(new(std::nothrow) GlobalData(store, global_obj));
  auto global = GlobalImpl::make(data);
  assert(global);
  global->set(val);
  return global;
}

auto Global::type() const -> own<GlobalType*> {
  // return impl(this)->data->type->copy();
  v8::HandleScope handle_scope(impl(this)->store()->isolate());
  auto v8_global = impl(this)->v8_object();
  auto kind = static_cast<ValKind>(wasm_v8::global_type_content(v8_global));
  auto mutability = wasm_v8::global_type_mutable(v8_global) ? VAR : CONST;
  return GlobalType::make(ValType::make(kind), mutability);
}

auto Global::get() const -> Val {
  v8::HandleScope handle_scope(impl(this)->store()->isolate());
  auto v8_global = impl(this)->v8_object();
  switch (type()->content()->kind()) {
    case I32: return Val(wasm_v8::global_get_i32(v8_global));
    case I64: return Val(wasm_v8::global_get_i64(v8_global));
    case F32: return Val(wasm_v8::global_get_f32(v8_global));
    case F64: return Val(wasm_v8::global_get_f64(v8_global));
    case ANYREF:
    case FUNCREF:
      UNIMPLEMENTED("globals of reference type");
    default:
      // TODO(wasm+): support new value types
      assert(false);
  }
/*
  auto global = impl(this);
  auto store = global->store();
  auto isolate = store->isolate();
  v8::HandleScope handle_scope(isolate);
  auto context = store->context();

  // TODO(wasm+): remove
  if (store->v8_function(V8_F_GLOBAL_GET).IsEmpty()) {
    UNIMPLEMENTED("Global::get");
  }

  auto maybe_value = store->v8_function(V8_F_GLOBAL_GET)->Call(
    context, global->v8_object(), 0, nullptr);
  if (maybe_value.IsEmpty()) return Val();
  auto value = maybe_value.ToLocalChecked();

  return v8_to_val(store, value, this->type()->content().get());
*/
}

void Global::set(const Val& val) {
  v8::HandleScope handle_scope(impl(this)->store()->isolate());
  auto v8_global = impl(this)->v8_object();
  switch (val.kind()) {
    case I32: return wasm_v8::global_set_i32(v8_global, val.i32());
    case I64: return wasm_v8::global_set_i64(v8_global, val.i64());
    case F32: return wasm_v8::global_set_f32(v8_global, val.f32());
    case F64: return wasm_v8::global_set_f64(v8_global, val.f64());
    case ANYREF:
    case FUNCREF:
      UNIMPLEMENTED("globals of reference type");
    default:
      // TODO(wasm+): support new value types
      assert(false);
  }
/*
  auto global = impl(this);
  auto store = global->store();
  auto isolate = store->isolate();
  v8::HandleScope handle_scope(isolate);
  auto context = store->context();

  assert(val.kind() == this->type()->content()->kind());

  // TODO(wasm+): remove
  if (store->v8_function(V8_F_GLOBAL_SET).IsEmpty()) {
    UNIMPLEMENTED("Global::set");
  }

  v8::Local<v8::Value> args[] = { val_to_v8(store, val) };
  void(store->v8_function(V8_F_GLOBAL_SET)->Call(
    context, global->v8_object(), 1, args));
*/
}


// Table Instances

struct TableData : ExternData {
  TableData(StoreImpl* store, v8::Local<v8::Object> obj) :
    ExternData(store, obj, EXTERN_TABLE) {}
};

using TableImpl = RefImpl<Table, TableData>;
template<> struct implement<Table> { using type = TableImpl; };


Table::~Table() {}

auto Table::copy() const -> own<Table*> {
  return impl(this)->copy();
}

auto Table::make(
  Store* store_abs, const TableType* type, const Ref* ref
) -> own<Table*> {
  auto store = impl(store_abs);
  auto isolate = store->isolate();
  v8::HandleScope handle_scope(isolate);
  auto context = store->context();

  // TODO(wasm+): handle reference initialiser
  v8::Local<v8::Value> args[] = {
    tabletype_to_v8(store, type),
    impl(ref)->v8_object()
  };
  auto maybe_obj =
    store->v8_function(V8_F_TABLE)->NewInstance(context, 2, args);
  if (maybe_obj.IsEmpty()) return own<Table*>();
  auto obj = maybe_obj.ToLocalChecked();

  auto data = make_own(new(std::nothrow) TableData(store, obj));
  return TableImpl::make(data);
}

auto Table::type() const -> own<TableType*> {
  // return impl(this)->data->type->copy();
  v8::HandleScope handle_scope(impl(this)->store()->isolate());
  auto v8_table = impl(this)->v8_object();
  uint32_t min = wasm_v8::table_type_min(v8_table);
  uint32_t max = wasm_v8::table_type_max(v8_table);
  // TODO(wasm+): support new element types.
  return TableType::make(ValType::make(FUNCREF), Limits(min, max));
}

auto Table::get(size_t index) const -> own<Ref*> {
  v8::HandleScope handle_scope(impl(this)->store()->isolate());
  auto maybe = wasm_v8::table_get(impl(this)->v8_object(), index);
  if (maybe.IsEmpty() || maybe.ToLocalChecked()->IsNull()) return own<Ref*>();
  // TODO(wasm+): other references
  auto obj = maybe.ToLocalChecked();
  assert(obj->IsFunction());
  auto data = make_own(new(std::nothrow) FuncData(impl(this)->store(), obj));
  return FuncImpl::make(data);
}

auto Table::set(size_t index, const Ref* ref) -> bool {
  v8::HandleScope handle_scope(impl(this)->store()->isolate());
  if (ref && !impl(ref)->v8_object()->IsFunction()) {
    UNIMPLEMENTED("non-function table elements");
    exit(1);
  }
  auto obj = ref
    ? v8::MaybeLocal<v8::Function>(
        v8::Local<v8::Function>::Cast(impl(ref)->v8_object()))
    : v8::MaybeLocal<v8::Function>();
  return wasm_v8::table_set(impl(this)->v8_object(), index, obj);
}

auto Table::size() const -> size_t {
  v8::HandleScope handle_scope(impl(this)->store()->isolate());
  return wasm_v8::table_size(impl(this)->v8_object());
}

auto Table::grow(size_t delta) -> bool {
  v8::HandleScope handle_scope(impl(this)->store()->isolate());
  return wasm_v8::table_grow(impl(this)->v8_object(), delta);
}


// Memory Instances

struct MemoryData : ExternData {
  MemoryData(StoreImpl* store, v8::Local<v8::Object> obj) :
    ExternData(store, obj, EXTERN_MEMORY) {}
};

using MemoryImpl = RefImpl<Memory, MemoryData>;
template<> struct implement<Memory> { using type = MemoryImpl; };


Memory::~Memory() {}

auto Memory::copy() const -> own<Memory*> {
  return impl(this)->copy();
}

auto Memory::make(Store* store_abs, const MemoryType* type) -> own<Memory*> {
  auto store = impl(store_abs);
  auto isolate = store->isolate();
  v8::HandleScope handle_scope(isolate);
  auto context = store->context();

  v8::Local<v8::Value> args[] = { memorytype_to_v8(store, type) };
  auto maybe_obj =
    store->v8_function(V8_F_MEMORY)->NewInstance(context, 1, args);
  if (maybe_obj.IsEmpty()) return own<Memory*>();
  auto obj = maybe_obj.ToLocalChecked();

  auto data = make_own(new(std::nothrow) MemoryData(store, obj));
  return MemoryImpl::make(data);
}

auto Memory::type() const -> own<MemoryType*> {
  // return impl(this)->data->type->copy();
  v8::HandleScope handle_scope(impl(this)->store()->isolate());
  auto v8_memory = impl(this)->v8_object();
  uint32_t min = wasm_v8::memory_type_min(v8_memory);
  uint32_t max = wasm_v8::memory_type_max(v8_memory);
  return MemoryType::make(Limits(min, max));
}

auto Memory::data() const -> byte_t* {
  v8::HandleScope handle_scope(impl(this)->store()->isolate());
  return wasm_v8::memory_data(impl(this)->v8_object());
}

auto Memory::data_size() const -> size_t {
  v8::HandleScope handle_scope(impl(this)->store()->isolate());
  return wasm_v8::memory_data_size(impl(this)->v8_object());
}

auto Memory::size() const -> pages_t {
  v8::HandleScope handle_scope(impl(this)->store()->isolate());
  return wasm_v8::memory_size(impl(this)->v8_object());
}

auto Memory::grow(pages_t delta) -> bool {
  v8::HandleScope handle_scope(impl(this)->store()->isolate());
  return wasm_v8::memory_grow(impl(this)->v8_object(), delta);
}


// Module Instances

struct InstanceData : RefData {
  vec<Extern*> exports;

  InstanceData(
    StoreImpl* store, v8::Local<v8::Object> obj, vec<Extern*>& exports
  ) : RefData(store, obj), exports(std::move(exports)) {}
};

using InstanceImpl = RefImpl<Instance, InstanceData>;
template<> struct implement<Instance> { using type = InstanceImpl; };


Instance::~Instance() {}

auto Instance::copy() const -> own<Instance*> {
  return impl(this)->copy();
}

auto Instance::make(
  Store* store_abs, const Module* module_abs, const vec<Extern*>& imports
) -> own<Instance*> {
  auto store = impl(store_abs);
  auto module = impl(module_abs);
  auto isolate = store->isolate();
  auto context = store->context();
  v8::HandleScope handle_scope(isolate);

  auto import_types = module_abs->imports();
  auto imports_obj = v8::Object::New(isolate);
  for (size_t i = 0; i < import_types.size(); ++i) {
    auto type = import_types[i];
    auto maybe_module = v8::String::NewFromOneByte(
      isolate, reinterpret_cast<const uint8_t*>(type->module().get()),
      v8::NewStringType::kNormal, type->module().size()
    );
    if (maybe_module.IsEmpty()) return own<Instance*>();
    auto module_str = maybe_module.ToLocalChecked();
    auto maybe_name = v8::String::NewFromOneByte(
      isolate, reinterpret_cast<const uint8_t*>(type->name().get()),
      v8::NewStringType::kNormal, type->name().size()
    );
    if (maybe_name.IsEmpty()) return own<Instance*>();
    auto name_str = maybe_name.ToLocalChecked();

    v8::Local<v8::Object> module_obj;
    if (imports_obj->HasOwnProperty(context, module_str).ToChecked()) {
      module_obj = v8::Local<v8::Object>::Cast(
        imports_obj->Get(context, module_str).ToLocalChecked());
    } else {
      module_obj = v8::Object::New(isolate);
      void(imports_obj->DefineOwnProperty(context, module_str, module_obj));
    }

    void(module_obj->DefineOwnProperty(
      context, name_str, extern_to_v8(imports[i])));
  }

  v8::Local<v8::Value> instantiate_args[] = {module->v8_object(), imports_obj};
  auto instance_obj = store->v8_function(V8_F_INSTANCE)->NewInstance(
    context, 2, instantiate_args).ToLocalChecked();
  // TODO: crashes, why?
  // auto exports_obj = wasm_v8::instance_exports(instance_obj);
  auto exports_obj = v8::Local<v8::Object>::Cast(
    instance_obj->Get(context, store->v8_string(V8_S_EXPORTS)).ToLocalChecked()
  );
  assert(!exports_obj.IsEmpty() && exports_obj->IsObject());

  auto export_types = module_abs->exports();
  auto exports = vec<Extern*>::make_uninitialized(export_types.size());
  if (!exports) return own<Instance*>();
  for (size_t i = 0; i < export_types.size(); ++i) {
    auto& name = export_types[i]->name();
    auto maybe_name_obj = v8::String::NewFromUtf8(isolate, name.get(),
      v8::NewStringType::kNormal, name.size());
    if (maybe_name_obj.IsEmpty()) return own<Instance*>();
    auto name_obj = maybe_name_obj.ToLocalChecked();
    auto obj = v8::Local<v8::Object>::Cast(
      exports_obj->Get(context, name_obj).ToLocalChecked());

    auto type = export_types[i]->type();
    switch (type->kind()) {
      case EXTERN_FUNC: {
        assert(wasm_v8::extern_kind(obj) == wasm_v8::EXTERN_FUNC);
        auto data = make_own(new(std::nothrow) FuncData(store, obj));
        exports[i].reset(FuncImpl::make(data));
      } break;
      case EXTERN_GLOBAL: {
        assert(wasm_v8::extern_kind(obj) == wasm_v8::EXTERN_GLOBAL);
        auto data = make_own(new(std::nothrow) GlobalData(store, obj));
        exports[i].reset(GlobalImpl::make(data));
      } break;
      case EXTERN_TABLE: {
        assert(wasm_v8::extern_kind(obj) == wasm_v8::EXTERN_TABLE);
        auto data = make_own(new(std::nothrow) TableData(store, obj));
        exports[i].reset(TableImpl::make(data));
      } break;
      case EXTERN_MEMORY: {
        assert(wasm_v8::extern_kind(obj) == wasm_v8::EXTERN_MEMORY);
        auto data = make_own(new(std::nothrow) MemoryData(store, obj));
        exports[i].reset(MemoryImpl::make(data));
      } break;
    }
  }

  auto data = make_own(
    new(std::nothrow) InstanceData(store, instance_obj, exports));
  return InstanceImpl::make(data);
}

auto Instance::exports() const -> vec<Extern*> {
  return impl(this)->data->exports.copy();
}

///////////////////////////////////////////////////////////////////////////////

}  // namespace wasm
