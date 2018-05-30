#include "wasm-bin.hh"

#define own

namespace wasm {
namespace bin {

// Numbers

uint32_t u32(const byte_t*& pos) {
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

own wasm_name_t name(const byte_t*& pos) {
  uint32_t len = bin::u32(pos);
  auto start = pos;
  pos += len;
  return wasm_name_new(len, start);
}

void name_skip(const byte_t*& pos) {
  uint32_t len = bin::u32(pos);
  pos += len;
}


// Types

own wasm_valtype_t* valtype(const byte_t*& pos) {
  switch (*pos++) {
    case 0x7f: return wasm_valtype_new_i32();
    case 0x7e: return wasm_valtype_new_i64();
    case 0x7d: return wasm_valtype_new_f32();
    case 0x7c: return wasm_valtype_new_f64();
    case 0x70: return wasm_valtype_new_funcref();
    case 0x6f: return wasm_valtype_new_anyref();
    default:
      // TODO(wasm+): support new value types
      assert(false);
  }
}

wasm_mut_t mut(const byte_t*& pos) {
  return *pos++ ? WASM_VAR : WASM_CONST;
}

wasm_limits_t limits(const byte_t*& pos) {
  byte_t tag = *pos++;
  auto min = bin::u32(pos);
  if ((tag & 0x01) == 0) {
    return wasm_limits_no_max(min);
  } else {
    auto max = bin::u32(pos);
    return wasm_limits(min, max);
  }
}

own wasm_functype_t* functype(const byte_t*& pos) {
  assert(*pos == 0x60);
  ++pos;
  uint32_t n = bin::u32(pos);
  auto params = wasm_valtype_vec_new_uninitialized(n);
  for (uint32_t i = 0; i < n; ++i) {
    params.data[i] = bin::valtype(pos);
  }
  uint32_t m = bin::u32(pos);
  auto results = wasm_valtype_vec_new_uninitialized(m);
  for (uint32_t i = 0; i < m; ++i) {
    results.data[i] = bin::valtype(pos);
  }
  return wasm_functype_new(params, results);
}

own wasm_globaltype_t* globaltype(const byte_t*& pos) {
  auto content = bin::valtype(pos);
  auto mut = bin::mut(pos);
  return wasm_globaltype_new(content, mut);
}

own wasm_tabletype_t* tabletype(const byte_t*& pos) {
  auto elem = bin::valtype(pos);
  auto limits = bin::limits(pos);
  return wasm_tabletype_new(elem, limits);
}

own wasm_memtype_t* memtype(const byte_t*& pos) {
  auto limits = bin::limits(pos);
  return wasm_memtype_new(limits);
}


void mut_skip(const byte_t*& pos) {
  ++pos;
}

void limits_skip(const byte_t*& pos) {
  byte_t tag = *pos++;
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

const byte_t* section(wasm_byte_vec_t binary, bin::sec_t sec) {
  const byte_t* end = binary.data + binary.size;
  const byte_t* pos = binary.data + 8;  // skip header
  while (pos < end && *pos != sec) {
    ++pos;
    uint32_t size = bin::u32(pos);
    pos += size;
  }
  if (pos == end) return nullptr;
  ++pos;
  bin::u32_skip(pos);
  return pos;
}


// Types

own wasm_functype_vec_t types(wasm_byte_vec_t binary) {
  auto pos = bin::section(binary, SEC_TYPE);
  if (pos == nullptr) return wasm_functype_vec_empty();
  uint32_t size = bin::u32(pos);
  // TODO(wasm+): support new deftypes
  auto v = wasm_functype_vec_new_uninitialized(size);
  for (uint32_t i = 0; i < size; ++i) v.data[i] = bin::functype(pos);
  return v;
}


// Imports

own wasm_importtype_vec_t imports(wasm_byte_vec_t binary, wasm_functype_vec_t types) {
  auto pos = bin::section(binary, SEC_IMPORT);
  if (pos == nullptr) return wasm_importtype_vec_empty();
  uint32_t size = bin::u32(pos);
  auto v = wasm_importtype_vec_new_uninitialized(size);
  for (uint32_t i = 0; i < size; ++i) {
    auto module = bin::name(pos);
    auto name = bin::name(pos);
    own wasm_externtype_t* type;
    switch (*pos++) {
      case 0x00: {
        uint32_t index = bin::u32(pos);
        type = wasm_functype_as_externtype(wasm_functype_clone(types.data[index]));
      } break;
      case 0x01: {
        type = wasm_tabletype_as_externtype(bin::tabletype(pos));
      } break;
      case 0x02: {
        type = wasm_memtype_as_externtype(bin::memtype(pos));
      } break;
      case 0x03: {
        type = wasm_globaltype_as_externtype(bin::globaltype(pos));
      } break;
      default: {
        assert(false);
      }
    }
    v.data[i] = wasm_importtype_new(module, name, type);
  }
  return v;
}


// Functions

own wasm_functype_vec_t funcs(wasm_byte_vec_t binary, wasm_importtype_vec_t imports, wasm_functype_vec_t types) {
  auto pos = bin::section(binary, SEC_FUNC);
  size_t size = pos != nullptr ? bin::u32(pos) : 0;
  for (uint32_t i = 0; i < imports.size; ++i) {
    auto et = wasm_importtype_type(imports.data[i]);
    if (wasm_externtype_kind(et) == WASM_EXTERN_FUNC) ++size;
  }
  own auto v = wasm_functype_vec_new_uninitialized(size);
  size_t j = 0;

  for (uint32_t i = 0; i < imports.size; ++i) {
    auto et = wasm_importtype_type(imports.data[i]);
    if (wasm_externtype_kind(et) == WASM_EXTERN_FUNC) {
      v.data[j++] = wasm_externtype_as_functype(et);
    } 
  }
  if (pos != nullptr) {
    for (; j < size; ++j) {
      uint32_t index = bin::u32(pos);
      v.data[j] = types.data[index];
    }
  }
  return v;
}


// Globals

own wasm_globaltype_vec_t globals(wasm_byte_vec_t binary, wasm_importtype_vec_t imports) {
  auto pos = bin::section(binary, SEC_GLOBAL);
  size_t size = pos != nullptr ? bin::u32(pos) : 0;
  for (uint32_t i = 0; i < imports.size; ++i) {
    auto et = wasm_importtype_type(imports.data[i]);
    if (wasm_externtype_kind(et) == WASM_EXTERN_GLOBAL) ++size;
  }
  own auto v = wasm_globaltype_vec_new_uninitialized(size);
  size_t j = 0;

  for (uint32_t i = 0; i < imports.size; ++i) {
    auto et = wasm_importtype_type(imports.data[i]);
    if (wasm_externtype_kind(et) == WASM_EXTERN_GLOBAL) {
      v.data[j++] = wasm_globaltype_clone(wasm_externtype_as_globaltype(et));
    } 
  }
  if (pos != nullptr) {
    for (; j < size; ++j) {
      v.data[j] = bin::globaltype(pos);
    }
  }
  return v;
}


// Tables

own wasm_tabletype_vec_t tables(wasm_byte_vec_t binary, wasm_importtype_vec_t imports) {
  auto pos = bin::section(binary, SEC_TABLE);
  size_t size = pos != nullptr ? bin::u32(pos) : 0;
  for (uint32_t i = 0; i < imports.size; ++i) {
    auto et = wasm_importtype_type(imports.data[i]);
    if (wasm_externtype_kind(et) == WASM_EXTERN_TABLE) ++size;
  }
  own auto v = wasm_tabletype_vec_new_uninitialized(size);
  size_t j = 0;

  for (uint32_t i = 0; i < imports.size; ++i) {
    auto et = wasm_importtype_type(imports.data[i]);
    if (wasm_externtype_kind(et) == WASM_EXTERN_TABLE) {
      v.data[j++] = wasm_tabletype_clone(wasm_externtype_as_tabletype(et));
    } 
  }
  if (pos != nullptr) {
    for (; j < size; ++j) {
      v.data[j] = bin::tabletype(pos);
    }
  }
  return v;
}


// Memories

own wasm_memtype_vec_t memories(wasm_byte_vec_t binary, wasm_importtype_vec_t imports) {
  auto pos = bin::section(binary, SEC_MEMORY);
  size_t size = pos != nullptr ? bin::u32(pos) : 0;
  for (uint32_t i = 0; i < imports.size; ++i) {
    auto et = wasm_importtype_type(imports.data[i]);
    if (wasm_externtype_kind(et) == WASM_EXTERN_MEMORY) ++size;
  }
  own auto v = wasm_memtype_vec_new_uninitialized(size);
  size_t j = 0;

  for (uint32_t i = 0; i < imports.size; ++i) {
    auto et = wasm_importtype_type(imports.data[i]);
    if (wasm_externtype_kind(et) == WASM_EXTERN_MEMORY) {
      v.data[j++] = wasm_memtype_clone(wasm_externtype_as_memtype(et));
    } 
  }
  if (pos != nullptr) {
    for (; j < size; ++j) {
      v.data[j] = bin::memtype(pos);
    }
  }
  return v;
}


// Exports

own wasm_exporttype_vec_t exports(wasm_byte_vec_t binary,
  wasm_functype_vec_t funcs, wasm_globaltype_vec_t globals,
  wasm_tabletype_vec_t tables, wasm_memtype_vec_t memories
) {
  own auto exports = wasm_exporttype_vec_empty();
  auto pos = bin::section(binary, SEC_EXPORT);
  if (pos != nullptr) {
    uint32_t size = bin::u32(pos);
    exports = wasm_exporttype_vec_new_uninitialized(size);
    for (uint32_t i = 0; i < size; ++i) {
      auto name = bin::name(pos);
      auto tag = *pos++;
      auto index = bin::u32(pos);
      own wasm_externtype_t* type;
      switch (tag) {
        case 0x00: {
          type = wasm_functype_as_externtype(wasm_functype_clone(funcs.data[index]));
        } break;
        case 0x01: {
          type = wasm_tabletype_as_externtype(wasm_tabletype_clone(tables.data[index]));
        } break;
        case 0x02: {
          type = wasm_memtype_as_externtype(wasm_memtype_clone(memories.data[index]));
        } break;
        case 0x03: {
          type = wasm_globaltype_as_externtype(wasm_globaltype_clone(globals.data[index]));
        } break;
        default: {
          assert(false);
        }
      }
      exports.data[i] = wasm_exporttype_new(name, type);
    }
  }
  return exports;
}

std::tuple<own wasm_importtype_vec_t, own wasm_exporttype_vec_t>
imports_exports(wasm_byte_vec_t binary) {
  auto types = bin::types(binary);
  auto imports = bin::imports(binary, types);
  auto funcs = bin::funcs(binary, imports, types);
  auto globals = bin::globals(binary, imports);
  auto tables = bin::tables(binary, imports);
  auto memories = bin::memories(binary, imports);
  auto exports = bin::exports(binary, funcs, globals, tables, memories);

  wasm_functype_vec_delete(types);
  if (funcs.data) delete[] funcs.data;
  wasm_globaltype_vec_delete(globals);
  wasm_tabletype_vec_delete(tables);
  wasm_memtype_vec_delete(memories);
  return std::make_tuple(imports, exports);
}

}  // namespace bin
}  // namespace wasm
