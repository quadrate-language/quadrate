# `use` math — Matrices & Quaternions

Matrix and quaternion types for 3D transformations. All types are part of the `math` module.

See also: [Scalar functions](math.md) | [Vectors](math-vectors.md)

## Mat4

4x4 matrix in column-major order (OpenGL compatible).

### Struct

| Field | Type | Description |
|-------|------|-------------|
| `m00` | `f64` | Element at row 0, column 0 |
| `m01` | `f64` | Element at row 1, column 0 |
| `m02` | `f64` | Element at row 2, column 0 |
| `m03` | `f64` | Element at row 3, column 0 |
| `m10` | `f64` | Element at row 0, column 1 |
| `m11` | `f64` | Element at row 1, column 1 |
| `m12` | `f64` | Element at row 2, column 1 |
| `m13` | `f64` | Element at row 3, column 1 |
| `m20` | `f64` | Element at row 0, column 2 |
| `m21` | `f64` | Element at row 1, column 2 |
| `m22` | `f64` | Element at row 2, column 2 |
| `m23` | `f64` | Element at row 3, column 2 |
| `m30` | `f64` | Element at row 0, column 3 |
| `m31` | `f64` | Element at row 1, column 3 |
| `m32` | `f64` | Element at row 2, column 3 |
| `m33` | `f64` | Element at row 3, column 3 |

### Constructors

#### `fn` mat4_identity

Create identity Mat4.

**Signature:** `( -- m:Mat4)`

| Output | Type | Description |
|--------|------|-------------|
| `m` | `Mat4` | Identity matrix |

**Example:**

```qd
math::mat4_identity  // m
```
---

#### `fn` mat4_rotation_x

Create rotation Mat4 around X axis.

**Signature:** `(angle:f64 -- m:Mat4)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `angle` | `f64` | Rotation angle in radians |

| Output | Type | Description |
|--------|------|-------------|
| `m` | `Mat4` | Rotation matrix |

**Example:**

```qd
1.57 math::mat4_rotation_x  // m
```
---

#### `fn` mat4_rotation_y

Create rotation Mat4 around Y axis.

**Signature:** `(angle:f64 -- m:Mat4)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `angle` | `f64` | Rotation angle in radians |

| Output | Type | Description |
|--------|------|-------------|
| `m` | `Mat4` | Rotation matrix |

**Example:**

```qd
1.57 math::mat4_rotation_y  // m
```
---

#### `fn` mat4_rotation_z

Create rotation Mat4 around Z axis.

**Signature:** `(angle:f64 -- m:Mat4)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `angle` | `f64` | Rotation angle in radians |

| Output | Type | Description |
|--------|------|-------------|
| `m` | `Mat4` | Rotation matrix |

**Example:**

```qd
1.57 math::mat4_rotation_z  // m
```
---

#### `fn` mat4_scaling

Create scale Mat4.

**Signature:** `(sx:f64 sy:f64 sz:f64 -- m:Mat4)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `sx` | `f64` | X scale factor |
| `sy` | `f64` | Y scale factor |
| `sz` | `f64` | Z scale factor |

| Output | Type | Description |
|--------|------|-------------|
| `m` | `Mat4` | Scale matrix |

**Example:**

```qd
2.0 2.0 2.0 math::mat4_scaling  // m
```
---

#### `fn` mat4_translation

Create translation Mat4.

**Signature:** `(x:f64 y:f64 z:f64 -- m:Mat4)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | X translation |
| `y` | `f64` | Y translation |
| `z` | `f64` | Z translation |

| Output | Type | Description |
|--------|------|-------------|
| `m` | `Mat4` | Translation matrix |

**Example:**

```qd
1.0 2.0 3.0 math::mat4_translation  // m
```
---

#### `fn` mat4_zero

Create zero Mat4.

**Signature:** `( -- m:Mat4)`

| Output | Type | Description |
|--------|------|-------------|
| `m` | `Mat4` | Zero matrix |

**Example:**

```qd
math::mat4_zero  // m
```
---

#### `fn` mat4_look_at

Create a look-at view matrix.

**Signature:** `(eye:Vec3 target:Vec3 up:Vec3 -- m:Mat4)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `eye` | `Vec3` | Camera position |
| `target` | `Vec3` | Point to look at |
| `up` | `Vec3` | Up direction |

| Output | Type | Description |
|--------|------|-------------|
| `m` | `Mat4` | View matrix |

**Example:**

```qd
eye target up math::mat4_look_at -> view
```
---

#### `fn` mat4_perspective

Create a perspective projection matrix.

**Signature:** `(fov:f64 aspect:f64 near:f64 far:f64 -- m:Mat4)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `fov` | `f64` | Field of view in radians |
| `aspect` | `f64` | Aspect ratio (width/height) |
| `near` | `f64` | Near clipping plane |
| `far` | `f64` | Far clipping plane |

| Output | Type | Description |
|--------|------|-------------|
| `m` | `Mat4` | Perspective projection matrix |

**Example:**

```qd
1.047 1.777 0.1 100.0 math::mat4_perspective -> proj
```
---

#### `fn` mat4_orthographic

Create an orthographic projection matrix.

**Signature:** `(left:f64 right:f64 bottom:f64 top:f64 near:f64 far:f64 -- m:Mat4)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `left` | `f64` | Left clipping plane |
| `right` | `f64` | Right clipping plane |
| `bottom` | `f64` | Bottom clipping plane |
| `top` | `f64` | Top clipping plane |
| `near` | `f64` | Near clipping plane |
| `far` | `f64` | Far clipping plane |

| Output | Type | Description |
|--------|------|-------------|
| `m` | `Mat4` | Orthographic projection matrix |

**Example:**

```qd
-1.0 1.0 -1.0 1.0 0.1 100.0 math::mat4_orthographic -> proj
```
---

#### `fn` mat4_from_quat

Create rotation matrix from quaternion.

**Signature:** `(q:Quat -- m:Mat4)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `q` | `Quat` | Rotation quaternion |

| Output | Type | Description |
|--------|------|-------------|
| `m` | `Mat4` | Rotation matrix |

**Example:**

```qd
q math::mat4_from_quat -> rot_mat
```

### Methods

#### `fn` mul

Matrix multiplication.

**Signature:** `(m:Mat4) mul(other:Mat4 -- result:Mat4)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `other` | `Mat4` | Matrix to multiply with |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Mat4` | Product of matrices (m * other) |

**Example:**

```qd
other m mul -> product
```
---

#### `fn` determinant

Matrix determinant.

**Signature:** `(m:Mat4) determinant( -- result:f64)`

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Determinant of matrix |

**Example:**

```qd
m determinant -> det
```
---

#### `fn` inverse

Matrix inverse.

**Signature:** `(m:Mat4) inverse( -- result:Mat4)`

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Mat4` | Inverse matrix (returns identity if singular) |

**Example:**

```qd
m inverse -> m_inv
```
---

#### `fn` transpose

Transpose this matrix.

**Signature:** `(m:Mat4) transpose( -- result:Mat4)`

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Mat4` | Transposed matrix |

**Example:**

```qd
m transpose  // m_t
```
---

#### `fn` transform_point

Transform a Vec3 point by this matrix (w=1).

**Signature:** `(m:Mat4) transform_point(v:Vec3 -- result:Vec3)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `v` | `Vec3` | Point to transform |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec3` | Transformed point |

**Example:**

```qd
point m transform_point -> transformed
```
---

#### `fn` transform_vec4

Transform a Vec4 by this matrix.

**Signature:** `(m:Mat4) transform_vec4(v:Vec4 -- result:Vec4)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `v` | `Vec4` | Vector to transform |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec4` | Transformed vector |

**Example:**

```qd
vec m transform_vec4 -> transformed
```

## Quat

Quaternion for representing rotations. Stored as (w, x, y, z) where w is the scalar part.

### Struct

| Field | Type | Description |
|-------|------|-------------|
| `w` | `f64` | Scalar (real) component |
| `x` | `f64` | X component of vector part |
| `y` | `f64` | Y component of vector part |
| `z` | `f64` | Z component of vector part |

### Constructors

#### `fn` quat_identity

Create identity quaternion (no rotation).

**Signature:** `( -- q:Quat)`

| Output | Type | Description |
|--------|------|-------------|
| `q` | `Quat` | Identity quaternion |

**Example:**

```qd
math::quat_identity  // q
```
---

#### `fn` quat_from_axis_angle

Create quaternion from axis and angle.

**Signature:** `(axis:Vec3 angle:f64 -- result:Quat)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `axis` | `Vec3` | Rotation axis (should be normalized) |
| `angle` | `f64` | Rotation angle in radians |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Quat` | Rotation quaternion |

**Example:**

```qd
axis 1.57 math::quat_from_axis_angle -> q
```
---

#### `fn` quat_from_euler

Create quaternion from Euler angles (XYZ order).

**Signature:** `(pitch:f64 yaw:f64 roll:f64 -- result:Quat)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `pitch` | `f64` | Rotation around X axis (radians) |
| `yaw` | `f64` | Rotation around Y axis (radians) |
| `roll` | `f64` | Rotation around Z axis (radians) |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Quat` | Rotation quaternion |

**Example:**

```qd
0.0 1.57 0.0 math::quat_from_euler -> q
```

### Methods

#### `fn` conjugate

Conjugate of this quaternion.

**Signature:** `(q:Quat) conjugate( -- result:Quat)`

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Quat` | Conjugate quaternion |

**Example:**

```qd
q conjugate  // q_conj
```
---

#### `fn` dot

Dot product with another quaternion.

**Signature:** `(q:Quat) dot(other:Quat -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `other` | `Quat` | Other quaternion |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Dot product |

**Example:**

```qd
other q dot  // d
```
---

#### `fn` length

Length (magnitude) of this quaternion.

**Signature:** `(q:Quat) length( -- result:f64)`

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Length of quaternion |

**Example:**

```qd
q length  // len
```
---

#### `fn` mul

Quaternion multiplication.

**Signature:** `(q:Quat) mul(other:Quat -- result:Quat)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `other` | `Quat` | Quaternion to multiply with |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Quat` | Product of this and other |

**Example:**

```qd
other q mul  // product
```
---

#### `fn` normalize

Normalize this quaternion to unit length.

**Signature:** `(q:Quat) normalize( -- result:Quat)`

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Quat` | Unit quaternion |

**Example:**

```qd
q normalize  // unit_q
```
---

#### `fn` inverse

Inverse of this quaternion.

**Signature:** `(q:Quat) inverse( -- result:Quat)`

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Quat` | Inverse quaternion |

**Example:**

```qd
q inverse -> q_inv
```
---

#### `fn` length_sq

Squared length of quaternion.

**Signature:** `(q:Quat) length_sq( -- result:f64)`

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Squared length |

**Example:**

```qd
q length_sq -> len_sq
```
---

#### `fn` slerp

Spherical linear interpolation to another quaternion.

**Signature:** `(q:Quat) slerp(other:Quat t:f64 -- result:Quat)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `other` | `Quat` | End quaternion |
| `t` | `f64` | Interpolation factor (0.0 to 1.0) |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Quat` | Interpolated quaternion |

**Example:**

```qd
other 0.5 q slerp -> mid
```
---

#### `fn` rotate_vec3

Rotate a Vec3 by this quaternion.

**Signature:** `(q:Quat) rotate_vec3(v:Vec3 -- result:Vec3)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `v` | `Vec3` | Vector to rotate |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec3` | Rotated vector |

**Example:**

```qd
v q rotate_vec3 -> rotated
```
