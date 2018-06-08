#include <iostream>
#include "wasm-bin.hh"

namespace wasm {
namespace bin {

// Numbers

auto u32(const byte_t*& pos) -> uint32_t {
  uint32_t n = 0;
  uint32_t shift = 0;
  byte_t b;
  do {
    b = *pos++;
    n += (b & 0x7f) >> shift;
    shift += 7;
  } while ((b & 0x80) != 0);
  return n;
}

void u32_skip(const byte_t*& pos) {
  bin::u32(pos);
}


// Names

auto name(const byte_t*& pos) -> own<wasm::name> {
  auto len = bin::u32(pos);
  auto start = pos;
  pos += len;
  return name::make(len, start);
}

void name_skip(const byte_t*& pos) {
  auto len = bin::u32(pos);
  pos += len;
}


// Types

auto valtype(const byte_t*& pos) -> own<wasm::valtype*> {
  switch (*pos++) {
    case 0x7f: return valtype::make(I32);
    case 0x7e: return valtype::make(I64);
    case 0x7d: return valtype::make(F32);
    case 0x7c: return valtype::make(F64);
    case 0x70: return valtype::make(FUNCREF);
    case 0x6f: return valtype::make(ANYREF);
    default:
      // TODO(wasm+): support new value types
      assert(false);
  }
}

auto mut(const byte_t*& pos) -> wasm::mut {
  return *pos++ ? VAR : CONST;
}

auto limits(const byte_t*& pos) -> limits {
  auto tag = *pos++;
  auto min = bin::u32(pos);
  if ((tag & 0x01) == 0) {
    return wasm::limits(min);
  } else {
    auto max = bin::u32(pos);
    return wasm::limits(min, max);
  }
}

auto stacktype(const byte_t*& pos) -> own<vec<wasm::valtype*>> {
  size_t n = bin::u32(pos);
  auto v = vec<wasm::valtype*>::make(n);
  for (uint32_t i = 0; i < n; ++i) {
    v.data[i] = bin::valtype(pos).release();
  }
  return v;
}

auto functype(const byte_t*& pos) -> own<wasm::functype*> {
  assert(*pos == 0x60);
  ++pos;
  auto params = bin::stacktype(pos);
  auto results = bin::stacktype(pos);
  return functype::make(std::move(params), std::move(results));
}

auto globaltype(const byte_t*& pos) -> own<wasm::globaltype*> {
  auto content = bin::valtype(pos);
  auto mut = bin::mut(pos);
  return globaltype::make(std::move(content), mut);
}

auto tabletype(const byte_t*& pos) -> own<wasm::tabletype*> {
  auto elem = bin::valtype(pos);
  auto limits = bin::limits(pos);
  return tabletype::make(std::move(elem), limits);
}

auto memtype(const byte_t*& pos) -> own<wasm::memtype*> {
  auto limits = bin::limits(pos);
  return memtype::make(limits);
}


void mut_skip(const byte_t*& pos) {
  ++pos;
}

void limits_skip(const byte_t*& pos) {
  auto tag = *pos++;
  bin::u32_skip(pos);
  if ((tag & 0x01) != 0) bin::u32_skip(pos);
}

void valtype_skip(const byte_t*& pos) {
  // TODO(wasm+): support new value types
  ++pos;
}

void globaltype_skip(const byte_t*& pos) {
  bin::valtype_skip(pos);
  bin::mut_skip(pos);
}

void tabletype_skip(const byte_t*& pos) {
  bin::valtype_skip(pos);
  bin::limits_skip(pos);
}

void memtype_skip(const byte_t*& pos) {
  bin::limits_skip(pos);
}


// Expressions

void expr_skip(const byte_t*& pos) {
  switch (*pos++) {
    case 0x41:  // i32.const
    case 0x42:  // i64.const
    case 0x23: {  // get_global
      bin::u32_skip(pos);
    } break;
    case 0x43: {  // f32.const
      pos += 4;
    } break;
    case 0x44: {  // f64.const
      pos += 8;
    } break;
    default: {
      // TODO(wasm+): support new expression forms
      assert(false);
    }
  }
  ++pos;  // end
}


// Sections

enum sec_t : byte_t {
  SEC_TYPE = 1,
  SEC_IMPORT = 2,
  SEC_FUNC = 3,
  SEC_TABLE = 4,
  SEC_MEMORY = 5,
  SEC_GLOBAL = 6,
  SEC_EXPORT = 7
};

auto section(vec<byte_t> binary, bin::sec_t sec) -> const byte_t* {
  const byte_t* end = binary.data + binary.size;
  const byte_t* pos = binary.data + 8;  // skip header
  while (pos < end && *pos != sec) {
    ++pos;
    auto size = bin::u32(pos);
    pos += size;
  }
  if (pos == end) return nullptr;
  ++pos;
  bin::u32_skip(pos);
  return pos;
}


// Type section

auto types(vec<byte_t> binary) -> own<vec<wasm::functype*>> {
std::cout << "t0" <<std::endl;
  auto pos = bin::section(binary, SEC_TYPE);
std::cout << "t1" <<std::endl;
  if (pos == nullptr) return vec<wasm::functype*>::make();
std::cout << "t2" <<std::endl;
  size_t size = bin::u32(pos);
std::cout << "t3 "<<size <<std::endl;
  // TODO(wasm+): support new deftypes
  auto v = vec<wasm::functype*>::make(size);
std::cout << "t4" <<std::endl;
  for (uint32_t i = 0; i < size; ++i) v.data[i] = bin::functype(pos).release();
std::cout << "t5" <<std::endl;
  return v;
}


// Import section

auto imports(vec<byte_t> binary, vec<wasm::functype*> types) -> own<vec<importtype*>> {
  auto pos = bin::section(binary, SEC_IMPORT);
  if (pos == nullptr) return vec<importtype*>::make();
  size_t size = bin::u32(pos);
  auto v = vec<importtype*>::make(size);
  for (uint32_t i = 0; i < size; ++i) {
    auto module = bin::name(pos);
    auto name = bin::name(pos);
    own<externtype*> type;
    switch (*pos++) {
      case 0x00: {
        auto index = bin::u32(pos);
        type = externtype::make(types.data[index]->clone());
      } break;
      case 0x01: {
        type = externtype::make(bin::tabletype(pos));
      } break;
      case 0x02: {
        type = externtype::make(bin::memtype(pos));
      } break;
      case 0x03: {
        type = externtype::make(bin::globaltype(pos));
      } break;
      default: {
        assert(false);
      }
    }
    v.data[i] = importtype::make(std::move(module), std::move(name), std::move(type)).release();
  }
  return v;
}

auto count(vec<importtype*> imports, externkind kind) -> uint32_t {
  uint32_t n = 0;
  for (uint32_t i = 0; i < imports.size; ++i) {
    if (imports.data[i]->type()->kind() == kind) ++n;
  }
  return n;
}


// Function section

auto funcs(vec<byte_t> binary, vec<importtype*> imports, vec<wasm::functype*> types) -> own<vec<wasm::functype*>> {
  auto pos = bin::section(binary, SEC_FUNC);
std::cout << "f0 " <<size_t(pos)<<std::endl;
  size_t size = pos != nullptr ? bin::u32(pos) : 0;
using charp=char*;
auto vv = new(std::nothrow) charp[4];
std::cout << vv << ": " << std::flush;
for (size_t i = 0; i<4;++i) std::cout << " " << (void*)vv[i]; std::cout <<std::endl;
auto vvu = new(std::nothrow) std::unique_ptr<char>[4];
std::cout << vvu << ": " << std::flush;
for (size_t i = 0; i<4;++i) std::cout << " " << (void*)vvu[i].get(); std::cout <<std::endl;
auto vvvm = vec<char*>::make(4);
auto vvm=vvvm.data.get();
std::cout << (void*)vvm << ": " << std::flush;
for (size_t i = 0; i<4;++i) std::cout << " " << (void*)vvm[i]; std::cout <<std::endl;
  auto v = vec<wasm::functype*>::make(size + count(imports, EXTERN_FUNC));
std::cout << "f " << (v.size - size) << "+" <<size<<"="<<v.size<<" "<<size_t(pos)<<std::endl;
for (size_t i = 0; i<v.size;++i) std::cout << " " << v.data[i]; std::cout <<std::endl;
  size_t j = 0;
  for (uint32_t i = 0; i < imports.size; ++i) {
    auto et = imports.data[i]->type();
    if (et->kind() == EXTERN_FUNC) v.data[j++] = et->func()->clone().release();
  }
std::cout << "f' " << j <<" " <<size_t(pos)<<std::endl;
  if (pos != nullptr) {
    for (; j < v.size; ++j) v.data[j] = types.data[bin::u32(pos)]->clone().release();
  }
std::cout << "f'' " << j <<std::endl;
  return v;
}


// Global section

auto globals(vec<byte_t> binary, vec<importtype*> imports) -> own<vec<wasm::globaltype*>> {
  auto pos = bin::section(binary, SEC_GLOBAL);
  size_t size = pos != nullptr ? bin::u32(pos) : 0;
  auto v = vec<wasm::globaltype*>::make(size + count(imports, EXTERN_GLOBAL));
  size_t j = 0;
  for (uint32_t i = 0; i < imports.size; ++i) {
    auto et = imports.data[i]->type();
    if (et->kind() == EXTERN_GLOBAL) v.data[j++] = et->global()->clone().release();
  }
  if (pos != nullptr) {
    for (; j < size; ++j) v.data[j] = bin::globaltype(pos).release();
  }
  return v;
}


// Table section

auto tables(vec<byte_t> binary, vec<importtype*> imports) -> own<vec<wasm::tabletype*>> {
  auto pos = bin::section(binary, SEC_TABLE);
  size_t size = pos != nullptr ? bin::u32(pos) : 0;
  auto v = vec<wasm::tabletype*>::make(size + count(imports, EXTERN_TABLE));
  size_t j = 0;
  for (uint32_t i = 0; i < imports.size; ++i) {
    auto et = imports.data[i]->type();
    if (et->kind() == EXTERN_TABLE) v.data[j++] = et->table()->clone().release();
  }
  if (pos != nullptr) {
    for (; j < size; ++j) v.data[j] = bin::tabletype(pos).release();
  }
  return v;
}


// Memory section

auto memories(vec<byte_t> binary, vec<importtype*> imports) -> own<vec<wasm::memtype*>> {
  auto pos = bin::section(binary, SEC_MEMORY);
  size_t size = pos != nullptr ? bin::u32(pos) : 0;
  auto v = vec<wasm::memtype*>::make(size + count(imports, EXTERN_MEMORY));
  size_t j = 0;
  for (uint32_t i = 0; i < imports.size; ++i) {
    auto et = imports.data[i]->type();
    if (et->kind() == EXTERN_MEMORY) v.data[j++] = et->memory()->clone().release();
  }
  if (pos != nullptr) {
    for (; j < size; ++j) v.data[j] = bin::memtype(pos).release();
  }
  return v;
}


// Export section

auto exports(vec<byte_t> binary,
  vec<wasm::functype*> funcs, vec<wasm::globaltype*> globals,
  vec<wasm::tabletype*> tables, vec<wasm::memtype*> memories
) -> own<vec<exporttype*>> {
  auto exports = vec<exporttype*>::make();
std::cout << "a1" <<std::endl;
for (size_t i = 0; i<funcs.size;++i) std::cout << " " << funcs.data[i]; std::cout <<std::endl;
  auto pos = bin::section(binary, SEC_EXPORT);
std::cout << "a2" <<std::endl;
  if (pos != nullptr) {
std::cout << "a3" <<std::endl;
    size_t size = bin::u32(pos);
    exports = vec<exporttype*>::make(size);
std::cout << "a4" <<std::endl;
    for (uint32_t i = 0; i < size; ++i) {
std::cout << "a5 " <<i <<std::endl;
      auto name = bin::name(pos);
std::cout << "a6 " << std::string(name.data.get(), name.size) <<std::endl;
      auto tag = *pos++;
      auto index = bin::u32(pos);
std::cout << "a7 " <<index <<std::endl;
      own<externtype*> type;
      switch (tag) {
        case 0x00: {
std::cout << "a1 " <<funcs.data[index] <<std::endl;
          type = externtype::make(funcs.data[index]->clone());
std::cout << "a1" <<std::endl;
        } break;
        case 0x01: {
std::cout << "a2" <<std::endl;
          type = externtype::make(tables.data[index]->clone());
        } break;
        case 0x02: {
std::cout << "a4" <<std::endl;
          type = externtype::make(memories.data[index]->clone());
        } break;
        case 0x03: {
std::cout << "a5" <<std::endl;
          type = externtype::make(globals.data[index]->clone());
        } break;
        default: {
          assert(false);
        }
      }
std::cout << "a8" <<std::endl;
      exports.data[i] = exporttype::make(std::move(name), std::move(type)).release();
std::cout << "a9" <<std::endl;
    }
  }
std::cout << "a10" <<std::endl;
  return exports;
}

auto imports_exports(vec<byte_t> binary
) -> std::tuple<own<vec<importtype*>>, own<vec<exporttype*>>> {
std::cout << -1 <<std::endl;
  auto types = bin::types(binary);
std::cout << -2 <<std::endl;
  auto imports = bin::imports(binary, types.get());
std::cout << -3 <<std::endl;
  auto funcs = bin::funcs(binary, imports.get(), types.get());
std::cout << -4 <<std::endl;
for (size_t i = 0; i<funcs.size;++i) std::cout << " " << funcs.data[i]; std::cout <<std::endl;
  auto globals = bin::globals(binary, imports.get());
std::cout << -5 <<std::endl;
  auto tables = bin::tables(binary, imports.get());
std::cout << -6 <<std::endl;
  auto memories = bin::memories(binary, imports.get());
std::cout << -7 <<std::endl;
  auto exports = bin::exports(
    binary, funcs.get(), globals.get(), tables.get(), memories.get());
std::cout << -8 <<" "<<!!imports<<" "<<!!exports<<std::endl;

  return std::make_tuple(std::move(imports), std::move(exports));
}

}  // namespace bin
}  // namespace wasm
