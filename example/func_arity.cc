#include <string>
#include <unistd.h>

#include "wasm.hh"
#include "example_helpers.hh"

int main(int argc, const char* argv[]) {
  instantiate_wasm("func_arity.wasm", [](wasm::Instance &instance) {
    // Extract export.
    std::cout << "Extracting exports..." << std::endl;
    auto exports = instance.exports();
    const wasm::Func *run_func = get_export_func(exports, 0);
    std::cout
        << "func_arity.wasm run param_arity="
        << run_func->param_arity()
        << " result_arity="
        << run_func->result_arity()
        << std::endl;
    assert(run_func->param_arity() == 1);
    assert(run_func->result_arity() == 0);
  });

  std::cout << "Done." << std::endl;
  return 0;
}

