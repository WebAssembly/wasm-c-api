#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wasm.h"

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#define own

const int N_THREADS = 10;
const int N_REPS = 3;

// A function to be called from Wasm code.
void hello_callback(const wasm_val_vec_t *args, own wasm_result_t *result) {
  wasm_val_t val = args->data[0];
  switch (val.kind) {
  case WASM_I32: {
    printf("> hello from thread: %d\n", val.of.i32);
  } break;
  default: { printf("> unexpected value kind"); }
  }
  wasm_result_new_empty(result);
}

int run(wasm_store_t *store, wasm_module_t *module, int thread_num) {
  own wasm_functype_t *hello_type =
      wasm_functype_new_1_0(wasm_valtype_new_i32());
  own wasm_func_t *hello_func =
      wasm_func_new(store, hello_type, hello_callback);
  wasm_functype_delete(hello_type);

  wasm_val_t hello_global_val = {.kind = WASM_I32};
  hello_global_val.of.i32 = thread_num;

  // Instantiate.
  wasm_extern_t *externs[] = {
      // func
      wasm_func_as_extern(hello_func),
      // global
      wasm_global_as_extern(wasm_global_new(
          store, wasm_globaltype_new(wasm_valtype_new_i32(), WASM_CONST),
          &hello_global_val)),
  };
  wasm_extern_vec_t imports = {2, externs};
  own wasm_instance_t *instance = wasm_instance_new(store, module, &imports);
  if (!instance) {
    printf("> Error instantiating module!\n");
    return 1;
  }

  // Extract export.
  own wasm_extern_vec_t exports;
  wasm_instance_exports(instance, &exports);
  if (exports.size == 0) {
    printf("> Error accessing exports!\n");
    return 1;
  }
  const wasm_func_t *run_func = wasm_extern_as_func(exports.data[0]);
  if (run_func == NULL) {
    printf("> Error accessing export!\n");
    return 1;
  }

  wasm_instance_delete(instance);

  // Call.
  wasm_val_vec_t args = {0, NULL};
  own wasm_result_t result;
  wasm_func_call(run_func, &args, &result);
  if (result.kind != WASM_RETURN) {
    printf("> Error calling function!\n");
    return 1;
  }

  wasm_extern_vec_delete(&exports);
  wasm_result_delete(&result);

  return 0;
}

int runExample(wasm_store_t *store, wasm_byte_vec_t binary, int thread_num) {
  // Compile.
  own wasm_module_t *module = wasm_module_new(store, &binary);
  if (!module) {
    printf("> Error compiling module!\n");
    return 1;
  }

  // Run the example N times.
  for (int i = 0; i < N_REPS; ++i) {
    usleep(1e5);
    run(store, module, thread_num);
  }

  wasm_module_delete(module);
  return 0;
}

typedef struct thread_args {
  wasm_engine_t *engine;
  wasm_byte_vec_t binary;
  int id;
} thread_args;

void *threadStart(void *args0) {
  thread_args *args = (thread_args *)args0;
  wasm_store_t *store = wasm_store_new(args->engine);
  runExample(store, args->binary, args->id);
  wasm_store_delete(store);
  return NULL;
}

int main(int argc, const char *argv[]) {
  // Initialize.
  wasm_engine_t *engine = wasm_engine_new(argc, argv);

  // Load binary.
  FILE *file = fopen("threads.wasm", "r");
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
    pthread_t pth;
    thread_args *args = malloc(sizeof(thread_args));
    args->id = i;
    args->engine = engine;
    args->binary = binary;
    printf("Initializing thread %d...\n", i);
    pthread_create(&pth, NULL, threadStart, args);
    threads[i] = pth;
  }

  for (int i = 0; i < N_THREADS; i++) {
    printf("Waiting for thread: %d\n", i);
    pthread_t pth = threads[i];
    pthread_join(pth, NULL);
  }

  wasm_byte_vec_delete(&binary);

  wasm_engine_delete(engine);

  return 0;
}
