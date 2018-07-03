(module
  (func $hello (import "" "hello") (param i32))
  (global $id (import "" "id") i32)
  (func (export "run") (call $hello (get_global $id)))
)
