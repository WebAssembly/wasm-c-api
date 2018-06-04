cmake_minimum_required(VERSION 3.9)
cmake_policy(SET CMP0048 NEW)
set(CMAKE_CXX_COMPILER "clang++")
set(CMAKE_C_COMPILER "clang")
project(wasm_c_api VERSION 0.0.1)

set(V8V8 ${CMAKE_SOURCE_DIR}/v8/v8)
set(V8_LIBS
	-Wl,--start-group
	${V8V8}/out.gn/x64.release/obj/libv8_base.a
	${V8V8}/out.gn/x64.release/obj/libv8_libbase.a
	${V8V8}/out.gn/x64.release/obj/libv8_external_snapshot.a
	${V8V8}/out.gn/x64.release/obj/libv8_libplatform.a
	${V8V8}/out.gn/x64.release/obj/libv8_libsampler.a
	${V8V8}/out.gn/x64.release/obj/third_party/icu/libicuuc.a
	${V8V8}/out.gn/x64.release/obj/third_party/icu/libicui18n.a
	${V8V8}/out.gn/x64.release/obj/src/inspector/libinspector.a
	-Wl,--end-group)

set(V8_INCLUDE
	${CMAKE_SOURCE_DIR}/include
	${V8V8}
	${V8V8}/include)

add_subdirectory(v8-samples)
add_subdirectory(src)
add_subdirectory(example)
