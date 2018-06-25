(module
  (func $print1 (import "" "print1") (param i32) (result i32))
  (func $print2 (import "" "print2") (param i32 i32) (result i32))
  (func $closure (import "" "closure") (result i32))
  (func (export "run") (param $x i32) (param $y i32) (result i32)
    (i32.add
      (i32.add
        (call $print2 (get_local $x) (get_local $y))
        (call $print1 (i32.add (get_local $x) (get_local $y)))
      )
      (call $closure)
    )
  )
)
