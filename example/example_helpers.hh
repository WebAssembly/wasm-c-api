#include <iostream>
#include <fstream>
#include <cstdlib>
#include <string>
#include <cinttypes>
#include <cstdio>
#include <functional>

#include "wasm.hh"

auto get_export_memory(wasm::vec<wasm::Extern*>& exports, size_t i) -> wasm::Memory* {
  if (exports.size() <= i || !exports[i]->memory()) {
    std::cout << "> Error accessing memory export " << i << "!" << std::endl;
    exit(1);
  }
  return exports[i]->memory();
}

auto get_export_func(const wasm::vec<wasm::Extern*>& exports, size_t i) -> const wasm::Func* {
  if (exports.size() <= i || !exports[i]->func()) {
    std::cout << "> Error accessing function export " << i << "!" << std::endl;
    exit(1);
  }
  return exports[i]->func();
}

template<class T, class U>
void check(T actual, U expected) {
  if (actual != expected) {
    std::cout << "> Error on result, expected " << expected << ", got " << actual << std::endl;
    exit(1);
  }
}

template<class... Args>
void check_ok(const wasm::Func* func, Args... xs) {
  wasm::Val args[] = {wasm::Val::i32(xs)...};
  if (func->call(args)) {
    std::cout << "> Error on result, expected return" << std::endl;
    exit(1);
  }
}

template<class... Args>
void check_trap(const wasm::Func* func, Args... xs) {
  wasm::Val args[] = {wasm::Val::i32(xs)...};
  if (! func->call(args)) {
    std::cout << "> Error on result, expected trap" << std::endl;
    exit(1);
  }
}

template<class... Args>
auto call(const wasm::Func* func, Args... xs) -> int32_t {
  wasm::Val args[] = {wasm::Val::i32(xs)...};
  wasm::Val results[1];
  if (func->call(args, results)) {
    std::cout << "> Error on result, expected return" << std::endl;
    exit(1);
  }
  return results[0].i32();
}

wasm::vec<byte_t> load_binary(const char *file_path) {
  // Load binary.
  std::cout << "Loading binary..." << std::endl;
  std::ifstream file(file_path);
  file.seekg(0, std::ios_base::end);
  auto file_size = file.tellg();
  file.seekg(0);
  auto binary = wasm::vec<byte_t>::make_uninitialized(file_size);
  file.read(binary.get(), file_size);
  file.close();
  if (file.fail()) {
    std::cout << "> Error loading module!" << std::endl;
    exit(EXIT_FAILURE);
  }
  return binary;
}

template <typename F>
void instantiate_wasm(const char *file_path, F action) {
  // Initialize.
  std::cout << "Initializing..." << std::endl;
  auto engine = wasm::Engine::make();
  auto store_ = wasm::Store::make(engine.get());
  auto store = store_.get();

  // Load binary.
  auto binary = load_binary(file_path);

  // Compile.
  std::cout << "Compiling module..." << std::endl;
  auto module = wasm::Module::make(store, binary);
  if (!module) {
    std::cout << "> Error compiling module!" << std::endl;
    exit(EXIT_FAILURE);
  }

  // Instantiate.
  std::cout << "Instantiating module..." << std::endl;
  std::unique_ptr<wasm::Instance> instance = wasm::Instance::make(store, module.get(), nullptr);
  if (!instance) {
    std::cout << "> Error instantiating module!" << std::endl;
    exit(EXIT_FAILURE);
  }
  action(*instance);
}
