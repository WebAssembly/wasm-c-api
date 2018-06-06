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


// Ownership

#define own

// The qualified `own` is used to indicate ownership of data in this API.
// It is intended to be interpreted similar to a `const` qualifier:
//
// - `own wasm_xxx_*` owns the pointed-to data
// - `own wasm_xxx_t` distributes to all fields of a struct or union `xxx`
// - `own wasm_xxx_vec_t` owns the vector as well as its elements(!)
// - an `own` function parameter passes ownership from caller to callee
// - an `own` function result passes ownership from callee to caller
//
// Own data is created by `wasm_xxx_new` functions and some others.
// It must be released with the corresponding `wasm_xxx_delete` function.
//
// Deleting a reference does not necessarily delete the underlying object,
// it merely indicates that this owner no longer uses it.


// Vectors

#define WASM_DECLARE_VEC(name, ptr_or_none) \
  typedef struct wasm_##name##_vec_t { \
    size_t size; \
    wasm_##name##_t ptr_or_none* data; \
  } wasm_##name##_vec_t; \
  \
  static inline wasm_##name##_vec_t wasm_##name##_vec(size_t size, wasm_##name##_t ptr_or_none xs[]) { \
    wasm_##name##_vec_t v = {size, xs}; \
    return v; \
  } \
  \
  static inline wasm_##name##_vec_t wasm_##name##_vec_empty() { \
    return wasm_##name##_vec(0, NULL); \
  } \
  \
  own wasm_##name##_vec_t wasm_##name##_vec_new(size_t, own wasm_##name##_t ptr_or_none const[]); \
  own wasm_##name##_vec_t wasm_##name##_vec_new_uninitialized(size_t); \
  own wasm_##name##_vec_t wasm_##name##_vec_clone(wasm_##name##_vec_t); \
  void wasm_##name##_vec_delete(own wasm_##name##_vec_t);


// Byte vectors

typedef byte_t wasm_byte_t;
WASM_DECLARE_VEC(byte, )

typedef wasm_byte_vec_t wasm_name_t;

#define wasm_name wasm_byte_vec
#define wasm_name_new wasm_byte_vec_new
#define wasm_name_clone wasm_byte_vec_clone
#define wasm_name_delete wasm_byte_vec_delete

static inline own wasm_name_t wasm_name_new_from_string(const char* s) {
  return wasm_name_new(strlen(s), s);
}


///////////////////////////////////////////////////////////////////////////////
// Type Representations

// Generic

#define WASM_DECLARE_TYPE(name) \
  typedef struct wasm_##name##_t wasm_##name##_t; \
  \
  void wasm_##name##_delete(own wasm_##name##_t*); \
  own wasm_##name##_t* wasm_##name##_clone(wasm_##name##_t*); \
  \
  WASM_DECLARE_VEC(name, *)


// Value Types

WASM_DECLARE_TYPE(valtype)

typedef wasm_valtype_t wasm_numtype_t;
typedef wasm_valtype_t wasm_reftype_t;

typedef enum wasm_valkind_t {
  WASM_I32_VAL, WASM_I64_VAL, WASM_F32_VAL, WASM_F64_VAL,
  WASM_ANYREF_VAL, WASM_FUNCREF_VAL
} wasm_valkind_t;

typedef wasm_valkind_t wasm_numkind_t;
typedef wasm_valkind_t wasm_refkind_t;

own wasm_valtype_t* wasm_valtype_new(wasm_valkind_t);

wasm_valkind_t wasm_valtype_kind(wasm_valtype_t*);

static inline bool wasm_valkind_is_numkind(wasm_valkind_t k) {
  return k < WASM_ANYREF_VAL;
}
static inline bool wasm_valkind_is_refkind(wasm_valkind_t k) {
  return k >= WASM_ANYREF_VAL;
}

static inline bool wasm_valtype_is_numtype(wasm_valtype_t* t) {
  return wasm_valkind_is_numkind(wasm_valtype_kind(t));
}
static inline bool wasm_valtype_is_reftype(wasm_valtype_t* t) {
  return wasm_valkind_is_refkind(wasm_valtype_kind(t));
}


// Tyoe atributes

typedef enum wasm_mut_t { WASM_CONST, WASM_VAR } wasm_mut_t;

typedef struct wasm_limits_t {
  size_t min;
  size_t max;
} wasm_limits_t;

static inline wasm_limits_t wasm_limits(size_t min, size_t max) {
  wasm_limits_t l = {min, max};
  return l;
}

static inline wasm_limits_t wasm_limits_no_max(size_t min) {
  return wasm_limits(min, SIZE_MAX);
}


// Function Types

WASM_DECLARE_TYPE(functype)

own wasm_functype_t* wasm_functype_new(own wasm_valtype_vec_t params, own wasm_valtype_vec_t results);

wasm_valtype_vec_t wasm_functype_params(wasm_functype_t*);
wasm_valtype_vec_t wasm_functype_results(wasm_functype_t*);


// Global Types

WASM_DECLARE_TYPE(globaltype)

own wasm_globaltype_t* wasm_globaltype_new(own wasm_valtype_t*, wasm_mut_t);

wasm_valtype_t* wasm_globaltype_content(wasm_globaltype_t*);
wasm_mut_t wasm_globaltype_mut(wasm_globaltype_t*);


// Table Types

WASM_DECLARE_TYPE(tabletype)

own wasm_tabletype_t* wasm_tabletype_new(own wasm_reftype_t*, wasm_limits_t);

wasm_reftype_t* wasm_tabletype_elem(wasm_tabletype_t*);
wasm_limits_t wasm_tabletype_limits(wasm_tabletype_t*);


// Memory Types

WASM_DECLARE_TYPE(memtype)

own wasm_memtype_t* wasm_memtype_new(wasm_limits_t);

wasm_limits_t wasm_memtype_limits(wasm_memtype_t*);


// Extern Types

WASM_DECLARE_TYPE(externtype)

typedef enum wasm_externkind_t {
  WASM_EXTERN_FUNC, WASM_EXTERN_GLOBAL, WASM_EXTERN_TABLE, WASM_EXTERN_MEMORY
} wasm_externkind_t;

wasm_externtype_t* wasm_functype_as_externtype(wasm_functype_t*);
wasm_externtype_t* wasm_globaltype_as_externtype(wasm_globaltype_t*);
wasm_externtype_t* wasm_tabletype_as_externtype(wasm_tabletype_t*);
wasm_externtype_t* wasm_memtype_as_externtype(wasm_memtype_t*);
wasm_functype_t* wasm_externtype_as_functype(wasm_externtype_t*);
wasm_globaltype_t* wasm_externtype_as_globaltype(wasm_externtype_t*);
wasm_tabletype_t* wasm_externtype_as_tabletype(wasm_externtype_t*);
wasm_memtype_t* wasm_externtype_as_memtype(wasm_externtype_t*);

wasm_externkind_t wasm_externtype_kind(wasm_externtype_t*);


// Import Types

WASM_DECLARE_TYPE(importtype)

own wasm_importtype_t* wasm_importtype_new(own wasm_name_t module, own wasm_name_t name, own wasm_externtype_t*);

wasm_name_t wasm_importtype_module(wasm_importtype_t*);
wasm_name_t wasm_importtype_name(wasm_importtype_t*);
wasm_externtype_t* wasm_importtype_type(wasm_importtype_t*);


// Export Types

WASM_DECLARE_TYPE(exporttype)

own wasm_exporttype_t* wasm_exporttype_new(own wasm_name_t, own wasm_externtype_t*);

wasm_name_t wasm_exporttype_name(wasm_exporttype_t*);
wasm_externtype_t* wasm_exporttype_type(wasm_exporttype_t*);


///////////////////////////////////////////////////////////////////////////////
// Runtime Environment

// Initialisation

typedef struct wasm_config_t wasm_config_t;

own wasm_config_t* wasm_config_new();
void wasm_config_delete(own wasm_config_t*);

// Embedders may provide custom functions for manipulating configs.

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
own wasm_ref_t* wasm_ref_clone(wasm_ref_t*);

own wasm_ref_t* wasm_ref_null();

bool wasm_ref_is_null(wasm_ref_t*);


#define WASM_DECLARE_REF(name) \
  typedef struct wasm_##name##_t wasm_##name##_t; \
  \
  void wasm_##name##_delete(own wasm_##name##_t*); \
  own wasm_##name##_t* wasm_##name##_clone(wasm_##name##_t*); \
  \
  wasm_ref_t* wasm_##name##_as_ref(wasm_##name##_t*); \
  wasm_##name##_t* wasm_ref_as_##name(wasm_ref_t*); \
  \
  void* wasm_##name##_get_host_info(wasm_##name##_t*); \
  void wasm_##name##_set_host_info(wasm_##name##_t*, void*); \
  void wasm_##name##_set_host_info_with_finalizer(wasm_##name##_t*, void*, void (*)(void*));


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

void wasm_val_delete(own wasm_val_t v);
own wasm_val_t wasm_val_clone(wasm_val_t);

WASM_DECLARE_VEC(val, )


// Modules

WASM_DECLARE_REF(module)

own wasm_module_t* wasm_module_new(wasm_store_t*, const wasm_byte_vec_t binary);

bool wasm_module_validate(wasm_store_t*, const wasm_byte_vec_t binary);

wasm_importtype_vec_t wasm_module_imports(wasm_module_t*);
wasm_exporttype_vec_t wasm_module_exports(wasm_module_t*);

own wasm_byte_vec_t wasm_module_serialize(wasm_module_t*);
own wasm_module_t* wasm_module_deserialize(wasm_byte_vec_t);


// Host Objects

WASM_DECLARE_REF(hostobj)

own wasm_hostobj_t* wasm_hostobj_new(wasm_store_t*);


// Function Instances

WASM_DECLARE_REF(func)

typedef own wasm_val_vec_t (*wasm_func_callback_t)(wasm_val_vec_t);
typedef own wasm_val_vec_t (*wasm_func_callback_with_env_t)(void*, wasm_val_vec_t);

own wasm_func_t* wasm_func_new(wasm_store_t*, wasm_functype_t*, wasm_func_callback_t);
own wasm_func_t* wasm_func_new_with_env(wasm_store_t*, wasm_functype_t* type, wasm_func_callback_with_env_t, own wasm_ref_t* env);

own wasm_functype_t* wasm_func_type(wasm_func_t*);

own wasm_val_vec_t wasm_func_call(wasm_func_t*, wasm_val_vec_t);


// Global Instances

WASM_DECLARE_REF(global)

own wasm_global_t* wasm_global_new(wasm_store_t*, wasm_globaltype_t*, wasm_val_t);

own wasm_globaltype_t* wasm_global_type(wasm_global_t*);

own wasm_val_t wasm_global_get(wasm_global_t*);
void wasm_global_set(wasm_global_t*, wasm_val_t);


// Table Instances

WASM_DECLARE_REF(table)

typedef uint32_t wasm_table_size_t;

own wasm_table_t* wasm_table_new(wasm_store_t*, wasm_tabletype_t*, wasm_ref_t*);

own wasm_tabletype_t* wasm_table_type(wasm_table_t*);

own wasm_ref_t* wasm_table_get(wasm_table_t*, wasm_table_size_t index);
void wasm_table_set(wasm_table_t*, wasm_table_size_t index, wasm_ref_t*);

wasm_table_size_t wasm_table_size(wasm_table_t*);
wasm_table_size_t wasm_table_grow(wasm_table_t*, wasm_table_size_t delta);


// Memory Instances

WASM_DECLARE_REF(memory)

typedef uint32_t wasm_memory_pages_t;

static const size_t MEMORY_PAGE_SIZE = 0x10000;

own wasm_memory_t* wasm_memory_new(wasm_store_t*, wasm_memtype_t*);

own wasm_memtype_t* wasm_memory_type(wasm_memory_t*);

byte_t* wasm_memory_data(wasm_memory_t*);
size_t wasm_memory_data_size(wasm_memory_t*);

wasm_memory_pages_t wasm_memory_size(wasm_memory_t*);
wasm_memory_pages_t wasm_memory_grow(wasm_memory_t*, wasm_memory_pages_t delta);


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

void wasm_extern_delete(own wasm_extern_t v);
own wasm_extern_t wasm_extern_clone(wasm_extern_t);

WASM_DECLARE_VEC(extern, )


// Module Instances

WASM_DECLARE_REF(instance)

own wasm_instance_t* wasm_instance_new(wasm_store_t*, wasm_module_t*, wasm_extern_vec_t imports);

own wasm_extern_t wasm_instance_export(wasm_instance_t*, size_t index);
own wasm_extern_vec_t wasm_instance_exports(wasm_instance_t*);


///////////////////////////////////////////////////////////////////////////////
// Convenience

// Value Type construction short-hands

static inline own wasm_valtype_t* wasm_valtype_new_i32() {
  return wasm_valtype_new(WASM_I32_VAL);
}
static inline own wasm_valtype_t* wasm_valtype_new_i64() {
  return wasm_valtype_new(WASM_I64_VAL);
}
static inline own wasm_valtype_t* wasm_valtype_new_f32() {
  return wasm_valtype_new(WASM_F32_VAL);
}
static inline own wasm_valtype_t* wasm_valtype_new_f64() {
  return wasm_valtype_new(WASM_F64_VAL);
}

static inline own wasm_valtype_t* wasm_valtype_new_anyref() {
  return wasm_valtype_new(WASM_ANYREF_VAL);
}
static inline own wasm_valtype_t* wasm_valtype_new_funcref() {
  return wasm_valtype_new(WASM_FUNCREF_VAL);
}


// Function Types construction short-hands

static inline own wasm_functype_t* wasm_functype_new_0_0() {
  return wasm_functype_new(wasm_valtype_vec_empty(), wasm_valtype_vec_empty());
}

static inline own wasm_functype_t* wasm_functype_new_1_0(own wasm_valtype_t* p) {
  wasm_valtype_t* ps[1] = {p};
  return wasm_functype_new(wasm_valtype_vec_new(1, ps), wasm_valtype_vec_empty());
}

static inline own wasm_functype_t* wasm_functype_new_2_0(own wasm_valtype_t* p1, own wasm_valtype_t* p2) {
  wasm_valtype_t* ps[2] = {p1, p2};
  return wasm_functype_new(wasm_valtype_vec_new(2, ps), wasm_valtype_vec_empty());
}

static inline own wasm_functype_t* wasm_functype_new_3_0(own wasm_valtype_t* p1, own wasm_valtype_t* p2, own wasm_valtype_t* p3) {
  wasm_valtype_t* ps[3] = {p1, p2, p3};
  return wasm_functype_new(wasm_valtype_vec_new(3, ps), wasm_valtype_vec_empty());
}

static inline own wasm_functype_t* wasm_functype_new_0_1(own wasm_valtype_t* r) {
  wasm_valtype_t* rs[1] = {r};
  return wasm_functype_new(wasm_valtype_vec_empty(), wasm_valtype_vec_new(1, rs));
}

static inline own wasm_functype_t* wasm_functype_new_1_1(own wasm_valtype_t* p, own wasm_valtype_t* r) {
  wasm_valtype_t* ps[1] = {p};
  wasm_valtype_t* rs[1] = {r};
  return wasm_functype_new(wasm_valtype_vec_new(1, ps), wasm_valtype_vec_new(1, rs));
}

static inline own wasm_functype_t* wasm_functype_new_2_1(own wasm_valtype_t* p1, own wasm_valtype_t* p2, own wasm_valtype_t* r) {
  wasm_valtype_t* ps[2] = {p1, p2};
  wasm_valtype_t* rs[1] = {r};
  return wasm_functype_new(wasm_valtype_vec_new(2, ps), wasm_valtype_vec_new(1, rs));
}

static inline own wasm_functype_t* wasm_functype_new_3_1(own wasm_valtype_t* p1, own wasm_valtype_t* p2, own wasm_valtype_t* p3, own wasm_valtype_t* r) {
  wasm_valtype_t* ps[3] = {p1, p2, p3};
  wasm_valtype_t* rs[1] = {r};
  return wasm_functype_new(wasm_valtype_vec_new(3, ps), wasm_valtype_vec_new(1, rs));
}

static inline own wasm_functype_t* wasm_functype_new_0_2(own wasm_valtype_t* r1, own wasm_valtype_t* r2) {
  wasm_valtype_t* rs[2] = {r1, r2};
  return wasm_functype_new(wasm_valtype_vec_empty(), wasm_valtype_vec_new(2, rs));
}

static inline own wasm_functype_t* wasm_functype_new_1_2(own wasm_valtype_t* p, own wasm_valtype_t* r1, own wasm_valtype_t* r2) {
  wasm_valtype_t* ps[1] = {p};
  wasm_valtype_t* rs[2] = {r1, r2};
  return wasm_functype_new(wasm_valtype_vec_new(1, ps), wasm_valtype_vec_new(2, rs));
}

static inline own wasm_functype_t* wasm_functype_new_2_2(own wasm_valtype_t* p1, own wasm_valtype_t* p2, own wasm_valtype_t* r1, own wasm_valtype_t* r2) {
  wasm_valtype_t* ps[2] = {p1, p2};
  wasm_valtype_t* rs[2] = {r1, r2};
  return wasm_functype_new(wasm_valtype_vec_new(2, ps), wasm_valtype_vec_new(2, rs));
}

static inline own wasm_functype_t* wasm_functype_new_3_2(own wasm_valtype_t* p1, own wasm_valtype_t* p2, own wasm_valtype_t* p3, own wasm_valtype_t* r1, own wasm_valtype_t* r2) {
  wasm_valtype_t* ps[3] = {p1, p2, p3};
  wasm_valtype_t* rs[2] = {r1, r2};
  return wasm_functype_new(wasm_valtype_vec_new(3, ps), wasm_valtype_vec_new(2, rs));
}


// Value construction short-hands

static inline own wasm_val_t wasm_i32_val(uint32_t i32) {
  wasm_val_t v = {WASM_I32_VAL};
  v.i32 = i32;
  return v;
}

static inline own wasm_val_t wasm_i64_val(uint64_t i64) {
  wasm_val_t v = {WASM_I64_VAL};
  v.i64 = i64;
  return v;
}

static inline own wasm_val_t wasm_f32_val(float32_t f32) {
  wasm_val_t v = {WASM_F32_VAL};
  v.f32 = f32;
  return v;
}

static inline own wasm_val_t wasm_f64_val(float64_t f64) {
  wasm_val_t v = {WASM_F64_VAL};
  v.f64 = f64;
  return v;
}

static inline own wasm_val_t wasm_ref_val(wasm_ref_t* ref) {
  wasm_val_t v = {WASM_ANYREF_VAL};
  v.ref = ref;
  return v;
}

static inline own wasm_val_t wasm_null_val() {
  return wasm_ref_val(wasm_ref_null());
}

static inline own wasm_val_t wasm_ptr_val(void* p) {
#if UINTPTR_MAX == UINT32_MAX
  return wasm_i32_val((uintptr_t)p);
#elif UINTPTR_MAX == UINT64_MAX
  return wasm_i64_val((uintptr_t)p);
#endif
}

static inline void* wasm_val_ptr(wasm_val_t v) {
#if UINTPTR_MAX == UINT32_MAX
  return (void*)(uintptr_t)v.i32;
#elif UINTPTR_MAX == UINT64_MAX
  return (void*)(uintptr_t)v.i64;
#endif
}


// Extern construction and release short-hands

static inline wasm_extern_t wasm_extern_func(wasm_func_t* func) {
  wasm_extern_t v = {WASM_EXTERN_FUNC};
  v.func = func;
  return v;
}

static inline wasm_extern_t wasm_extern_global(wasm_global_t* global) {
  wasm_extern_t v = {WASM_EXTERN_GLOBAL};
  v.global = global;
  return v;
}

static inline wasm_extern_t wasm_extern_table(wasm_table_t* table) {
  wasm_extern_t v = {WASM_EXTERN_TABLE};
  v.table = table;
  return v;
}

static inline wasm_extern_t wasm_extern_memory(wasm_memory_t* memory) {
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
