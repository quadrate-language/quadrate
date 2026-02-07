# `use` math — Vectors

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
other v add  // sum
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
other v dot  // d
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
v length_sq  // len_sq
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
v length  // len
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
other 0.5 v lerp  // mid
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
v neg  // neg_v
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
v normalize  // unit
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
2.0 v scale  // scaled
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
other v subtract  // diff
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
other v distance -> d
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
v angle -> a
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
1.57 v rotate -> rotated
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
v perpendicular -> perp
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
other v add  // sum
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
other v cross  // c
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
other v dot  // d
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
v length_sq  // len_sq
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
v length  // len
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
other 0.5 v lerp  // mid
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
v neg  // neg_v
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
v normalize  // unit
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
normal v reflect  // reflected
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
2.0 v scale  // scaled
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
other v subtract  // diff
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
other v distance -> d
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
other v angle_between -> a
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
other v add  // sum
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
other v dot  // d
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
v length  // len
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
v normalize  // unit
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
2.0 v scale  // scaled
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
other v subtract  // diff
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
v length_sq -> len_sq
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
v neg -> neg_v
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
other 0.5 v lerp -> mid
```
