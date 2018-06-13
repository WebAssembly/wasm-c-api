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
  borrowed(borrowed<T>&& that) : it(std::move(that.it)) {}
  ~borrowed() { it.release(); }
};

template<class T>
struct borrowed_vec {
  vec<T> it;
  borrowed_vec(vec<T>&& v) : it(std::move(v)) {}
  borrowed_vec(borrowed_vec<T>&& that) : it(std::move(that.it)) {}
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
  extern "C++" inline auto hide(const wasm::name* x) -> const wasm_##name##_t* { \
    return static_cast<const wasm_##name##_t*>(x); \
  } \
  extern "C++" inline auto reveal(wasm_##name##_t* x) -> wasm::name* { \
    return x; \
  } \
  extern "C++" inline auto reveal(const wasm_##name##_t* x) -> const wasm::name* { \
    return x; \
  } \
  extern "C++" inline auto get(own<wasm::name*>& x) -> wasm_##name##_t* { \
    return hide(x.get()); \
  } \
  extern "C++" inline auto get(const own<wasm::name*>& x) -> const wasm_##name##_t* { \
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
    static_assert(sizeof(wasm_##name##_t ptr_or_none) == sizeof(name ptr_or_none), "C/C++ incompatibility"); \
    return reinterpret_cast<wasm_##name##_t ptr_or_none*>(v); \
  } \
  extern "C++" inline auto hide(name ptr_or_none const* v) -> wasm_##name##_t ptr_or_none const* { \
    static_assert(sizeof(wasm_##name##_t ptr_or_none) == sizeof(name ptr_or_none), "C/C++ incompatibility"); \
    return reinterpret_cast<wasm_##name##_t ptr_or_none const*>(v); \
  } \
  extern "C++" inline auto reveal(wasm_##name##_t ptr_or_none* v) -> name ptr_or_none* { \
    static_assert(sizeof(wasm_##name##_t ptr_or_none) == sizeof(name ptr_or_none), "C/C++ incompatibility"); \
    return reinterpret_cast<name ptr_or_none*>(v); \
  } \
  extern "C++" inline auto reveal(wasm_##name##_t ptr_or_none const* v) -> name ptr_or_none const* { \
    static_assert(sizeof(wasm_##name##_t ptr_or_none) == sizeof(name ptr_or_none), "C/C++ incompatibility"); \
    return reinterpret_cast<name ptr_or_none const*>(v); \
  } \
  extern "C++" inline auto get(wasm::vec<name ptr_or_none>& v) -> wasm_##name##_vec_t { \
    return wasm_##name##_vec(v.size(), hide(v.get())); \
  } \
  extern "C++" inline auto get(const wasm::vec<name ptr_or_none>& v) -> const wasm_##name##_vec_t { \
    return wasm_##name##_vec(v.size(), const_cast<wasm_##name##_t ptr_or_none*>(hide(v.get()))); \
  } \
  extern "C++" inline auto release(wasm::vec<name ptr_or_none>&& v) -> wasm_##name##_vec_t { \
    return wasm_##name##_vec(v.size(), hide(v.release())); \
  } \
  extern "C++" inline auto adopt(wasm_##name##_vec_t v) -> wasm::vec<name ptr_or_none> { \
    return wasm::vec<name ptr_or_none>::adopt(v.size, reveal(v.data)); \
  } \
  extern "C++" inline auto borrow(wasm_##name##_vec_t v) -> borrowed_vec<name ptr_or_none> { \
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
      v2[i] = adopt(wasm_##name##_clone(v.data[i])); \
    } \
    return release(std::move(v2)); \
  } \
  \
  void wasm_##name##_vec_delete(wasm_##name##_vec_t v) { \
    if (v.data) { \
      for (size_t i = 0; i < v.size; ++i) { \
        if (!is_empty(v.data[i])) wasm_##name##_delete(v.data[i]); \
      } \
      delete[] reveal(v.data); \
    } \
  }

extern "C++" {
template<class T>
inline auto is_empty(T* p) -> bool { return !p; }
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

const wasm_valtype_vec_t wasm_functype_params(const wasm_functype_t* ft) {
  return get(ft->params());
}

const wasm_valtype_vec_t wasm_functype_results(const wasm_functype_t* ft) {
  return get(ft->results());
}


// Global Types

WASM_DEFINE_TYPE(globaltype)

wasm_globaltype_t* wasm_globaltype_new(wasm_valtype_t* content, wasm_mut_t mut) {
  return release(globaltype::make(adopt(content), reveal(mut)));
}

const wasm_valtype_t* wasm_globaltype_content(const wasm_globaltype_t* gt) {
  return get(gt->content());
}

wasm_mut_t wasm_globaltype_mut(const wasm_globaltype_t* gt) {
  return hide(gt->mut());
}


// Table Types

WASM_DEFINE_TYPE(tabletype)

wasm_tabletype_t* wasm_tabletype_new(wasm_valtype_t* elem, wasm_limits_t limits) {
  return release(tabletype::make(adopt(elem), reveal(limits)));
}

const wasm_valtype_t* wasm_tabletype_elem(const wasm_tabletype_t* tt) {
  return get(tt->element());
}

wasm_limits_t wasm_tabletype_limits(const wasm_tabletype_t* tt) {
  return hide(tt->limits());
}


// Memory Types

WASM_DEFINE_TYPE(memtype)

wasm_memtype_t* wasm_memtype_new(wasm_limits_t limits) {
  return release(memtype::make(reveal(limits)));
}

wasm_limits_t wasm_memtype_limits(const wasm_memtype_t* mt) {
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

const wasm_name_t wasm_importtype_module(const wasm_importtype_t* it) {
  return get(it->module());
}

const wasm_name_t wasm_importtype_name(const wasm_importtype_t* it) {
  return get(it->name());
}

const wasm_externtype_t* wasm_importtype_type(const wasm_importtype_t* it) {
  return get(it->type());
}


// Export Types

WASM_DEFINE_TYPE(exporttype)

wasm_exporttype_t* wasm_exporttype_new(wasm_name_t name, wasm_externtype_t* type) {
  return release(exporttype::make(adopt(name), adopt(type)));
}

const wasm_name_t wasm_exporttype_name(const wasm_exporttype_t* et) {
  return get(et->name());
}

const wasm_externtype_t* wasm_exporttype_type(const wasm_exporttype_t* et) {
  return get(et->type());
}


///////////////////////////////////////////////////////////////////////////////
// Runtime Values

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


// Values

extern "C++" {

inline auto is_empty(wasm_val_t v) -> bool {
 return !is_ref(reveal(v.kind)) || !v.ref;
}

inline auto hide(wasm::val v) -> wasm_val_t {
  wasm_val_t v2 = { hide(v.kind()) };
  switch (v.kind()) {
    case I32: v2.i32 = v.i32(); break;
    case I64: v2.i64 = v.i64(); break;
    case F32: v2.f32 = v.f32(); break;
    case F64: v2.f64 = v.f64(); break;
    case ANYREF:
    case FUNCREF: v2.ref = hide(v.ref()); break;
    default: assert(false);
  }
  return v2;
}

inline auto release(wasm::val v) -> wasm_val_t {
  wasm_val_t v2 = { hide(v.kind()) };
  switch (v.kind()) {
    case I32: v2.i32 = v.i32(); break;
    case I64: v2.i64 = v.i64(); break;
    case F32: v2.f32 = v.f32(); break;
    case F64: v2.f64 = v.f64(); break;
    case ANYREF:
    case FUNCREF: v2.ref = release(v.release_ref()); break;
    default: assert(false);
  }
  return v2;
}

inline auto adopt(wasm_val_t v) -> wasm::val {
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

struct borrowed_val {
  val it;
  borrowed_val(val&& v) : it(std::move(v)) {}
  borrowed_val(borrowed_val&& that) : it(std::move(that.it)) {}
  ~borrowed_val() { it.release_ref(); }
};

inline auto borrow(wasm_val_t v) -> borrowed_val {
  val v2;
  switch (reveal(v.kind)) {
    case I32: v2 = val(v.i32); break;
    case I64: v2 = val(v.i64); break;
    case F32: v2 = val(v.f32); break;
    case F64: v2 = val(v.f64); break;
    case ANYREF:
    case FUNCREF: v2 = val(adopt(v.ref)); break;
    default: assert(false);
  }
  return borrowed_val(std::move(v2));
}

}  // extern "C++"

WASM_DEFINE_VEC(val, )


void wasm_val_delete(wasm_val_t v) {
  if (is_ref(reveal(v.kind))) adopt(v.ref);
}

wasm_val_t wasm_val_clone(wasm_val_t v) {
  wasm_val_t v2 = v;
  if (is_ref(reveal(v.kind))) {
    v2.ref = release(v.ref->clone());
  }
  return v2;
}


///////////////////////////////////////////////////////////////////////////////
// Runtime Objects

// Modules


WASM_DEFINE_REF(module)

bool wasm_module_validate(wasm_store_t* store, wasm_byte_vec_t binary) {
  auto store_ = borrow(store);
  auto binary_ = borrow(binary);
  return module::validate(store_.it, binary_.it);
}

wasm_module_t* wasm_module_new(wasm_store_t* store, wasm_byte_vec_t binary) {
  auto store_ = borrow(store);
  auto binary_ = borrow(binary);
  return release(module::make(store_.it, binary_.it));
}


wasm_importtype_vec_t wasm_module_imports(wasm_module_t* module) {
  return release(reveal(module)->imports());
}

wasm_exporttype_vec_t wasm_module_exports(wasm_module_t* module) {
  return release(reveal(module)->exports());
}

wasm_byte_vec_t wasm_module_serialize(wasm_module_t* module) {
  return release(reveal(module)->serialize());
}

wasm_module_t* wasm_module_deserialize(wasm_byte_vec_t binary) {
  auto binary_ = borrow(binary);
  return release(module::deserialize(binary_.it));
}


// Host Objects

WASM_DEFINE_REF(hostobj)

wasm_hostobj_t* wasm_hostobj_new(wasm_store_t* store) {
  auto store_ = borrow(store);
  return release(hostobj::make(store_.it));
}


// Function Instances

WASM_DEFINE_REF(func)

extern "C++" {

vec<val> wasm_callback(void* env, const vec<val>& args) {
  auto f = reinterpret_cast<wasm_func_callback_t>(env);
  return adopt(f(get(args)));
}

using wasm_callback_env =
  std::tuple<wasm_func_callback_with_env_t, void*, void (*)(void*)>;

vec<val> wasm_callback_with_env(void* env, const vec<val>& args) {
  auto t = *static_cast<wasm_callback_env*>(env);
  return adopt(std::get<0>(t)(std::get<1>(t), get(args)));
}

void wasm_callback_env_finalizer(void* env) {
  auto t = static_cast<wasm_callback_env*>(env);
  std::get<2>(*t)(std::get<1>(*t));
  delete t;
}

}  // extern "C++"

wasm_func_t* wasm_func_new(wasm_store_t* store, wasm_functype_t* type, wasm_func_callback_t callback) {
  auto store_ = borrow(store);
  auto type_ = borrow(type);
  return release(func::make(store_.it, type_.it, wasm_callback, reinterpret_cast<void*>(callback)));
}

wasm_func_t *wasm_func_new_with_env(wasm_store_t* store, wasm_functype_t* type, wasm_func_callback_with_env_t callback, wasm_ref_t *env, void (*finalizer)(void*)) {
  auto store_ = borrow(store);
  auto type_ = borrow(type);
  auto env2 = new wasm_callback_env(callback, env);
  return release(func::make(store_.it, type_.it, wasm_callback_with_env, env2));
}

wasm_functype_t* wasm_func_type(wasm_func_t* func) {
  return release(func->type());
}

wasm_val_vec_t wasm_func_call(wasm_func_t* func, wasm_val_vec_t args) {
  auto func_ = borrow(func);
  auto args_ = borrow(args);
  return release(func_.it->call(args_.it));
}


// Global Instances

WASM_DEFINE_REF(global)

wasm_global_t* wasm_global_new(wasm_store_t* store, wasm_globaltype_t* type, wasm_val_t val) {
  auto store_ = borrow(store);
  auto type_ = borrow(type);
  auto val_ = borrow(val);
  return release(global::make(store_.it, type_.it, val_.it));
}

wasm_globaltype_t* wasm_global_type(wasm_global_t* global) {
  return release(global->type());
}

wasm_val_t wasm_global_get(wasm_global_t* global) {
  return release(global->get());
}

void wasm_global_set(wasm_global_t* global, wasm_val_t val) {
  auto val_ = borrow(val);
  global->set(val_.it);
}


// Table Instances

WASM_DEFINE_REF(table)

wasm_table_t* wasm_table_new(wasm_store_t* store, wasm_tabletype_t* type, wasm_ref_t* ref) {
  auto store_ = borrow(store);
  auto type_ = borrow(type);
  auto ref_ = borrow(ref);
  return release(table::make(store_.it, type_.it, ref_.it));
}

wasm_tabletype_t* wasm_table_type(wasm_table_t* table) {
  return release(table->type());
}

wasm_ref_t* wasm_table_get(wasm_table_t* table, wasm_table_size_t index) {
  return release(table->get(index));
}

void wasm_table_set(wasm_table_t* table, wasm_table_size_t index, wasm_ref_t* ref) {
  auto ref_ = borrow(ref);
  table->set(index, ref_.it);
}

wasm_table_size_t wasm_table_size(wasm_table_t* table) {
  return table->size();
}

wasm_table_size_t wasm_table_grow(wasm_table_t* table, wasm_table_size_t delta) {
  return table->grow(delta);
}


// Memory Instances

WASM_DEFINE_REF(memory)

wasm_memory_t* wasm_memory_new(wasm_store_t* store, wasm_memtype_t* type) {
  auto store_ = borrow(store);
  auto type_ = borrow(type);
  return release(memory::make(store_.it, type_.it));
}

wasm_memtype_t* wasm_memory_type(wasm_memory_t* memory) {
  return release(memory->type());
}

wasm_byte_t* wasm_memory_data(wasm_memory_t* memory) {
  return memory->data();
}

size_t wasm_memory_data_size(wasm_memory_t* memory) {
  return memory->data_size();
}

wasm_memory_pages_t wasm_memory_size(wasm_memory_t* memory) {
  return memory->size();
}

wasm_memory_pages_t wasm_memory_grow(wasm_memory_t* memory, wasm_memory_pages_t delta) {
  return memory->grow(delta);
}


// Externals

WASM_DEFINE_REF(external)
WASM_DEFINE_VEC(external, *)

wasm_external_t* wasm_func_as_external(wasm_func_t* func) {
  return hide(static_cast<wasm::external*>(reveal(func)));
}
wasm_external_t* wasm_global_as_external(wasm_global_t* global) {
  return hide(static_cast<wasm::external*>(reveal(global)));
}
wasm_external_t* wasm_table_as_external(wasm_table_t* table) {
  return hide(static_cast<wasm::external*>(reveal(table)));
}
wasm_external_t* wasm_memory_as_external(wasm_memory_t* memory) {
  return hide(static_cast<wasm::external*>(reveal(memory)));
}

wasm_externkind_t wasm_external_kind(wasm_external_t* external) {
  return hide(external->kind());
}

wasm_func_t* wasm_external_as_func(wasm_external_t* external) {
  return hide(external->func());
}
wasm_global_t* wasm_external_as_global(wasm_external_t* external) {
  return hide(external->global());
}
wasm_table_t* wasm_external_as_table(wasm_external_t* external) {
  return hide(external->table());
}
wasm_memory_t* wasm_external_as_memory(wasm_external_t* external) {
  return hide(external->memory());
}


// Module Instances

WASM_DEFINE_REF(instance)

wasm_instance_t* wasm_instance_new(wasm_store_t* store, wasm_module_t* module, wasm_external_vec_t imports) {
  auto store_ = borrow(store);
  auto module_ = borrow(module);
  auto imports_ = borrow(imports);
  return release(instance::make(store_.it, module_.it, imports_.it));
}

wasm_external_vec_t wasm_instance_exports(wasm_instance_t* instance) {
  return release(instance->exports());
}

}  // extern "C"
