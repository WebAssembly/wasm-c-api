#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "wasm.h"

#define own

wasm_memory_t* get_export_memory(const wasm_extern_vec_t* exports, size_t i) {
  if (exports->size <= i || !wasm_extern_as_memory(exports->data[i])) {
    printf("> Error accessing memory export %zu!\n", i);
    exit(1);
  }
  return wasm_extern_as_memory(exports->data[i]);
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

void check_call(wasm_func_t* func, wasm_val_t args[], int32_t expected) {
  wasm_val_t results[1];
  if (wasm_func_call(func, args, results) || results[0].of.i32 != expected) {
    printf("> Error on result\n");
    exit(1);
  }
}

void check_call0(wasm_func_t* func, int32_t expected) {
  check_call(func, NULL, expected);
}

void check_call1(wasm_func_t* func, int32_t arg, int32_t expected) {
  wasm_val_t args[] = { {.kind = WASM_I32, .of = {.i32 = arg}} };
  check_call(func, args, expected);
}

void check_call2(wasm_func_t* func, int32_t arg1, int32_t arg2, int32_t expected) {
  wasm_val_t args[2] = {
    {.kind = WASM_I32, .of = {.i32 = arg1}},
    {.kind = WASM_I32, .of = {.i32 = arg2}}
  };
  check_call(func, args, expected);
}

void check_ok(wasm_func_t* func, wasm_val_t args[]) {
  if (wasm_func_call(func, args, NULL)) {
    printf("> Error on result, expected empty\n");
    exit(1);
  }
}

void check_ok2(wasm_func_t* func, int32_t arg1, int32_t arg2) {
  wasm_val_t args[2] = {
    {.kind = WASM_I32, .of = {.i32 = arg1}},
    {.kind = WASM_I32, .of = {.i32 = arg2}}
  };
  check_ok(func, args);
}

void check_trap(wasm_func_t* func, wasm_val_t args[]) {
  wasm_val_t results[1];
  own wasm_trap_t* trap = wasm_func_call(func, args, results);
  if (! trap) {
    printf("> Error on result, expected trap\n");
    exit(1);
  }
  wasm_trap_delete(trap);
}

void check_trap1(wasm_func_t* func, int32_t arg) {
  wasm_val_t args[1] = { {.kind = WASM_I32, .of = {.i32 = arg}} };
  check_trap(func, args);
}

void check_trap2(wasm_func_t* func, int32_t arg1, int32_t arg2) {
  wasm_val_t args[2] = {
    {.kind = WASM_I32, .of = {.i32 = arg1}},
    {.kind = WASM_I32, .of = {.i32 = arg2}}
  };
  check_trap(func, args);
}

void load_binary(const char *file_path, wasm_byte_vec_t *binary) {
  // Load binary.
  printf("Loading binary...\n");
  FILE* file = fopen(file_path, "r");
  if (!file) {
    printf("> Error loading module!\n");
    exit(EXIT_FAILURE);
  }
  fseek(file, 0L, SEEK_END);
  size_t file_size = ftell(file);
  fseek(file, 0L, SEEK_SET);
  wasm_byte_vec_new_uninitialized(binary, file_size);
  fread(binary->data, file_size, 1, file);
  fclose(file);
}

void instantiate_wasm(const char *file_path, void (*action)(wasm_instance_t*)) {
  // Initialize.
  printf("Initializing...\n");
  wasm_engine_t* engine = wasm_engine_new();
  wasm_store_t* store = wasm_store_new(engine);

  wasm_byte_vec_t binary;
  load_binary(file_path, &binary);

  // Compile.
  printf("Compiling module...\n");
  own wasm_module_t* module = wasm_module_new(store, &binary);
  if (!module) {
    printf("> Error compiling module!\n");
    exit(EXIT_FAILURE);
  }

  wasm_byte_vec_delete(&binary);

  // Instantiate.
  printf("Instantiating module...\n");
  own wasm_instance_t* instance = wasm_instance_new(store, module, NULL);
  if (!instance) {
    printf("> Error instantiating module!\n");
    exit(EXIT_FAILURE);
  }

  action(instance);

  wasm_module_delete(module);

  wasm_instance_delete(instance);

  // Shut down.
  printf("Shutting down...\n");
  wasm_store_delete(store);
  wasm_engine_delete(engine);

  // All done.
  printf("Done.\n");
}
