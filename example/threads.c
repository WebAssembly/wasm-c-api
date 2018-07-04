#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#include "wasm.h"

#define own

const int N_THREADS = 10;
const int N_REPS = 3;

// A function to be called from Wasm code.
void callback(const wasm_val_vec_t* args, own wasm_result_t* result) {
  assert(args->data[0].kind == WASM_I32);
  printf("> Thread %d running\n", args->data[0].of.i32);
  wasm_result_new_empty(result);
}


typedef struct {
  wasm_engine_t* engine;
  wasm_byte_vec_t binary;
  int id;
} thread_args;

void* run(void* args_abs) {
  thread_args* args = (thread_args*)args_abs;

  // Create store.
  wasm_store_t* store = wasm_store_new(args->engine);

  // Compile.
  own wasm_module_t* module = wasm_module_new(store, &args->binary);
  if (!module) {
    printf("> Error compiling module!\n");
    return NULL;
  }

  // Run the example N times.
  for (int i = 0; i < N_REPS; ++i) {
    usleep(100000);

    // Create imports.
    own wasm_functype_t* func_type = wasm_functype_new_1_0(wasm_valtype_new_i32());
    own wasm_func_t* func = wasm_func_new(store, func_type, callback);
    wasm_functype_delete(func_type);

    wasm_val_t val = {.kind = WASM_I32, .of = {.i32 = (int32_t)args->id}};
    own wasm_globaltype_t* global_type =
      wasm_globaltype_new(wasm_valtype_new_i32(), WASM_CONST);
    own wasm_global_t* global = wasm_global_new(store, global_type, &val);
    wasm_globaltype_delete(global_type);

    // Instantiate.
    wasm_extern_t* externs[] = {
      wasm_func_as_extern(func),
      wasm_global_as_extern(global),
    };
    wasm_extern_vec_t imports = {2, externs};
    own wasm_instance_t* instance = wasm_instance_new(store, module, &imports);
    if (!instance) {
      printf("> Error instantiating module!\n");
      return NULL;
    }

    wasm_func_delete(func);
    wasm_global_delete(global);

    // Extract export.
    own wasm_extern_vec_t exports;
    wasm_instance_exports(instance, &exports);
    if (exports.size == 0) {
      printf("> Error accessing exports!\n");
      return NULL;
    }
    const wasm_func_t *run_func = wasm_extern_as_func(exports.data[0]);
    if (run_func == NULL) {
      printf("> Error accessing export!\n");
      return NULL;
    }

    wasm_instance_delete(instance);

    // Call.
    wasm_val_vec_t args = {0, NULL};
    own wasm_result_t result;
    wasm_func_call(run_func, &args, &result);
    if (result.kind != WASM_RETURN) {
      printf("> Error calling function!\n");
      return NULL;
    }

    wasm_extern_vec_delete(&exports);
    wasm_result_delete(&result);
  }

  wasm_module_delete(module);
  wasm_store_delete(store);

  return NULL;
}

int main(int argc, const char *argv[]) {
  // Initialize.
  wasm_engine_t* engine = wasm_engine_new(argc, argv);

  // Load binary.
  FILE* file = fopen("threads.wasm", "r");
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

  pthread_t threads[N_THREADS];
  for (int i = 0; i < N_THREADS; i++) {
    thread_args* args = malloc(sizeof(thread_args));
    args->id = i;
    args->engine = engine;
    args->binary = binary;
    printf("Initializing thread %d...\n", i);
    pthread_create(&threads[i], NULL, &run, args);
  }

  for (int i = 0; i < N_THREADS; i++) {
    printf("Waiting for thread: %d\n", i);
    pthread_join(threads[i], NULL);
  }

  wasm_byte_vec_delete(&binary);
  wasm_engine_delete(engine);

  return 0;
}
