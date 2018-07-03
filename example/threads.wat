(module
  (func $message (import "" "hello") (param i32))
  (global $id (import "" "id") i32)
  (func (export "run") (call $message (get_global $id)))
)
