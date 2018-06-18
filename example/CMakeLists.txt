set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")

configure_file(${V8V8}/out.gn/x64.release/natives_blob.bin
               ${CMAKE_CURRENT_BINARY_DIR}/natives_blob.bin
	       COPYONLY)
configure_file(${V8V8}/out.gn/x64.release/snapshot_blob.bin
               ${CMAKE_CURRENT_BINARY_DIR}/snapshot_blob.bin
	       COPYONLY)

function(example name source)
  add_executable (${name} ${source})
  target_include_directories(${name} PRIVATE ${V8_INCLUDE})
  target_link_libraries (${name} ${V8_LIBS} wasm_v8 pthread)
endfunction(example)

example(callback callback.c)
example(hello hello.c)
example(trap trap.c)

example(callback_cpp callback.cc)
example(hello_cpp hello.cc)
example(trap_cpp trap.cc)
