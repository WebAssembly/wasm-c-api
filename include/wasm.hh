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
auto make_own(T x) { return own<T>(std::move(x)); }

template<class T>
auto make_own(own<T*>& x) { return own<T*>(std::move(x)); }


// Vectors

template<class T>
struct vec_traits {
  static void construct(size_t size, T data[]) {}
  static void destruct(size_t size, T data[]) {}
  static void move(size_t size, T* data, T init[]) {
    for (size_t i = 0; i < size; ++i) data[i].reset(init[i]);
  }
  static void clone(size_t size, T data[], const T init[]) {
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
  static void clone(size_t size, T* data[], const T* const init[]) {
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
    auto get() const -> const T* { return elem_; }
    auto operator->() -> T* { return elem_; }
    auto operator->() const -> const T* { return elem_; }
  };
};


template<class T>
class vec {
  static const size_t invalid_size = SIZE_MAX;

  size_t size_;
  std::unique_ptr<T[]> data_;

  vec(size_t size) : vec(size, size ? new(std::nothrow) T[size] : nullptr) {}
  vec(size_t size, T* data) : size_(size), data_(data) {
    assert(!!size_ == !!data_ || size_ == invalid_size);
  }

public:
  template<class U>
  vec(vec<U>&& that) : vec(that.size_, that.data_.release()) {}

  ~vec() {
    if (data_) vec_traits<T>::destruct(size_, data_.get());
  }

  operator bool() const {
    return bool(size_ != invalid_size);
  }

  auto size() const -> size_t {
    return size_;
  }

  auto get() const -> const T* {
    return data_.get();
  }

  auto get() -> T* {
    return data_.get();
  }

  auto release() -> T* {
    return data_.release();
  }

  void reset(vec& that = vec()) {
    size_ = that.size_;
    data_.reset(that.data_.release());
  }

  auto operator=(vec&& that) -> vec& {
    reset(that);
    return *this;
  }

  auto operator[](size_t i) -> typename vec_traits<T>::proxy {
    return typename vec_traits<T>::proxy(data_[i]);
  }

  auto operator[](size_t i) const -> const typename vec_traits<T>::proxy {
    return typename vec_traits<T>::proxy(data_[i]);
  }

  auto clone() const -> vec {
    auto v = vec(size_);
    if (v) vec_traits<T>::clone(size_, v.data_.get(), data_.get());
    return v;
  }

  static auto make_uninitialized(size_t size = 0) -> vec {
    auto v = vec(size);
    if (v) vec_traits<T>::construct(size, v.data_.get());
    return v;
  }

  static auto make(size_t size, own<T> init[]) -> vec {
    auto v = vec(size);
    if (v) vec_traits<T>::move(size, v.data_.get(), init);
    return v;
  }

  template<class... Ts>
  static auto make(Ts&&... args) -> vec {
    own<T> data[] = { make_own(std::move(args))... };
    return make(sizeof...(Ts), data);
  }

  static auto adopt(size_t size, T data[]) -> vec {
    return vec(size, data);
  }

  static auto invalid() -> vec {
    return vec(invalid_size, nullptr);
  }
};


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

  auto func() const -> const functype*;
  auto global() const -> const globaltype*;
  auto table() const -> const tabletype*;
  auto memory() const -> const memtype*;
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
  union impl {
    int32_t i32;
    int64_t i64;
    float32_t f32;
    float64_t f64;
    wasm::ref* ref;
  } impl_;

  val(valkind kind, impl impl) : kind_(kind), impl_(impl) {}

public:
  val() : kind_(ANYREF) { impl_.ref = nullptr; }
  val(int32_t i) : kind_(I32) { impl_.i32 = i; }
  val(int64_t i) : kind_(I64) { impl_.i64 = i; }
  val(float32_t z) : kind_(F32) { impl_.f32 = z; }
  val(float64_t z) : kind_(F64) { impl_.f64 = z; }
  val(own<wasm::ref*>&& r) : kind_(ANYREF) { impl_.ref = r.release(); }

  val(val&& that) : kind_(that.kind_), impl_(that.impl_) {
    if (is_ref(kind_)) that.impl_.ref = nullptr;
  }

  ~val() {
   reset(); }

  void reset() {
    if (is_ref(kind_) && impl_.ref) {
      delete impl_.ref;
      impl_.ref = nullptr;
    }
  }

  void reset(val& that) {
    reset();
    kind_ = that.kind_;
    impl_ = that.impl_;
    if (is_ref(kind_)) that.impl_.ref = nullptr;
  }

  auto operator=(val&& that) -> val& {
    reset(that);
    return *this;
  } 

  auto kind() const -> valkind { return kind_; }
  auto i32() const -> int32_t { assert(kind_ == I32); return impl_.i32; }
  auto i64() const -> int64_t { assert(kind_ == I64); return impl_.i64; }
  auto f32() const -> float32_t { assert(kind_ == F32); return impl_.f32; }
  auto f64() const -> float64_t { assert(kind_ == F64); return impl_.f64; }
  auto ref() const -> wasm::ref* { assert(is_ref(kind_) || 1); return impl_.ref; }

  auto clone() const -> val {
    if (is_ref(kind_) && impl_.ref != nullptr) {
      impl impl = {.ref = impl_.ref->clone().release()};
      return val(kind_, impl);
    } else {
      return val(kind_, impl_);
    }
  }

  auto release_ref() -> own<wasm::ref*> {
    if (!is_ref(kind_)) return own<wasm::ref*>();
    auto ref = impl_.ref;
    ref = nullptr;
    return own<wasm::ref*>(ref);
  }
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

  auto func() -> wasm::func*;
  auto global() -> wasm::global*;
  auto table() -> wasm::table*;
  auto memory() -> wasm::memory*;

  auto func() const -> const wasm::func*;
  auto global() const -> const wasm::global*;
  auto table() const -> const wasm::table*;
  auto memory() const -> const wasm::memory*;
};


// Function Instances

class func : public external {
public:
  func() = delete;
  ~func();

  using callback = auto (*)(const vec<val>&) -> vec<val>;
  using callback_with_env = auto (*)(void*, const vec<val>&) -> vec<val>;

  static auto make(own<store*>&, own<functype*>&, callback) -> own<func*>;
  static auto make(own<store*>&, own<functype*>&, callback_with_env, void*, void (*finalizer)(void*) = nullptr) -> own<func*>;
  auto clone() const -> own<func*>;

  auto type() const -> own<functype*>;
  auto call(const vec<val>&) const -> vec<val>;

  template<class... Args>
  auto call(const Args&... vals) const -> vec<val> {
    return call(vec<val>::make(vals.clone()...));
  }
};


// Global Instances

class global : public external {
public:
  global() = delete;
  ~global();

  static auto make(own<store*>&, own<globaltype*>&, val&) -> own<global*>;
  auto clone() const -> own<global*>;

  auto type() const -> own<globaltype*>;
  auto get() const -> val;
  void set(val&);
};


// Table Instances

class table : public external {
public:
  table() = delete;
  ~table();

  using size_t = uint32_t;

  static auto make(own<store*>&, own<tabletype*>&, own<ref*>&) -> own<table*>;
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
