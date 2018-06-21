set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -fPIC")

file(GLOB SRCS *.cc *.hh)
add_library(wasm_v8 SHARED ${SRCS})
target_include_directories(wasm_v8 PRIVATE ${V8_INCLUDE} ${V8V8}/out.gn/x64.release/gen ${V8V8}/src)
target_link_libraries (wasm_v8 ${V8_LIBS})
