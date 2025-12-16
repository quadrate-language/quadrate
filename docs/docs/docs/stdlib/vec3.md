# `use` vec3

3D vector math operations.

## Structs

### `struct` Vec3

3D vector with x, y, and z components.

| Field | Type | Description |
|-------|------|-------------|
| `x` | `f64` | X component |
| `y` | `f64` | Y component |
| `z` | `f64` | Z component |

## Functions

### `fn` add

Add two Vec3 vectors.

**Signature:** `(a:Vec3 b:Vec3 -- result:Vec3)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `a` | `Vec3` | First vector |
| `b` | `Vec3` | Second vector |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec3` | Sum of a and b |

**Example:**

```qd
v1 v2 vec3::add  // v3
```

---

### `fn` cross

Cross product of two Vec3 vectors.

**Signature:** `(a:Vec3 b:Vec3 -- result:Vec3)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `a` | `Vec3` | First vector |
| `b` | `Vec3` | Second vector |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec3` | Cross product |

**Example:**

```qd
v1 v2 vec3::cross  // v3
```

---

### `fn` dot

Dot product of two Vec3 vectors.

**Signature:** `(a:Vec3 b:Vec3 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `a` | `Vec3` | First vector |
| `b` | `Vec3` | Second vector |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Dot product |

**Example:**

```qd
v1 v2 vec3::dot  // d
```

---

### `fn` length_sq

Squared length of a Vec3 (avoids sqrt).

**Signature:** `(v:Vec3 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `v` | `Vec3` | Input vector |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Squared length of vector |

**Example:**

```qd
v vec3::length_sq  // len_sq
```

---

### `fn` length

Length (magnitude) of a Vec3.

**Signature:** `(v:Vec3 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `v` | `Vec3` | Input vector |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Length of vector |

**Example:**

```qd
v vec3::length  // len
```

---

### `fn` lerp

Linear interpolation between two Vec3 vectors.

**Signature:** `(a:Vec3 b:Vec3 t:f64 -- result:Vec3)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `a` | `Vec3` | Start vector |
| `b` | `Vec3` | End vector |
| `t` | `f64` | Interpolation factor (0.0 to 1.0) |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec3` | Interpolated vector |

**Example:**

```qd
v1 v2 0.5 vec3::lerp  // mid
```

---

### `fn` neg

Negate a Vec3.

**Signature:** `(v:Vec3 -- result:Vec3)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `v` | `Vec3` | Input vector |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec3` | Negated vector |

**Example:**

```qd
v vec3::neg  // neg_v
```

---

### `fn` normalize

Normalize a Vec3 to unit length.

**Signature:** `(v:Vec3 -- result:Vec3)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `v` | `Vec3` | Input vector |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec3` | Unit vector in same direction |

**Example:**

```qd
v vec3::normalize  // unit
```

---

### `fn` one

Create a Vec3 with all components set to one.

**Signature:** `( -- v:Vec3)`

| Output | Type | Description |
|--------|------|-------------|
| `v` | `Vec3` | One vector (1, 1, 1) |

**Example:**

```qd
vec3::one  // v
```

---

### `fn` reflect

Reflect vector v around normal n.

**Signature:** `(v:Vec3 n:Vec3 -- result:Vec3)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `v` | `Vec3` | Vector to reflect |
| `n` | `Vec3` | Normal vector |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec3` | Reflected vector |

**Example:**

```qd
v normal vec3::reflect  // reflected
```

---

### `fn` scale

Multiply Vec3 by scalar.

**Signature:** `(v:Vec3 s:f64 -- result:Vec3)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `v` | `Vec3` | Vector to scale |
| `s` | `f64` | Scalar multiplier |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec3` | Scaled vector |

**Example:**

```qd
v 2.0 vec3::scale  // v2
```

---

### `fn` subtract

Subtract two Vec3 vectors.

**Signature:** `(a:Vec3 b:Vec3 -- result:Vec3)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `a` | `Vec3` | First vector |
| `b` | `Vec3` | Second vector |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec3` | Difference of a and b |

**Example:**

```qd
v1 v2 vec3::subtract  // v3
```

---

### `fn` unit_x

Unit vector along the X axis.

**Signature:** `( -- v:Vec3)`

| Output | Type | Description |
|--------|------|-------------|
| `v` | `Vec3` | Unit X vector (1, 0, 0) |

**Example:**

```qd
vec3::unit_x  // v
```

---

### `fn` unit_y

Unit vector along the Y axis.

**Signature:** `( -- v:Vec3)`

| Output | Type | Description |
|--------|------|-------------|
| `v` | `Vec3` | Unit Y vector (0, 1, 0) |

**Example:**

```qd
vec3::unit_y  // v
```

---

### `fn` unit_z

Unit vector along the Z axis.

**Signature:** `( -- v:Vec3)`

| Output | Type | Description |
|--------|------|-------------|
| `v` | `Vec3` | Unit Z vector (0, 0, 1) |

**Example:**

```qd
vec3::unit_z  // v
```

---

### `fn` zero

Create a Vec3 with all components set to zero.

**Signature:** `( -- v:Vec3)`

| Output | Type | Description |
|--------|------|-------------|
| `v` | `Vec3` | Zero vector (0, 0, 0) |

**Example:**

```qd
vec3::zero  // v
```

