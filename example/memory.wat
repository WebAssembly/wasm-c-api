(module
  (memory (export "memory") 2 3)

  (func (export "size") (result i32) (memory.size))
  (func (export "load") (param i32) (result i32) (i32.load8_s (get_local 0)))
  (func (export "store") (param i32 i32)
    (i32.store8 (get_local 0) (get_local 1))
  )

  (data (i32.const 0x1000) "\01\02\03\04")
)
