// WebAssembly C++ API

#ifndef __WASM_HH
#define __WASM_HH

#include <cstddef>
#include <cstdint>
#include <memory>
#include <limits>


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

template<class T> struct owner { using type = T; };
template<class T> struct owner<T*> { using type = std::unique_ptr<T>; };
template<class T> struct owner<T[]> { using type = std::unique_ptr<T[]>; };

template<class T>
using own = typename owner<T>::type;

template<class T>
auto make_own(T t) { return own<T>(t); }


// Vectors

template<class T>
struct vec_traits {
  static void construct(size_t size, T data[]) {}
  static void destruct(size_t size, T data[]) {}
  static void copy(size_t size, T* data, const T init[]) {
    for (size_t i = 0; i < size; ++i) data[i] = init[i];
  }
  static void clone(size_t size, T data[]) {}
};

template<class T>
struct vec_traits<T*> {
  static void construct(size_t size, T* data[]) {
    for (size_t i = 0; i < size; ++i) data[i] = nullptr;
  }
  static void destruct(size_t size, T* data[]) {
    for (size_t i = 0; i < size; ++i) if (data[i]) delete data[i];
  }
  static void copy(size_t size, T* data[], T* const init[]) {
    for (size_t i = 0; i < size; ++i) data[i] = init[i];
  }
  static void clone(size_t size, T* data[]) {
    for (size_t i = 0; i < size; ++i) {
      if (data[i]) data[i] = data[i]->clone().release();
    }
  }
};


template<class T> struct vec;
template<class T> struct own_vec;
template<class T> struct owner<vec<T>> { using type = own_vec<T>; };


template<class T>
struct vec {
  size_t size;
  T* data;

  explicit vec(size_t size = 0, T data[] = nullptr) : size(size), data(data) {}

  operator bool() const {
    return bool(data);
  }

  auto operator[](size_t i) const -> T& {
    return data[i];
  }

  auto clone() -> own<vec<T>> {
    auto v = own_vec<T>::make(size, data);
    vec_traits<T>::clone(v.size, v.data.get());
    return v;
  }

  static auto make(size_t size = 0) -> own<vec<T>> {
    return own_vec<T>::make(size);
  }

  static auto make(size_t size, const T* data) -> own<vec<T>> {
    return own_vec<T>::make(size, data);
  }

  template<class... Ts>
  static auto make(own<Ts>... args) -> own<vec<T>> {
    T data[] = {args.release()...};
    return vec::make(sizeof...(Ts), data);
  }
};


template<class T>
class own_proxy {
  T& item;

public:
  own_proxy(T& item) : item(item) {}
  auto operator=(T val) -> own_proxy { item = val; return *this; }
  auto get() -> T { return item; }
};

template<class T>
class own_proxy<T*> {
  T*& item;

public:
  own_proxy(T*& item) : item(item) {}
  auto operator=(own<T*> val) -> own_proxy {
    if (item) delete item;
    item = std::move(val);
    return *this;
  }
  auto get() -> T* { return item; }
  auto operator->() -> T* { return item; }
};


template<class T>
struct own_vec {
  size_t size;
  own<T[]> data;

  own_vec() : size(0) {}

  template<class U>
  own_vec(own_vec<U>&& that) : size(that.size), data(that.data.release()) {
    that.size = 0;
  }

  ~own_vec() { vec_traits<T>::destruct(size, data.get()); }

  template<class U>
  auto operator=(own_vec<U>&& that) -> own_vec& {
    reset();
    size = that.size;
    data.reset(that.data.release());
    that.size = 0;
    return *this;
  }

  auto get() -> vec<T> {
    return vec<T>(size, data.get());
  }

  auto release() -> vec<T> {
    auto s = size;
    size = 0;
    return vec<T>(s, data.release());
  }

  void reset() {
    vec_traits<T>::destruct(size, data.get());
    size = 0;
    data.reset();
  }

  operator bool() const {
    return bool(data);
  }

  auto operator[](size_t i) const -> own_proxy<T> {
    return own_proxy<T>(data[i]);
  }

  auto clone() -> own<vec<T>> {
    auto v = own_vec<T>(size, data.get());
    vec_traits<T>::clone(size, v.data.get());
    return v;
  }

  static auto make(size_t size = 0) -> own_vec<T> {
    auto data = new(std::nothrow) T[size];
    if (!data) size = 0;
    vec_traits<T>::construct(size, data);
    return own_vec<T>(size, data);
  }

  static auto make(size_t size, const T* init) -> own<vec<T>> {
    auto data = new(std::nothrow) T[size];
    if (!data) size = 0;
    vec_traits<T>::copy(size, data, init);
    return own_vec<T>(size, data);
  }

private:
  own_vec(size_t size, T data[]) : size(size), data(data) {}
};


///////////////////////////////////////////////////////////////////////////////
// Type Representations

// Tyoe atributes

enum mut { CONST, VAR };

struct limits {
  uint32_t min;
  uint32_t max;

  limits(uint32_t min, uint32_t max = std::numeric_limits<uint32_t>::max()) :
    min(min), max(max) {}
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

  static auto make(
    own<vec<valtype*>>&& params = vec<valtype*>::make(),
    own<vec<valtype*>>&& results = vec<valtype*>::make()
  ) -> own<functype*>;

  auto clone() const -> own<functype*>;

  auto params() -> vec<valtype*>;
  auto results() -> vec<valtype*>;
};


// Global Types

class globaltype {
public:
  globaltype() = delete;
  ~globaltype();

  static auto make(own<valtype*>&&, mut) -> own<globaltype*>;
  auto clone() const -> own<globaltype*>;

  auto content() const -> valtype*;
  auto mut() const -> mut;
};


// Table Types

class tabletype {
public:
  tabletype() = delete;
  ~tabletype();

  static auto make(own<valtype*>&&, limits) -> own<tabletype*>;
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

  auto clone() -> own<externtype*>;

  auto kind() const -> externkind;
  auto func() -> functype*;
  auto global() -> globaltype*;
  auto table() -> tabletype*;
  auto memory() -> memtype*;
};


// Import Types

using name = vec<byte_t>;

class importtype {
public:
  importtype() = delete;
  ~importtype();

  static auto make(own<name>&& module, own<name>&& name, own<externtype*>&&) -> own<importtype*>;
  auto clone() const -> own<importtype*>;

  auto module() -> name;
  auto name() -> name;
  auto type() const -> externtype*;
};


// Export Types

class exporttype {
public:
  exporttype() = delete;
  ~exporttype();

  static auto make(own<name>&& name, own<externtype*>&&) -> own<exporttype*>;
  auto clone() const -> own<exporttype*>;

  auto name() -> name;
  auto type() const -> externtype*;
};


///////////////////////////////////////////////////////////////////////////////
// Runtime Environment

// Configuration

class config {
public:
  config() = delete;
  ~config();

  static auto make() -> own<config*>;

  // Embedders may provide custom methods for manipulating configs.
};


// Engine

class engine {
public:
  engine() = delete;
  ~engine();

  static auto make(int argc, const char* const argv[], own<config*>&& = config::make()) -> own<engine*>;
};


// Store

class store {
public:
  store() = delete;
  ~store();

  static auto make(own<engine*>&) -> own<store*>;
};


///////////////////////////////////////////////////////////////////////////////
// Runtime Objects

// References

class ref {
public:
  ref() = delete;
  ~ref();

  auto clone() const -> own<ref*>;

  auto get_host_info() const -> void*;
  void set_host_info(void* info, void (*finalizer)(void*) = nullptr);
};


// Values

class val {
  valkind kind_;
  union {
    int32_t i32_;
    int64_t i64_;
    float32_t f32_;
    float64_t f64_;
    wasm::ref* ref_;
  };

public:
  val() : kind_(ANYREF), ref_(nullptr) {}
  val(int32_t i) : kind_(I32), i32_(i) {}
  val(int64_t i) : kind_(I64), i64_(i) {}
  val(float32_t z) : kind_(F32), f32_(z) {}
  val(float64_t z) : kind_(F64), f64_(z) {}
  val(own<wasm::ref*>&& r) : kind_(ANYREF), ref_(r.release()) {}

  ~val() { if (is_ref(kind_) && ref_) delete ref_; }

  auto kind() const -> valkind { return kind_; }
  auto i32() const -> int32_t { return kind_ == I32 ? i32_ : 0; }
  auto i64() const -> int64_t { return kind_ == I64 ? i64_ : 0; }
  auto f32() const -> float32_t { return kind_ == F32 ? f32_ : 0; }
  auto f64() const -> float64_t { return kind_ == F64 ? f64_ : 0; }
  auto ref() const -> wasm::ref* { return is_ref(kind_) ? ref_ : nullptr; }

  auto clone() const -> val;
};


// Modules

class module : public ref {
public:
  module() = delete;
  ~module();

  using binary = vec<byte_t>;
  using serialized = vec<byte_t>;

  static auto validate(own<store*>&, binary) -> bool;
  static auto make(own<store*>&, binary) -> own<module*>;
  auto clone() const -> own<module*>;

  auto imports() -> own<vec<importtype*>>;
  auto exports() -> own<vec<exporttype*>>;

  auto serialize() -> own<serialized>;
  static auto deserialize(serialized) -> own<module*>;
};


// Host Objects

class hostobj : public ref {
public:
  hostobj() = delete;
  ~hostobj();

  static auto make(own<store*>&) -> own<hostobj*>;
  auto clone() const -> own<hostobj*>;
};


// Externals

class func;
class global;
class table;
class memory;

class external : public ref {
public:
  external() = delete;
  ~external();

  auto clone() const -> own<external*>;

  auto kind() const -> externkind;
  auto func() -> func*;
  auto global() -> global*;
  auto table() -> table*;
  auto memory() -> memory*;
};


// Function Instances

class func : public external {
public:
  func() = delete;
  ~func();

  using callback = auto (*)(vec<val>) -> own<vec<val>>;
  using callback_with_env = auto (*)(void*, vec<val>) -> own<vec<val>>;

  static auto make(own<store*>&, own<functype*>&, callback) -> own<func*>;
  static auto make(own<store*>&, own<functype*>&, callback_with_env, void*) -> own<func*>;
  auto clone() const -> own<func*>;

  auto type() const -> own<functype*>;
  auto call(vec<val>) const -> own<vec<val>>;
};


// Global Instances

class global : public external {
public:
  global() = delete;
  ~global();

  static auto make(own<store*>&, own<globaltype*>&, val) -> own<global*>;
  auto clone() const -> own<global*>;

  auto type() const -> own<globaltype*>;
  auto get() const -> val;
  void set(val);
};


// Table Instances

class table : public external {
public:
  table() = delete;
  ~table();

  using size_t = uint32_t;

  static auto make(own<store*>&, own<tabletype*>&, ref*) -> own<table*>;
  auto clone() const -> own<table*>;

  auto type() const -> own<tabletype*>;
  auto get(size_t index) const -> own<ref*>;
  void set(size_t index, ref*);
  auto size() const -> size_t;
  auto grow(size_t delta) -> size_t;
};


// Memory Instances

class memory : public external {
public:
  memory() = delete;
  ~memory();

  static auto make(own<store*>&, own<memtype*>&) -> own<memory*>;
  auto clone() const -> own<memory*>;

  using pages_t = uint32_t;

  static const size_t page_size = 0x10000;

  auto type() const -> own<memtype*>;
  auto data() const -> byte_t*;
  auto data_size() const -> size_t;
  auto size() const -> pages_t;
  auto grow(pages_t delta) -> pages_t;
};


// Module Instances

class instance : public ref {
public:
  instance() = delete;
  ~instance();

  static auto make(own<store*>&, own<module*>&, vec<external*>) -> own<instance*>;
  auto clone() const -> own<instance*>;

  auto exports() const -> own<vec<external*>>;
};


///////////////////////////////////////////////////////////////////////////////

}  // namespave wasm

#endif  // #ifdef __WASM_HH
