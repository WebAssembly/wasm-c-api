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
  T it;
  borrowed(T x) : it(x) {}
  borrowed(borrowed<T>&& that) : it(that.it) {}
  borrowed(const borrowed<T>&& that) : it(const_cast<T>(that.it)) {}
};

template<class T>
struct borrowed_vec {
  vec<T> it;
  borrowed_vec(vec<T>&& v) : it(std::move(v)) {}
  borrowed_vec(borrowed_vec<T>&& that) : it(std::move(that.it)) {}
  ~borrowed_vec() { it.release(); }
};

}  // extern "C++"


#define WASM_DEFINE_OWN(name, Name) \
  struct wasm_##name##_t : Name {}; \
  \
  void wasm_##name##_delete(wasm_##name##_t* x) { \
    delete x; \
  } \
  \
  extern "C++" inline auto hide(Name* x) -> wasm_##name##_t* { \
    return static_cast<wasm_##name##_t*>(x); \
  } \
  extern "C++" inline auto hide(const Name* x) -> const wasm_##name##_t* { \
    return static_cast<const wasm_##name##_t*>(x); \
  } \
  extern "C++" inline auto reveal(wasm_##name##_t* x) -> Name* { \
    return x; \
  } \
  extern "C++" inline auto reveal(const wasm_##name##_t* x) -> const Name* { \
    return x; \
  } \
  extern "C++" inline auto get(own<Name*>& x) -> wasm_##name##_t* { \
    return hide(x.get()); \
  } \
  extern "C++" inline auto get(const own<Name*>& x) -> const wasm_##name##_t* { \
    return hide(x.get()); \
  } \
  extern "C++" inline auto release(own<Name*>&& x) -> wasm_##name##_t* { \
    return hide(x.release()); \
  } \
  extern "C++" inline auto adopt(wasm_##name##_t* x) -> own<Name*> { \
    return make_own(x); \
  } \
  extern "C++" inline auto borrow(wasm_##name##_t* x) -> borrowed<Name*> { \
    return borrowed<Name*>(x); \
  } \
  extern "C++" inline auto borrow(const wasm_##name##_t* x) -> const borrowed<Name*> { \
    /* TODO: fishy? */ \
    return borrowed<Name*>(const_cast<wasm_##name##_t*>(x)); \
  }


// Vectors

#define WASM_DEFINE_VEC_BASE(name, Name, ptr_or_none) \
  extern "C++" inline auto hide(vec<Name ptr_or_none>& v) \
  -> wasm_##name##_vec_t* { \
    static_assert( \
      sizeof(wasm_##name##_vec_t) == sizeof(vec<Name>), \
      "C/C++ incompatibility" \
    ); \
    return reinterpret_cast<wasm_##name##_vec_t*>(&v); \
  } \
  extern "C++" inline auto hide(const vec<Name ptr_or_none>& v) \
  -> const wasm_##name##_vec_t* { \
    static_assert( \
      sizeof(wasm_##name##_vec_t) == sizeof(vec<Name>), \
      "C/C++ incompatibility" \
    ); \
    return reinterpret_cast<const wasm_##name##_vec_t*>(&v); \
  } \
  extern "C++" inline auto hide(Name ptr_or_none* v) \
  -> wasm_##name##_t ptr_or_none* { \
    static_assert( \
      sizeof(wasm_##name##_t ptr_or_none) == sizeof(Name ptr_or_none), \
      "C/C++ incompatibility" \
    ); \
    return reinterpret_cast<wasm_##name##_t ptr_or_none*>(v); \
  } \
  extern "C++" inline auto hide(Name ptr_or_none const* v) \
  -> wasm_##name##_t ptr_or_none const* { \
    static_assert( \
      sizeof(wasm_##name##_t ptr_or_none) == sizeof(Name ptr_or_none), \
      "C/C++ incompatibility" \
    ); \
    return reinterpret_cast<wasm_##name##_t ptr_or_none const*>(v); \
  } \
  extern "C++" inline auto reveal(wasm_##name##_t ptr_or_none* v) \
  -> Name ptr_or_none* { \
    static_assert( \
      sizeof(wasm_##name##_t ptr_or_none) == sizeof(Name ptr_or_none), \
      "C/C++ incompatibility" \
    ); \
    return reinterpret_cast<Name ptr_or_none*>(v); \
  } \
  extern "C++" inline auto reveal(wasm_##name##_t ptr_or_none const* v) \
  -> Name ptr_or_none const* { \
    static_assert( \
      sizeof(wasm_##name##_t ptr_or_none) == sizeof(Name ptr_or_none), \
      "C/C++ incompatibility" \
    ); \
    return reinterpret_cast<Name ptr_or_none const*>(v); \
  } \
  extern "C++" inline auto get(vec<Name ptr_or_none>& v) \
  -> wasm_##name##_vec_t { \
    wasm_##name##_vec_t v2 = { v.size(), hide(v.get()) }; \
    return v2; \
  } \
  extern "C++" inline auto get(const vec<Name ptr_or_none>& v) \
  -> const wasm_##name##_vec_t { \
    wasm_##name##_vec_t v2 = { \
      v.size(), const_cast<wasm_##name##_t ptr_or_none*>(hide(v.get())) }; \
    return v2; \
  } \
  extern "C++" inline auto release(vec<Name ptr_or_none>&& v) \
  -> wasm_##name##_vec_t { \
    wasm_##name##_vec_t v2 = { v.size(), hide(v.release()) }; \
    return v2; \
  } \
  extern "C++" inline auto adopt(wasm_##name##_vec_t* v) \
  -> vec<Name ptr_or_none> { \
    return vec<Name ptr_or_none>::adopt(v->size, reveal(v->data)); \
  } \
  extern "C++" inline auto borrow(const wasm_##name##_vec_t* v) \
  -> borrowed_vec<Name ptr_or_none> { \
    return borrowed_vec<Name ptr_or_none>( \
      vec<Name ptr_or_none>::adopt(v->size, reveal(v->data))); \
  } \
  \
  void wasm_##name##_vec_new_uninitialized( \
    wasm_##name##_vec_t* out, size_t size \
  ) { \
    *out = release(vec<Name ptr_or_none>::make_uninitialized(size)); \
  } \
  void wasm_##name##_vec_new_empty(wasm_##name##_vec_t* out) { \
    wasm_##name##_vec_new_uninitialized(out, 0); \
  }

// Vectors with no ownership management of elements
#define WASM_DEFINE_VEC_PLAIN(name, Name, ptr_or_none) \
  WASM_DEFINE_VEC_BASE(name, Name, ptr_or_none) \
  \
  void wasm_##name##_vec_new( \
    wasm_##name##_vec_t* out, \
    size_t size, \
    wasm_##name##_t ptr_or_none const data[] \
  ) { \
    auto v2 = vec<Name ptr_or_none>::make_uninitialized(size); \
    if (v2.size() != 0) { \
      memcpy(v2.get(), data, size * sizeof(wasm_##name##_t ptr_or_none)); \
    } \
    *out = release(std::move(v2)); \
  } \
  \
  void wasm_##name##_vec_copy( \
    wasm_##name##_vec_t* out, wasm_##name##_vec_t* v \
  ) { \
    wasm_##name##_vec_new(out, v->size, v->data); \
  } \
  \
  void wasm_##name##_vec_delete(wasm_##name##_vec_t* v) { \
    if (v->data) delete[] v->data; \
  }

// Vectors who own their elements
#define WASM_DEFINE_VEC(name, Name, ptr_or_none) \
  WASM_DEFINE_VEC_BASE(name, Name, ptr_or_none) \
  \
  void wasm_##name##_vec_new( \
    wasm_##name##_vec_t* out, \
    size_t size, \
    wasm_##name##_t ptr_or_none const data[] \
  ) { \
    auto v2 = vec<Name ptr_or_none>::make_uninitialized(size); \
    for (size_t i = 0; i < v2.size(); ++i) { \
      v2[i] = adopt(data[i]); \
    } \
    *out = release(std::move(v2)); \
  } \
  \
  void wasm_##name##_vec_copy( \
    wasm_##name##_vec_t* out, wasm_##name##_vec_t* v \
  ) { \
    auto v2 = vec<Name ptr_or_none>::make_uninitialized(v->size); \
    for (size_t i = 0; i < v2.size(); ++i) { \
      v2[i] = adopt(wasm_##name##_copy(v->data[i])); \
    } \
    *out = release(std::move(v2)); \
  } \
  \
  void wasm_##name##_vec_delete(wasm_##name##_vec_t* v) { \
    if (v->data) { \
      for (size_t i = 0; i < v->size; ++i) { \
        if (!is_empty(v->data[i])) wasm_##name##_delete(v->data[i]); \
      } \
      delete[] reveal(v->data); \
    } \
  }

extern "C++" {
template<class T>
inline auto is_empty(T* p) -> bool { return !p; }
}


// Byte vectors

using byte = byte_t;
WASM_DEFINE_VEC_PLAIN(byte, byte, )


///////////////////////////////////////////////////////////////////////////////
// Runtime Environment

// Configuration

WASM_DEFINE_OWN(config, Config)

wasm_config_t* wasm_config_new() {
  return release(Config::make());
}


// Engine

WASM_DEFINE_OWN(engine, Engine)

wasm_engine_t* wasm_engine_new(int argc, const char *const argv[]) {
  return release(Engine::make(argc, argv));
}

wasm_engine_t* wasm_engine_new_with_config(
  int argc, const char *const argv[], wasm_config_t* config
) {
  return release(Engine::make(argc, argv, adopt(config)));
}


// Stores

WASM_DEFINE_OWN(store, Store)

wasm_store_t* wasm_store_new(wasm_engine_t* engine) {
  auto engine_ = borrow(engine);
  return release(Store::make(engine_.it));
};


///////////////////////////////////////////////////////////////////////////////
// Type Representations

// Type attributes

extern "C++" inline auto hide(Mutability mutability) -> wasm_mutability_t {
  return static_cast<wasm_mutability_t>(mutability);
}

extern "C++" inline auto reveal(wasm_mutability_t mutability) -> Mutability {
  return static_cast<Mutability>(mutability);
}


extern "C++" inline auto hide(const Limits& limits) -> const wasm_limits_t* {
  return reinterpret_cast<const wasm_limits_t*>(&limits);
}

extern "C++" inline auto reveal(wasm_limits_t limits) -> Limits {
  return Limits(limits.min, limits.max);
}


extern "C++" inline auto hide(ValKind kind) -> wasm_valkind_t {
  return static_cast<wasm_valkind_t>(kind);
}

extern "C++" inline auto reveal(wasm_valkind_t kind) -> ValKind {
  return static_cast<ValKind>(kind);
}


extern "C++" inline auto hide(ExternKind kind) -> wasm_externkind_t {
  return static_cast<wasm_externkind_t>(kind);
}

extern "C++" inline auto reveal(wasm_externkind_t kind) -> ExternKind {
  return static_cast<ExternKind>(kind);
}



// Generic

#define WASM_DEFINE_TYPE(name, Name) \
  WASM_DEFINE_OWN(name, Name) \
  WASM_DEFINE_VEC(name, Name, *) \
  \
  wasm_##name##_t* wasm_##name##_copy(wasm_##name##_t* t) { \
    return release(t->copy()); \
  }


// Value Types

WASM_DEFINE_TYPE(valtype, ValType)

wasm_valtype_t* wasm_valtype_new(wasm_valkind_t k) {
  return release(ValType::make(reveal(k)));
}

wasm_valkind_t wasm_valtype_kind(const wasm_valtype_t* t) {
  return hide(t->kind());
}


// Function Types

WASM_DEFINE_TYPE(functype, FuncType)

wasm_functype_t* wasm_functype_new(
  wasm_valtype_vec_t* params, wasm_valtype_vec_t* results
) {
  return release(FuncType::make(adopt(params), adopt(results)));
}

const wasm_valtype_vec_t* wasm_functype_params(const wasm_functype_t* ft) {
  return hide(ft->params());
}

const wasm_valtype_vec_t* wasm_functype_results(const wasm_functype_t* ft) {
  return hide(ft->results());
}


// Global Types

WASM_DEFINE_TYPE(globaltype, GlobalType)

wasm_globaltype_t* wasm_globaltype_new(
  wasm_valtype_t* content, wasm_mutability_t mutability
) {
  return release(GlobalType::make(adopt(content), reveal(mutability)));
}

const wasm_valtype_t* wasm_globaltype_content(const wasm_globaltype_t* gt) {
  return hide(gt->content());
}

wasm_mutability_t wasm_globaltype_mutability(const wasm_globaltype_t* gt) {
  return hide(gt->mutability());
}


// Table Types

WASM_DEFINE_TYPE(tabletype, TableType)

wasm_tabletype_t* wasm_tabletype_new(
  wasm_valtype_t* element, const wasm_limits_t* limits
) {
  return release(TableType::make(adopt(element), reveal(*limits)));
}

const wasm_valtype_t* wasm_tabletype_element(const wasm_tabletype_t* tt) {
  return hide(tt->element());
}

const wasm_limits_t* wasm_tabletype_limits(const wasm_tabletype_t* tt) {
  return hide(tt->limits());
}


// Memory Types

WASM_DEFINE_TYPE(memorytype, MemoryType)

wasm_memorytype_t* wasm_memorytype_new(const wasm_limits_t* limits) {
  return release(MemoryType::make(reveal(*limits)));
}

const wasm_limits_t* wasm_memorytype_limits(const wasm_memorytype_t* mt) {
  return hide(mt->limits());
}


// Extern Types

WASM_DEFINE_TYPE(externtype, ExternType)

wasm_externkind_t wasm_externtype_kind(const wasm_externtype_t* et) {
  return hide(et->kind());
}

wasm_externtype_t* wasm_functype_as_externtype(wasm_functype_t* ft) {
  return hide(static_cast<ExternType*>(ft));
}
wasm_externtype_t* wasm_globaltype_as_externtype(wasm_globaltype_t* gt) {
  return hide(static_cast<ExternType*>(gt));
}
wasm_externtype_t* wasm_tabletype_as_externtype(wasm_tabletype_t* tt) {
  return hide(static_cast<ExternType*>(tt));
}
wasm_externtype_t* wasm_memorytype_as_externtype(wasm_memorytype_t* mt) {
  return hide(static_cast<ExternType*>(mt));
}

const wasm_externtype_t* wasm_functype_as_externtype_const(
  const wasm_functype_t* ft
) {
  return hide(static_cast<const ExternType*>(ft));
}
const wasm_externtype_t* wasm_globaltype_as_externtype_const(
  const wasm_globaltype_t* gt
) {
  return hide(static_cast<const ExternType*>(gt));
}
const wasm_externtype_t* wasm_tabletype_as_externtype_const(
  const wasm_tabletype_t* tt
) {
  return hide(static_cast<const ExternType*>(tt));
}
const wasm_externtype_t* wasm_memorytype_as_externtype_const(
  const wasm_memorytype_t* mt
) {
  return hide(static_cast<const ExternType*>(mt));
}

wasm_functype_t* wasm_externtype_as_functype(wasm_externtype_t* et) {
  return et->kind() == EXTERN_FUNC
    ? hide(static_cast<FuncType*>(reveal(et))) : nullptr;
}
wasm_globaltype_t* wasm_externtype_as_globaltype(wasm_externtype_t* et) {
  return et->kind() == EXTERN_GLOBAL
    ? hide(static_cast<GlobalType*>(reveal(et))) : nullptr;
}
wasm_tabletype_t* wasm_externtype_as_tabletype(wasm_externtype_t* et) {
  return et->kind() == EXTERN_TABLE
    ? hide(static_cast<TableType*>(reveal(et))) : nullptr;
}
wasm_memorytype_t* wasm_externtype_as_memorytype(wasm_externtype_t* et) {
  return et->kind() == EXTERN_MEMORY
    ? hide(static_cast<MemoryType*>(reveal(et))) : nullptr;
}

const wasm_functype_t* wasm_externtype_as_functype_const(
  const wasm_externtype_t* et
) {
  return et->kind() == EXTERN_FUNC
    ? hide(static_cast<const FuncType*>(reveal(et))) : nullptr;
}
const wasm_globaltype_t* wasm_externtype_as_globaltype_const(
  const wasm_externtype_t* et
) {
  return et->kind() == EXTERN_GLOBAL
    ? hide(static_cast<const GlobalType*>(reveal(et))) : nullptr;
}
const wasm_tabletype_t* wasm_externtype_as_tabletype_const(
  const wasm_externtype_t* et
) {
  return et->kind() == EXTERN_TABLE
    ? hide(static_cast<const TableType*>(reveal(et))) : nullptr;
}
const wasm_memorytype_t* wasm_externtype_as_memorytype_const(
  const wasm_externtype_t* et
) {
  return et->kind() == EXTERN_MEMORY
    ? hide(static_cast<const MemoryType*>(reveal(et))) : nullptr;
}


// Import Types

WASM_DEFINE_TYPE(importtype, ImportType)

wasm_importtype_t* wasm_importtype_new(
  wasm_name_t* module, wasm_name_t* name, wasm_externtype_t* type
) {
  return release(ImportType::make(adopt(module), adopt(name), adopt(type)));
}

const wasm_name_t* wasm_importtype_module(const wasm_importtype_t* it) {
  return hide(it->module());
}

const wasm_name_t* wasm_importtype_name(const wasm_importtype_t* it) {
  return hide(it->name());
}

const wasm_externtype_t* wasm_importtype_type(const wasm_importtype_t* it) {
  return hide(it->type());
}


// Export Types

WASM_DEFINE_TYPE(exporttype, ExportType)

wasm_exporttype_t* wasm_exporttype_new(
  wasm_name_t* name, wasm_externtype_t* type
) {
  return release(ExportType::make(adopt(name), adopt(type)));
}

const wasm_name_t* wasm_exporttype_name(const wasm_exporttype_t* et) {
  return hide(et->name());
}

const wasm_externtype_t* wasm_exporttype_type(const wasm_exporttype_t* et) {
  return hide(et->type());
}


///////////////////////////////////////////////////////////////////////////////
// Runtime Values

// References

#define WASM_DEFINE_REF_BASE(name, Name) \
  WASM_DEFINE_OWN(name, Name) \
  \
  wasm_##name##_t* wasm_##name##_copy(const wasm_##name##_t* t) { \
    return release(t->copy()); \
  } \
  \
  void* wasm_##name##_get_host_info(const wasm_##name##_t* r) { \
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

#define WASM_DEFINE_REF(name, Name) \
  WASM_DEFINE_REF_BASE(name, Name) \
  \
  const wasm_ref_t* wasm_##name##_as_ref(const wasm_##name##_t* r) { \
    return hide(static_cast<const Ref*>(reveal(r))); \
  } \
  const wasm_##name##_t* wasm_ref_as_##name(const wasm_ref_t* r) { \
    return hide(static_cast<const Name*>(reveal(r))); \
  }


WASM_DEFINE_REF_BASE(ref, Ref)


// Values

extern "C++" {

inline auto is_empty(wasm_val_t v) -> bool {
 return !is_ref(reveal(v.kind)) || !v.of.ref;
}

inline auto hide(Val v) -> wasm_val_t {
  wasm_val_t v2 = { hide(v.kind()) };
  switch (v.kind()) {
    case I32: v2.of.i32 = v.i32(); break;
    case I64: v2.of.i64 = v.i64(); break;
    case F32: v2.of.f32 = v.f32(); break;
    case F64: v2.of.f64 = v.f64(); break;
    case ANYREF:
    case FUNCREF: v2.of.ref = hide(v.ref()); break;
    default: assert(false);
  }
  return v2;
}

inline auto release(Val v) -> wasm_val_t {
  wasm_val_t v2 = { hide(v.kind()) };
  switch (v.kind()) {
    case I32: v2.of.i32 = v.i32(); break;
    case I64: v2.of.i64 = v.i64(); break;
    case F32: v2.of.f32 = v.f32(); break;
    case F64: v2.of.f64 = v.f64(); break;
    case ANYREF:
    case FUNCREF: v2.of.ref = release(v.release_ref()); break;
    default: assert(false);
  }
  return v2;
}

inline auto adopt(wasm_val_t v) -> Val {
  switch (reveal(v.kind)) {
    case I32: return Val(v.of.i32);
    case I64: return Val(v.of.i64);
    case F32: return Val(v.of.f32);
    case F64: return Val(v.of.f64);
    case ANYREF:
    case FUNCREF: return Val(adopt(v.of.ref));
    default: assert(false);
  }
}

struct borrowed_val {
  Val it;
  borrowed_val(Val&& v) : it(std::move(v)) {}
  borrowed_val(borrowed_val&& that) : it(std::move(that.it)) {}
  ~borrowed_val() { if (it.is_ref()) it.release_ref(); }
};

inline auto borrow(const wasm_val_t* v) -> borrowed_val {
  Val v2;
  switch (reveal(v->kind)) {
    case I32: v2 = Val(v->of.i32); break;
    case I64: v2 = Val(v->of.i64); break;
    case F32: v2 = Val(v->of.f32); break;
    case F64: v2 = Val(v->of.f64); break;
    case ANYREF:
    case FUNCREF: v2 = Val(adopt(v->of.ref)); break;
    default: assert(false);
  }
  return borrowed_val(std::move(v2));
}

}  // extern "C++"


WASM_DEFINE_VEC_BASE(val, Val, )

void wasm_val_vec_new(
  wasm_val_vec_t* out, size_t size, wasm_val_t const data[]
) {
  auto v2 = vec<Val>::make_uninitialized(size);
  for (size_t i = 0; i < v2.size(); ++i) {
    v2[i] = adopt(data[i]);
  }
  *out = release(std::move(v2));
}

void wasm_val_vec_copy(wasm_val_vec_t* out, wasm_val_vec_t* v) {
  auto v2 = vec<Val>::make_uninitialized(v->size);
  for (size_t i = 0; i < v2.size(); ++i) {
    wasm_val_t val;
    wasm_val_copy(&v->data[i], &val);
    v2[i] = adopt(val);
  }
  *out = release(std::move(v2));
}

void wasm_val_vec_delete(wasm_val_vec_t* v) {
  if (v->data) {
    for (size_t i = 0; i < v->size; ++i) {
      if (!is_empty(v->data[i])) wasm_val_delete(&v->data[i]);
    }
    delete[] reveal(v->data);
  }
}


void wasm_val_delete(wasm_val_t* v) {
  if (is_ref(reveal(v->kind))) adopt(v->of.ref);
}

void wasm_val_copy(wasm_val_t* out, const wasm_val_t* v) {
  *out = *v;
  if (is_ref(reveal(v->kind))) {
    out->of.ref = release(v->of.ref->copy());
  }
}


// Results

extern "C++" {

wasm_result_t release(Result result) {
  wasm_result_t r = { static_cast<wasm_result_kind_t>(result.kind()) };
  switch (result.kind()) {
    case Result::RETURN: r.of.vals = release(std::move(result.vals())); break;
    case Result::TRAP: r.of.trap = release(std::move(result.trap())); break;
  }
  return r;
}

Result adopt(wasm_result_t result) {
  switch (result.kind) {
    case WASM_RETURN: return Result(adopt(&result.of.vals));
    case WASM_TRAP: return Result(adopt(&result.of.trap));
  }
}

}  // extern "C++"


///////////////////////////////////////////////////////////////////////////////
// Runtime Objects

// Modules


WASM_DEFINE_REF(module, Module)

bool wasm_module_validate(wasm_store_t* store, const wasm_byte_vec_t* binary) {
  auto store_ = borrow(store);
  auto binary_ = borrow(binary);
  return Module::validate(store_.it, binary_.it);
}

wasm_module_t* wasm_module_new(
  wasm_store_t* store, const wasm_byte_vec_t* binary
) {
  auto store_ = borrow(store);
  auto binary_ = borrow(binary);
  return release(Module::make(store_.it, binary_.it));
}


void wasm_module_imports(
  const wasm_module_t* module, wasm_importtype_vec_t* out
) {
  *out = release(reveal(module)->imports());
}

void wasm_module_exports(
  const wasm_module_t* module, wasm_exporttype_vec_t* out
) {
  *out = release(reveal(module)->exports());
}

void wasm_module_serialize(const wasm_module_t* module, wasm_byte_vec_t* out) {
  *out = release(reveal(module)->serialize());
}

wasm_module_t* wasm_module_deserialize(const wasm_byte_vec_t* binary) {
  auto binary_ = borrow(binary);
  return release(Module::deserialize(binary_.it));
}


// Foreign Objects

WASM_DEFINE_REF(foreign, Foreign)

wasm_foreign_t* wasm_foreign_new(wasm_store_t* store) {
  auto store_ = borrow(store);
  return release(Foreign::make(store_.it));
}


// Function Instances

WASM_DEFINE_REF(func, Func)

extern "C++" {

Result wasm_callback(void* env, const vec<Val>& args) {
  auto f = reinterpret_cast<wasm_func_callback_t>(env);
  wasm_result_t result;
  f(hide(args), &result);
  return adopt(result);
}

using wasm_callback_env =
  std::tuple<wasm_func_callback_with_env_t, void*, void (*)(void*)>;

Result wasm_callback_with_env(void* env, const vec<Val>& args) {
  auto t = *static_cast<wasm_callback_env*>(env);
  wasm_result_t result;
  std::get<0>(t)(std::get<1>(t), hide(args), &result);
  return adopt(result);
}

void wasm_callback_env_finalizer(void* env) {
  auto t = static_cast<wasm_callback_env*>(env);
  std::get<2>(*t)(std::get<1>(*t));
  delete t;
}

}  // extern "C++"

wasm_func_t* wasm_func_new(
  wasm_store_t* store, const wasm_functype_t* type,
  wasm_func_callback_t callback
) {
  auto store_ = borrow(store);
  auto type_ = borrow(type);
  return release(Func::make(
    store_.it, type_.it, wasm_callback, reinterpret_cast<void*>(callback)));
}

wasm_func_t *wasm_func_new_with_env(
  wasm_store_t* store, const wasm_functype_t* type,
  wasm_func_callback_with_env_t callback, void *env, void (*finalizer)(void*)
) {
  auto store_ = borrow(store);
  auto type_ = borrow(type);
  auto env2 = new wasm_callback_env(callback, env);
  return release(Func::make(store_.it, type_.it, wasm_callback_with_env, env2));
}

wasm_functype_t* wasm_func_type(const wasm_func_t* func) {
  return release(func->type());
}

void wasm_func_call(
  const wasm_func_t* func, const wasm_val_vec_t* args, wasm_result_t* out
) {
  auto func_ = borrow(func);
  auto args_ = borrow(args);
  *out = release(func_.it->call(args_.it));
}


// Global Instances

WASM_DEFINE_REF(global, Global)

wasm_global_t* wasm_global_new(
  wasm_store_t* store, const wasm_globaltype_t* type, const wasm_val_t* val
) {
  auto store_ = borrow(store);
  auto type_ = borrow(type);
  auto val_ = borrow(val);
  return release(Global::make(store_.it, type_.it, val_.it));
}

wasm_globaltype_t* wasm_global_type(const wasm_global_t* global) {
  return release(global->type());
}

void wasm_global_get(const wasm_global_t* global, wasm_val_t* out) {
  *out = release(global->get());
}

void wasm_global_set(wasm_global_t* global, const wasm_val_t* val) {
  auto val_ = borrow(val);
  global->set(val_.it);
}


// Table Instances

WASM_DEFINE_REF(table, Table)

wasm_table_t* wasm_table_new(
  wasm_store_t* store, const wasm_tabletype_t* type, wasm_ref_t* ref
) {
  auto store_ = borrow(store);
  auto type_ = borrow(type);
  auto ref_ = borrow(ref);
  return release(Table::make(store_.it, type_.it, ref_.it));
}

wasm_tabletype_t* wasm_table_type(const wasm_table_t* table) {
  return release(table->type());
}

wasm_ref_t* wasm_table_get(const wasm_table_t* table, wasm_table_size_t index) {
  return release(table->get(index));
}

bool wasm_table_set(
  wasm_table_t* table, wasm_table_size_t index, wasm_ref_t* ref
) {
  auto ref_ = borrow(ref);
  return table->set(index, ref_.it);
}

wasm_table_size_t wasm_table_size(const wasm_table_t* table) {
  return table->size();
}

bool wasm_table_grow(wasm_table_t* table, wasm_table_size_t delta) {
  return table->grow(delta);
}


// Memory Instances

WASM_DEFINE_REF(memory, Memory)

wasm_memory_t* wasm_memory_new(
  wasm_store_t* store, const wasm_memorytype_t* type
) {
  auto store_ = borrow(store);
  auto type_ = borrow(type);
  return release(Memory::make(store_.it, type_.it));
}

wasm_memorytype_t* wasm_memory_type(const wasm_memory_t* memory) {
  return release(memory->type());
}

wasm_byte_t* wasm_memory_data(wasm_memory_t* memory) {
  return memory->data();
}

size_t wasm_memory_data_size(const wasm_memory_t* memory) {
  return memory->data_size();
}

wasm_memory_pages_t wasm_memory_size(const wasm_memory_t* memory) {
  return memory->size();
}

bool wasm_memory_grow(wasm_memory_t* memory, wasm_memory_pages_t delta) {
  return memory->grow(delta);
}


// Externals

WASM_DEFINE_REF(extern, Extern)
WASM_DEFINE_VEC(extern, Extern, *)

wasm_externkind_t wasm_extern_kind(const wasm_extern_t* external) {
  return hide(external->kind());
}
wasm_externtype_t* wasm_extern_type(const wasm_extern_t* external) {
  return release(external->type());
}

wasm_extern_t* wasm_func_as_extern(wasm_func_t* func) {
  return hide(static_cast<Extern*>(reveal(func)));
}
wasm_extern_t* wasm_global_as_extern(wasm_global_t* global) {
  return hide(static_cast<Extern*>(reveal(global)));
}
wasm_extern_t* wasm_table_as_extern(wasm_table_t* table) {
  return hide(static_cast<Extern*>(reveal(table)));
}
wasm_extern_t* wasm_memory_as_extern(wasm_memory_t* memory) {
  return hide(static_cast<Extern*>(reveal(memory)));
}

const wasm_extern_t* wasm_func_as_extern_const(const wasm_func_t* func) {
  return hide(static_cast<const Extern*>(reveal(func)));
}
const wasm_extern_t* wasm_global_as_extern_const(const wasm_global_t* global) {
  return hide(static_cast<const Extern*>(reveal(global)));
}
const wasm_extern_t* wasm_table_as_extern_const(const wasm_table_t* table) {
  return hide(static_cast<const Extern*>(reveal(table)));
}
const wasm_extern_t* wasm_memory_as_extern_const(const wasm_memory_t* memory) {
  return hide(static_cast<const Extern*>(reveal(memory)));
}

wasm_func_t* wasm_extern_as_func(wasm_extern_t* external) {
  return hide(external->func());
}
wasm_global_t* wasm_extern_as_global(wasm_extern_t* external) {
  return hide(external->global());
}
wasm_table_t* wasm_extern_as_table(wasm_extern_t* external) {
  return hide(external->table());
}
wasm_memory_t* wasm_extern_as_memory(wasm_extern_t* external) {
  return hide(external->memory());
}

const wasm_func_t* wasm_extern_as_func_const(const wasm_extern_t* external) {
  return hide(external->func());
}
const wasm_global_t* wasm_extern_as_global_const(const wasm_extern_t* external) {
  return hide(external->global());
}
const wasm_table_t* wasm_extern_as_table_const(const wasm_extern_t* external) {
  return hide(external->table());
}
const wasm_memory_t* wasm_extern_as_memory_const(const wasm_extern_t* external) {
  return hide(external->memory());
}


// Module Instances

WASM_DEFINE_REF(instance, Instance)

wasm_instance_t* wasm_instance_new(
  wasm_store_t* store,
  const wasm_module_t* module,
  const wasm_extern_vec_t* imports
) {
  auto store_ = borrow(store);
  auto module_ = borrow(module);
  auto imports_ = borrow(imports);
  return release(Instance::make(store_.it, module_.it, imports_.it));
}

void wasm_instance_exports(
  const wasm_instance_t* instance, wasm_extern_vec_t* out
) {
  *out = release(instance->exports());
}

}  // extern "C"
