cmake_minimum_required(VERSION 3.9)
cmake_policy(SET CMP0048 NEW)
set(CMAKE_CXX_COMPILER "clang++")
set(CMAKE_C_COMPILER "clang")
project(wasm_c_api VERSION 0.0.1)

set(V8V8 ${CMAKE_SOURCE_DIR}/v8/v8)
set(V8_ARCH "x64")
set(V8_MODE "debug")
set(V8_BUILD "${V8_ARCH}.${V8_MODE}")
set(V8_OUT "${V8V8}/out.gn/${V8_BUILD}")
set(V8_LIBS
	-Wl,--start-group
	${V8_OUT}/obj/libv8_base.a
	${V8_OUT}/obj/libv8_libbase.a
	${V8_OUT}/obj/libv8_external_snapshot.a
	${V8_OUT}/obj/libv8_libplatform.a
	${V8_OUT}/obj/libv8_libsampler.a
	${V8_OUT}/obj/third_party/icu/libicuuc.a
	${V8_OUT}/obj/third_party/icu/libicui18n.a
	${V8_OUT}/obj/src/inspector/libinspector.a
	-Wl,--end-group)

set(V8_INCLUDE
	${CMAKE_SOURCE_DIR}/include
	${V8V8}
	${V8V8}/include)

add_subdirectory(src)
add_subdirectory(example)
