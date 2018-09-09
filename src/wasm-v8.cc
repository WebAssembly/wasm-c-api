#include "wasm.hh"
#include "wasm-bin.hh"
#include "wasm-v8-lowlevel.hh"

#include "v8.h"
#include "libplatform/libplatform.h"

#include <iostream>

#ifdef DEBUG
#include <atomic>
#endif


namespace wasm_v8 {
  using namespace v8::wasm;
}

namespace v8 {
  namespace internal {
    extern bool FLAG_expose_gc;
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

template<class T>
void ignore(T) {}


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
// Debug aids

struct Stats {
  enum category_t {
    BYTE, CONFIG, ENGINE, STORE,
    VALTYPE, FUNCTYPE, GLOBALTYPE, TABLETYPE, MEMORYTYPE,
    EXTERNTYPE, IMPORTTYPE, EXPORTTYPE,
    VAL, REF, TRAP,
    MODULE, INSTANCE, FUNC, GLOBAL, TABLE, MEMORY, EXTERN,
    STRONG_COUNT,
    FUNCDATA_FUNCTYPE, FUNCDATA_VALTYPE,
    CATEGORY_COUNT
  };
  enum cardinality_t {
    OWN, VEC, SHARED, CARDINALITY_COUNT
  };

#ifdef DEBUG
  static const char* name[STRONG_COUNT];
  static const char* left[CARDINALITY_COUNT];
  static const char* right[CARDINALITY_COUNT];

  std::atomic<size_t> made[CATEGORY_COUNT][CARDINALITY_COUNT];
  std::atomic<size_t> freed[CATEGORY_COUNT][CARDINALITY_COUNT];

  Stats() {
    for (int i = 0; i < CATEGORY_COUNT; ++i) {
      for (int j = 0; j < CARDINALITY_COUNT; ++j) {
        made[i][j] = freed[i][j] = 0;
      }
    }
  }

  ~Stats() {
    // Hack for func data weakly owned by V8 heap.
    freed[FUNCTYPE][OWN] +=
      made[FUNCDATA_FUNCTYPE][OWN] - freed[FUNCDATA_FUNCTYPE][OWN];
    freed[VALTYPE][OWN] +=
      made[FUNCDATA_VALTYPE][OWN] - freed[FUNCDATA_VALTYPE][OWN];
    freed[VALTYPE][VEC] +=
      made[FUNCDATA_VALTYPE][VEC] - freed[FUNCDATA_VALTYPE][VEC];
    // Hack for shared modules.
    freed[BYTE][VEC] += made[MODULE][SHARED] - freed[MODULE][SHARED];

    bool leak = false;
    for (int i = 0; i < STRONG_COUNT; ++i) {
      for (int j = 0; j < CARDINALITY_COUNT; ++j) {
        assert(made[i][j] >= freed[i][j]);
        auto live = made[i][j] - freed[i][j];
        if (live) {
          std::cerr << "Leaked " << live << " instances of wasm::"
            << left[j] << name[i] << right[j]
            << ", made " << made[i][j] << ", freed " << freed[i][j] << "!"
            << std::endl;
          leak = true;
        }
      }
    }
    if (leak) exit(1);
  }
#endif

  void make(category_t i, void* ptr, cardinality_t j = OWN, size_t n = 1) {
#ifdef DEBUG
#ifdef DEBUG_LOG
    if (ptr) {
      std::clog << "[make] " << ptr
        << " wasm::" << left[j] << name[i] << right[j] << std::endl;
    }
#endif
    made[i][j] += n;
#endif
  }

  void free(category_t i, void* ptr, cardinality_t j = OWN, size_t n = 1) {
#ifdef DEBUG
#ifdef DEBUG_LOG
    if (ptr) {
      std::clog << "[free] " << ptr
        << " wasm::" << left[j] << name[i] << right[j] << std::endl;
    }
#endif
    freed[i][j] += n;
    if (freed[i][j] > made[i][j]) {
      std::cerr << "Deleting instance of wasm::"
        << left[j] << name[i] << right[j] << " when none is alive"
        << ", made " << made[i][j] << ", freed " << freed[i][j] << "!"
        << std::endl;
      exit(1);
    }
#endif
  }

  static category_t categorize(const v8::Persistent<v8::Object>& pobj) {
#ifdef DEBUG
    auto isolate = wasm_v8::object_isolate(pobj);
    v8::HandleScope handle_scope(isolate);
    auto obj = pobj.Get(isolate);
    if (wasm_v8::object_is_func(obj)) return FUNC;
    if (wasm_v8::object_is_global(obj)) return GLOBAL;
    if (wasm_v8::object_is_table(obj)) return TABLE;
    if (wasm_v8::object_is_memory(obj)) return MEMORY;
    if (wasm_v8::object_is_module(obj)) return MODULE;
    if (wasm_v8::object_is_instance(obj)) return INSTANCE;
    if (wasm_v8::object_is_error(obj)) return TRAP;
#endif
    return REF;
  }
};

#ifdef DEBUG
const char* Stats::name[STRONG_COUNT] = {
  "byte_t", "Config", "Engine", "Store",
  "ValType", "FuncType", "GlobalType", "TableType", "MemoryType",
  "ExternType", "ImportType", "ExportType",
  "Val", "Ref", "Trap",
  "Module", "Instance", "Func", "Global", "Table", "Memory", "Extern"
};

const char* Stats::left[CARDINALITY_COUNT] = {
  "", "vec<", "Shared<"
};

const char* Stats::right[CARDINALITY_COUNT] = {
  "", ">", ">"
};
#endif


Stats stats;


// Vectors

#ifdef DEBUG

#define DEFINE_VEC(type, STAT) \
  template<> void vec<type>::make_data() { \
    if (data_) stats.make(Stats::STAT, data_.get(), Stats::VEC); \
  } \
  \
  template<> void vec<type>::free_data() { \
    if (data_) stats.free(Stats::STAT, data_.get(), Stats::VEC); \
  }

DEFINE_VEC(byte_t, BYTE)
DEFINE_VEC(ValType*, VALTYPE)
DEFINE_VEC(FuncType*, FUNCTYPE)
DEFINE_VEC(GlobalType*, GLOBALTYPE)
DEFINE_VEC(TableType*, TABLETYPE)
DEFINE_VEC(MemoryType*, MEMORYTYPE)
DEFINE_VEC(ExternType*, EXTERNTYPE)
DEFINE_VEC(ImportType*, IMPORTTYPE)
DEFINE_VEC(ExportType*, EXPORTTYPE)
DEFINE_VEC(Ref*, REF)
DEFINE_VEC(Trap*, TRAP)
DEFINE_VEC(Module*, MODULE)
DEFINE_VEC(Instance*, INSTANCE)
DEFINE_VEC(Func*, FUNC)
DEFINE_VEC(Global*, GLOBAL)
DEFINE_VEC(Table*, TABLE)
DEFINE_VEC(Memory*, MEMORY)
DEFINE_VEC(Extern*, EXTERN)
DEFINE_VEC(Val, VAL)

#endif  // #ifdef DEBUG


///////////////////////////////////////////////////////////////////////////////
// Runtime Environment

// Configuration

struct ConfigImpl {
  ConfigImpl() { stats.make(Stats::CONFIG, this); }
  ~ConfigImpl() { stats.free(Stats::CONFIG, this); }
};

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
    stats.make(Stats::ENGINE, this);
  }

  ~EngineImpl() {
    v8::V8::Dispose();
    v8::V8::ShutdownPlatform();
    stats.free(Stats::ENGINE, this);
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

auto Engine::make(own<Config*>&& config) -> own<Engine*> {
  v8::internal::FLAG_expose_gc = true;
  v8::internal::FLAG_experimental_wasm_mut_global = true;
  // v8::V8::SetFlagsFromCommandLine(&argc, const_cast<char**>(argv), false);
  auto engine = new(std::nothrow) EngineImpl;
  if (!engine) return own<Engine*>();
  // v8::V8::InitializeICUDefaultLocation(argv[0]);
  // v8::V8::InitializeExternalStartupData(argv[0]);
  engine->platform = v8::platform::NewDefaultPlatform();
  v8::V8::InitializePlatform(engine->platform.get());
  v8::V8::Initialize();
  return make_own(seal<Engine>(engine));
}


// Stores

enum v8_string_t {
  V8_S_EMPTY,
  V8_S_I32, V8_S_I64, V8_S_F32, V8_S_F64, V8_S_ANYREF, V8_S_ANYFUNC,
  V8_S_VALUE, V8_S_MUTABLE, V8_S_ELEMENT, V8_S_MINIMUM, V8_S_MAXIMUM,
  V8_S_COUNT
};

enum v8_symbol_t {
  V8_Y_CALLBACK, V8_Y_ENV,
  V8_Y_COUNT
};

enum v8_function_t {
  V8_F_WEAKMAP, V8_F_WEAKMAP_PROTO, V8_F_WEAKMAP_GET, V8_F_WEAKMAP_SET,
  V8_F_MODULE, V8_F_GLOBAL, V8_F_TABLE, V8_F_MEMORY,
  V8_F_INSTANCE, V8_F_VALIDATE,
  V8_F_COUNT,
};

class StoreImpl {
  friend own<Store*> Store::make(Engine*);

  v8::Isolate::CreateParams create_params_;
  v8::Isolate *isolate_;
  v8::Eternal<v8::Context> context_;
  v8::Eternal<v8::String> strings_[V8_S_COUNT];
  v8::Eternal<v8::Symbol> symbols_[V8_Y_COUNT];
  v8::Eternal<v8::Function> functions_[V8_F_COUNT];
  v8::Eternal<v8::Object> host_data_map_;
  v8::Eternal<v8::Symbol> callback_symbol_;
  v8::Persistent<v8::Object>* handle_pool_ = nullptr;  // TODO: use v8::Value

public:
  StoreImpl() {
    stats.make(Stats::STORE, this);
  }

  ~StoreImpl() {
#ifdef DEBUG
    isolate_->RequestGarbageCollectionForTesting(
      v8::Isolate::kFullGarbageCollection);
#endif
    context()->Exit();
    isolate_->Exit();
    isolate_->Dispose();
    delete create_params_.array_buffer_allocator;
    stats.free(Stats::STORE, this);
  }

  auto isolate() const -> v8::Isolate* {
    return isolate_;
  }

  auto context() const -> v8::Local<v8::Context> {
    return context_.Get(isolate_);
  }

  auto v8_string(v8_string_t i) const -> v8::Local<v8::String> {
    return strings_[i].Get(isolate_);
  }
  auto v8_string(v8_symbol_t i) const -> v8::Local<v8::Symbol> {
    return symbols_[i].Get(isolate_);
  }
  auto v8_function(v8_function_t i) const -> v8::Local<v8::Function> {
    return functions_[i].Get(isolate_);
  }

  auto host_data_map() const -> v8::Local<v8::Object> {
    return host_data_map_.Get(isolate_);
  }

  static auto get(v8::Isolate* isolate) -> StoreImpl* {
    return static_cast<StoreImpl*>(isolate->GetData(0));
  }

  auto make_handle() -> v8::Persistent<v8::Object>* {
    if (handle_pool_ == nullptr) {
      static const size_t n = 100;
      handle_pool_ = new(std::nothrow) v8::Persistent<v8::Object>[n];
      if (!handle_pool_) return nullptr;
      for (size_t i = 0; i < n; ++i) {
        auto next = i == n - 1 ? nullptr : &handle_pool_[i + 1];
        auto v8_next = wasm_v8::foreign_new(isolate_, next);
        handle_pool_[i].Reset(isolate_, v8::Local<v8::Object>::Cast(v8_next));
      }
    }
    auto handle = handle_pool_;
    handle_pool_ = reinterpret_cast<v8::Persistent<v8::Object>*>(
      wasm_v8::foreign_get(handle->Get(isolate_)));
    return handle;
  }

  void free_handle(v8::Persistent<v8::Object>* handle) {
    // TODO: shrink pool?
    auto next = wasm_v8::foreign_new(isolate_, handle_pool_);
    handle->Reset(isolate_, v8::Local<v8::Object>::Cast(next));
    handle_pool_ = handle;
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

  // Create isolate.
  store->create_params_.array_buffer_allocator =
    v8::ArrayBuffer::Allocator::NewDefaultAllocator();
  auto isolate = v8::Isolate::New(store->create_params_);
  if (!isolate) return own<Store*>();

  {
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);

    // Create context.
    auto context = v8::Context::New(isolate);
    if (context.IsEmpty()) return own<Store*>();
    v8::Context::Scope context_scope(context);

    store->isolate_ = isolate;
    store->context_ = v8::Eternal<v8::Context>(isolate, context);

    // Create strings.
    static const char* const raw_strings[V8_S_COUNT] = {
      "",
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

    for (int i = 0; i < V8_Y_COUNT; ++i) {
      auto symbol = v8::Symbol::New(isolate);
      store->symbols_[i] = v8::Eternal<v8::Symbol>(isolate, symbol);
    }

    // Extract functions.
    auto global = context->Global();
    auto maybe_wasm_name = v8::String::NewFromUtf8(isolate, "WebAssembly",
        v8::NewStringType::kNormal);
    if (maybe_wasm_name.IsEmpty()) return own<Store*>();
    auto wasm_name = maybe_wasm_name.ToLocalChecked();
    auto maybe_wasm = global->Get(context, wasm_name);
    if (maybe_wasm.IsEmpty()) return own<Store*>();
    auto wasm = v8::Local<v8::Object>::Cast(maybe_wasm.ToLocalChecked());
    v8::Local<v8::Object> weakmap;
    v8::Local<v8::Object> weakmap_proto;

    struct {
      const char* name;
      v8::Local<v8::Object>* carrier;
    } raw_functions[V8_F_COUNT] = {
      {"WeakMap", &global}, {"prototype", &weakmap},
      {"get", &weakmap_proto}, {"set", &weakmap_proto},
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
      auto obj = v8::Local<v8::Object>::Cast(maybe_obj.ToLocalChecked());
      if (i == V8_F_WEAKMAP_PROTO) {
        assert(obj->IsObject());
        weakmap_proto = obj;
      } else {
        assert(obj->IsFunction());
        auto function = v8::Local<v8::Function>::Cast(obj);
        store->functions_[i] = v8::Eternal<v8::Function>(isolate, function);
        if (i == V8_F_WEAKMAP) weakmap = function;
      }
    }

    // Create host data weak map.
    v8::Local<v8::Value> empty_args[] = {};
    auto maybe_weakmap =
      store->v8_function(V8_F_WEAKMAP)->NewInstance(context, 0, empty_args);
    if (maybe_weakmap.IsEmpty()) return own<Store*>();
    auto map = v8::Local<v8::Object>::Cast(maybe_weakmap.ToLocalChecked());
    assert(map->IsWeakMap());
    store->host_data_map_ = v8::Eternal<v8::Object>(isolate, map);
  }

  store->isolate()->Enter();
  store->context()->Enter();
  isolate->SetData(0, store.get());

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


ValType::~ValType() {
  stats.free(Stats::VALTYPE, this);
}

void ValType::operator delete(void*) {}

auto ValType::make(ValKind k) -> own<ValType*> {
  auto result = seal<ValType>(valtypes[k]);
  stats.make(Stats::VALTYPE, result);
  return own<ValType*>(result);
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
    params(std::move(params)), results(std::move(results))
  {
    stats.make(Stats::FUNCTYPE, this);
  }

  ~FuncTypeImpl() {
    stats.free(Stats::FUNCTYPE, this);
  }
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
    content(std::move(content)), mutability(mutability)
  {
    stats.make(Stats::GLOBALTYPE, this);
  }

  ~GlobalTypeImpl() {
    stats.free(Stats::GLOBALTYPE, this);
  }
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
    ExternTypeImpl(EXTERN_TABLE), element(std::move(element)), limits(limits)
  {
    stats.make(Stats::TABLETYPE, this);
  }

  ~TableTypeImpl() {
    stats.free(Stats::TABLETYPE, this);
  }
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
    ExternTypeImpl(EXTERN_MEMORY), limits(limits)
  {
    stats.make(Stats::MEMORYTYPE, this);
  }

  ~MemoryTypeImpl() {
    stats.free(Stats::MEMORYTYPE, this);
  }
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
    module(std::move(module)), name(std::move(name)), type(std::move(type))
  {
    stats.make(Stats::IMPORTTYPE, this);
  }

  ~ImportTypeImpl() {
    stats.free(Stats::IMPORTTYPE, this);
  }
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
    name(std::move(name)), type(std::move(type))
  {
    stats.make(Stats::EXPORTTYPE, this);
  }

  ~ExportTypeImpl() {
    stats.free(Stats::EXPORTTYPE, this);
  }
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
  ignore(desc->DefineOwnProperty(context, store->v8_string(V8_S_MINIMUM),
    v8::Integer::NewFromUnsigned(isolate, limits.min)));
  if (limits.max != Limits(0).max) {
    ignore(desc->DefineOwnProperty(context, store->v8_string(V8_S_MAXIMUM),
      v8::Integer::NewFromUnsigned(isolate, limits.max)));
  }
}

auto globaltype_to_v8(
  StoreImpl* store, const GlobalType* type
) -> v8::Local<v8::Object> {
  auto isolate = store->isolate();
  auto context = store->context();
  auto desc = v8::Object::New(isolate);
  ignore(desc->DefineOwnProperty(context, store->v8_string(V8_S_VALUE),
    valtype_to_v8(store, type->content())));
  ignore(desc->DefineOwnProperty(context, store->v8_string(V8_S_MUTABLE),
    mutability_to_v8(store, type->mutability())));
  return desc;
}

auto tabletype_to_v8(
  StoreImpl* store, const TableType* type
) -> v8::Local<v8::Object> {
  auto isolate = store->isolate();
  auto context = store->context();
  auto desc = v8::Object::New(isolate);
  ignore(desc->DefineOwnProperty(context, store->v8_string(V8_S_ELEMENT),
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

template<class Ref>
class RefImpl : public v8::Persistent<v8::Object> {
public:
  RefImpl() = delete;
  ~RefImpl() = delete;

  static auto make(StoreImpl* store, v8::Local<v8::Object> obj) -> own<Ref*> {
    static_assert(sizeof(RefImpl) == sizeof(v8::Persistent<v8::Object>),
      "incompatible object layout");
    auto self = static_cast<RefImpl*>(store->make_handle());
    if (!self) return nullptr;
    self->Reset(store->isolate(), obj);
    stats.make(Stats::categorize(*self), self);
    return make_own(seal<Ref>(self));
  }

  auto copy() const -> own<Ref*> {
    v8::HandleScope handle_scope(isolate());
    return make(store(), v8_object());
  }

  auto store() const -> StoreImpl* {
    return reinterpret_cast<StoreImpl*>(isolate()->GetData(0));
  }

  auto isolate() const -> v8::Isolate* {
    return wasm_v8::object_isolate(*this);
  }

  auto v8_object() const -> v8::Local<v8::Object> {
    return Get(isolate());
  }

  auto get_host_info() const -> void* {
    v8::HandleScope handle_scope(isolate());
    auto store = this->store();

    v8::Local<v8::Value> args[] = { v8_object() };
    auto maybe_result = store->v8_function(V8_F_WEAKMAP_GET)->Call(
      store->context(), store->host_data_map(), 1, args);
    if (maybe_result.IsEmpty()) return nullptr;

    auto data = wasm_v8::foreign_get(maybe_result.ToLocalChecked());
    return reinterpret_cast<HostData*>(data)->info;
  }

  void set_host_info(void* info, void (*finalizer)(void*)) {
    v8::HandleScope handle_scope(isolate());
    auto store = this->store();
    auto previous = this->template ClearWeak<HostData>();

    auto data = new HostData{info, finalizer, previous};
    auto foreign = wasm_v8::foreign_new(store->isolate(), data);
    v8::Local<v8::Value> args[] = { v8_object(), foreign };
    auto maybe_result = store->v8_function(V8_F_WEAKMAP_SET)->Call(
      store->context(), store->host_data_map(), 2, args);
    if (maybe_result.IsEmpty()) return;

    this->template SetWeak<HostData>(
      data, &v8_finalizer, v8::WeakCallbackType::kParameter);
  }

private:
  struct HostData {
    void* info;
    void (*finalizer)(void*);
    HostData* other;
  };

  static void v8_finalizer(const v8::WeakCallbackInfo<HostData>& info) {
    auto data = info.GetParameter();
    while (data) {
      if (data->finalizer) (*data->finalizer)(data->info);
      auto next = data->other;
      delete data;
      data = next;
    }
  }
};

template<> struct implement<Ref> { using type = RefImpl<Ref>; };


Ref::~Ref() {
  stats.free(Stats::categorize(*impl(this)), this);
}

void Ref::operator delete(void *p) {}

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

// Traps

template<> struct implement<Trap> { using type = RefImpl<Trap>; };


Trap::~Trap() {}

auto Trap::copy() const -> own<Trap*> {
  return impl(this)->copy();
}

auto Trap::make(Store* store_abs, const Message& message) -> own<Trap*> {
  auto store = impl(store_abs);
  v8::Isolate* isolate = store->isolate();
  v8::HandleScope handle_scope(isolate);

  auto maybe_string = v8::String::NewFromUtf8(isolate, message.get(),
    v8::NewStringType::kNormal, message.size());
  if (maybe_string.IsEmpty()) return own<Trap*>();
  auto exception = v8::Exception::Error(maybe_string.ToLocalChecked());
  return RefImpl<Trap>::make(store, v8::Handle<v8::Object>::Cast(exception));
}

auto Trap::message() const -> Message {
  auto isolate = impl(this)->isolate();
  v8::HandleScope handle_scope(isolate);

  auto message = v8::Exception::CreateMessage(isolate, impl(this)->v8_object());
  v8::String::Utf8Value string(isolate, message->Get());
  return vec<byte_t>::make(std::string(*string));
}


// Foreign Objects

template<> struct implement<Foreign> { using type = RefImpl<Foreign>; };


Foreign::~Foreign() {}

auto Foreign::copy() const -> own<Foreign*> {
  return impl(this)->copy();
}

auto Foreign::make(Store* store_abs) -> own<Foreign*> {
  auto store = impl(store_abs);
  auto isolate = store->isolate();
  v8::HandleScope handle_scope(isolate);

  auto obj = v8::Object::New(isolate);
  return RefImpl<Foreign>::make(store, obj);
}


// Modules

template<> struct implement<Module> { using type = RefImpl<Module>; };


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
  return RefImpl<Module>::make(store, maybe_obj.ToLocalChecked());
}

auto Module::imports() const -> vec<ImportType*> {
  v8::HandleScope handle_scope(impl(this)->isolate());
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
  v8::HandleScope handle_scope(impl(this)->isolate());
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
  v8::HandleScope handle_scope(impl(this)->isolate());
  auto module = impl(this)->v8_object();
  auto binary_size = wasm_v8::module_binary_size(module);
  auto serial_size = wasm_v8::module_serialize_size(module);
  auto size_size = wasm::bin::u64_size(binary_size);
  auto buffer = vec<byte_t>::make_uninitialized(
    size_size + binary_size + serial_size);
  auto ptr = buffer.get();
  wasm::bin::encode_u64(ptr, binary_size);
  std::memcpy(ptr, wasm_v8::module_binary(module), binary_size);
  ptr += binary_size;
  if (!wasm_v8::module_serialize(module, ptr, serial_size)) buffer.reset();
  return std::move(buffer);
}

auto Module::deserialize(Store* store_abs, const vec<byte_t>& serialized) -> own<Module*> {
  auto store = impl(store_abs);
  auto isolate = store->isolate();
  v8::HandleScope handle_scope(isolate);
  auto ptr = serialized.get();
  auto binary_size = wasm::bin::u64(ptr);
  auto size_size = ptr - serialized.get();
  auto serial_size = serialized.size() - size_size - binary_size;
  auto maybe_obj = wasm_v8::module_deserialize(
    isolate, ptr, binary_size, ptr + binary_size, serial_size);
  if (maybe_obj.IsEmpty()) return nullptr;
  return RefImpl<Module>::make(store, maybe_obj.ToLocalChecked());
}


// TODO(wasm+): do better when V8 can do better.
template<> struct implement<Shared<Module>> { using type = vec<byte_t>; };

template<>
Shared<Module>::~Shared() {
  stats.free(Stats::MODULE, this, Stats::SHARED);
  impl(this)->~vec();
}

template<>
void Shared<Module>::operator delete(void* p) {
  ::operator delete(p);
}

auto Module::share() const -> own<Shared<Module>*> {
  auto shared = seal<Shared<Module>>(new vec<byte_t>(std::move(serialize())));
  stats.make(Stats::MODULE, shared, Stats::SHARED);
  return make_own(shared);
}

auto Module::obtain(Store* store, const Shared<Module>* shared) -> own<Module*> {
  return Module::deserialize(store, *impl(shared));
}




// Externals

template<> struct implement<Extern> { using type = RefImpl<Extern>; };


Extern::~Extern() {}

auto Extern::copy() const -> own<Extern*> {
  return impl(this)->copy();
}

auto Extern::kind() const -> ExternKind {
  v8::HandleScope handle_scope(impl(this)->isolate());
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

auto extern_to_v8(const Extern* ex) -> v8::Local<v8::Value> {
  return impl(ex)->v8_object();
}


// Function Instances

template<> struct implement<Func> { using type = RefImpl<Func>; };


Func::~Func() {}

auto Func::copy() const -> own<Func*> {
  return impl(this)->copy();
}

struct FuncData {
  Store* store;
  own<FuncType*> type;
  enum Kind { CALLBACK, CALLBACK_WITH_ENV } kind;
  union {
    Func::callback callback;
    Func::callback_with_env callback_with_env;
  };
  void* env;

  FuncData(Store* store, const FuncType* type, Kind kind) :
    store(store), type(type->copy()), kind(kind)
  {
    stats.make(Stats::FUNCDATA_FUNCTYPE, nullptr);
    stats.make(Stats::FUNCDATA_VALTYPE, nullptr, Stats::OWN, type->params().size());
    stats.make(Stats::FUNCDATA_VALTYPE, nullptr, Stats::OWN, type->results().size());
    if (type->params().get()) stats.make(Stats::FUNCDATA_VALTYPE, nullptr, Stats::VEC);
    if (type->results().get()) stats.make(Stats::FUNCDATA_VALTYPE, nullptr, Stats::VEC);
  }

  ~FuncData() {
    stats.free(Stats::FUNCDATA_FUNCTYPE, nullptr);
    stats.free(Stats::FUNCDATA_VALTYPE, nullptr, Stats::OWN, type->params().size());
    stats.free(Stats::FUNCDATA_VALTYPE, nullptr, Stats::OWN, type->results().size());
    if (type->params().get()) stats.free(Stats::FUNCDATA_VALTYPE, nullptr, Stats::VEC);
    if (type->results().get()) stats.free(Stats::FUNCDATA_VALTYPE, nullptr, Stats::VEC);
  }

  static void v8_callback(const v8::FunctionCallbackInfo<v8::Value>&);
  static void finalize_func_data(void* data);
};

namespace {

auto make_func(Store* store_abs, FuncData* data) -> own<Func*> {
  auto store = impl(store_abs);
  auto isolate = store->isolate();
  v8::HandleScope handle_scope(isolate);
  auto context = store->context();

  // Create V8 function
  auto v8_data = wasm_v8::foreign_new(isolate, data);
  auto function_template = v8::FunctionTemplate::New(
    isolate, &FuncData::v8_callback, v8_data);
  auto maybe_func_obj = function_template->GetFunction(context);
  if (maybe_func_obj.IsEmpty()) return own<Func*>();
  auto func_obj = maybe_func_obj.ToLocalChecked();

  // Create wrapper instance
  auto binary = wasm::bin::wrapper(data->type.get());
  auto module = Module::make(store_abs, binary);

  auto imports_obj = v8::Object::New(isolate);
  auto module_obj = v8::Object::New(isolate);
  auto str = store->v8_string(V8_S_EMPTY);
  ignore(imports_obj->DefineOwnProperty(context, str, module_obj));
  ignore(module_obj->DefineOwnProperty(context, str, func_obj));

  v8::Local<v8::Value> instantiate_args[] = {
    impl(module.get())->v8_object(), imports_obj
  };
  auto instance_obj = store->v8_function(V8_F_INSTANCE)->NewInstance(
    context, 2, instantiate_args).ToLocalChecked();
  assert(!instance_obj.IsEmpty());
  assert(instance_obj->IsObject());
  auto exports_obj = wasm_v8::instance_exports(instance_obj);
  assert(!exports_obj.IsEmpty());
  assert(exports_obj->IsObject());
  auto wrapped_func_obj = v8::Local<v8::Function>::Cast(
    exports_obj->Get(context, str).ToLocalChecked());
  assert(!wrapped_func_obj.IsEmpty());
  assert(wrapped_func_obj->IsFunction());

  auto func = RefImpl<Func>::make(store, wrapped_func_obj);
  func->set_host_info(data, &FuncData::finalize_func_data);
  return func;
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
  Store* store, const FuncType* type, Func::callback callback
) -> own<Func*> {
  auto data = new FuncData(store, type, FuncData::CALLBACK);
  data->callback = callback;
  return make_func(store, data);
}

auto Func::make(
  Store* store, const FuncType* type,
  callback_with_env callback, void* env, void (*finalizer)(void*)
) -> own<Func*> {
  auto data = new FuncData(store, type, FuncData::CALLBACK_WITH_ENV);
  data->callback_with_env = callback;
  data->env = env;
  return make_func(store, data);
}

auto Func::type() const -> own<FuncType*> {
  // return impl(this)->data->type->copy();
  v8::HandleScope handle_scope(impl(this)->isolate());
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
    auto exception = handler.Exception();
    if (!exception->IsObject()) {
      auto maybe_string = exception->ToString(store->context());
      auto string = maybe_string.IsEmpty()
        ? store->v8_string(V8_S_EMPTY) : maybe_string.ToLocalChecked();
      exception = v8::Exception::Error(string);
    }
    return Result(RefImpl<Trap>::make(
      store, v8::Handle<v8::Object>::Cast(exception)));
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
  auto v8_data = v8::Local<v8::Object>::Cast(info.Data());
  auto self = reinterpret_cast<FuncData*>(wasm_v8::foreign_get(v8_data));
  auto store = impl(self->store);
  auto isolate = store->isolate();
  v8::HandleScope handle_scope(isolate);

  auto context = store->context();
  auto& type_params = self->type->params();
  auto& type_results = self->type->results();

  assert(type_params.size() == info.Length());

  auto args = vec<Val>::make_uninitialized(type_params.size());
  for (size_t i = 0; i < type_params.size(); ++i) {
    args[i] = v8_to_val(store, info[i], type_params[i]);
  }

  auto result = self->kind == CALLBACK_WITH_ENV
    ? self->callback_with_env(self->env, args)
    : self->callback(args);

  if (result.kind() == Result::TRAP) {
    isolate->ThrowException(impl(result.trap())->v8_object());
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

void FuncData::finalize_func_data(void* data) {
  delete reinterpret_cast<FuncData*>(data);
}


// Global Instances

template<> struct implement<Global> { using type = RefImpl<Global>; };


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
  auto exports_obj = wasm_v8::instance_exports(instance_obj);
  auto obj = v8::Local<v8::Object>::Cast(
    exports_obj->Get(context, store->v8_string(V8_S_EMPTY)).ToLocalChecked());
  assert(!obj.IsEmpty() && obj->IsObject());

  auto global = RefImpl<Global>::make(store, obj);
  assert(global);
  global->set(val);
  return global;
}

auto Global::type() const -> own<GlobalType*> {
  // return impl(this)->data->type->copy();
  v8::HandleScope handle_scope(impl(this)->isolate());
  auto v8_global = impl(this)->v8_object();
  auto kind = static_cast<ValKind>(wasm_v8::global_type_content(v8_global));
  auto mutability = wasm_v8::global_type_mutable(v8_global) ? VAR : CONST;
  return GlobalType::make(ValType::make(kind), mutability);
}

auto Global::get() const -> Val {
  v8::HandleScope handle_scope(impl(this)->isolate());
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
  v8::HandleScope handle_scope(impl(this)->isolate());
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
  ignore(store->v8_function(V8_F_GLOBAL_SET)->Call(
    context, global->v8_object(), 1, args));
*/
}


// Table Instances

template<> struct implement<Table> { using type = RefImpl<Table>; };


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

  v8::Local<v8::Value> init = v8::Null(isolate);
  if (ref) init = impl(ref)->v8_object();
  v8::Local<v8::Value> args[] = {tabletype_to_v8(store, type), init};
  auto maybe_obj =
    store->v8_function(V8_F_TABLE)->NewInstance(context, 2, args);
  if (maybe_obj.IsEmpty()) return own<Table*>();
  auto table = RefImpl<Table>::make(store, maybe_obj.ToLocalChecked());
  // TODO(wasm+): pass reference initialiser as parameter
  if (table && ref) {
    auto size = type->limits().min;
    auto obj = maybe_obj.ToLocalChecked();
    auto maybe_func = v8::MaybeLocal<v8::Function>(
      v8::Local<v8::Function>::Cast(init));
    for (size_t i = 0; i < size; ++i) {
      wasm_v8::table_set(obj, i, maybe_func);
    }
  }
  return table;
}

auto Table::type() const -> own<TableType*> {
  // return impl(this)->data->type->copy();
  v8::HandleScope handle_scope(impl(this)->isolate());
  auto v8_table = impl(this)->v8_object();
  uint32_t min = wasm_v8::table_type_min(v8_table);
  uint32_t max = wasm_v8::table_type_max(v8_table);
  // TODO(wasm+): support new element types.
  return TableType::make(ValType::make(FUNCREF), Limits(min, max));
}

auto Table::get(size_t index) const -> own<Ref*> {
  v8::HandleScope handle_scope(impl(this)->isolate());
  auto maybe = wasm_v8::table_get(impl(this)->v8_object(), index);
  if (maybe.IsEmpty() || maybe.ToLocalChecked()->IsNull()) return own<Ref*>();
  // TODO(wasm+): other references
  auto obj = maybe.ToLocalChecked();
  assert(obj->IsFunction());
  return RefImpl<Func>::make(impl(this)->store(), obj);
}

auto Table::set(size_t index, const Ref* ref) -> bool {
  v8::HandleScope handle_scope(impl(this)->isolate());
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
  v8::HandleScope handle_scope(impl(this)->isolate());
  return wasm_v8::table_size(impl(this)->v8_object());
}

auto Table::grow(size_t delta, const Ref* ref) -> bool {
  v8::HandleScope handle_scope(impl(this)->isolate());
  auto obj = ref
    ? v8::MaybeLocal<v8::Function>(
        v8::Local<v8::Function>::Cast(impl(ref)->v8_object()))
    : v8::MaybeLocal<v8::Function>();
  return wasm_v8::table_grow(impl(this)->v8_object(), delta, obj);
}


// Memory Instances

template<> struct implement<Memory> { using type = RefImpl<Memory>; };


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
  return RefImpl<Memory>::make(store, maybe_obj.ToLocalChecked());
}

auto Memory::type() const -> own<MemoryType*> {
  // return impl(this)->data->type->copy();
  v8::HandleScope handle_scope(impl(this)->isolate());
  auto v8_memory = impl(this)->v8_object();
  uint32_t min = wasm_v8::memory_type_min(v8_memory);
  uint32_t max = wasm_v8::memory_type_max(v8_memory);
  return MemoryType::make(Limits(min, max));
}

auto Memory::data() const -> byte_t* {
  v8::HandleScope handle_scope(impl(this)->isolate());
  return wasm_v8::memory_data(impl(this)->v8_object());
}

auto Memory::data_size() const -> size_t {
  v8::HandleScope handle_scope(impl(this)->isolate());
  return wasm_v8::memory_data_size(impl(this)->v8_object());
}

auto Memory::size() const -> pages_t {
  v8::HandleScope handle_scope(impl(this)->isolate());
  return wasm_v8::memory_size(impl(this)->v8_object());
}

auto Memory::grow(pages_t delta) -> bool {
  v8::HandleScope handle_scope(impl(this)->isolate());
  return wasm_v8::memory_grow(impl(this)->v8_object(), delta);
}


// Module Instances

template<> struct implement<Instance> { using type = RefImpl<Instance>; };


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

  assert(wasm_v8::object_isolate(module->v8_object()) == isolate);

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
      ignore(imports_obj->DefineOwnProperty(context, module_str, module_obj));
    }

    ignore(module_obj->DefineOwnProperty(
      context, name_str, extern_to_v8(imports[i])));
  }

  v8::Local<v8::Value> instantiate_args[] = {module->v8_object(), imports_obj};
  auto obj = store->v8_function(V8_F_INSTANCE)->NewInstance(
    context, 2, instantiate_args).ToLocalChecked();
  return RefImpl<Instance>::make(store, obj);
}

auto Instance::exports() const -> vec<Extern*> {
  auto instance = impl(this);
  auto store = instance->store();
  auto isolate = store->isolate();
  auto context = store->context();
  v8::HandleScope handle_scope(isolate);

  auto module_obj = wasm_v8::instance_module(instance->v8_object());
  auto exports_obj = wasm_v8::instance_exports(instance->v8_object());
  assert(!module_obj.IsEmpty() && module_obj->IsObject());
  assert(!exports_obj.IsEmpty() && exports_obj->IsObject());

  auto module = RefImpl<Module>::make(store, module_obj);
  auto export_types = module->exports();
  auto exports = vec<Extern*>::make_uninitialized(export_types.size());
  if (!exports) return vec<Extern*>::invalid();

  for (size_t i = 0; i < export_types.size(); ++i) {
    auto& name = export_types[i]->name();
    auto maybe_name_obj = v8::String::NewFromUtf8(isolate, name.get(),
      v8::NewStringType::kNormal, name.size());
    if (maybe_name_obj.IsEmpty()) return vec<Extern*>::invalid();
    auto name_obj = maybe_name_obj.ToLocalChecked();
    auto obj = v8::Local<v8::Object>::Cast(
      exports_obj->Get(context, name_obj).ToLocalChecked());

    auto type = export_types[i]->type();
    switch (type->kind()) {
      case EXTERN_FUNC: {
        assert(wasm_v8::extern_kind(obj) == wasm_v8::EXTERN_FUNC);
        exports[i].reset(RefImpl<Func>::make(store, obj));
      } break;
      case EXTERN_GLOBAL: {
        assert(wasm_v8::extern_kind(obj) == wasm_v8::EXTERN_GLOBAL);
        exports[i].reset(RefImpl<Global>::make(store, obj));
      } break;
      case EXTERN_TABLE: {
        assert(wasm_v8::extern_kind(obj) == wasm_v8::EXTERN_TABLE);
        exports[i].reset(RefImpl<Table>::make(store, obj));
      } break;
      case EXTERN_MEMORY: {
        assert(wasm_v8::extern_kind(obj) == wasm_v8::EXTERN_MEMORY);
        exports[i].reset(RefImpl<Memory>::make(store, obj));
      } break;
    }
  }

  return exports;
}

///////////////////////////////////////////////////////////////////////////////

}  // namespace wasm
