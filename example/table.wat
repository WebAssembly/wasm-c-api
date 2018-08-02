(module
  (table (export "table") 2 10 anyfunc)

  (func (export "call_indirect") (param i32 i32) (result i32)
    (call_indirect (param i32) (result i32) (get_local 0) (get_local 1))
  )

  (func $f (export "f") (param i32) (result i32) (get_local 0))
  (func (export "g") (param i32) (result i32) (i32.const 666))

  (elem (i32.const 1) $f)
)
