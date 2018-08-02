#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "wasm.h"

#define own

// A function to be called from Wasm code.
void neg_callback(const wasm_val_vec_t* args, own wasm_result_t* result) {
  printf("Calling back...\n");
  wasm_val_t vals[1];
  vals[0].kind = WASM_I32;
  vals[0].of.i32 = -args->data[0].of.i32;
  wasm_result_new_vals(result, 1, vals);
}


wasm_table_t* get_export_table(const wasm_extern_vec_t* exports, size_t i) {
  if (exports->size <= i || !wasm_extern_as_table(exports->data[i])) {
    printf("> Error accessing table export %zu!\n", i);
    exit(1);
  }
  return wasm_extern_as_table(exports->data[i]);
}

wasm_func_t* get_export_func(const wasm_extern_vec_t* exports, size_t i) {
  if (exports->size <= i || !wasm_extern_as_func(exports->data[i])) {
    printf("> Error accessing function export %zu!\n", i);
    exit(1);
  }
  return wasm_extern_as_func(exports->data[i]);
}


void check(bool success) {
  if (!success) {
    printf("> Error, expected success\n");
    exit(1);
  }
}

void check_table(wasm_table_t* table, int32_t i, bool expect_set) {
  own wasm_ref_t* ref = wasm_table_get(table, i);
  check((ref != NULL) == expect_set);
  if (ref) wasm_ref_delete(ref);
}

void check_call(wasm_func_t* func, int32_t arg1, int32_t arg2, int32_t expected) {
  wasm_val_t vals[2] = {
    {.kind = WASM_I32, .of = {.i32 = arg1}},
    {.kind = WASM_I32, .of = {.i32 = arg2}}
  };
  wasm_val_vec_t args = {2, vals};
  wasm_result_t result;
  wasm_func_call(func, &args, &result);
  if (result.kind != WASM_RETURN || result.of.vals.size != 1 || result.of.vals.data[0].of.i32 != expected) {
    printf("> Error on result\n");
    exit(1);
  }
  wasm_result_delete(&result);
}

void check_trap(wasm_func_t* func, int32_t arg1, int32_t arg2) {
  wasm_val_t vals[2] = {
    {.kind = WASM_I32, .of = {.i32 = arg1}},
    {.kind = WASM_I32, .of = {.i32 = arg2}}
  };
  wasm_val_vec_t args = {2, vals};
  wasm_result_t result;
  wasm_func_call(func, &args, &result);
  if (result.kind != WASM_TRAP) {
    printf("> Error on result, expected trap\n");
    exit(1);
  }
  wasm_result_delete(&result);
}

void test_table_grow_beyond_max_limit(wasm_store_t *store, wasm_func_t *f) {
  printf("Growing table beyond max limit...\n");
  wasm_limits_t l = { .min = 1, .max = 2 };
  wasm_tabletype_t *tt = wasm_tabletype_new(wasm_valtype_new(WASM_FUNCREF), &l);
  wasm_table_t *t = wasm_table_new(store, tt, wasm_func_as_ref(f));
  check(!wasm_table_grow(t, 3));
}

void test_table_new_null_ref(wasm_store_t *store) {
  printf("Creating table with NULL ref...\n");
  wasm_limits_t l = { .min = 1, .max = 2 };
  wasm_tabletype_t *tt = wasm_tabletype_new(wasm_valtype_new(WASM_FUNCREF), &l);
  wasm_table_t *t = wasm_table_new(store, tt, NULL);
}

int main(int argc, const char* argv[]) {
  // Initialize.
  printf("Initializing...\n");
  wasm_engine_t* engine = wasm_engine_new();
  wasm_store_t* store = wasm_store_new(engine);

  // Load binary.
  printf("Loading binary...\n");
  FILE* file = fopen("table.wasm", "r");
  if (!file) {
    printf("> Error loading module!\n");
    return 1;
  }
  fseek(file, 0L, SEEK_END);
  size_t file_size = ftell(file);
  fseek(file, 0L, SEEK_SET);
  wasm_byte_vec_t binary;
  wasm_byte_vec_new_uninitialized(&binary, file_size);
  fread(binary.data, file_size, 1, file);
  fclose(file);

  // Compile.
  printf("Compiling module...\n");
  own wasm_module_t* module = wasm_module_new(store, &binary);
  if (!module) {
    printf("> Error compiling module!\n");
    return 1;
  }

  wasm_byte_vec_delete(&binary);

  // Instantiate.
  printf("Instantiating module...\n");
  wasm_extern_vec_t imports = { 0, NULL };
  own wasm_instance_t* instance = wasm_instance_new(store, module, &imports);
  if (!instance) {
    printf("> Error instantiating module!\n");
    return 1;
  }

  // Extract export.
  printf("Extracting exports...\n");
  own wasm_extern_vec_t exports;
  wasm_instance_exports(instance, &exports);
  size_t i = 0;
  wasm_table_t* table = get_export_table(&exports, i++);
  wasm_func_t* call_indirect = get_export_func(&exports, i++);
  wasm_func_t* f = get_export_func(&exports, i++);
  wasm_func_t* g = get_export_func(&exports, i++);

  wasm_module_delete(module);

  // Create external function.
  printf("Creating callback...\n");
  own wasm_functype_t* neg_type = wasm_functype_new_1_1(wasm_valtype_new_i32(), wasm_valtype_new_i32());
  own wasm_func_t* h = wasm_func_new(store, neg_type, neg_callback);

  wasm_functype_delete(neg_type);

  // Check initial table.
  printf("Checking table...\n");
  check(wasm_table_size(table) == 2);
  check_table(table, 0, false);
  check_table(table, 1, true);
  check_trap(call_indirect, 0, 0);
  check_call(call_indirect, 7, 1, 7);
  check_trap(call_indirect, 0, 2);

  // Mutate table.
  printf("Mutating table...\n");
  check(wasm_table_set(table, 0, wasm_func_as_ref(g)));
  check(wasm_table_set(table, 1, NULL));
  check(! wasm_table_set(table, 2, wasm_func_as_ref(f)));
  check_table(table, 0, true);
  check_table(table, 1, false);
  check_call(call_indirect, 7, 0, 666);
  check_trap(call_indirect, 0, 1);
  check_trap(call_indirect, 0, 2);

  // Grow table.
  printf("Growing table...\n");
  check(wasm_table_grow(table, 3));
  check(wasm_table_size(table) == 5);
  check(wasm_table_set(table, 2, wasm_func_as_ref(f)));
  check(wasm_table_set(table, 3, wasm_func_as_ref(h)));
  check(! wasm_table_set(table, 5, NULL));
  check_table(table, 2, true);
  check_table(table, 3, true);
  check_table(table, 4, false);
  check_call(call_indirect, 5, 2, 5);
  check_call(call_indirect, 6, 3, -6);
  check_trap(call_indirect, 0, 4);
  check_trap(call_indirect, 0, 5);

  test_table_grow_beyond_max_limit(store, f);
  test_table_new_null_ref(store);

  wasm_func_delete(h);
  wasm_extern_vec_delete(&exports);
  wasm_instance_delete(instance);

  // Shut down.
  printf("Shutting down...\n");
  wasm_store_delete(store);
  wasm_engine_delete(engine);

  // All done.
  printf("Done.\n");
  return 0;
}
