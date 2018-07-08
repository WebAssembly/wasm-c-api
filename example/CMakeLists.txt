set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")

configure_file(${V8_OUT}/natives_blob.bin
               ${CMAKE_CURRENT_BINARY_DIR}/natives_blob.bin
	           COPYONLY)
configure_file(${V8_OUT}/snapshot_blob.bin
               ${CMAKE_CURRENT_BINARY_DIR}/snapshot_blob.bin
	           COPYONLY)

file(GLOB WASM *.wasm)
foreach(f ${WASM})
  configure_file(${f} ${CMAKE_CURRENT_BINARY_DIR} COPYONLY)
endforeach(f)

function(example name source)
  add_executable (${name} ${source})
  target_include_directories(${name} PRIVATE ${V8_INCLUDE})
  target_link_libraries (${name} ${V8_LIBS} wasm_v8 pthread)
endfunction(example)

file(GLOB SRCS *.c *.cc)

foreach(f ${SRCS})
  get_filename_component(ext ${f} EXT)
  get_filename_component(name ${f} NAME_WE)
  if(${ext} STREQUAL ".c")
    example(${name} ${f})
  else()
    example(${name}_cpp ${f})
  endif()
endforeach(f)
