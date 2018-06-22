#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "wasm.h"

#define own

// Print a Wasm value
void wasm_val_print(wasm_val_t val) {
  switch (val.kind) {
    case WASM_I32: {
      printf("%" PRIu32, val.of.i32);
    } break;
    case WASM_I64: {
      printf("%" PRIu64, val.of.i64);
    } break;
    case WASM_F32: {
      printf("%f", val.of.f32);
    } break;
    case WASM_F64: {
      printf("%g", val.of.f64);
    } break;
    case WASM_ANYREF:
    case WASM_FUNCREF: {
      if (val.of.ref == NULL) {
        printf("null");
      } else {
        printf("ref(%p)", val.of.ref);
      }
    } break;
  }
}

// A function to be called from Wasm code.
void print_callback(const wasm_val_vec_t* args, own wasm_result_t* result) {
  printf("Calling back...\n>");
  for (size_t i = 0; i < args->size; ++i) {
    printf(" ");
    wasm_val_print(args->data[i]);
  }
  printf("\n");

  wasm_val_t vals[1];
  vals[0].kind = WASM_I32;
  vals[0].of.i32 = (int32_t)args->size;
  wasm_result_new_vals(result, 1, vals);
}


int main(int argc, const char* argv[]) {
  // Initialize.
  printf("Initializing...\n");
  wasm_engine_t* engine = wasm_engine_new(argv[0]);
  wasm_store_t* store = wasm_store_new(engine);

  // Load binary.
  printf("Loading binary...\n");
  FILE* file = fopen("callback.wasm", "r");
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

  // Create external print functions.
  printf("Creating callbacks...\n");
  own wasm_functype_t* print_type1 = wasm_functype_new_1_1(wasm_valtype_new_i32(), wasm_valtype_new_i32());
  own wasm_func_t* print_func1 = wasm_func_new(store, print_type1, print_callback);

  own wasm_functype_t* print_type2 = wasm_functype_new_2_1(wasm_valtype_new_i32(), wasm_valtype_new_i32(), wasm_valtype_new_i32());
  own wasm_func_t* print_func2 = wasm_func_new(store, print_type2, print_callback);

  wasm_functype_delete(print_type1);
  wasm_functype_delete(print_type2);

  // Instantiate.
  printf("Instantiating module...\n");
  wasm_extern_t* externs[] = {
    wasm_func_as_extern(print_func1), wasm_func_as_extern(print_func2)
  };
  wasm_extern_vec_t imports = { 2, externs };
  own wasm_instance_t* instance = wasm_instance_new(store, module, &imports);
  if (!instance) {
    printf("> Error instantiating module!\n");
    return 1;
  }

  // Extract export.
  printf("Extracting export...\n");
  own wasm_extern_vec_t exports;
  wasm_instance_exports(instance, &exports);
  if (exports.size == 0) {
    printf("> Error accessing exports!\n");
    return 1;
  }
  const wasm_func_t* run_func = wasm_extern_as_func(exports.data[0]);
  if (run_func == NULL) {
    printf("> Error accessing export!\n");
    return 1;
  }

  wasm_module_delete(module);
  wasm_instance_delete(instance);

  // Call.
  printf("Calling export...\n");
  wasm_val_t vals[2];
  vals[0].kind = WASM_I32;
  vals[0].of.i32 = 3;
  vals[1].kind = WASM_I32;
  vals[1].of.i32 = 4;
  wasm_val_vec_t args = { 2, vals };
  own wasm_result_t result;
  wasm_func_call(run_func, &args, &result);
  if (result.kind != WASM_RETURN) {
    printf("> Error calling function!\n");
    return 1;
  }

  wasm_extern_vec_delete(&exports);

  // Print result.
  printf("Printing result...\n");
  printf("> %u\n", result.of.vals.data[0].of.i32);

  wasm_result_delete(&result);

  // Shut down.
  printf("Shutting down...\n");
  wasm_store_delete(store);
  wasm_engine_delete(engine);

  // All done.
  printf("Done.\n");
  return 0;
}
