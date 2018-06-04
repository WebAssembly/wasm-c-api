SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")

add_library(wasm_v8 STATIC wasm-v8-lowlevel.cc wasm-bin.cc wasm-v8.cc wasm-bin.hh wasm-v8-lowlevel.hh)
target_include_directories(wasm_v8 PRIVATE ${V8_INCLUDE} ${V8V8}/out.gn/x64.release/gen ${V8V8}/src)
target_link_libraries (wasm_v8 ${V8_LIBS})
