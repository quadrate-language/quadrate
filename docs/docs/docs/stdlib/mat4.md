# `use` mat4

4x4 matrix math operations for 3D transformations.

## Structs

### `struct` Mat4

4x4 matrix in column-major order (OpenGL compatible).

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

## Functions

### `fn` identity

Create identity Mat4.

**Signature:** `( -- m:Mat4)`

| Output | Type | Description |
|--------|------|-------------|
| `m` | `Mat4` | Identity matrix |

**Example:**

```qd
mat4::identity -> m
```

---

### `fn` zero

Create zero Mat4.

**Signature:** `( -- m:Mat4)`

| Output | Type | Description |
|--------|------|-------------|
| `m` | `Mat4` | Zero matrix |

**Example:**

```qd
mat4::zero -> m
```

---

### `fn` transpose

Transpose of Mat4.

**Signature:** `(m:Mat4 -- result:Mat4)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `m` | `Mat4` | Input matrix |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Mat4` | Transposed matrix |

**Example:**

```qd
m mat4::transpose -> m_t
```

---

### `fn` translation

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
1.0 2.0 3.0 mat4::translation -> m
```

---

### `fn` scaling

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
2.0 2.0 2.0 mat4::scaling -> m
```

---

### `fn` rotation_x

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
1.57 mat4::rotation_x -> m
```

---

### `fn` rotation_y

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
1.57 mat4::rotation_y -> m
```

---

### `fn` rotation_z

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
1.57 mat4::rotation_z -> m
```
