#include <iostream>
#include <fstream>
#include <cstdlib>
#include <string>
#include <cinttypes>

#include "wasm.hh"


// A function to be called from Wasm code.
auto neg_callback(const wasm::vec<wasm::Val>& args) -> wasm::Result {
  std::cout << "Calling back..." << std::endl;
  return wasm::Result(wasm::Val(-args[0].i32()));
}


auto get_export_table(wasm::vec<wasm::Extern*>& exports, size_t i) -> wasm::Table* {
  if (exports.size() <= i || !exports[i]->table()) {
    std::cout << "> Error accessing table export " << i << "!" << std::endl;
    exit(1);
  }
  return exports[i]->table();
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

void run(int argc, const char* argv[]) {
  // Initialize.
  std::cout << "Initializing..." << std::endl;
  auto engine = wasm::Engine::make(argc, argv);
  auto store_ = wasm::Store::make(engine.get());
  auto store = store_.get();

  // Load binary.
  std::cout << "Loading binary..." << std::endl;
  std::ifstream file("table.wasm");
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
  auto table = get_export_table(exports, i++);
  auto call_indirect = get_export_func(exports, i++);
  auto f = get_export_func(exports, i++);
  auto g = get_export_func(exports, i++);

  // Create external function.
  std::cout << "Creating callback..." << std::endl;
  auto neg_type = wasm::FuncType::make(
    wasm::vec<wasm::ValType*>::make(wasm::ValType::make(wasm::I32)),
    wasm::vec<wasm::ValType*>::make(wasm::ValType::make(wasm::I32))
  );
  auto h = wasm::Func::make(store, neg_type.get(), neg_callback);

  // Check initial table.
  std::cout << "Checking table..." << std::endl;
  check(table->size(), 2);
  check_trap(call_indirect->call(wasm::Val::i32(0), wasm::Val::i32(0)));
  check(call_indirect->call(wasm::Val::i32(7), wasm::Val::i32(1))[0].i32(), 7);
  check_trap(call_indirect->call(wasm::Val::i32(0), wasm::Val::i32(2)));

  // Mutate table.
  std::cout << "Mutating table..." << std::endl;
  check(table->set(0, g));
  check(table->set(1, nullptr));
  check(! table->set(2, f));
  check(call_indirect->call(wasm::Val::i32(7), wasm::Val::i32(0))[0].i32(), 666);
  check_trap(call_indirect->call(wasm::Val::i32(0), wasm::Val::i32(1)));
  check_trap(call_indirect->call(wasm::Val::i32(0), wasm::Val::i32(2)));

  // Grow table.
  std::cout << "Growing table..." << std::endl;
  check(table->grow(3));
  check(table->size(), 5);
  check(table->set(2, f));
  check(table->set(3, h.get()));
  check(! table->set(5, nullptr));
  check(call_indirect->call(wasm::Val::i32(5), wasm::Val::i32(2))[0].i32(), 5);
  check(call_indirect->call(wasm::Val::i32(6), wasm::Val::i32(3))[0].i32(), -6);
  check_trap(call_indirect->call(wasm::Val::i32(0), wasm::Val::i32(4)));
  check_trap(call_indirect->call(wasm::Val::i32(0), wasm::Val::i32(5)));

  // Shut down.
  std::cout << "Shutting down..." << std::endl;
}


int main(int argc, const char* argv[]) {
  run(argc, argv);
  std::cout << "Done." << std::endl;
  return 0;
}

