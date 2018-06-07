SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")

add_executable (hello hello.c)
target_include_directories(hello PRIVATE ${V8_INCLUDE})
target_link_libraries (hello ${V8_LIBS} wasm_v8 pthread)
configure_file(${V8V8}/out.gn/x64.release/natives_blob.bin
               ${CMAKE_CURRENT_BINARY_DIR}/natives_blob.bin
	       COPYONLY)
configure_file(${V8V8}/out.gn/x64.release/snapshot_blob.bin
               ${CMAKE_CURRENT_BINARY_DIR}/snapshot_blob.bin
	       COPYONLY)
