set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -fPIC")

set(SRCS wasm-c.cc wasm-v8.cc wasm-bin.cc)
add_library(wasm_v8 SHARED ${SRCS})
target_include_directories(wasm_v8 PRIVATE ${V8_INCLUDE} ${V8_OUT}/gen ${V8V8}/src)
target_link_libraries (wasm_v8 ${V8_LIBS})
