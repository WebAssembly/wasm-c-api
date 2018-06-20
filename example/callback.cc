#include <iostream>
#include <fstream>
#include <cstdlib>
#include <string>
#include <cinttypes>

#include "wasm.hh"

// Print a Wasm value
auto operator<<(std::ostream& out, const wasm::Val& val) -> std::ostream& {
  switch (val.kind()) {
    case wasm::I32: {
      out << val.i32();
    } break;
    case wasm::I64: {
      out << val.i64();
    } break;
    case wasm::F32: {
      out << val.f32();
    } break;
    case wasm::F64: {
      out << val.f64();
    } break;
    case wasm::ANYREF:
    case wasm::FUNCREF: {
      if (val.ref() == nullptr) {
        out << "null";
      } else {
        out << "ref(" << val.ref() << ")";
      }
    } break;
  }
  return out;
}

// A function to be called from Wasm code.
auto print_callback(const wasm::vec<wasm::Val>& args) -> wasm::Result {
  std::cout << "Calling back..." << std::endl << ">";
  for (size_t i = 0; i < args.size(); ++i) {
    std::cout << " " << args[i];
  }
  std::cout << std::endl;

  int32_t n = args.size();
  return wasm::Result(wasm::Val(n));
}


void run(int argc, const char* argv[]) {
  // Initialize.
  std::cout << "Initializing..." << std::endl;
  auto engine = wasm::Engine::make(argc, argv);
  auto store = wasm::Store::make(engine);

  // Load binary.
  std::cout << "Loading binary..." << std::endl;
  std::ifstream file("callback.wasm");
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

  // Create external print functions.
  std::cout << "Creating callbacks..." << std::endl;
  auto print_type1 = wasm::FuncType::make(
    wasm::vec<wasm::ValType*>::make(wasm::ValType::make(wasm::I32)),
    wasm::vec<wasm::ValType*>::make(wasm::ValType::make(wasm::I32))
  );
  auto print_func1 = wasm::Func::make(store, print_type1, print_callback);

  auto print_type2 = wasm::FuncType::make(
    wasm::vec<wasm::ValType*>::make(wasm::ValType::make(wasm::I32), wasm::ValType::make(wasm::I32)),
    wasm::vec<wasm::ValType*>::make(wasm::ValType::make(wasm::I32))
  );
  auto print_func2 = wasm::Func::make(store, print_type2, print_callback);

  // Instantiate.
  std::cout << "Instantiating module..." << std::endl;
  auto imports = wasm::vec<wasm::Extern*>::make(print_func1, print_func2);
  auto instance = wasm::Instance::make(store, module, imports);
  if (!instance) {
    std::cout << "> Error instantiating module!" << std::endl;
    return;
  }

  // Extract export.
  std::cout << "Extracting export..." << std::endl;
  auto exports = instance->exports();
  if (exports.size() == 0 || exports[0]->kind() != wasm::EXTERN_FUNC || !exports[0]->func()) {
    std::cout << "> Error accessing export!" << std::endl;
    return;
  }
  auto run_func = exports[0]->func();

  // Call.
  std::cout << "Calling export..." << std::endl;
  auto result = run_func->call(wasm::Val(3), wasm::Val(4));
  if (result.kind() != wasm::Result::RETURN) {
    std::cout << "> Error calling function!" << std::endl;
    return;
  }

  // Print result.
  std::cout << "Printing result..." << std::endl;
  std::cout << "> " << result[0].i32() << std::endl;

  // Shut down.
  std::cout << "Shutting down..." << std::endl;
}


int main(int argc, const char* argv[]) {
  run(argc, argv);
  std::cout << "Done." << std::endl;
  return 0;
}

