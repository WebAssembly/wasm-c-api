#include <iostream>
#include <fstream>
#include <cstdlib>
#include <string>
#include <cinttypes>

#include "wasm.hh"


// A function to be called from Wasm code.
auto hello_callback(const wasm::vec<wasm::Val>& args) -> wasm::Result {
  std::cout << "Calling back..." << std::endl;
  std::cout << "> Hello world!" << std::endl;
  return wasm::Result();
}


// TODO(wasm+): use these until V8/JS can handle i64 paramaeters and results.
auto i64_reinterpret_f64(float64_t x) -> int64_t {
  return *reinterpret_cast<int64_t*>(&x);
}

auto f64_reinterpret_i64(int64_t x) -> float64_t {
  return *reinterpret_cast<float64_t*>(&x);
}

auto get_export_global(wasm::vec<wasm::Extern*>& exports, size_t i) -> wasm::Global* {
  if (exports.size() <= i || exports[i]->kind() != wasm::EXTERN_GLOBAL || !exports[i]->global()) {
    std::cout << "> Error accessing export!" << std::endl;
    exit(1);
  }
  return exports[i]->global();
}

auto get_export_func(const wasm::vec<wasm::Extern*>& exports, size_t i) -> const wasm::Func* {
  if (exports.size() <= i || exports[i]->kind() != wasm::EXTERN_FUNC || !exports[i]->func()) {
    std::cout << "> Error accessing export!" << std::endl;
    exit(1);
  }
  return exports[i]->func();
}

template<class T, class U>
void check(T actual, U expected) {
  if (actual != expected) {
    std::cout << "> Error reading value, expected " << expected << ", got " << actual << std::endl;
    exit(1);
  }
}

void run(int argc, const char* argv[]) {
  // Initialize.
  std::cout << "Initializing..." << std::endl;
  auto engine = wasm::Engine::make(argv[0]);
  auto store = wasm::Store::make(engine);

  // Load binary.
  std::cout << "Loading binary..." << std::endl;
  std::ifstream file("global.wasm");
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

  // Create external globals.
  std::cout << "Creating globals..." << std::endl;
  auto const_f32_type = wasm::GlobalType::make(
    wasm::ValType::make(wasm::F32), wasm::CONST);
  auto const_i64_type = wasm::GlobalType::make(
    wasm::ValType::make(wasm::I64), wasm::CONST);
  auto var_f32_type = wasm::GlobalType::make(
    wasm::ValType::make(wasm::F32), wasm::VAR);
  auto var_i64_type = wasm::GlobalType::make(
    wasm::ValType::make(wasm::I64), wasm::VAR);
  auto const_f32_import = wasm::Global::make(store, const_f32_type, wasm::Val::f32(1));
  auto const_i64_import = wasm::Global::make(store, const_i64_type, wasm::Val::i64(2));
  auto var_f32_import = wasm::Global::make(store, var_f32_type, wasm::Val::f32(3));
  auto var_i64_import = wasm::Global::make(store, var_i64_type, wasm::Val::i64(4));

  // Instantiate.
  std::cout << "Instantiating module..." << std::endl;
  auto imports = wasm::vec<wasm::Extern*>::make(
    const_f32_import->copy(), const_i64_import->copy(),
    var_f32_import->copy(), var_i64_import->copy()
  );
  auto instance = wasm::Instance::make(store, module, imports);
  if (!instance) {
    std::cout << "> Error instantiating module!" << std::endl;
    return;
  }

  // Extract export.
  std::cout << "Extracting exports..." << std::endl;
  auto exports = instance->exports();
  size_t i = 0;
  auto const_f32_export = get_export_global(exports, i++);
  auto const_i64_export = get_export_global(exports, i++);
  auto var_f32_export = get_export_global(exports, i++);
  auto var_i64_export = get_export_global(exports, i++);
  auto get_const_f32_import = get_export_func(exports, i++);
  auto get_const_i64_import = get_export_func(exports, i++);
  auto get_var_f32_import = get_export_func(exports, i++);
  auto get_var_i64_import = get_export_func(exports, i++);
  auto get_const_f32_export = get_export_func(exports, i++);
  auto get_const_i64_export = get_export_func(exports, i++);
  auto get_var_f32_export = get_export_func(exports, i++);
  auto get_var_i64_export = get_export_func(exports, i++);
  auto set_var_f32_import = get_export_func(exports, i++);
  auto set_var_i64_import = get_export_func(exports, i++);
  auto set_var_f32_export = get_export_func(exports, i++);
  auto set_var_i64_export = get_export_func(exports, i++);

  // Interact.
  std::cout << "Accessing globals..." << std::endl;

  // Check initial values.
  check(const_f32_import->get().f32(), 1);
  check(const_i64_import->get().i64(), 2);
  check(var_f32_import->get().f32(), 3);
  check(var_i64_import->get().i64(), 4);
  check(const_f32_export->get().f32(), 5);
  check(const_i64_export->get().i64(), 6);
  check(var_f32_export->get().f32(), 7);
  check(var_i64_export->get().i64(), 8);

  check(get_const_f32_import->call()[0].f32(), 1);
  check(get_const_i64_import->call()[0].f64(), f64_reinterpret_i64(2));
  // TODO(v8): mutable imports don't work yet in 6.8
  //check(get_var_f32_import->call()[0].f32(), 3);
  //check(get_var_i64_import->call()[0].f64(), f64_reinterpret_i64(4));
  check(get_const_f32_export->call()[0].f32(), 5);
  check(get_const_i64_export->call()[0].f64(), f64_reinterpret_i64(6));
  check(get_var_f32_export->call()[0].f32(), 7);
  check(get_var_i64_export->call()[0].f64(), f64_reinterpret_i64(8));

  // Modify variables through API and check again.
  var_f32_import->set(wasm::Val::f32(33));
  var_i64_import->set(wasm::Val::i64(34));
  var_f32_export->set(wasm::Val::f32(37));
  var_i64_export->set(wasm::Val::i64(38));

  check(var_f32_import->get().f32(), 33);
  check(var_i64_import->get().i64(), 34);
  check(var_f32_export->get().f32(), 37);
  check(var_i64_export->get().i64(), 38);

  // TODO(v8): mutable imports don't work yet in 6.8
  //check(get_var_f32_import->call()[0].f32(), 33);
  //check(get_var_i64_import->call()[0].f64(), f64_reinterpret_i64(34));
  check(get_var_f32_export->call()[0].f32(), 37);
  check(get_var_i64_export->call()[0].f64(), f64_reinterpret_i64(38));

  // Modify variables through calls and check again.
  // TODO(v8): mutable imports don't work yet in 6.8
  //set_var_f32_import->call(wasm::Val::f32(73));
  //set_var_i64_import->call(wasm::Val::f64(f64_reinterpret_i64(74)));
  set_var_f32_export->call(wasm::Val::f32(77));
  set_var_i64_export->call(wasm::Val::f64(f64_reinterpret_i64(78)));

  // TODO(v8): mutable imports don't work yet in 6.8
  //check(var_f32_import->get().f32(), 73);
  //check(var_i64_import->get().i64(), 74);
  check(var_f32_export->get().f32(), 77);
  check(var_i64_export->get().i64(), 78);

  // TODO(v8): mutable imports don't work yet in 6.8
  //check(get_var_f32_import->call()[0].f32(), 73);
  //check(get_var_i64_import->call()[0].f64(), f64_reinterpret_i64(74));
  check(get_var_f32_export->call()[0].f32(), 77);
  check(get_var_i64_export->call()[0].f64(), f64_reinterpret_i64(78));

  // Shut down.
  std::cout << "Shutting down..." << std::endl;
}


int main(int argc, const char* argv[]) {
  run(argc, argv);
  std::cout << "Done." << std::endl;
  return 0;
}

