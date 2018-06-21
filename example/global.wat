(module
  (global $f32_import (import "" "const f32") f32)
  (global $i64_import (import "" "const i64") i64)
  (global $mut_f32_import (import "" "var f32") (mut f32))
  (global $mut_i64_import (import "" "var i64") (mut i64))

  (global $f32_export (export "const f32") f32 (f32.const 5))
  (global $i64_export (export "const i64") i64 (i64.const 6))
  (global $mut_f32_export (export "var f32") (mut f32) (f32.const 7))
  (global $mut_i64_export (export "var i64") (mut i64) (i64.const 8))

  (func (export "get const f32 import") (result f32) (get_global $f32_import))
  (func (export "get const i64 import") (result f64) (f64.reinterpret/i64 (get_global $i64_import)))
  (func (export "get var f32 import") (result f32) (get_global $mut_f32_import))
  (func (export "get var i64 import") (result f64) (f64.reinterpret/i64 (get_global $mut_i64_import)))

  (func (export "get const f32 export") (result f32) (get_global $f32_export))
  (func (export "get const i64 export") (result f64) (f64.reinterpret/i64 (get_global $i64_export)))
  (func (export "get var f32 export") (result f32) (get_global $mut_f32_export))
  (func (export "get var i64 export") (result f64) (f64.reinterpret/i64 (get_global $mut_i64_export)))

  (func (export "set var f32 import") (param f32) (set_global $mut_f32_import (get_global 0)))
  (func (export "set var i64 import") (param f64) (set_global $mut_i64_import (i64.reinterpret/f64 (get_local 0))))

  (func (export "set var f32 export") (param f32) (set_global $mut_f32_export (get_global 0)))
  (func (export "set var f64 export") (param f64) (set_global $mut_i64_export (i64.reinterpret/f64 (get_local 0))))
)
