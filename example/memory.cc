#include <iostream>
#include <fstream>
#include <cstdlib>
#include <string>
#include <cinttypes>

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

void check_ok(const wasm::Result& result) {
  if (result.kind() != wasm::Result::RETURN) {
    std::cout << "> Error on result, expected return" << std::endl;
    exit(1);
  }
}

void check_trap(const wasm::Result& result) {
  if (result.kind() != wasm::Result::TRAP) {
    std::cout << "> Error on result, expected trap" << std::endl;
    exit(1);
  }
}

void check(bool success) {
  if (!success) {
    std::cout << "> Error, expected success" << std::endl;
    exit(1);
  }
}

void run() {
  // Initialize.
  std::cout << "Initializing..." << std::endl;
  auto engine = wasm::Engine::make();
  auto store_ = wasm::Store::make(engine.get());
  auto store = store_.get();

  // Load binary.
  std::cout << "Loading binary..." << std::endl;
  std::ifstream file("memory.wasm");
  file.seekg(0, std::ios_base::end);
  auto file_size = file.tellg();
  file.seekg(0);
  auto binary = wasm::vec<byte_t>::make_uninitialized(file_size);
  file.read(binary.get(), file_size);
  file.close();
  if (file.fail()) {
    std::cout << "> Error loading module!" << std::endl;
    return;
  }

  // Compile.
  std::cout << "Compiling module..." << std::endl;
  auto module = wasm::Module::make(store, binary);
  if (!module) {
    std::cout << "> Error compiling module!" << std::endl;
    return;
  }

  // Instantiate.
  std::cout << "Instantiating module..." << std::endl;
  auto imports = wasm::vec<wasm::Extern*>::make();
  auto instance = wasm::Instance::make(store, module.get(), imports);
  if (!instance) {
    std::cout << "> Error instantiating module!" << std::endl;
    return;
  }

  // Extract export.
  std::cout << "Extracting exports..." << std::endl;
  auto exports = instance->exports();
  size_t i = 0;
  auto memory = get_export_memory(exports, i++);
  auto size_func = get_export_func(exports, i++);
  auto load_func = get_export_func(exports, i++);
  auto store_func = get_export_func(exports, i++);

  // Check initial memory.
  std::cout << "Checking memory..." << std::endl;
  check(memory->size(), 2);
  check(memory->data_size(), 0x20000);
  check(memory->data()[0], 0);
  check(memory->data()[0x1000], 1);
  check(memory->data()[0x1003], 4);

  check(size_func->call()[0].i32(), 2);
  check(load_func->call(wasm::Val::i32(0))[0].i32(), 0);
  check(load_func->call(wasm::Val::i32(0x1000))[0].i32(), 1);
  check(load_func->call(wasm::Val::i32(0x1003))[0].i32(), 4);
  check(load_func->call(wasm::Val::i32(0x1ffff))[0].i32(), 0);
  check_trap(load_func->call(wasm::Val::i32(0x20000)));

  // Mutate memory.
  std::cout << "Mutating memory..." << std::endl;
  memory->data()[0x1003] = 5;
  check_ok(store_func->call(wasm::Val::i32(0x1002), wasm::Val::i32(6)));
  check_trap(store_func->call(wasm::Val::i32(0x20000), wasm::Val::i32(0)));

  check(memory->data()[0x1002], 6);
  check(memory->data()[0x1003], 5);
  check(load_func->call(wasm::Val::i32(0x1002))[0].i32(), 6);
  check(load_func->call(wasm::Val::i32(0x1003))[0].i32(), 5);

  // Grow memory.
  std::cout << "Growing memory..." << std::endl;
  check(memory->grow(1));
  check(memory->size(), 3);
  check(memory->data_size(), 0x30000);

  check_ok(load_func->call(wasm::Val::i32(0x20000)));
  check_ok(store_func->call(wasm::Val::i32(0x20000), wasm::Val::i32(0)));
  check_trap(load_func->call(wasm::Val::i32(0x30000)));
  check_trap(store_func->call(wasm::Val::i32(0x30000), wasm::Val::i32(0)));

  // Shut down.
  std::cout << "Shutting down..." << std::endl;
}


int main(int argc, const char* argv[]) {
  run();
  std::cout << "Done." << std::endl;
  return 0;
}

