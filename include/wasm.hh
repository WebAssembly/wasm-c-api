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

static_assert(sizeof(float) == sizeof(int32_t), "incompatible float type");
static_assert(sizeof(double) == sizeof(int64_t), "incompatible double type");
static_assert(sizeof(intptr_t) == sizeof(int32_t) ||
              sizeof(intptr_t) == sizeof(int64_t), "incompatible pointer type");

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
auto make_own(T x) { return own<T>(x); }

template<class T>
auto make_own(std::unique_ptr<T>& x) { return own<T*>(std::move(x)); }


// Vectors

template<class T>
struct vec_traits {
  static void construct(size_t size, T data[]) {}
  static void destruct(size_t size, T data[]) {}
  static void move(size_t size, T* data, T init[]) {
    for (size_t i = 0; i < size; ++i) data[i] = init[i];
  }
  static void clone(size_t size, T data[], T init[]) {
    for (size_t i = 0; i < size; ++i) data[i] = init[i];
  }

  using proxy = T&;
};

template<class T>
struct vec_traits<T*> {
  static void construct(size_t size, T* data[]) {
    for (size_t i = 0; i < size; ++i) data[i] = nullptr;
  }
  static void destruct(size_t size, T* data[]) {
    for (size_t i = 0; i < size; ++i) {
      if (data[i]) delete data[i];
    }
  }
  static void move(size_t size, T* data[], own<T*> init[]) {
    for (size_t i = 0; i < size; ++i) data[i] = init[i].release();
  }
  static void clone(size_t size, T* data[], T* init[]) {
    for (size_t i = 0; i < size; ++i) {
      if (init[i]) data[i] = init[i]->clone().release();
    }
  }

  class proxy {
    T*& elem_;
  public:
    proxy(T*& elem) : elem_(elem) {}
    auto operator=(own<T*>&& elem) -> proxy& {
      reset(std::move(elem));
      return *this;
    }
    void reset(own<T*>&& val = own<T*>()) {
      if (elem_) delete elem_;
      elem_ = val.release();
    }
    auto release() -> T* {
      auto elem = elem_;
      elem_ = nullptr;
      return elem;
    }
    auto move() -> own<T*> { return make_own(release()); }
    auto get() -> T* { return elem_; }
    auto operator->() -> T* { return elem_; }
  };
};


template<class T>
struct vec_impl {
  size_t size;
  T data[0];

  vec_impl(size_t size) : size(size) {}

  void* operator new(size_t base_size, size_t count) {
    return ::new(std::nothrow) byte_t[base_size + sizeof(T[count])];
  }
  void operator delete(void* p) {
    ::delete[] static_cast<byte_t*>(p);
  }
};


template<class T>
class vec {
  std::unique_ptr<vec_impl<T>> impl_;

  static inline auto empty() -> vec_impl<T>*;

  vec(size_t size) : impl_(size ? new(size) vec_impl<T>(size) : empty()) {}

public:
  vec() {}

  template<class U>
  vec(vec<U>&& that) : impl_(that.impl_.release()) {}

  ~vec() {
    if (impl_) {
      if (impl_.get() == empty()) {
        impl_.release();
      } else {
        vec_traits<T>::destruct(impl_->size, impl_->data);
      }
    }
  }

  template<class U>
  auto operator=(vec<U>&& that) -> vec& {
    impl_.reset(that.impl_.release());
    return *this;
  }

  auto size() const -> size_t {
    return impl_->size;
  }

  auto get() -> T* {
    return impl_->data;
  }

  template<class U>
  void reset(vec<U>& that) {
    impl_.reset(that.impl_.release());
  }

  operator bool() const {
    return bool(impl_);
  }

  auto operator[](size_t i) const -> typename vec_traits<T>::proxy {
    return typename vec_traits<T>::proxy(impl_->data[i]);
  }

  auto clone() -> vec<T> {
    auto v = vec(impl_->size);
    if (v) vec_traits<T>::clone(impl_->size, v.impl_->data, impl_->data);
    return v;
  }

  static auto make_uninitialized(size_t size = 0) -> vec<T> {
    auto v = vec(size);
    if (v) vec_traits<T>::construct(size, v.impl_->data);
    return v;
  }

  static auto make(size_t size, own<T> init[]) -> vec<T> {
    auto v = vec(size);
    if (v) vec_traits<T>::move(size, v.impl_->data, init);
    return v;
  }

  template<class... Ts>
  static auto make(Ts&&... args) -> vec<T> {
    own<T> data[] = { make_own(args)... };
    return make(sizeof...(Ts), data);
  }
};

extern vec_impl<void*>* empty_vec_impl;

template<class T>
inline auto vec<T>::empty() -> vec_impl<T>* {
  return reinterpret_cast<vec_impl<T>*>(empty_vec_impl);
}


///////////////////////////////////////////////////////////////////////////////
// Runtime Environment

// Configuration

class config {
public:
  config() = delete;
  ~config();
  void operator delete(void*);

  static auto make() -> own<config*>;

  // Embedders may provide custom methods for manipulating configs.
};


// Engine

class engine {
public:
  engine() = delete;
  ~engine();
  void operator delete(void*);

  static auto make(int argc, const char* const argv[], own<config*>&& = config::make()) -> own<engine*>;
};


// Store

class store {
public:
  store() = delete;
  ~store();
  void operator delete(void*);

  static auto make(own<engine*>&) -> own<store*>;
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
  void operator delete(void*);

  static auto make(valkind) -> own<valtype*>;
  auto clone() const -> own<valtype*>;

  auto kind() const -> valkind;
  auto is_num() const -> bool { return wasm::is_num(kind()); }
  auto is_ref() const -> bool { return wasm::is_ref(kind()); }
};


// External Types

enum externkind {
  EXTERN_FUNC, EXTERN_GLOBAL, EXTERN_TABLE, EXTERN_MEMORY
};

class functype;
class globaltype;
class tabletype;
class memtype;

class externtype {
public:
  externtype() = delete;
  ~externtype();
  void operator delete(void*);

  auto clone() const-> own<externtype*>;

  auto kind() const -> externkind;
  auto func() -> functype*;
  auto global() -> globaltype*;
  auto table() -> tabletype*;
  auto memory() -> memtype*;
};


// Function Types

enum class arrow { ARROW };

class functype : public externtype {
public:
  functype() = delete;
  ~functype();

  static auto make(
    vec<valtype*>&& params = vec<valtype*>::make(),
    vec<valtype*>&& results = vec<valtype*>::make()
  ) -> own<functype*>;

  auto clone() const -> own<functype*>;

  auto params() -> vec<valtype*>&;
  auto results() -> vec<valtype*>&;
};


// Global Types

class globaltype : public externtype {
public:
  globaltype() = delete;
  ~globaltype();

  static auto make(own<valtype*>&&, mut) -> own<globaltype*>;
  auto clone() const -> own<globaltype*>;

  auto content() -> own<valtype*>&;
  auto mut() const -> mut;
};


// Table Types

class tabletype : public externtype {
public:
  tabletype() = delete;
  ~tabletype();

  static auto make(own<valtype*>&&, limits) -> own<tabletype*>;
  auto clone() const -> own<tabletype*>;

  auto element() -> own<valtype*>&;
  auto limits() const -> limits;
};


// Memory Types

class memtype : public externtype {
public:
  memtype() = delete;
  ~memtype();

  static auto make(limits) -> own<memtype*>;
  auto clone() const -> own<memtype*>;

  auto limits() const -> limits;
};


// Import Types

using name = vec<byte_t>;

class importtype {
public:
  importtype() = delete;
  ~importtype();
  void operator delete(void*);

  static auto make(name&& module, name&& name, own<externtype*>&&) -> own<importtype*>;
  auto clone() const -> own<importtype*>;

  auto module() -> name&;
  auto name() -> name&;
  auto type() -> own<externtype*>&;
};


// Export Types

class exporttype {
public:
  exporttype() = delete;
  ~exporttype();
  void operator delete(void*);

  static auto make(name&& name, own<externtype*>&&) -> own<exporttype*>;
  auto clone() const -> own<exporttype*>;

  auto name() -> name&;
  auto type() -> own<externtype*>&;
};


///////////////////////////////////////////////////////////////////////////////
// Runtime Objects

// References

class ref {
public:
  ref() = delete;
  ~ref();
  void operator delete(void*);

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

  static auto validate(own<store*>&, size_t, const byte_t[]) -> bool;
  static auto make(own<store*>&, size_t, const byte_t[]) -> own<module*>;
  auto clone() const -> own<module*>;

  static auto validate(own<store*>& store, vec<byte_t>& binary) -> bool {
    return validate(store, binary.size(), binary.get());
  }
  static auto make(own<store*>& store, vec<byte_t>& binary) -> own<module*> {
    return make(store, binary.size(), binary.get());
  }

  auto imports() -> vec<importtype*>;
  auto exports() -> vec<exporttype*>;

  auto serialize() -> vec<byte_t>;
  static auto deserialize(vec<byte_t>&) -> own<module*>;
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

  using callback = auto (*)(vec<val>&) -> vec<val>;
  using callback_with_env = auto (*)(void*, vec<val>&) -> vec<val>;

  static auto make(own<store*>&, own<functype*>&, callback) -> own<func*>;
  static auto make(own<store*>&, own<functype*>&, callback_with_env, void*) -> own<func*>;
  auto clone() const -> own<func*>;

  auto type() const -> own<functype*>;
  auto call(vec<val>) const -> vec<val>;

  template<class... Args>
  auto call(Args... vals) const -> vec<val> {
    return call(vec<val>::make(vals...));
  }
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
  void set(size_t index, own<ref*>&);
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

  static auto make(own<store*>&, own<module*>&, vec<external*>&) -> own<instance*>;
  auto clone() const -> own<instance*>;

  auto exports() const -> vec<external*>;
};


///////////////////////////////////////////////////////////////////////////////

}  // namespave wasm

#endif  // #ifdef __WASM_HH
