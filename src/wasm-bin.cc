#include "wasm-bin.hh"

namespace wasm {
namespace bin {

////////////////////////////////////////////////////////////////////////////////
// Encoding

void encode_u32(char*& ptr, size_t n) {
  for (int i = 0; i < 5; ++i) {
    *ptr++ = (n & 0x7f) | (i == 4 ? 0x00 : 0x80);
    n = n >> 7;
  }
}

auto valtype_to_byte(const ValType* type) -> byte_t {
  switch (type->kind()) {
    case I32: return 0x7f;
    case I64: return 0x7e;
    case F32: return 0x7d;
    case F64: return 0x7c;
    case FUNCREF: return 0x70;
    case ANYREF: return 0x6f;
  }
  assert(false);
}

auto wrapper(const own<FuncType*>& type) -> vec<byte_t> {
  auto in_arity = type->params().size();
  auto out_arity = type->results().size();
  auto size = 39 + in_arity + out_arity;
  auto binary = vec<byte_t>::make_uninitialized(size);
  auto ptr = binary.get();

  memcpy(ptr, "\x00""asm\x01\x00\x00\x00", 8);
  ptr += 8;

  *ptr++ = 0x01;  // type section
  encode_u32(ptr, 12 + in_arity + out_arity);  // size
  *ptr++ = 1;  // length
  *ptr++ = 0x60;  // function
  encode_u32(ptr, in_arity);
  for (size_t i = 0; i < in_arity; ++i) {
    *ptr++ = valtype_to_byte(type->params()[i].get());
  }
  encode_u32(ptr, out_arity);
  for (size_t i = 0; i < out_arity; ++i) {
    *ptr++ = valtype_to_byte(type->results()[i].get());
  }

  *ptr++ = 0x02;  // import section
  *ptr++ = 5;  // size
  *ptr++ = 1;  // length
  *ptr++ = 0;  // module length
  *ptr++ = 0;  // name length
  *ptr++ = 0x00;  // func
  *ptr++ = 0;  // type index

  *ptr++ = 0x07;  // export section
  *ptr++ = 4;  // size
  *ptr++ = 1;  // length
  *ptr++ = 0;  // name length
  *ptr++ = 0x00;  // func
  *ptr++ = 0;  // func index

  assert(ptr - binary.get() == size);
  return binary;
}


////////////////////////////////////////////////////////////////////////////////
// Decoding

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

auto name(const byte_t*& pos) -> Name {
  auto size = bin::u32(pos);
  auto start = pos;
  auto name = Name::make_uninitialized(size);
  memcpy(name.get(), start, size);
  pos += size;
  return name;
}

void name_skip(const byte_t*& pos) {
  auto size = bin::u32(pos);
  pos += size;
}


// Types

auto valtype(const byte_t*& pos) -> own<wasm::ValType*> {
  switch (*pos++) {
    case 0x7f: return ValType::make(I32);
    case 0x7e: return ValType::make(I64);
    case 0x7d: return ValType::make(F32);
    case 0x7c: return ValType::make(F64);
    case 0x70: return ValType::make(FUNCREF);
    case 0x6f: return ValType::make(ANYREF);
    default:
      // TODO(wasm+): support new value types
      assert(false);
  }
}

auto mutability(const byte_t*& pos) -> Mutability {
  return *pos++ ? VAR : CONST;
}

auto limits(const byte_t*& pos) -> Limits {
  auto tag = *pos++;
  auto min = bin::u32(pos);
  if ((tag & 0x01) == 0) {
    return Limits(min);
  } else {
    auto max = bin::u32(pos);
    return Limits(min, max);
  }
}

auto stacktype(const byte_t*& pos) -> vec<ValType*> {
  size_t size = bin::u32(pos);
  auto v = vec<ValType*>::make_uninitialized(size);
  for (uint32_t i = 0; i < size; ++i) v[i] = bin::valtype(pos);
  return v;
}

auto functype(const byte_t*& pos) -> own<FuncType*> {
  assert(*pos == 0x60);
  ++pos;
  auto params = bin::stacktype(pos);
  auto results = bin::stacktype(pos);
  return FuncType::make(std::move(params), std::move(results));
}

auto globaltype(const byte_t*& pos) -> own<GlobalType*> {
  auto content = bin::valtype(pos);
  auto mutability = bin::mutability(pos);
  return GlobalType::make(std::move(content), mutability);
}

auto tabletype(const byte_t*& pos) -> own<TableType*> {
  auto elem = bin::valtype(pos);
  auto limits = bin::limits(pos);
  return TableType::make(std::move(elem), limits);
}

auto memorytype(const byte_t*& pos) -> own<MemoryType*> {
  auto limits = bin::limits(pos);
  return MemoryType::make(limits);
}


void mutability_skip(const byte_t*& pos) {
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
  bin::mutability_skip(pos);
}

void tabletype_skip(const byte_t*& pos) {
  bin::valtype_skip(pos);
  bin::limits_skip(pos);
}

void memorytype_skip(const byte_t*& pos) {
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

auto section(const vec<byte_t>& binary, bin::sec_t sec) -> const byte_t* {
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

auto types(const vec<byte_t>& binary) -> vec<FuncType*> {
  auto pos = bin::section(binary, SEC_TYPE);
  if (pos == nullptr) return vec<FuncType*>::make();
  size_t size = bin::u32(pos);
  // TODO(wasm+): support new deftypes
  auto v = vec<FuncType*>::make_uninitialized(size);
  for (uint32_t i = 0; i < size; ++i) {
    v[i] = bin::functype(pos);
  }
  return v;
}


// Import section

auto imports(
  const vec<byte_t>& binary, const vec<FuncType*>& types
) -> vec<ImportType*> {
  auto pos = bin::section(binary, SEC_IMPORT);
  if (pos == nullptr) return vec<ImportType*>::make();
  size_t size = bin::u32(pos);
  auto v = vec<ImportType*>::make_uninitialized(size);
  for (uint32_t i = 0; i < size; ++i) {
    auto module = bin::name(pos);
    auto name = bin::name(pos);
    own<ExternType*> type;
    switch (*pos++) {
      case 0x00: type = types[bin::u32(pos)]->copy(); break;
      case 0x01: type = bin::tabletype(pos); break;
      case 0x02: type = bin::memorytype(pos); break;
      case 0x03: type = bin::globaltype(pos); break;
      default: assert(false);
    }
    v[i] = ImportType::make(
      std::move(module), std::move(name), std::move(type));
  }
  return v;
}

auto count(const vec<ImportType*>& imports, ExternKind kind) -> uint32_t {
  uint32_t n = 0;
  for (uint32_t i = 0; i < imports.size(); ++i) {
    if (imports[i]->type()->kind() == kind) ++n;
  }
  return n;
}


// Function section

auto funcs(
  const vec<byte_t>& binary,
  const vec<ImportType*>& imports, const vec<FuncType*>& types
) -> vec<FuncType*> {
  auto pos = bin::section(binary, SEC_FUNC);
  size_t size = pos != nullptr ? bin::u32(pos) : 0;
  auto v = vec<FuncType*>::make_uninitialized(
    size + count(imports, EXTERN_FUNC));
  size_t j = 0;
  for (uint32_t i = 0; i < imports.size(); ++i) {
    auto& et = imports[i]->type();
    if (et->kind() == EXTERN_FUNC) {
      v[j++] = et->func()->copy();
    }
  }
  if (pos != nullptr) {
    for (; j < v.size(); ++j) {
      v[j] = types[bin::u32(pos)]->copy();
    }
  }
  return v;
}


// Global section

auto globals(
  const vec<byte_t>& binary, const vec<ImportType*>& imports
) -> vec<GlobalType*> {
  auto pos = bin::section(binary, SEC_GLOBAL);
  size_t size = pos != nullptr ? bin::u32(pos) : 0;
  auto v = vec<GlobalType*>::make_uninitialized(
    size + count(imports, EXTERN_GLOBAL));
  size_t j = 0;
  for (uint32_t i = 0; i < imports.size(); ++i) {
    auto& et = imports[i]->type();
    if (et->kind() == EXTERN_GLOBAL) {
      v[j++] = et->global()->copy();
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

auto tables(
  const vec<byte_t>& binary, const vec<ImportType*>& imports
) -> vec<TableType*> {
  auto pos = bin::section(binary, SEC_TABLE);
  size_t size = pos != nullptr ? bin::u32(pos) : 0;
  auto v = vec<TableType*>::make_uninitialized(
    size + count(imports, EXTERN_TABLE));
  size_t j = 0;
  for (uint32_t i = 0; i < imports.size(); ++i) {
    auto& et = imports[i]->type();
    if (et->kind() == EXTERN_TABLE) {
      v[j++] = et->table()->copy();
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

auto memories(
  const vec<byte_t>& binary, const vec<ImportType*>& imports
) -> vec<MemoryType*> {
  auto pos = bin::section(binary, SEC_MEMORY);
  size_t size = pos != nullptr ? bin::u32(pos) : 0;
  auto v = vec<MemoryType*>::make_uninitialized(
    size + count(imports, EXTERN_MEMORY));
  size_t j = 0;
  for (uint32_t i = 0; i < imports.size(); ++i) {
    auto& et = imports[i]->type();
    if (et->kind() == EXTERN_MEMORY) {
      v[j++] = et->memory()->copy();
    }
  }
  if (pos != nullptr) {
    for (; j < v.size(); ++j) {
      v[j] = bin::memorytype(pos);
    }
  }
  return v;
}


// Export section

auto exports(const vec<byte_t>& binary,
  const vec<FuncType*>& funcs, const vec<GlobalType*>& globals,
  const vec<TableType*>& tables, const vec<MemoryType*>& memories
) -> vec<ExportType*> {
  auto exports = vec<ExportType*>::make();
  auto pos = bin::section(binary, SEC_EXPORT);
  if (pos != nullptr) {
    size_t size = bin::u32(pos);
    exports = vec<ExportType*>::make_uninitialized(size);
    for (uint32_t i = 0; i < size; ++i) {
      auto name = bin::name(pos);
      auto tag = *pos++;
      auto index = bin::u32(pos);
      own<ExternType*> type;
      switch (tag) {
        case 0x00: type = funcs[index]->copy(); break;
        case 0x01: type = tables[index]->copy(); break;
        case 0x02: type = memories[index]->copy(); break;
        case 0x03: type = globals[index]->copy(); break;
        default: assert(false);
      }
      exports[i] = ExportType::make(std::move(name), std::move(type));
    }
  }
  return exports;
}

auto imports(const vec<byte_t>& binary) -> vec<ImportType*> {
  return bin::imports(binary, bin::types(binary));
}

auto exports(const vec<byte_t>& binary) -> vec<ExportType*> {
  auto types = bin::types(binary);
  auto imports = bin::imports(binary, types);
  auto funcs = bin::funcs(binary, imports, types);
  auto globals = bin::globals(binary, imports);
  auto tables = bin::tables(binary, imports);
  auto memories = bin::memories(binary, imports);
  return bin::exports(binary, funcs, globals, tables, memories);
}

}  // namespace bin
}  // namespace wasm
