// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "wasm.h"

#define own

// Print a Wasm value
void wasm_val_print(wasm_val_t val) {
  switch (val.kind) {
    case WASM_I32_VAL: {
      printf("%" PRIu32, val.i32);
    } break;
    case WASM_I64_VAL: {
      printf("%" PRIu64, val.i64);
    } break;
    case WASM_F32_VAL: {
      printf("%f", val.f32);
    } break;
    case WASM_F64_VAL: {
      printf("%g", val.f64);
    } break;
    case WASM_ANYREF_VAL:
    case WASM_FUNCREF_VAL: {
      if (val.ref == NULL) {
        printf("null");
      } else {
        printf("ref(%p)", val.ref);
      }
    } break;
  }
}

// A function to be called from Wasm code.
own wasm_val_vec_t print_wasm(wasm_val_vec_t args) {
  printf("Calling back...\n>");
  for (size_t i = 0; i < args.size; ++i) {
    printf(" ");
    wasm_val_print(args.data[i]);
  }
  printf("\n");

  wasm_val_t results[] = { wasm_i32_val((uint32_t)args.size) };
  return wasm_val_vec_new(1, results);
}


int main(int argc, const char* argv[]) {
  // Initialize.
  printf("Initializing...\n");
  wasm_init(argc, argv);
  wasm_store_t* store = wasm_store_new();

  // Load binary.
  printf("Loading binary...\n");
  FILE* file = fopen("hello.wasm", "r");
  if (!file) {
    printf("> Error loading module!\n");
    return 1;
  }
  fseek(file, 0L, SEEK_END);
  size_t file_size = ftell(file);
  fseek(file, 0L, SEEK_SET);
  wasm_byte_vec_t binary = wasm_byte_vec_new_uninitialized(file_size);
  fread(binary.data, file_size, 1, file);
  fclose(file);

  // Compile.
  printf("Compiling module...\n");
  own wasm_module_t* module = wasm_module_new(store, binary);
  if (!module) {
    printf("> Error compiling module!\n");
    return 1;
  }

  wasm_byte_vec_delete(binary);

  // Create external print functions.
  printf("Creating callbacks...\n");
  own wasm_functype_t* print_type1 = wasm_functype_new_1_1(wasm_valtype_new_i32(), wasm_valtype_new_i32());
  own wasm_func_t* print_func1 = wasm_func_new(store, print_type1, print_wasm);

  own wasm_functype_t* print_type2 = wasm_functype_new_2_1(wasm_valtype_new_i32(), wasm_valtype_new_i32(), wasm_valtype_new_i32());
  own wasm_func_t* print_func2 = wasm_func_new(store, print_type2, print_wasm);

  wasm_functype_delete(print_type1);
  wasm_functype_delete(print_type2);

  // Instantiate.
  printf("Instantiating module...\n");
  wasm_extern_t imports[] = {
    wasm_extern_func(print_func1), wasm_extern_func(print_func2)
  };
  own wasm_instance_t* instance = wasm_instance_new(store, module, wasm_extern_vec(2, imports));
  if (!instance) {
    printf("> Error instantiating module!\n");
    return 1;
  }

  // Extract export.
  printf("Extracting exports...\n");
  own wasm_extern_t exp = wasm_instance_export(instance, 0);
  if (exp.kind != WASM_EXTERN_FUNC || exp.func == NULL) {
    printf("> Error accessing export!");
    return 1;
  }
  own wasm_func_t* run_func = exp.func;

  wasm_module_delete(module);
  wasm_instance_delete(instance);

  // Call.
  printf("Calling exports...\n");
  wasm_val_t args[] = {wasm_i32_val(3), wasm_i32_val(4)};
  own wasm_val_vec_t results = wasm_func_call(run_func, wasm_val_vec(2, args));

  // Print result.
  printf("Printing result...\n");
  printf("> %u\n", results.data[0].i32);

  wasm_func_delete(run_func);
  wasm_val_vec_delete(results);

  // Shut down.
  printf("Shutting down...\n");
  wasm_store_delete(store);
  wasm_deinit();

  // All done.
  printf("Done.\n");
  return 0;
}
