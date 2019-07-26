(module
  (memory (import "" "memory") 0)

  (func (export "size") (result i32) (memory.size))
  (func (export "load") (param i32) (result i32) (i32.load8_s (local.get 0)))
  (func (export "store") (param i32 i32)
    (i32.store8 (local.get 0) (local.get 1))
  )
  (func (export "grow") (param i32) (result i32) (memory.grow (local.get 0)))
)
