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
//
// For vectors, `const wasm_xxx_vec_t` is used informally to indicate that
// neither the vector nor its elements should be modified.
// TODO: introduce proper `wasm_xxx_const_vec_t`?


#define WASM_DECLARE_OWN(name) \
  typedef struct wasm_##name##_t wasm_##name##_t; \
  \
  void wasm_##name##_delete(own wasm_##name##_t*);


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
  static inline wasm_##name##_vec_t wasm_##name##_vec_const(size_t size, const wasm_##name##_t ptr_or_none xs[]) { \
    wasm_##name##_vec_t v = {size, (wasm_##name##_t ptr_or_none*)xs}; \
    return v; \
  } \
  \
  static inline wasm_##name##_vec_t wasm_##name##_vec_empty() { \
    return wasm_##name##_vec(0, NULL); \
  } \
  \
  own wasm_##name##_vec_t wasm_##name##_vec_new_empty(); \
  own wasm_##name##_vec_t wasm_##name##_vec_new_uninitialized(size_t); \
  own wasm_##name##_vec_t wasm_##name##_vec_new(size_t, own wasm_##name##_t ptr_or_none const[]); \
  own wasm_##name##_vec_t wasm_##name##_vec_copy(wasm_##name##_vec_t); \
  void wasm_##name##_vec_delete(own wasm_##name##_vec_t);


// Byte vectors

typedef byte_t wasm_byte_t;
WASM_DECLARE_VEC(byte, )

typedef wasm_byte_vec_t wasm_name_t;

#define wasm_name wasm_byte_vec
#define wasm_name_new wasm_byte_vec_new
#define wasm_name_new_empty wasm_byte_vec_new_empty
#define wasm_name_new_new_uninitialized wasm_byte_vec_new_uninitialized
#define wasm_name_copy wasm_byte_vec_copy
#define wasm_name_delete wasm_byte_vec_delete

static inline own wasm_name_t wasm_name_new_from_string(const char* s) {
  return wasm_name_new(strlen(s), s);
}


///////////////////////////////////////////////////////////////////////////////
// Runtime Environment

// Configuration

WASM_DECLARE_OWN(config)

own wasm_config_t* wasm_config_new();

// Embedders may provide custom functions for manipulating configs.


// Engine

WASM_DECLARE_OWN(engine)

own wasm_engine_t* wasm_engine_new(int argc, const char* const argv[]);
own wasm_engine_t* wasm_engine_new_with_config(int argc, const char* const argv[], own wasm_config_t*);


// Store

WASM_DECLARE_OWN(store)

own wasm_store_t* wasm_store_new(wasm_engine_t*);


///////////////////////////////////////////////////////////////////////////////
// Type Representations

// Tyoe atributes

typedef enum wasm_mutability_t {
  WASM_CONST,
  WASM_VAR
} wasm_mutability_t;

typedef struct wasm_limits_t {
  uint32_t min;
  uint32_t max;
} wasm_limits_t;

static inline wasm_limits_t wasm_limits(uint32_t min, uint32_t max) {
  wasm_limits_t l = {min, max};
  return l;
}

static inline wasm_limits_t wasm_limits_no_max(uint32_t min) {
  return wasm_limits(min, 0xffffffff);
}


// Generic

#define WASM_DECLARE_TYPE(name) \
  WASM_DECLARE_OWN(name) \
  WASM_DECLARE_VEC(name, *) \
  \
  own wasm_##name##_t* wasm_##name##_copy(wasm_##name##_t*);


// Value Types

WASM_DECLARE_TYPE(valtype)

typedef enum wasm_valkind_t {
  WASM_I32_VAL,
  WASM_I64_VAL,
  WASM_F32_VAL,
  WASM_F64_VAL,
  WASM_ANYREF_VAL,
  WASM_FUNCREF_VAL
} wasm_valkind_t;

own wasm_valtype_t* wasm_valtype_new(wasm_valkind_t);

wasm_valkind_t wasm_valtype_kind(wasm_valtype_t*);

static inline bool wasm_valkind_is_num(wasm_valkind_t k) {
  return k < WASM_ANYREF_VAL;
}
static inline bool wasm_valkind_is_ref(wasm_valkind_t k) {
  return k >= WASM_ANYREF_VAL;
}

static inline bool wasm_valtype_is_num(wasm_valtype_t* t) {
  return wasm_valkind_is_num(wasm_valtype_kind(t));
}
static inline bool wasm_valtype_is_ref(wasm_valtype_t* t) {
  return wasm_valkind_is_ref(wasm_valtype_kind(t));
}


// Function Types

WASM_DECLARE_TYPE(functype)

own wasm_functype_t* wasm_functype_new(own wasm_valtype_vec_t params, own wasm_valtype_vec_t results);

const wasm_valtype_vec_t wasm_functype_params(const wasm_functype_t*);
const wasm_valtype_vec_t wasm_functype_results(const wasm_functype_t*);


// Global Types

WASM_DECLARE_TYPE(globaltype)

own wasm_globaltype_t* wasm_globaltype_new(own wasm_valtype_t*, wasm_mutability_t);

const wasm_valtype_t* wasm_globaltype_content(const wasm_globaltype_t*);
wasm_mutability_t wasm_globaltype_mutability(const wasm_globaltype_t*);


// Table Types

WASM_DECLARE_TYPE(tabletype)

own wasm_tabletype_t* wasm_tabletype_new(own wasm_valtype_t*, wasm_limits_t);

const wasm_valtype_t* wasm_tabletype_element(const wasm_tabletype_t*);
wasm_limits_t wasm_tabletype_limits(const wasm_tabletype_t*);


// Memory Types

WASM_DECLARE_TYPE(memorytype)

own wasm_memorytype_t* wasm_memorytype_new(wasm_limits_t);

wasm_limits_t wasm_memorytype_limits(const wasm_memorytype_t*);


// Extern Types

WASM_DECLARE_TYPE(externtype)

typedef enum wasm_externkind_t {
  WASM_EXTERN_FUNC, WASM_EXTERN_GLOBAL, WASM_EXTERN_TABLE, WASM_EXTERN_MEMORY
} wasm_externkind_t;

const wasm_externtype_t* wasm_functype_as_externtype(const wasm_functype_t*);
const wasm_externtype_t* wasm_globaltype_as_externtype(const wasm_globaltype_t*);
const wasm_externtype_t* wasm_tabletype_as_externtype(const wasm_tabletype_t*);
const wasm_externtype_t* wasm_memorytype_as_externtype(const wasm_memorytype_t*);

wasm_externkind_t wasm_externtype_kind(const wasm_externtype_t*);

const wasm_functype_t* wasm_externtype_as_functype(const wasm_externtype_t*);
const wasm_globaltype_t* wasm_externtype_as_globaltype(const wasm_externtype_t*);
const wasm_tabletype_t* wasm_externtype_as_tabletype(const wasm_externtype_t*);
const wasm_memorytype_t* wasm_externtype_as_memorytype(const wasm_externtype_t*);


// Import Types

WASM_DECLARE_TYPE(importtype)

own wasm_importtype_t* wasm_importtype_new(own wasm_name_t module, own wasm_name_t name, own wasm_externtype_t*);

const wasm_name_t wasm_importtype_module(const wasm_importtype_t*);
const wasm_name_t wasm_importtype_name(const wasm_importtype_t*);
const wasm_externtype_t* wasm_importtype_type(const wasm_importtype_t*);


// Export Types

WASM_DECLARE_TYPE(exporttype)

own wasm_exporttype_t* wasm_exporttype_new(own wasm_name_t, own wasm_externtype_t*);

const wasm_name_t wasm_exporttype_name(const wasm_exporttype_t*);
const wasm_externtype_t* wasm_exporttype_type(const wasm_exporttype_t*);


///////////////////////////////////////////////////////////////////////////////
// Runtime Objects

// Values

struct wasm_ref_t;

typedef struct wasm_val_t {
  wasm_valkind_t kind;
  union {
    int32_t i32;
    int64_t i64;
    float32_t f32;
    float64_t f64;
    struct wasm_ref_t* ref;
  };
} wasm_val_t;

void wasm_val_delete(own wasm_val_t v);
own wasm_val_t wasm_val_copy(wasm_val_t);

WASM_DECLARE_VEC(val, )


// References

#define WASM_DECLARE_REF_BASE(name) \
  WASM_DECLARE_OWN(name) \
  \
  own wasm_##name##_t* wasm_##name##_copy(const wasm_##name##_t*); \
  \
  void* wasm_##name##_get_host_info(const wasm_##name##_t*); \
  void wasm_##name##_set_host_info(wasm_##name##_t*, void*); \
  void wasm_##name##_set_host_info_with_finalizer(wasm_##name##_t*, void*, void (*)(void*));

#define WASM_DECLARE_REF(name) \
  WASM_DECLARE_REF_BASE(name) \
  \
  const wasm_ref_t* wasm_##name##_as_ref(const wasm_##name##_t*); \
  const wasm_##name##_t* wasm_ref_as_##name(const wasm_ref_t*);


WASM_DECLARE_REF_BASE(ref)


// Modules

WASM_DECLARE_REF(module)

own wasm_module_t* wasm_module_new(wasm_store_t*, const wasm_byte_vec_t binary);

bool wasm_module_validate(wasm_store_t*, const wasm_byte_vec_t binary);

own wasm_importtype_vec_t wasm_module_imports(const wasm_module_t*);
own wasm_exporttype_vec_t wasm_module_exports(const wasm_module_t*);

own wasm_byte_vec_t wasm_module_serialize(const wasm_module_t*);
own wasm_module_t* wasm_module_deserialize(const wasm_byte_vec_t);


// Foreign Objects

WASM_DECLARE_REF(foreign)

own wasm_foreign_t* wasm_foreign_new(wasm_store_t*);


// Function Instances

WASM_DECLARE_REF(func)

typedef own wasm_val_vec_t (*wasm_func_callback_t)(const wasm_val_vec_t);
typedef own wasm_val_vec_t (*wasm_func_callback_with_env_t)(void*, const wasm_val_vec_t);

own wasm_func_t* wasm_func_new(wasm_store_t*, const wasm_functype_t*, wasm_func_callback_t);
own wasm_func_t* wasm_func_new_with_env(wasm_store_t*, const wasm_functype_t* type, wasm_func_callback_with_env_t, wasm_ref_t* env, void (*finalizer)(void*));

own wasm_functype_t* wasm_func_type(const wasm_func_t*);

own wasm_val_vec_t wasm_func_call(const wasm_func_t*, const wasm_val_vec_t);


// Global Instances

WASM_DECLARE_REF(global)

own wasm_global_t* wasm_global_new(wasm_store_t*, const wasm_globaltype_t*, wasm_val_t);

own wasm_globaltype_t* wasm_global_type(const wasm_global_t*);

own wasm_val_t wasm_global_get(const wasm_global_t*);
void wasm_global_set(wasm_global_t*, wasm_val_t);


// Table Instances

WASM_DECLARE_REF(table)

typedef uint32_t wasm_table_size_t;

own wasm_table_t* wasm_table_new(wasm_store_t*, const wasm_tabletype_t*, wasm_ref_t*);

own wasm_tabletype_t* wasm_table_type(const wasm_table_t*);

own wasm_ref_t* wasm_table_get(const wasm_table_t*, wasm_table_size_t index);
void wasm_table_set(wasm_table_t*, wasm_table_size_t index, wasm_ref_t*);

wasm_table_size_t wasm_table_size(const wasm_table_t*);
wasm_table_size_t wasm_table_grow(wasm_table_t*, wasm_table_size_t delta);


// Memory Instances

WASM_DECLARE_REF(memory)

typedef uint32_t wasm_memory_pages_t;

static const size_t MEMORY_PAGE_SIZE = 0x10000;

own wasm_memory_t* wasm_memory_new(wasm_store_t*, const wasm_memorytype_t*);

own wasm_memorytype_t* wasm_memory_type(const wasm_memory_t*);

byte_t* wasm_memory_data(wasm_memory_t*);
size_t wasm_memory_data_size(const wasm_memory_t*);

wasm_memory_pages_t wasm_memory_size(const wasm_memory_t*);
wasm_memory_pages_t wasm_memory_grow(wasm_memory_t*, wasm_memory_pages_t delta);


// Externals

WASM_DECLARE_REF(extern)
WASM_DECLARE_VEC(extern, *)

const wasm_extern_t* wasm_func_as_extern(const wasm_func_t*);
const wasm_extern_t* wasm_global_as_extern(const wasm_global_t*);
const wasm_extern_t* wasm_table_as_extern(const wasm_table_t*);
const wasm_extern_t* wasm_memory_as_extern(const wasm_memory_t*);

wasm_externkind_t wasm_extern_kind(const wasm_extern_t*);

const wasm_func_t* wasm_extern_as_func(const wasm_extern_t*);
const wasm_global_t* wasm_extern_as_global(const wasm_extern_t*);
const wasm_table_t* wasm_extern_as_table(const wasm_extern_t*);
const wasm_memory_t* wasm_extern_as_memory(const wasm_extern_t*);


// Module Instances

WASM_DECLARE_REF(instance)

own wasm_instance_t* wasm_instance_new(wasm_store_t*, const wasm_module_t*, const wasm_extern_vec_t imports);

own wasm_extern_vec_t wasm_instance_exports(const wasm_instance_t*);


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
  return wasm_functype_new(wasm_valtype_vec_new_empty(), wasm_valtype_vec_new_empty());
}

static inline own wasm_functype_t* wasm_functype_new_1_0(own wasm_valtype_t* p) {
  wasm_valtype_t* ps[1] = {p};
  return wasm_functype_new(wasm_valtype_vec_new(1, ps), wasm_valtype_vec_new_empty());
}

static inline own wasm_functype_t* wasm_functype_new_2_0(own wasm_valtype_t* p1, own wasm_valtype_t* p2) {
  wasm_valtype_t* ps[2] = {p1, p2};
  return wasm_functype_new(wasm_valtype_vec_new(2, ps), wasm_valtype_vec_new_empty());
}

static inline own wasm_functype_t* wasm_functype_new_3_0(own wasm_valtype_t* p1, own wasm_valtype_t* p2, own wasm_valtype_t* p3) {
  wasm_valtype_t* ps[3] = {p1, p2, p3};
  return wasm_functype_new(wasm_valtype_vec_new(3, ps), wasm_valtype_vec_new_empty());
}

static inline own wasm_functype_t* wasm_functype_new_0_1(own wasm_valtype_t* r) {
  wasm_valtype_t* rs[1] = {r};
  return wasm_functype_new(wasm_valtype_vec_new_empty(), wasm_valtype_vec_new(1, rs));
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
  return wasm_functype_new(wasm_valtype_vec_new_empty(), wasm_valtype_vec_new(2, rs));
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
  return wasm_ref_val(NULL);
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


///////////////////////////////////////////////////////////////////////////////

#undef own

#ifdef __cplusplus
};  // extern "C"
#endif

#endif  // #ifdef __WASM_H
