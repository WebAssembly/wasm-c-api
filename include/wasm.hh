// WebAssembly C++ API

#ifndef __WASM_HH
#define __WASM_HH

#include <cstddef>
#include <cstdint>
#include <memory>


///////////////////////////////////////////////////////////////////////////////
// Auxiliaries

// Machine types

static_assert(sizeof(float) == sizeof(uint32_t), "incompatible float type");
static_assert(sizeof(double) == sizeof(uint64_t), "incompatible double type");
static_assert(sizeof(intptr_t) == sizeof(uint32_t) ||
              sizeof(intptr_t) == sizeof(uint64_t), "incompatible pointer type");

using byte_t = char;
using float32_t = float;
using float64_t = double;


namespace wasm {

// Ownership

template<class T> struct ownership;
template<class T> struct ownership<byte_t> { using type = byte_t; };
template<class T> struct ownership<T*> { using type = std::unique_ptr<T>; };
template<class T> struct ownership<T[]> {
  using type = std::unique_ptr<typename ownership<T>::type[]>;
};

template<class T> using own = typename ownership<T>::type;


// Vectors

template<class T> struct own_vec;
template<class T> struct ownership<vec<T>> { using type = own_vec<T>; };

template<class T>
struct vec {
  const size_t size = 0;
  T* const data = nullptr;

  auto operator[](size_t i) const -> T& {
    return data[i];
  }

  auto clone() const -> own<vec<T>> {
    return make(size, data);
  }

  static auto make(size_t size) -> own<vec<T>> {
    return own<vec<T>>(vec(size, size == 0 ? nullptr : new T[size]));
  }

  static auto make(size_t size, const T data[]) -> own<vec<T>> {
    auto v = make(size);
    for (size_t i = 0; i < size; ++i) v[i] = data[i];
    return v;
  }

  template<class... Ts>
  static auto make(own<Ts>... args) -> own<vec<T>> {
    T data[] = {args.release()...};
    return make(sizeof...(Ts), data);
  }

  template<class U>
  static auto make(U& x) -> own<vec<T>> {
    return make(x.length(), x);
  }
};

template<>
inline auto vec<byte_t>::make(size_t size, const byte_t data[]) -> own<vec<byte_t>> {
  auto v = make(size);
  if (size != 0) memcpy(v.borrow().data, data, size);
  return v;
}

template<class T>
struct own_vec : vec<T> {
  ~own_vec() {
    for (size_t i = 0; i < this->s; ++i) delete this->data[i];
    delete[] this->data;
  }
};


///////////////////////////////////////////////////////////////////////////////
// Type Representations

// Tyoe atributes

enum class mut { CONST, VAR };

struct limits {
  size_t min;
  size_t max;

  limits(min, max = SIZE_MAX) : min(min), max(max) {}
};


// Value Types

enum valkind { I32, I64, F32, F64, ANYREF, FUNCREF };

inline bool is_num(valkind k) { return k < ANYREF; }
inline bool is_ref(valkind k) { return k >= ANYREF; }


class valtype {
public:
  valtype() = delete;
  ~valtype();

  static auto make(valkind) -> own<valtype*>;
  auto clone() const -> own<valtype*>;

  auto kind() const -> valkind;
  auto is_num() const -> bool { return wasm::is_num(kind()); }
  auto is_ref() const -> bool { return wasm::is_ref(kind()); }
};


// Function Types

enum class arrow { ARROW };

class functype {
public:
  functype() = delete;
  ~functype();

  static auto make(own<vec<valtype*>> params, own<vec<valtype*>> results) -> own<functype*>;

/*
  auto make() {
    return make(vec<valtype*>(), vec<valtype*>());
  }
  auto make(std::initializer_list<own<valtype*>> params) {
    return make(vec<valtype*>::make(params.size(),), vec<valtype*>());
  }
*/

  auto clone() const -> own<functype*>;

  auto params() const -> vec<valtype*>;
  auto results() const -> vec<valtype*>;
};


// Global Types

class globaltype {
public:
  globaltype() = delete;
  ~globaltype();

  static auto make(own<valtype*>, mut) -> own<globaltype*>;
  auto clone() const -> own<functype*>;

  auto content() const -> valtype*;
  auto mut() const -> mut;
};


// Table Types

class tabletype {
public:
  tabletype() = delete;
  ~tabletype();

  static auto make(own<valtype*>, limits) -> own<tabletype*>;
  auto clone() const -> own<tabletype*>;

  auto element() const -> valtype*;
  auto limits() const -> limits;
};


// Memory Types

class memtype {
public:
  memtype() = delete;
  ~memtype();

  static auto make(limits) -> own<memtype*>;
  auto clone() const -> own<memtype*>;

  auto limits() const -> limits;
};


// External Types

enum externkind {
  EXTERN_FUNC, EXTERN_GLOBAL, EXTERN_TABLE, EXTERN_MEMORY
};

class externtype {
public:
  externtype() = delete;
  ~externtype();

  static auto make(own<functype*>) -> own<externtype*>;
  static auto make(own<globaltype*>) -> own<externtype*>;
  static auto make(own<tabletype*>) -> own<externtype*>;
  static auto make(own<memtype*>) -> own<externtype*>;

  auto clone() const -> own<externtype*>;

  auto kind() const -> externkind;
  auto func() const -> functype*;
  auto global() const -> globaltype*;
  auto table() const -> tabletype*;
  auto memory() const -> memtype*;
};


// Import Types

using name = vec<byte_t>;

class importtype {
public:
  importtype() = delete;
  ~importtype();

  static auto make(own<name> module, own<name> name, own<externtype*>) -> own<importtype*>;
  auto clone() const -> own<importtype*>;

  auto module() const -> name;
  auto name() const -> name;
  auto type() const -> externtype*;
};


// Export Types

class exporttype {
public:
  exporttype() = delete;
  ~exporttype();

  static auto make(own<name> name, own<externtype*>) -> own<exporttype*>;
  auto clone() const -> own<exporttype*>;

  auto name() const -> name;
  auto type() const -> externtype*;
};


///////////////////////////////////////////////////////////////////////////////
// Runtime Environment

// Initialisation

class config {
public:
  config() = delete;
  ~config();

  static auto make() -> own<config*>;

  // Embedders may provide custom methods for manipulating configs.
};

void init(int argc, const char* const argv[], own<config*> = config::make());
void deinit();


// Store

class store {
public:
  store() = delete;
  ~store();

  static auto make() -> own<store*>;
};


///////////////////////////////////////////////////////////////////////////////
// Runtime Objects

// References

template<class T> struct own_ref;
class ref;
template<> struct ownership<ref*> { using type = own_ref<ref>; };

class ref {
public:
  ref() = delete;
  ~ref();

  auto clone() const -> own<ref*>;

  auto get_host_info() const -> void*;
  void set_host_info(void* info, void (*finalizer)(void*) = nullptr);
};


// Values

struct own_val;
template<> struct ownership<val> { using type = own_val; };

struct val {
  val() : kind_(ANYREF), ref_(nullptr) {}
  val(int32_t i) : kind_(I32), i32_(i) {}
  val(int64_t i) : kind_(I64), i64_(i) {}
  val(float32_t z) : kind_(F32), f32_(z) {}
  val(float64_t z) : kind_(F64), f64_(z) {}
  val(wasm::ref* r) : kind_(ANYREF), ref_(r) {}

  auto kind() const -> valkind { return kind_; }
  auto i32() const -> int32_t { return kind_ == I32 ? i32_ : 0; }
  auto i64() const -> int64_t { return kind_ == I64 ? i64_ : 0; }
  auto f32() const -> float32_t { return kind_ == F32 ? f32_ : 0; }
  auto f64() const -> float64_t { return kind_ == F64 ? f64_ : 0; }
  auto ref() const -> wasm::ref* { return is_ref(kind_) ? ref_ : nullptr; }

  auto clone() const -> own<val>;

private:
  const valkind kind_;
  union {
    const int32_t i32_;
    const int64_t i64_;
    const float32_t f32_;
    const float64_t f64_;
    wasm::ref* ref_;
  };
};

struct own_val : val {
  own_val(val& v) : val(v) {}
  ~own_val() { if (is_ref(kind_) && ref_ != nullptr) delete ref_; }
};


// Modules

class module;
template<> struct ownership<module*> { using type = own_ref<module>; };

class module : public ref {
public:
  module() = delete;
  ~module();

  using binary = vec<byte_t>;
  using serialized = vec<byte_t>;

  static auto make(store*, binary) -> own<module*>;
  static auto validate(binary) -> bool;

  auto imports() -> own<vec<importtype*>>;
  auto exports() -> own<vec<exporttype*>>;

  auto serialize() -> own<serialized>;
  static auto deserialize(serialized) -> own<module*>;
};


// Host Objects

class hostobj;
template<> struct ownership<hostobj*> { using type = own_ref<hostobj>; };

class hostobj : public ref {
public:
  hostobj() = delete;
  ~hostobj();

  static auto make(store*) -> own<hostobj*>;
};


// Externals

class external;
template<> struct ownership<external*> { using type = own_ref<external>; };

class external : public ref {
public:
  external() = delete;
  ~external();

  virtual auto kind() const -> externkind = 0;

  template<class T>
  inline auto to() -> T*;
};


// Function Instances

class func;
template<> struct ownership<func*> { using type = own_ref<func>; };

class func : public external {
public:
  func() = delete;
  ~func();

  using callback = auto (*)(vec<val>) -> own<vec<val>>;
  using callback_env = auto (*)(void*, vec<val>) -> own<vec<val>>;

  static auto make(store*, functype*, callback) -> own<func*>;
  static auto make(store*, functype*, callback_env, void*) -> own<func*>;

  auto kind() const -> externkind override { return EXTERN_FUNC; };
  auto type() const -> own<functype*>;
  auto call(vec<val>) const -> own<vec<val>>;
};


// Global Instances

class global;
template<> struct ownership<global*> { using type = own_ref<global>; };

class global : public external {
public:
  global() = delete;
  ~global();

  static auto make(store*, globaltype*, val) -> own<global*>;

  auto kind() const -> externkind override { return EXTERN_GLOBAL; };
  auto type() const -> own<globaltype*>;
  auto get() const -> own<val>;
  void set(val);
};


// Table Instances

class table;
template<> struct ownership<table*> { using type = own_ref<table>; };

class table : public external {
public:
  table() = delete;
  ~table();

  using size_t = uint32_t;

  static auto make(store*, tabletype*, ref*) -> own<table*>;

  auto kind() const -> externkind override { return EXTERN_TABLE; };
  auto type() const -> own<tabletype*>;
  auto get(size_t index) const -> own<ref*>;
  void set(size_t index, ref*);
  auto size() const -> size_t;
  auto grow(size_t delta) -> size_t;
};


// Memory Instances

class memory;
template<> struct ownership<memory*> { using type = own_ref<memory>; };

class memory : public external {
public:
  memory() = delete;
  ~memory();

  static auto make(store*, memtype*) -> own<memory*>;

  using pages_t = uint32_t;

  static const size_t page_size = 0x10000;

  auto kind() const -> externkind override { return EXTERN_MEMORY; };
  auto type() const -> own<memtype*>;
  auto data() const -> byte_t*;
  auto data_size() const -> size_t;
  auto size() const -> pages_t;
  auto grow(pages_t delta) -> pages_t;
};


// Module Instances

class instance;
template<> struct ownership<instance*> { using type = own_ref<instance>; };

class instance : public ref {
public:
  instance() = delete;
  ~instance();

  static auto make(store*, module*, vec<external*>) -> own<instance*>;

  auto exports() const -> own<vec<external*>>;
};


///////////////////////////////////////////////////////////////////////////////
// Convenience

// Value Type construction short-hands

inline own<valtype*> valtype_i32() { return valtype::make(I32); }
inline own<valtype*> valtype_i64() { return valtype::make(I64); }
inline own<valtype*> valtype_f32() { return valtype::make(F32); }
inline own<valtype*> valtype_f64() { return valtype::make(F64); }
inline own<valtype*> valtype_anyref() { return valtype::make(ANYREF); }
inline own<valtype*> valtype_funcref() { return valtype::make(FUNCREF); }


// Function Types construction short-hands

/*

// Value construction short-hands

inline own wasm_val_t wasm_null_val() {
  return wasm_ref_val(wasm_ref_null());
}

inline own wasm_val_t wasm_ptr_val(void* p) {
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

*/

///////////////////////////////////////////////////////////////////////////////

}  // namespave wasm

#endif  // #ifdef __WASM_HH
