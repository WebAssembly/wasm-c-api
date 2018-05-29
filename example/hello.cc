// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "libplatform/libplatform.h"
#include "v8.h"
#include "wasm.h"

// TODO: temporary hacks while we are converting over
extern "C++" {
  v8::Isolate *wasm_store_isolate(wasm_store_t*);
  v8::Local<v8::Context> wasm_store_context(wasm_store_t*);
  v8::Local<v8::Object> wasm_instance_obj(wasm_instance_t*);
  v8::Local<v8::Function> wasm_func_obj(wasm_func_t*);
}

wasm_val_vec_t print_wasm(wasm_val_vec_t args) {
  printf(">");
  for (size_t i = 0; i < args.size; ++i) {
    switch (args.data[i].kind) {
      case WASM_I32_VAL: {
        printf(" %" PRIu32, args.data[i].i32);
      } break;
      case WASM_I64_VAL: {
        printf(" %" PRIu64, args.data[i].i64);
      } break;
      case WASM_F32_VAL: {
        printf(" %f", args.data[i].f32);
      } break;
      case WASM_F64_VAL: {
        printf(" %g", args.data[i].f64);
      } break;
      case WASM_ANYREF_VAL:
      case WASM_FUNCREF_VAL: {
        if (args.data[i].ref == NULL) {
          printf("null");
        } else {
          printf("ref");
        }
      } break;
    }
  }
  printf("\n");

  wasm_val_t results[] = { wasm_i32_val((uint32_t)args.size) };
  return wasm_val_vec(1, results);
}


int main(int argc, const char* argv[]) {
  wasm_init(argc, argv);

  wasm_store_t *store = wasm_store_new();
  v8::Isolate *isolate = wasm_store_isolate(store);
  v8::Local<v8::Context> context = wasm_store_context(store);
  {
    // Create a stack-allocated handle scope.
    v8::HandleScope handle_scope(isolate);

    // Load wasm code.
    FILE *file = fopen("hello.wasm", "r");
    if (!file) {
      printf("Error loading module!\n");
      return 1;
    }

    fseek(file, 0L, SEEK_END);
    size_t file_size = ftell(file);
    fseek(file, 0L, SEEK_SET);
    wasm_byte_vec_t binary = wasm_byte_vec_new_uninitialized(file_size);
    fread(binary.data, file_size, 1, file);
    fclose(file);

    // Compile.
    wasm_module_t *module = wasm_module_new(store, binary);
    wasm_byte_vec_delete(binary);
    if (!module) {
      printf("Error compiling module!\n");
      return 1;
    }

    // Create external print functions.
    auto print_type1 = wasm_functype_new_1_1(wasm_valtype_new_i32(), wasm_valtype_new_i32());
    wasm_func_t* print_func1 = wasm_func_new(store, print_type1, print_wasm);

    auto print_type2 = wasm_functype_new_2_1(wasm_valtype_new_i32(), wasm_valtype_new_i32(), wasm_valtype_new_i32());
    wasm_func_t* print_func2 = wasm_func_new(store, print_type2, print_wasm);

    // Instantiate.
    wasm_extern_t imports[] = {
      wasm_extern_func(print_func1), wasm_extern_func(print_func2)
    };
    wasm_instance_t* instance = wasm_instance_new(store, module, wasm_extern_vec(2, imports));

    // TODO: convert rest to C API

    // Extract export.
    auto exports_obj = wasm_instance_obj(instance);
    v8::Local<v8::String> run_name =
      v8::String::NewFromUtf8(isolate, "run",
                              v8::NewStringType::kNormal).ToLocalChecked();
    auto run =
      v8::Local<v8::Function>::Cast(exports_obj->Get(context, run_name).ToLocalChecked());

    // Call export.
    v8::Local<v8::Value> run_args[] = {
      v8::Integer::New(isolate, 3), v8::Integer::New(isolate, 4)
    };
    v8::Local<v8::Value> result =
      run->Call(context, v8::Undefined(isolate), 2, run_args).ToLocalChecked();

    // Convert the result to an UTF8 string and print it.
    v8::String::Utf8Value utf8(isolate, result);
    printf("%s\n", *utf8);
  }

  // Dispose the isolate and tear down V8.
  wasm_store_delete(store);
  wasm_deinit();
  return 0;
}
