// WebAssembly C API

#ifndef __WASM_H
#define __WASM_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <memory.h>
#include <assert.h>


#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////
// Auxiliaries

// Machine types

inline void assertions() {
  static_assert(sizeof(float) == sizeof(uint32_t), "incompatible float type");
  static_assert(sizeof(double) == sizeof(uint64_t), "incompatible double type");
  static_assert(sizeof(intptr_t) == sizeof(uint32_t) ||
                sizeof(intptr_t) == sizeof(uint64_t), "incompatible pointer type");
}

typedef char byte_t;
typedef float float32_t;
typedef double float64_t;


// Ownership markers

#define own


// Vectors

#define WASM_DECLARE_VEC(name, ptr_or_none) \
  typedef struct name##_vec_t { \
    size_t size; \
    name##_t ptr_or_none* data; \
  } name##_vec_t; \
  \
  inline name##_vec_t name##_vec(size_t size, name##_t ptr_or_none xs[]) { \
    name##_vec_t v = {size, xs}; \
    return v; \
  } \
  \
  inline name##_vec_t name##_vec_empty() { \
    return name##_vec(0, NULL); \
  } \
  \
  own name##_vec_t name##_vec_new(size_t, own name##_t ptr_or_none const[]); \
  own name##_vec_t name##_vec_new_uninitialized(size_t); \
  own name##_vec_t name##_vec_clone(name##_vec_t); \
  void name##_vec_delete(own name##_vec_t);

// An `own name##_vec_t` has `own name##_t ptr_or_none own* data`.


// Byte vectors

typedef byte_t wasm_byte_t;
WASM_DECLARE_VEC(wasm_byte, )

typedef wasm_byte_vec_t wasm_name_t;

#define wasm_name wasm_byte_vec
#define wasm_name_new wasm_byte_vec_new
#define wasm_name_clone wasm_byte_vec_clone
#define wasm_name_delete wasm_byte_vec_delete

inline own wasm_name_t wasm_name_new_from_string(const char* s) {
  return wasm_name_new(strlen(s), s);
}


///////////////////////////////////////////////////////////////////////////////
// Type Representations

// Value Types

typedef enum wasm_valkind_t {
  WASM_I32_VAL, WASM_I64_VAL, WASM_F32_VAL, WASM_F64_VAL,
  WASM_ANYREF_VAL, WASM_FUNCREF_VAL
} wasm_valkind_t;
typedef wasm_valkind_t wasm_numkind_t;
typedef wasm_valkind_t wasm_refkind_t;

inline bool wasm_valkind_is_numkind(wasm_valkind_t k) {
  return k < WASM_ANYREF_VAL;
}
inline bool wasm_valkind_is_refkind(wasm_valkind_t k) {
  return k >= WASM_ANYREF_VAL;
}


typedef struct wasm_valtype_t wasm_valtype_t;
typedef wasm_valtype_t wasm_numtype_t;
typedef wasm_valtype_t wasm_reftype_t;

own wasm_valtype_t* wasm_valtype_new(wasm_valkind_t);
own wasm_valtype_t* wasm_valtype_clone(wasm_valtype_t*);
void wasm_valtype_delete(own wasm_valtype_t*);

wasm_valkind_t wasm_valtype_kind(wasm_valtype_t*);

inline bool wasm_valtype_is_numtype(wasm_valtype_t* t) {
  return wasm_valkind_is_numkind(wasm_valtype_kind(t));
}
inline bool wasm_valtype_is_reftype(wasm_valtype_t* t) {
  return wasm_valkind_is_refkind(wasm_valtype_kind(t));
}

WASM_DECLARE_VEC(wasm_valtype, *)


// Tyoe atributes

typedef enum wasm_mut_t { WASM_CONST, WASM_VAR } wasm_mut_t;

typedef struct wasm_limits_t {
  size_t min;
  size_t max;
} wasm_limits_t;

inline wasm_limits_t wasm_limits(size_t min, size_t max) {
  wasm_limits_t l = {min, max};
  return l;
}

inline wasm_limits_t wasm_limits_no_max(size_t min) {
  return wasm_limits(min, SIZE_MAX);
}


// Function Types

typedef struct wasm_functype_t wasm_functype_t;

own wasm_functype_t* wasm_functype_new(own wasm_valtype_vec_t params, own wasm_valtype_vec_t results);
own wasm_functype_t* wasm_functype_clone(wasm_functype_t*);
void wasm_functype_delete(own wasm_functype_t*);

wasm_valtype_vec_t wasm_functype_params(wasm_functype_t*);
wasm_valtype_vec_t wasm_functype_results(wasm_functype_t*);

WASM_DECLARE_VEC(wasm_functype, *)


// Global Types

typedef struct wasm_globaltype_t wasm_globaltype_t;

own wasm_globaltype_t* wasm_globaltype_new(own wasm_valtype_t*, wasm_mut_t);
own wasm_globaltype_t* wasm_globaltype_clone(wasm_globaltype_t*);
void wasm_globaltype_delete(own wasm_globaltype_t*);

wasm_valtype_t* wasm_globaltype_content(wasm_globaltype_t*);
wasm_mut_t wasm_globaltype_mut(wasm_globaltype_t*);

WASM_DECLARE_VEC(wasm_globaltype, *)


// Table Types

typedef struct wasm_tabletype_t wasm_tabletype_t;

own wasm_tabletype_t* wasm_tabletype_new(own wasm_reftype_t*, wasm_limits_t);
own wasm_tabletype_t* wasm_tabletype_clone(wasm_tabletype_t*);
void wasm_tabletype_delete(own wasm_tabletype_t*);

wasm_reftype_t* wasm_tabletype_elem(wasm_tabletype_t*);
wasm_limits_t wasm_tabletype_limits(wasm_tabletype_t*);

WASM_DECLARE_VEC(wasm_tabletype, *)


// Memory Types

typedef struct wasm_memtype_t wasm_memtype_t;

own wasm_memtype_t* wasm_memtype_new(wasm_limits_t);
own wasm_memtype_t* wasm_memtype_clone(wasm_memtype_t*);
void wasm_memtype_delete(own wasm_memtype_t*);

wasm_limits_t wasm_memtype_limits(wasm_memtype_t*);

WASM_DECLARE_VEC(wasm_memtype, *)


// Extern Types

typedef enum wasm_externkind_t {
  WASM_EXTERN_FUNC, WASM_EXTERN_GLOBAL, WASM_EXTERN_TABLE, WASM_EXTERN_MEMORY
} wasm_externkind_t;

typedef struct wasm_externtype_t wasm_externtype_t;

wasm_externtype_t* wasm_functype_as_externtype(wasm_functype_t*);
wasm_externtype_t* wasm_globaltype_as_externtype(wasm_globaltype_t*);
wasm_externtype_t* wasm_tabletype_as_externtype(wasm_tabletype_t*);
wasm_externtype_t* wasm_memtype_as_externtype(wasm_memtype_t*);
wasm_functype_t* wasm_externtype_as_functype(wasm_externtype_t*);
wasm_globaltype_t* wasm_externtype_as_globaltype(wasm_externtype_t*);
wasm_tabletype_t* wasm_externtype_as_tabletype(wasm_externtype_t*);
wasm_memtype_t* wasm_externtype_as_memtype(wasm_externtype_t*);

own wasm_externtype_t* wasm_externtype_clone(wasm_externtype_t*);
void wasm_externtype_delete(own wasm_externtype_t*);

wasm_externkind_t wasm_externtype_kind(wasm_externtype_t*);

WASM_DECLARE_VEC(wasm_externtype, *)


// Import Types

typedef struct wasm_importtype_t wasm_importtype_t;

own wasm_importtype_t* wasm_importtype_new(wasm_name_t module, wasm_name_t name, own wasm_externtype_t*);
own wasm_importtype_t* wasm_importtype_clone(wasm_importtype_t*);
void wasm_importtype_delete(own wasm_importtype_t*);

wasm_name_t wasm_importtype_module(wasm_importtype_t*);
wasm_name_t wasm_importtype_name(wasm_importtype_t*);
wasm_externtype_t* wasm_importtype_type(wasm_importtype_t*);

WASM_DECLARE_VEC(wasm_importtype, *)


// Export Types

typedef struct wasm_exporttype_t wasm_exporttype_t;

own wasm_exporttype_t* wasm_exporttype_new(wasm_name_t, own wasm_externtype_t*);
own wasm_exporttype_t* wasm_exporttype_clone(wasm_exporttype_t*);
void wasm_exporttype_delete(own wasm_exporttype_t*);

wasm_name_t wasm_exporttype_name(wasm_exporttype_t*);
wasm_externtype_t* wasm_exporttype_type(wasm_exporttype_t*);

WASM_DECLARE_VEC(wasm_exporttype, *)


///////////////////////////////////////////////////////////////////////////////
// Runtime Environment

// Initialisation

typedef struct wasm_config_t wasm_config_t;

own wasm_config_t* wasm_config_new();
void wasm_config_delete(own wasm_config_t*);

// Embedders may provide custom functions for manipulating store_configs.

void wasm_init(int argc, const char* const argv[]);
void wasm_init_with_config(int argc, const char* const argv[], own wasm_config_t*);
void wasm_deinit();


// Store

typedef struct wasm_store_t wasm_store_t;

own wasm_store_t* wasm_store_new();
void wasm_store_delete(own wasm_store_t*);


///////////////////////////////////////////////////////////////////////////////
// Runtime Objects

// References

typedef struct wasm_ref_t wasm_ref_t;

void wasm_ref_delete(own wasm_ref_t*);

own wasm_ref_t* wasm_ref_null();

bool wasm_ref_is_null(wasm_ref_t*);


// Values

typedef struct wasm_val_t {
  wasm_valkind_t kind;
  union {
    uint32_t i32;
    uint64_t i64;
    float32_t f32;
    float64_t f64;
    wasm_ref_t* ref;
  };
} wasm_val_t;

inline void wasm_val_delete(own wasm_val_t v) {
  if (wasm_valkind_is_refkind(v.kind)) wasm_ref_delete(v.ref);
}

WASM_DECLARE_VEC(wasm_val, )


// Modules

typedef struct wasm_module_t wasm_module_t;

own wasm_module_t* wasm_module_new(wasm_store_t*, const wasm_byte_vec_t binary);
void wasm_module_delete(own wasm_module_t*);

bool wasm_module_validate(wasm_store_t*, const wasm_byte_vec_t binary);

wasm_importtype_vec_t wasm_module_imports(wasm_module_t*);
wasm_exporttype_vec_t wasm_module_exports(wasm_module_t*);

own wasm_byte_vec_t wasm_module_serialize(wasm_module_t*);
own wasm_module_t* wasm_module_deserialize(wasm_byte_vec_t);


// Host Objects

typedef struct wasm_hostobj_t wasm_hostobj_t;

own wasm_hostobj_t* wasm_hostobj_new(wasm_store_t*);
void wasm_hostobj_delete(own wasm_hostobj_t*);

own wasm_ref_t* wasm_hostobj_as_ref(own wasm_hostobj_t*);
own wasm_hostobj_t* wasm_ref_as_hostobj(own wasm_ref_t*);

void* wasm_hostobj_get_host_info(wasm_hostobj_t*);
void wasm_hostobj_set_host_info(wasm_hostobj_t*, void*);
void wasm_hostobj_set_host_info_with_finalizer(wasm_hostobj_t*, void*, void (*)(void*));


// Function Instances

typedef own wasm_val_vec_t (*wasm_func_callback_t)(wasm_val_vec_t);

typedef struct wasm_func_t wasm_func_t;

own wasm_func_t* wasm_func_new(wasm_store_t*, wasm_functype_t*, wasm_func_callback_t);
own wasm_func_t* wasm_func_new_with_env(wasm_store_t*, wasm_func_callback_t, own wasm_ref_t* env);
void wasm_func_delete(own wasm_func_t*);

wasm_ref_t* wasm_func_as_ref(wasm_func_t*);
wasm_func_t* wasm_ref_as_func(wasm_ref_t*);

wasm_functype_t* wasm_func_type(wasm_func_t*);

own wasm_val_vec_t wasm_func_call(wasm_func_t*, wasm_val_vec_t);

void* wasm_func_get_host_info(wasm_func_t*);
void wasm_func_set_host_info(wasm_func_t*, void*);
void wasm_func_set_host_info_with_finalizer(wasm_func_t*, void*, void (*)(void*));


// Global Instances

typedef struct wasm_global_t wasm_global_t;

own wasm_global_t* wasm_global_new(wasm_store_t*, wasm_globaltype_t*, wasm_val_t);
void wasm_global_delete(own wasm_global_t*);

wasm_ref_t* wasm_global_as_ref(wasm_global_t*);
wasm_global_t* wasm_ref_as_global(wasm_ref_t*);

wasm_globaltype_t* wasm_global_type(wasm_global_t*);

wasm_val_t wasm_global_get_val(wasm_global_t*);
void wasm_global_set_val(wasm_global_t*, wasm_val_t);

void* wasm_global_get_host_info(wasm_global_t*);
void wasm_global_set_host_info(wasm_global_t*, void*);
void wasm_global_set_host_info_with_finalizer(wasm_global_t*, void*, void (*)(own wasm_global_t*));


// Table Instances

typedef uint32_t wasm_table_size_t;
typedef struct wasm_table_t wasm_table_t;

own wasm_table_t* wasm_table_new(wasm_store_t*, wasm_tabletype_t*, own wasm_ref_t*);
void wasm_table_delete(own wasm_table_t*);

wasm_ref_t* wasm_table_as_ref(wasm_table_t*);
wasm_table_t* wasm_ref_as_table(wasm_ref_t*);

wasm_tabletype_t* wasm_table_get_type(wasm_table_t*);

wasm_ref_t* wasm_table_get_slot(wasm_table_t*, wasm_table_size_t index);
void wasm_table_set_slot(wasm_table_t*, wasm_table_size_t index, own wasm_ref_t*);

wasm_table_size_t wasm_table_size(wasm_table_t*);
wasm_table_size_t wasm_table_grow(wasm_table_t*, wasm_table_size_t delta);

void* wasm_table_get_host_info(wasm_table_t*);
void wasm_table_set_host_info(wasm_table_t*, void*);
void wasm_table_set_host_info_with_finalizer(wasm_table_t*, void*, void (*)(own wasm_table_t*));


// Memory Instances

typedef uint32_t wasm_memory_size_t;
typedef uint32_t wasm_memory_pages_t;
typedef struct wasm_memory_t wasm_memory_t;

static const size_t MEMORY_PAGE_SIZE = 0x10000;

own wasm_memory_t* wasm_memory_new(wasm_store_t*, wasm_memtype_t*);
void wasm_memory_delete(own wasm_memory_t*);

wasm_ref_t* wasm_memory_as_ref(wasm_memory_t*);
wasm_memory_t* wasm_ref_as_memory(wasm_ref_t*);

wasm_memtype_t* wasm_memory_type(wasm_memory_t*);

byte_t* wasm_memory_data(wasm_memory_t*);
size_t wasm_memory_data_size(wasm_memory_t*);

wasm_memory_pages_t wasm_memory_size(wasm_memory_t*);
wasm_memory_pages_t wasm_memory_grow(wasm_memory_t*, wasm_memory_pages_t delta);

void* wasm_memory_get_host_info(wasm_memory_t*);
void wasm_memory_set_host_info(wasm_memory_t*, void*);
void wasm_memory_set_host_info_with_finalizer(wasm_memory_t*, void*, void (*)(own wasm_memory_t*));


// Externals

typedef struct wasm_extern_t {
  wasm_externkind_t kind;
  union {
    wasm_func_t* func;
    wasm_global_t* global;
    wasm_table_t* table;
    wasm_memory_t* memory;
  };
} wasm_extern_t;

inline void wasm_extern_delete(own wasm_extern_t ex) {
  switch (ex.kind) {
    case WASM_EXTERN_FUNC: return wasm_func_delete(ex.func);
    case WASM_EXTERN_GLOBAL: return wasm_global_delete(ex.global);
    case WASM_EXTERN_TABLE: return wasm_table_delete(ex.table);
    case WASM_EXTERN_MEMORY: return wasm_memory_delete(ex.memory);
  }
}

WASM_DECLARE_VEC(wasm_extern, )


// Module Instances

typedef struct wasm_instance_t wasm_instance_t;

own wasm_instance_t* wasm_instance_new(wasm_store_t*, wasm_module_t*, wasm_extern_vec_t imports);
void wasm_instance_delete(own wasm_instance_t*);

own wasm_extern_t wasm_instance_export(wasm_instance_t*, size_t index);
own wasm_extern_vec_t wasm_instance_exports(wasm_instance_t*);

void* wasm_instance_get_host_info(wasm_instance_t*);
void wasm_instance_set_host_info(wasm_instance_t*, void*);
void wasm_instance_set_host_info_with_finalizer(wasm_instance_t*, void*, void (*)(own wasm_instance_t*));


///////////////////////////////////////////////////////////////////////////////
// Convenience

// Value Type construction short-hands

inline own wasm_valtype_t* wasm_valtype_new_i32() {
  return wasm_valtype_new(WASM_I32_VAL);
}
inline own wasm_valtype_t* wasm_valtype_new_i64() {
  return wasm_valtype_new(WASM_I64_VAL);
}
inline own wasm_valtype_t* wasm_valtype_new_f32() {
  return wasm_valtype_new(WASM_F32_VAL);
}
inline own wasm_valtype_t* wasm_valtype_new_f64() {
  return wasm_valtype_new(WASM_F64_VAL);
}

inline own wasm_valtype_t* wasm_valtype_new_anyref() {
  return wasm_valtype_new(WASM_ANYREF_VAL);
}
inline own wasm_valtype_t* wasm_valtype_new_funcref() {
  return wasm_valtype_new(WASM_FUNCREF_VAL);
}


// Function Types construction short-hands

inline own wasm_functype_t* wasm_functype_new_0_0() {
  return wasm_functype_new(wasm_valtype_vec_empty(), wasm_valtype_vec_empty());
}

inline own wasm_functype_t* wasm_functype_new_1_0(own wasm_valtype_t* p) {
  wasm_valtype_t* ps[1] = {p};
  return wasm_functype_new(wasm_valtype_vec_new(1, ps), wasm_valtype_vec_empty());
}

inline own wasm_functype_t* wasm_functype_new_2_0(own wasm_valtype_t* p1, own wasm_valtype_t* p2) {
  wasm_valtype_t* ps[2] = {p1, p2};
  return wasm_functype_new(wasm_valtype_vec_new(2, ps), wasm_valtype_vec_empty());
}

inline own wasm_functype_t* wasm_functype_new_3_0(own wasm_valtype_t* p1, own wasm_valtype_t* p2, own wasm_valtype_t* p3) {
  wasm_valtype_t* ps[3] = {p1, p2, p3};
  return wasm_functype_new(wasm_valtype_vec_new(3, ps), wasm_valtype_vec_empty());
}

inline own wasm_functype_t* wasm_functype_new_0_1(own wasm_valtype_t* r) {
  wasm_valtype_t* rs[1] = {r};
  return wasm_functype_new(wasm_valtype_vec_empty(), wasm_valtype_vec_new(1, rs));
}

inline own wasm_functype_t* wasm_functype_new_1_1(own wasm_valtype_t* p, own wasm_valtype_t* r) {
  wasm_valtype_t* ps[1] = {p};
  wasm_valtype_t* rs[1] = {r};
  return wasm_functype_new(wasm_valtype_vec_new(1, ps), wasm_valtype_vec_new(1, rs));
}

inline own wasm_functype_t* wasm_functype_new_2_1(own wasm_valtype_t* p1, own wasm_valtype_t* p2, own wasm_valtype_t* r) {
  wasm_valtype_t* ps[2] = {p1, p2};
  wasm_valtype_t* rs[1] = {r};
  return wasm_functype_new(wasm_valtype_vec_new(2, ps), wasm_valtype_vec_new(1, rs));
}

inline own wasm_functype_t* wasm_functype_new_3_1(own wasm_valtype_t* p1, own wasm_valtype_t* p2, own wasm_valtype_t* p3, own wasm_valtype_t* r) {
  wasm_valtype_t* ps[3] = {p1, p2, p3};
  wasm_valtype_t* rs[1] = {r};
  return wasm_functype_new(wasm_valtype_vec_new(3, ps), wasm_valtype_vec_new(1, rs));
}

inline own wasm_functype_t* wasm_functype_new_0_2(own wasm_valtype_t* r1, own wasm_valtype_t* r2) {
  wasm_valtype_t* rs[2] = {r1, r2};
  return wasm_functype_new(wasm_valtype_vec_empty(), wasm_valtype_vec_new(2, rs));
}

inline own wasm_functype_t* wasm_functype_new_1_2(own wasm_valtype_t* p, own wasm_valtype_t* r1, own wasm_valtype_t* r2) {
  wasm_valtype_t* ps[1] = {p};
  wasm_valtype_t* rs[2] = {r1, r2};
  return wasm_functype_new(wasm_valtype_vec_new(1, ps), wasm_valtype_vec_new(2, rs));
}

inline own wasm_functype_t* wasm_functype_new_2_2(own wasm_valtype_t* p1, own wasm_valtype_t* p2, own wasm_valtype_t* r1, own wasm_valtype_t* r2) {
  wasm_valtype_t* ps[2] = {p1, p2};
  wasm_valtype_t* rs[2] = {r1, r2};
  return wasm_functype_new(wasm_valtype_vec_new(2, ps), wasm_valtype_vec_new(2, rs));
}

inline own wasm_functype_t* wasm_functype_new_3_2(own wasm_valtype_t* p1, own wasm_valtype_t* p2, own wasm_valtype_t* p3, own wasm_valtype_t* r1, own wasm_valtype_t* r2) {
  wasm_valtype_t* ps[3] = {p1, p2, p3};
  wasm_valtype_t* rs[2] = {r1, r2};
  return wasm_functype_new(wasm_valtype_vec_new(3, ps), wasm_valtype_vec_new(2, rs));
}


// Value construction short-hands

inline wasm_val_t wasm_i32_val(uint32_t i32) {
  wasm_val_t v = {WASM_I32_VAL};
  v.i32 = i32;
  return v;
}

inline wasm_val_t wasm_i64_val(uint64_t i64) {
  wasm_val_t v = {WASM_I64_VAL};
  v.i64 = i64;
  return v;
}

inline wasm_val_t wasm_f32_val(float32_t f32) {
  wasm_val_t v = {WASM_F32_VAL};
  v.f32 = f32;
  return v;
}

inline wasm_val_t wasm_f64_val(float64_t f64) {
  wasm_val_t v = {WASM_F64_VAL};
  v.f64 = f64;
  return v;
}

inline wasm_val_t wasm_ref_val(wasm_ref_t* ref) {
  wasm_val_t v = {WASM_ANYREF_VAL};
  v.ref = ref;
  return v;
}

inline wasm_val_t wasm_ptr_val(void* p) {
#if UINTPTR_MAX == UINT32_MAX
  return wasm_i32_val((uintptr_t)p);
#elif UINTPTR_MAX == UINT64_MAX
  return wasm_i64_val((uintptr_t)p);
#endif
}

inline void* wasm_val_ptr(wasm_val_t v) {
#if UINTPTR_MAX == UINT32_MAX
  return (void*)(uintptr_t)v.i32;
#elif UINTPTR_MAX == UINT64_MAX
  return (void*)(uintptr_t)v.i64;
#endif
}


// Extern construction and release short-hands

inline wasm_extern_t wasm_extern_func(wasm_func_t* func) {
  wasm_extern_t v = {WASM_EXTERN_FUNC};
  v.func = func;
  return v;
}

inline wasm_extern_t wasm_extern_global(wasm_global_t* global) {
  wasm_extern_t v = {WASM_EXTERN_GLOBAL};
  v.global = global;
  return v;
}

inline wasm_extern_t wasm_extern_table(wasm_table_t* table) {
  wasm_extern_t v = {WASM_EXTERN_TABLE};
  v.table = table;
  return v;
}

inline wasm_extern_t wasm_extern_memory(wasm_memory_t* memory) {
  wasm_extern_t v = {WASM_EXTERN_MEMORY};
  v.memory = memory;
  return v;
}


///////////////////////////////////////////////////////////////////////////////

#undef own

#ifdef __cplusplus
};  // extern "C"
#endif

#endif  // #ifdef __WASM_H
