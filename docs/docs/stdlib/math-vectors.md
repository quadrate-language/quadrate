# `use` math — Vectors

<!-- doccheck: page-context use math -->

<!-- doccheck: page-setup math::Vec2 { x = 3.0 y = 4.0 } -> v2 -->

<!-- doccheck: page-setup math::Vec2 { x = 1.0 y = 0.0 } -> other2 -->

<!-- doccheck: page-setup math::Vec3 { x = 1.0 y = 2.0 z = 2.0 } -> v3 -->

<!-- doccheck: page-setup math::Vec3 { x = 0.0 y = 1.0 z = 0.0 } -> other3 -->

<!-- doccheck: page-setup math::Vec4 { x = 1.0 y = 2.0 z = 2.0 w = 1.0 } -> v4 -->

<!-- doccheck: page-setup math::Vec4 { x = 0.0 y = 1.0 z = 0.0 w = 1.0 } -> other4 -->

<!-- doccheck: page-setup math::quat_identity -> q -->

<!-- doccheck: page-setup math::quat_identity -> other_q -->

<!-- doccheck: page-setup math::mat4_identity -> m -->

<!-- doccheck: page-setup math::mat4_identity -> other_m -->

<!-- doccheck: page-setup math::Vec3 { x = 0.0 y = 1.0 z = 0.0 } -> axis -->

<!-- doccheck: page-setup math::Vec3 { x = 0.0 y = 1.0 z = 0.0 } -> normal -->

<!-- doccheck: page-setup math::Vec3 { x = 1.0 y = 0.0 z = 0.0 } -> point -->

<!-- doccheck: page-setup math::Vec3 { x = 0.0 y = 0.0 z = 5.0 } -> eye -->

<!-- doccheck: page-setup math::Vec3 { x = 0.0 y = 0.0 z = 0.0 } -> target -->

<!-- doccheck: page-setup math::Vec3 { x = 0.0 y = 1.0 z = 0.0 } -> up -->

Vector types for 2D, 3D, and 4D math. All types are part of the `math` module.

See also: [Scalar functions](math.md) | [Matrices & Quaternions](math-matrices.md)

## Vec2

2D vector with x and y components.

### Struct

| Field | Type | Description |
|-------|------|-------------|
| `x` | `f64` | X component |
| `y` | `f64` | Y component |

### Constructors

#### `fn` vec2_one

Create a Vec2 with all components set to one.

**Signature:** `( -- v:Vec2)`

| Output | Type | Description |
|--------|------|-------------|
| `v` | `Vec2` | One vector (1, 1) |

**Example:**

```qd
math::vec2_one  // v
```
---

#### `fn` vec2_zero

Create a Vec2 with all components set to zero.

**Signature:** `( -- v:Vec2)`

| Output | Type | Description |
|--------|------|-------------|
| `v` | `Vec2` | Zero vector (0, 0) |

**Example:**

```qd
math::vec2_zero  // v
```

### Methods

#### `fn` add

Add another Vec2 to this vector.

**Signature:** `(v:Vec2) add(other:Vec2 -- result:Vec2)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `other` | `Vec2` | Vector to add |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec2` | Sum of vectors |

**Example:**

```qd
v2 other2 add  // sum
```
---

#### `fn` dot

Dot product with another Vec2.

**Signature:** `(v:Vec2) dot(other:Vec2 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `other` | `Vec2` | Other vector |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Dot product |

**Example:**

```qd
v2 other2 dot  // d
```
---

#### `fn` length_sq

Squared length of this vector (avoids sqrt).

**Signature:** `(v:Vec2) length_sq( -- result:f64)`

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Squared length of vector |

**Example:**

```qd
v2 length_sq  // len_sq
```
---

#### `fn` length

Length (magnitude) of this vector.

**Signature:** `(v:Vec2) length( -- result:f64)`

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Length of vector |

**Example:**

```qd
v2 length  // len
```
---

#### `fn` lerp

Linear interpolation to another Vec2.

**Signature:** `(v:Vec2) lerp(other:Vec2 t:f64 -- result:Vec2)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `other` | `Vec2` | End vector |
| `t` | `f64` | Interpolation factor (0.0 to 1.0) |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec2` | Interpolated vector |

**Example:**

```qd
v2 other2 0.5 lerp  // mid
```
---

#### `fn` neg

Negate this vector.

**Signature:** `(v:Vec2) neg( -- result:Vec2)`

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec2` | Negated vector |

**Example:**

```qd
v2 neg  // neg_v
```
---

#### `fn` normalize

Normalize this vector to unit length.

**Signature:** `(v:Vec2) normalize( -- result:Vec2)`

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec2` | Unit vector in same direction |

**Example:**

```qd
v2 normalize  // unit
```
---

#### `fn` scale

Multiply this vector by a scalar.

**Signature:** `(v:Vec2) scale(s:f64 -- result:Vec2)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `f64` | Scalar multiplier |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec2` | Scaled vector |

**Example:**

```qd
v2 2.0 scale  // scaled
```
---

#### `fn` subtract

Subtract another Vec2 from this vector.

**Signature:** `(v:Vec2) subtract(other:Vec2 -- result:Vec2)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `other` | `Vec2` | Vector to subtract |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec2` | Difference of vectors |

**Example:**

```qd
v2 other2 subtract  // diff
```
---

#### `fn` distance

Distance to another Vec2.

**Signature:** `(v:Vec2) distance(other:Vec2 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `other` | `Vec2` | Other point |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Distance between points |

**Example:**

```qd
v2 other2 distance  // d
```
---

#### `fn` angle

Angle of this vector from positive X axis.

**Signature:** `(v:Vec2) angle( -- result:f64)`

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Angle in radians [-Pi, Pi] |

**Example:**

```qd
v2 angle  // a
```
---

#### `fn` rotate

Rotate this vector by an angle.

**Signature:** `(v:Vec2) rotate(angle:f64 -- result:Vec2)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `angle` | `f64` | Rotation angle in radians |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec2` | Rotated vector |

**Example:**

```qd
v2 1.57 rotate  // rotated
```
---

#### `fn` perpendicular

Get perpendicular vector (90 degrees counterclockwise rotation).

**Signature:** `(v:Vec2) perpendicular( -- result:Vec2)`

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec2` | Perpendicular vector |

**Example:**

```qd
v2 perpendicular  // perp
```

## Vec3

3D vector with x, y, and z components.

### Struct

| Field | Type | Description |
|-------|------|-------------|
| `x` | `f64` | X component |
| `y` | `f64` | Y component |
| `z` | `f64` | Z component |

### Constructors

#### `fn` vec3_one

Create a Vec3 with all components set to one.

**Signature:** `( -- v:Vec3)`

| Output | Type | Description |
|--------|------|-------------|
| `v` | `Vec3` | One vector (1, 1, 1) |

**Example:**

```qd
math::vec3_one  // v
```
---

#### `fn` vec3_unit_x

Unit vector along the X axis.

**Signature:** `( -- v:Vec3)`

| Output | Type | Description |
|--------|------|-------------|
| `v` | `Vec3` | Unit X vector (1, 0, 0) |

**Example:**

```qd
math::vec3_unit_x  // v
```
---

#### `fn` vec3_unit_y

Unit vector along the Y axis.

**Signature:** `( -- v:Vec3)`

| Output | Type | Description |
|--------|------|-------------|
| `v` | `Vec3` | Unit Y vector (0, 1, 0) |

**Example:**

```qd
math::vec3_unit_y  // v
```
---

#### `fn` vec3_unit_z

Unit vector along the Z axis.

**Signature:** `( -- v:Vec3)`

| Output | Type | Description |
|--------|------|-------------|
| `v` | `Vec3` | Unit Z vector (0, 0, 1) |

**Example:**

```qd
math::vec3_unit_z  // v
```
---

#### `fn` vec3_zero

Create a Vec3 with all components set to zero.

**Signature:** `( -- v:Vec3)`

| Output | Type | Description |
|--------|------|-------------|
| `v` | `Vec3` | Zero vector (0, 0, 0) |

**Example:**

```qd
math::vec3_zero  // v
```

### On the stack: v3

`Vec3` is for storing a vector; the `v3_*` words are for computing with one. They take and
leave a vector as three `f64`s on the stack, x deepest, so nothing is allocated and each word
compiles to native code. In a loop, unpack a `Vec3` once, compute with `v3_*`, and build a
`Vec3` only when the result has to be kept.

| Word | Signature |
|------|-----------|
| [`v3_add`](math.md#fn-v3_add) | `(ax ay az bx by bz -- x y z)` |
| [`v3_sub`](math.md#fn-v3_sub) | `(ax ay az bx by bz -- x y z)` |
| [`v3_scale`](math.md#fn-v3_scale) | `(x y z k -- x y z)` |
| [`v3_dot`](math.md#fn-v3_dot) | `(ax ay az bx by bz -- d)` |
| [`v3_cross`](math.md#fn-v3_cross) | `(ax ay az bx by bz -- x y z)` |
| [`v3_length_sq`](math.md#fn-v3_length_sq) | `(x y z -- l)` |
| [`v3_length`](math.md#fn-v3_length) | `(x y z -- l)` |
| [`v3_normalize`](math.md#fn-v3_normalize) | `(x y z -- x y z)` |

**Example:**

```qd
use math

fn main() {
	math::Vec3 { x = 1.0 y = 2.0 z = 2.0 } -> v
	v <<x v <<y v <<z 2.0 math::v3_scale math::v3_length print  // 6
}
```

### Methods

#### `fn` add

Add another Vec3 to this vector.

**Signature:** `(v:Vec3) add(other:Vec3 -- result:Vec3)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `other` | `Vec3` | Vector to add |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec3` | Sum of vectors |

**Example:**

```qd
v3 other3 add  // sum
```
---

#### `fn` cross

Cross product with another Vec3.

**Signature:** `(v:Vec3) cross(other:Vec3 -- result:Vec3)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `other` | `Vec3` | Other vector |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec3` | Cross product (v x other) |

**Example:**

```qd
v3 other3 cross  // c
```
---

#### `fn` dot

Dot product with another Vec3.

**Signature:** `(v:Vec3) dot(other:Vec3 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `other` | `Vec3` | Other vector |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Dot product |

**Example:**

```qd
v3 other3 dot  // d
```
---

#### `fn` length_sq

Squared length of this vector (avoids sqrt).

**Signature:** `(v:Vec3) length_sq( -- result:f64)`

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Squared length of vector |

**Example:**

```qd
v3 length_sq  // len_sq
```
---

#### `fn` length

Length (magnitude) of this vector.

**Signature:** `(v:Vec3) length( -- result:f64)`

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Length of vector |

**Example:**

```qd
v3 length  // len
```
---

#### `fn` lerp

Linear interpolation to another Vec3.

**Signature:** `(v:Vec3) lerp(other:Vec3 t:f64 -- result:Vec3)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `other` | `Vec3` | End vector |
| `t` | `f64` | Interpolation factor (0.0 to 1.0) |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec3` | Interpolated vector |

**Example:**

```qd
v3 other3 0.5 lerp  // mid
```
---

#### `fn` neg

Negate this vector.

**Signature:** `(v:Vec3) neg( -- result:Vec3)`

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec3` | Negated vector |

**Example:**

```qd
v3 neg  // neg_v
```
---

#### `fn` normalize

Normalize this vector to unit length.

**Signature:** `(v:Vec3) normalize( -- result:Vec3)`

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec3` | Unit vector in same direction |

**Example:**

```qd
v3 normalize  // unit
```
---

#### `fn` reflect

Reflect this vector around a normal.

**Signature:** `(v:Vec3) reflect(n:Vec3 -- result:Vec3)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `n` | `Vec3` | Normal vector |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec3` | Reflected vector |

**Example:**

```qd
v3 normal reflect  // reflected
```
---

#### `fn` scale

Multiply this vector by a scalar.

**Signature:** `(v:Vec3) scale(s:f64 -- result:Vec3)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `f64` | Scalar multiplier |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec3` | Scaled vector |

**Example:**

```qd
v3 2.0 scale  // scaled
```
---

#### `fn` subtract

Subtract another Vec3 from this vector.

**Signature:** `(v:Vec3) subtract(other:Vec3 -- result:Vec3)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `other` | `Vec3` | Vector to subtract |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec3` | Difference of vectors |

**Example:**

```qd
v3 other3 subtract  // diff
```
---

#### `fn` distance

Distance to another Vec3.

**Signature:** `(v:Vec3) distance(other:Vec3 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `other` | `Vec3` | Other point |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Distance between points |

**Example:**

```qd
v3 other3 distance  // d
```
---

#### `fn` angle_between

Angle between this vector and another.

**Signature:** `(v:Vec3) angle_between(other:Vec3 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `other` | `Vec3` | Other vector |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Angle in radians [0, Pi] |

**Example:**

```qd
v3 other3 angle_between  // a
```

## Vec4

4D vector with x, y, z, and w components.

### Struct

| Field | Type | Description |
|-------|------|-------------|
| `x` | `f64` | X component |
| `y` | `f64` | Y component |
| `z` | `f64` | Z component |
| `w` | `f64` | W component |

### Constructors

#### `fn` vec4_one

Create a Vec4 with all components set to one.

**Signature:** `( -- v:Vec4)`

| Output | Type | Description |
|--------|------|-------------|
| `v` | `Vec4` | One vector (1, 1, 1, 1) |

**Example:**

```qd
math::vec4_one  // v
```
---

#### `fn` vec4_zero

Create a Vec4 with all components set to zero.

**Signature:** `( -- v:Vec4)`

| Output | Type | Description |
|--------|------|-------------|
| `v` | `Vec4` | Zero vector (0, 0, 0, 0) |

**Example:**

```qd
math::vec4_zero  // v
```

### Methods

#### `fn` add

Add another Vec4 to this vector.

**Signature:** `(v:Vec4) add(other:Vec4 -- result:Vec4)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `other` | `Vec4` | Vector to add |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec4` | Sum of vectors |

**Example:**

```qd
v4 other4 add  // sum
```
---

#### `fn` dot

Dot product with another Vec4.

**Signature:** `(v:Vec4) dot(other:Vec4 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `other` | `Vec4` | Other vector |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Dot product |

**Example:**

```qd
v4 other4 dot  // d
```
---

#### `fn` length

Length (magnitude) of this vector.

**Signature:** `(v:Vec4) length( -- result:f64)`

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Length of vector |

**Example:**

```qd
v4 length  // len
```
---

#### `fn` normalize

Normalize this vector to unit length.

**Signature:** `(v:Vec4) normalize( -- result:Vec4)`

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec4` | Unit vector in same direction |

**Example:**

```qd
v4 normalize  // unit
```
---

#### `fn` scale

Multiply this vector by a scalar.

**Signature:** `(v:Vec4) scale(s:f64 -- result:Vec4)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `f64` | Scalar multiplier |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec4` | Scaled vector |

**Example:**

```qd
v4 2.0 scale  // scaled
```
---

#### `fn` subtract

Subtract another Vec4 from this vector.

**Signature:** `(v:Vec4) subtract(other:Vec4 -- result:Vec4)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `other` | `Vec4` | Vector to subtract |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec4` | Difference of vectors |

**Example:**

```qd
v4 other4 subtract  // diff
```
---

#### `fn` length_sq

Squared length of this vector (avoids sqrt).

**Signature:** `(v:Vec4) length_sq( -- result:f64)`

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Squared length of vector |

**Example:**

```qd
v4 length_sq  // len_sq
```
---

#### `fn` neg

Negate this vector.

**Signature:** `(v:Vec4) neg( -- result:Vec4)`

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec4` | Negated vector |

**Example:**

```qd
v4 neg  // neg_v
```
---

#### `fn` lerp

Linear interpolation to another Vec4.

**Signature:** `(v:Vec4) lerp(other:Vec4 t:f64 -- result:Vec4)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `other` | `Vec4` | End vector |
| `t` | `f64` | Interpolation factor (0.0 to 1.0) |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec4` | Interpolated vector |

**Example:**

```qd
v4 other4 0.5 lerp  // mid
```
