#include "example_helpers.h"
#include "wasm.h"

void run(own wasm_instance_t *instance) {
  own wasm_extern_vec_t exports;
  wasm_instance_exports(instance, &exports);
  wasm_func_t* run_func = get_export_func(&exports, 0);
  size_t nParams = wasm_func_param_arity(run_func);
  size_t nResults = wasm_func_result_arity(run_func);
  printf("func_arity.wasm run param_arity=%zu result_arity=%zu\n", nParams, nResults);
  assert(nParams == 1);
  assert(nResults == 0);
  wasm_extern_vec_delete(&exports);
}

int main() {
    instantiate_wasm("func_arity.wasm", run);
    return 0;
}
