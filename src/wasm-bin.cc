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

auto name(const byte_t*& pos) -> wasm::name {
  auto size = bin::u32(pos);
  auto start = pos;
  auto name = name::make_uninitialized(size);
  memcpy(name.get(), start, size);
  pos += size;
  return name;
}

void name_skip(const byte_t*& pos) {
  auto size = bin::u32(pos);
  pos += size;
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

auto stacktype(const byte_t*& pos) -> vec<wasm::valtype*> {
  size_t size = bin::u32(pos);
  auto v = vec<wasm::valtype*>::make_uninitialized(size);
  for (uint32_t i = 0; i < size; ++i) v[i] = bin::valtype(pos);
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

auto section(vec<byte_t>& binary, bin::sec_t sec) -> const byte_t* {
  const byte_t* end = binary.get() + binary.size();
  const byte_t* pos = binary.get() + 8;  // skip header
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

auto types(vec<byte_t>& binary) -> vec<wasm::functype*> {
  auto pos = bin::section(binary, SEC_TYPE);
  if (pos == nullptr) return vec<wasm::functype*>::make();
  size_t size = bin::u32(pos);
  // TODO(wasm+): support new deftypes
  auto v = vec<wasm::functype*>::make_uninitialized(size);
  for (uint32_t i = 0; i < size; ++i) {
    v[i] = bin::functype(pos);
  }
  return v;
}


// Import section

auto imports(vec<byte_t>& binary, vec<wasm::functype*>& types)
-> vec<importtype*> {
  auto pos = bin::section(binary, SEC_IMPORT);
  if (pos == nullptr) return vec<importtype*>::make();
  size_t size = bin::u32(pos);
  auto v = vec<importtype*>::make_uninitialized(size);
  for (uint32_t i = 0; i < size; ++i) {
    auto module = bin::name(pos);
    auto name = bin::name(pos);
    own<externtype*> type;
    switch (*pos++) {
      case 0x00: type = types[bin::u32(pos)]->clone(); break;
      case 0x01: type = bin::tabletype(pos); break;
      case 0x02: type = bin::memtype(pos); break;
      case 0x03: type = bin::globaltype(pos); break;
      default: assert(false);
    }
    v[i] = importtype::make(std::move(module), std::move(name), std::move(type));
  }
  return v;
}

auto count(vec<importtype*>& imports, externkind kind) -> uint32_t {
  uint32_t n = 0;
  for (uint32_t i = 0; i < imports.size(); ++i) {
    if (imports[i]->type()->kind() == kind) ++n;
  }
  return n;
}


// Function section

auto funcs(
  vec<byte_t>& binary, vec<importtype*>& imports, vec<wasm::functype*>& types
) -> vec<wasm::functype*> {
  auto pos = bin::section(binary, SEC_FUNC);
  size_t size = pos != nullptr ? bin::u32(pos) : 0;
  auto v = vec<wasm::functype*>::make_uninitialized(size + count(imports, EXTERN_FUNC));
  size_t j = 0;
  for (uint32_t i = 0; i < imports.size(); ++i) {
    auto& et = imports[i]->type();
    if (et->kind() == EXTERN_FUNC) {
      v[j++] = et->func()->clone();
    }
  }
  if (pos != nullptr) {
    for (; j < v.size(); ++j) {
      v[j] = types[bin::u32(pos)]->clone();
    }
  }
  return v;
}


// Global section

auto globals(vec<byte_t>& binary, vec<importtype*>& imports)
-> vec<wasm::globaltype*> {
  auto pos = bin::section(binary, SEC_GLOBAL);
  size_t size = pos != nullptr ? bin::u32(pos) : 0;
  auto v = vec<wasm::globaltype*>::make_uninitialized(size + count(imports, EXTERN_GLOBAL));
  size_t j = 0;
  for (uint32_t i = 0; i < imports.size(); ++i) {
    auto& et = imports[i]->type();
    if (et->kind() == EXTERN_GLOBAL) {
      v[j++] = et->global()->clone();
    }
  }
  if (pos != nullptr) {
    for (; j < v.size(); ++j) {
      v[j] = bin::globaltype(pos);
    }
  }
  return v;
}


// Table section

auto tables(vec<byte_t>& binary, vec<importtype*>& imports)
-> vec<wasm::tabletype*> {
  auto pos = bin::section(binary, SEC_TABLE);
  size_t size = pos != nullptr ? bin::u32(pos) : 0;
  auto v = vec<wasm::tabletype*>::make_uninitialized(size + count(imports, EXTERN_TABLE));
  size_t j = 0;
  for (uint32_t i = 0; i < imports.size(); ++i) {
    auto& et = imports[i]->type();
    if (et->kind() == EXTERN_TABLE) {
      v[j++] = et->table()->clone();
    }
  }
  if (pos != nullptr) {
    for (; j < v.size(); ++j) {
      v[j] = bin::tabletype(pos);
    }
  }
  return v;
}


// Memory section

auto memories(vec<byte_t>& binary, vec<importtype*>& imports)
-> vec<wasm::memtype*> {
  auto pos = bin::section(binary, SEC_MEMORY);
  size_t size = pos != nullptr ? bin::u32(pos) : 0;
  auto v = vec<wasm::memtype*>::make_uninitialized(size + count(imports, EXTERN_MEMORY));
  size_t j = 0;
  for (uint32_t i = 0; i < imports.size(); ++i) {
    auto& et = imports[i]->type();
    if (et->kind() == EXTERN_MEMORY) {
      v[j++] = et->memory()->clone();
    }
  }
  if (pos != nullptr) {
    for (; j < v.size(); ++j) {
      v[j] = bin::memtype(pos);
    }
  }
  return v;
}


// Export section

auto exports(vec<byte_t>& binary,
  vec<wasm::functype*>& funcs, vec<wasm::globaltype*>& globals,
  vec<wasm::tabletype*>& tables, vec<wasm::memtype*>& memories
) -> vec<exporttype*> {
  auto exports = vec<exporttype*>::make();
  auto pos = bin::section(binary, SEC_EXPORT);
  if (pos != nullptr) {
    size_t size = bin::u32(pos);
    exports = vec<exporttype*>::make_uninitialized(size);
    for (uint32_t i = 0; i < size; ++i) {
      auto name = bin::name(pos);
      auto tag = *pos++;
      auto index = bin::u32(pos);
      own<externtype*> type;
      switch (tag) {
        case 0x00: type = funcs[index]->clone(); break;
        case 0x01: type = tables[index]->clone(); break;
        case 0x02: type = memories[index]->clone(); break;
        case 0x03: type = globals[index]->clone(); break;
        default: assert(false);
      }
      exports[i] = exporttype::make(std::move(name), std::move(type));
    }
  }
  return exports;
}

auto imports_exports(vec<byte_t>& binary)
-> std::tuple<vec<importtype*>, vec<exporttype*>> {
  auto types = bin::types(binary);
  auto imports = bin::imports(binary, types);
  auto funcs = bin::funcs(binary, imports, types);
  auto globals = bin::globals(binary, imports);
  auto tables = bin::tables(binary, imports);
  auto memories = bin::memories(binary, imports);
  auto exports = bin::exports(binary, funcs, globals, tables, memories);

  return std::make_tuple(std::move(imports), std::move(exports));
}

}  // namespace bin
}  // namespace wasm
