#include "wasm-bin.hh"

#include <stdio.h>

#define own

namespace wasm {
namespace bin {

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


void name_skip(const byte_t*& pos) {
  uint32_t len = bin::u32(pos);
  pos += len;
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
  ++pos;
}

void tabletype_skip(const byte_t*& pos) {
  bin::valtype_skip(pos);
  bin::limits_skip(pos);
}

void memtype_skip(const byte_t*& pos) {
  bin::limits_skip(pos);
}

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


enum sec_t : byte_t {
  SEC_TYPE = 1,
  SEC_IMPORT = 2,
  SEC_FUNC = 3,
  SEC_TABLE = 4,
  SEC_MEMORY = 5,
  SEC_GLOBAL = 6
};

const byte_t* section(wasm_byte_vec_t binary, bin::sec_t sec) {
  const byte_t* end = binary.data + binary.size;
  const byte_t* pos = binary.data + 8;  // skip header
  while (pos < end && *pos != sec) {
    ++pos;
    uint32_t size = bin::u32(pos);
    pos += size;
  }
  return pos == end ? nullptr : pos;
}

own wasm_functype_vec_t types(wasm_byte_vec_t binary) {
  const byte_t* pos = bin::section(binary, SEC_TYPE);
  if (pos == nullptr) return wasm_functype_vec_empty();
  ++pos;  // section index
  bin::u32_skip(pos);  // section size
  uint32_t size = bin::u32(pos);
  // TODO(wasm+): support new deftypes
  auto v = wasm_functype_vec_new_uninitialized(size);
  for (uint32_t i = 0; i < size; ++i) v.data[i] = bin::functype(pos);
  return v;
}


own wasm_functype_vec_t funcs(wasm_byte_vec_t binary) {
  using wasm_functype_ptr_t = wasm_functype_t*;

  auto deftypes = bin::types(binary);
  if (deftypes.data == nullptr) return wasm_functype_vec_empty();

  own auto imports = wasm_functype_vec_empty();
  auto pos = bin::section(binary, SEC_IMPORT);
  if (pos != nullptr) {
    ++pos;  // section index
    bin::u32_skip(pos);  // section size
    uint32_t size = bin::u32(pos);
    imports.data = new wasm_functype_ptr_t[size];
    for (uint32_t i = 0; i < size; ++i) {
      bin::name_skip(pos);
      bin::name_skip(pos);
      switch (*pos++) {
        case 0x00: {
          uint32_t index = bin::u32(pos);
          imports.data[imports.size++] =
            wasm_functype_clone(deftypes.data[index]);
        } break;
        case 0x01: bin::tabletype_skip(pos); break;
        case 0x02: bin::memtype_skip(pos); break;
        case 0x03: bin::globaltype_skip(pos); break;
        default: assert(false);
      }
    }
  }

  own auto funcs = wasm_functype_vec_empty();
  pos = bin::section(binary, SEC_FUNC);
  if (pos != nullptr) {
    ++pos;  // section index
    bin::u32_skip(pos);  // section size
    funcs.size = bin::u32(pos);
    funcs.data = new wasm_functype_ptr_t[funcs.size];
    for (uint32_t i = 0; i < funcs.size; ++i) {
      uint32_t index = bin::u32(pos);
      funcs.data[i] = wasm_functype_clone(deftypes.data[i]);
    }
  }

  own auto v = wasm_functype_vec_new_uninitialized(imports.size + funcs.size);
  uint32_t i = 0;
  for (uint32_t j = 0; j < imports.size; ++j, ++i) v.data[i] = imports.data[j];
  for (uint32_t j = 0; j < funcs.size; ++j, ++i) v.data[i] = funcs.data[j];
  if (imports.data) delete[] imports.data;
  if (funcs.data) delete[] funcs.data;

  return v;
}

own wasm_globaltype_vec_t globals(wasm_byte_vec_t binary) {
  using wasm_globaltype_ptr_t = wasm_globaltype_t*;

  own auto imports = wasm_globaltype_vec_empty();
  auto pos = bin::section(binary, SEC_IMPORT);
  if (pos != nullptr) {
    ++pos;  // section index
    bin::u32_skip(pos);  // section size
    uint32_t size = bin::u32(pos);
    imports.data = new wasm_globaltype_ptr_t[size];
    for (uint32_t i = 0; i < size; ++i) {
      bin::name_skip(pos);
      bin::name_skip(pos);
      switch (*pos++) {
        case 0x00: bin::u32_skip(pos); break;
        case 0x01: bin::tabletype_skip(pos); break;
        case 0x02: bin::memtype_skip(pos); break;
        case 0x03: {
          imports.data[imports.size++] = bin::globaltype(pos);
        } break;
        default: assert(false);
      }
    }
  }

  own auto globals = wasm_globaltype_vec_empty();
  pos = bin::section(binary, SEC_GLOBAL);
  if (pos != nullptr) {
    ++pos;  // section index
    bin::u32_skip(pos);  // section size
    globals.size = bin::u32(pos);
    globals.data = new wasm_globaltype_ptr_t[globals.size];
    for (uint32_t i = 0; i < globals.size; ++i) {
      globals.data[i] = bin::globaltype(pos);
      bin::expr_skip(pos);
    }
  }

  own auto v = wasm_globaltype_vec_new_uninitialized(imports.size + globals.size);
  uint32_t i = 0;
  for (uint32_t j = 0; j < imports.size; ++j, ++i) v.data[i] = imports.data[j];
  for (uint32_t j = 0; j < globals.size; ++j, ++i) v.data[i] = globals.data[j];
  if (imports.data) delete[] imports.data;
  if (globals.data) delete[] globals.data;

  return v;
}

own wasm_tabletype_vec_t tables(wasm_byte_vec_t binary) {
  using wasm_tabletype_ptr_t = wasm_tabletype_t*;

  own auto imports = wasm_tabletype_vec_empty();
  auto pos = bin::section(binary, SEC_IMPORT);
  if (pos != nullptr) {
    ++pos;  // section index
    bin::u32_skip(pos);  // section size
    uint32_t size = bin::u32(pos);
    imports.data = new wasm_tabletype_ptr_t[size];
    for (uint32_t i = 0; i < size; ++i) {
      bin::name_skip(pos);
      bin::name_skip(pos);
      switch (*pos++) {
        case 0x00: bin::u32_skip(pos); break;
        case 0x01: {
          imports.data[imports.size++] = bin::tabletype(pos);
        } break;
        case 0x02: bin::memtype_skip(pos); break;
        case 0x03: bin::globaltype_skip(pos); break;
        default: assert(false);
      }
    }
  }

  own auto tables = wasm_tabletype_vec_empty();
  pos = bin::section(binary, SEC_TABLE);
  if (pos != nullptr) {
    ++pos;  // section index
    bin::u32_skip(pos);  // section size
    tables.size = bin::u32(pos);
    tables.data = new wasm_tabletype_ptr_t[tables.size];
    for (uint32_t i = 0; i < tables.size; ++i) {
      tables.data[i] = bin::tabletype(pos);
    }
  }

  own auto v = wasm_tabletype_vec_new_uninitialized(imports.size + tables.size);
  uint32_t i = 0;
  for (uint32_t j = 0; j < imports.size; ++j, ++i) v.data[i] = imports.data[j];
  for (uint32_t j = 0; j < tables.size; ++j, ++i) v.data[i] = tables.data[j];
  if (imports.data) delete[] imports.data;
  if (tables.data) delete[] tables.data;

  return v;
}

own wasm_memtype_vec_t memories(wasm_byte_vec_t binary) {
  using wasm_memtype_ptr_t = wasm_memtype_t*;

  own auto imports = wasm_memtype_vec_empty();
  auto pos = bin::section(binary, SEC_IMPORT);
  if (pos != nullptr) {
    ++pos;  // section index
    bin::u32_skip(pos);  // section size
    uint32_t size = bin::u32(pos);
    imports.data = new wasm_memtype_ptr_t[size];
    for (uint32_t i = 0; i < size; ++i) {
      bin::name_skip(pos);
      bin::name_skip(pos);
      switch (*pos++) {
        case 0x00: bin::u32_skip(pos); break;
        case 0x01: bin::tabletype_skip(pos); break;
        case 0x02: {
          imports.data[imports.size++] = bin::memtype(pos);
        } break;
        case 0x03: bin::globaltype_skip(pos); break;
        default: assert(false);
      }
    }
  }

  own auto mems = wasm_memtype_vec_empty();
  pos = bin::section(binary, SEC_MEMORY);
  if (pos != nullptr) {
    ++pos;  // section index
    bin::u32_skip(pos);  // section size
    mems.size = bin::u32(pos);
    mems.data = new wasm_memtype_ptr_t[mems.size];
    for (uint32_t i = 0; i < mems.size; ++i) {
      mems.data[i] = bin::memtype(pos);
    }
  }

  own auto v = wasm_memtype_vec_new_uninitialized(imports.size + mems.size);
  uint32_t i = 0;
  for (uint32_t j = 0; j < imports.size; ++j, ++i) v.data[i] = imports.data[j];
  for (uint32_t j = 0; j < mems.size; ++j, ++i) v.data[i] = mems.data[j];
  if (imports.data) delete[] imports.data;
  if (mems.data) delete[] mems.data;

  return v;
}

}  // namespace bin
}  // namespace wasm
