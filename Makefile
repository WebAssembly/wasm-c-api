OUT_DIR = out
WASM_DIR = .
EXAMPLE_DIR = example

EXAMPLE_NAME = hello
EXAMPLE_OUT = ${OUT_DIR}/${EXAMPLE_DIR}
EXAMPLE_WAT = hello

V8_VERSION = master  # or e.g. branch-heads/6.3
V8_ARCH = x64
V8_MODE = release
V8_DIR = v8

WASM_INTERPRETER = ../spec.master/interpreter/wasm   # change as needed

WASM_INCLUDE = ${WASM_DIR}/include
WASM_SRC = ${WASM_DIR}/src
WASM_OUT = ${OUT_DIR}/${WASM_SRC}
WASM_LIBS = wasm-v8 wasm-bin wasm-v8-lowlevel
WASM_O = ${WASM_LIBS:%=${WASM_OUT}/%.o}

V8_BUILD = ${V8_ARCH}.${V8_MODE}
V8_V8 = ${V8_DIR}/v8
V8_DEPOT_TOOLS = ${V8_DIR}/depot_tools
V8_PATH = ${V8_DEPOT_TOOLS}:${PATH}
V8_INCLUDE = ${V8_V8}/include
V8_SRC = ${V8_V8}/src
V8_OUT = ${V8_V8}/out.gn/${V8_BUILD}
V8_LIBS = base libbase external_snapshot libplatform libsampler
V8_ICU_LIBS = uc i18n
V8_OTHER_LIBS = src/inspector/libinspector
V8_BIN = natives_blob snapshot_blob snapshot_blob_trusted


# Example

.PHONY: example
example: ${EXAMPLE_OUT}/${EXAMPLE_NAME} ${V8_BIN:%=${EXAMPLE_OUT}/%.bin} ${EXAMPLE_WAT:%=${EXAMPLE_OUT}/%.wasm}
	cd ${EXAMPLE_OUT}; ./${EXAMPLE_NAME}

${EXAMPLE_OUT}/${EXAMPLE_NAME}.o: ${EXAMPLE_DIR}/${EXAMPLE_NAME}.c ${WASM_INCLUDE}/wasm.h ${EXAMPLE_OUT}
	clang -c -I. -I${V8_INCLUDE} -I${WASM_INCLUDE} $< -o $@

${EXAMPLE_OUT}/${EXAMPLE_NAME}: ${EXAMPLE_OUT}/${EXAMPLE_NAME}.o ${WASM_O}
	clang++ $< -o $@ \
		${V8_LIBS:%=${V8_OUT}/obj/libv8_%.a} \
		${V8_ICU_LIBS:%=${V8_OUT}/obj/third_party/icu/libicu%.a} \
		${V8_OTHER_LIBS:%=${V8_OUT}/obj/%.a} \
		${WASM_O} \
		-ldl -pthread

${EXAMPLE_OUT}/%.bin: ${V8_OUT}/%.bin
	cp $< $@

${EXAMPLE_OUT}/%.wasm: ${EXAMPLE_DIR}/%.wasm
	cp $< $@

%.wasm: %.wat
	${WASM_INTERPRETER} -d $< -o $@

${EXAMPLE_OUT}: ${OUT_DIR}
	mkdir -p $@


# Wasm C API

.PHONY: wasm
wasm: ${WASM_LIBS:%=%.o}

${WASM_O}: ${WASM_OUT}/%.o: ${WASM_SRC}/%.cc ${WASM_OUT}
	clang++ -c -I. -I${V8_INCLUDE} -I${V8_SRC} -I${V8_V8} -I${V8_OUT}/gen -I${WASM_INCLUDE} -I${WASM_SRC} $< -o $@ -std=c++0x

${WASM_OUT}: ${OUT_DIR}
	mkdir -p $@

${OUT_DIR}:
	mkdir -p $@


# V8

.PHONY: v8
v8:
	(cd ${V8_V8}; PATH=${V8_PATH} tools/dev/v8gen.py ${V8_BUILD})
	echo >>${V8_OUT}/args.gn is_component_build = false
	echo >>${V8_OUT}/args.gn v8_static_library = true
	(cd ${V8_V8}; PATH=${V8_PATH} ninja -C out.gn/${V8_BUILD})

.PHONY: v8-checkout
v8-checkout:
	mkdir -p ${V8_DIR}
	(cd ${V8_DIR}; git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git)
	(cd ${V8_DIR}; PATH=${V8_PATH} fetch v8)
	(cd ${V8_V8}; git checkout ${V8_VERSION})

.PHONY: v8-update
v8-update:
	(cd ${V8_V8}; PATH=${V8_PATH} gclient sync)
	(cd ${V8_V8}; git pull)


# Clean-up

.PHONY: clean
clean:
	rm -rf ${OUT_DIR}
