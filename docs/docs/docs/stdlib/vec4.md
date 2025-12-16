# `use` vec4

4D vector math operations (homogeneous coordinates).

## Structs

### `struct` Vec4

4D vector with x, y, z, and w components.

| Field | Type | Description |
|-------|------|-------------|
| `x` | `f64` | X component |
| `y` | `f64` | Y component |
| `z` | `f64` | Z component |
| `w` | `f64` | W component |

**Creating a Vec4:**

```qd
Vec4 { x = 1.0 y = 2.0 z = 3.0 w = 4.0 } -> v
```

## Functions

### `fn` zero

Create a Vec4 with all components set to zero.

**Signature:** `( -- v:Vec4)`

| Output | Type | Description |
|--------|------|-------------|
| `v` | `Vec4` | Zero vector (0, 0, 0, 0) |

**Example:**

```qd
vec4::zero -> v
```

---

### `fn` one

Create a Vec4 with all components set to one.

**Signature:** `( -- v:Vec4)`

| Output | Type | Description |
|--------|------|-------------|
| `v` | `Vec4` | One vector (1, 1, 1, 1) |

**Example:**

```qd
vec4::one -> v
```

---

### `fn` add

Add two Vec4 vectors.

**Signature:** `(a:Vec4 b:Vec4 -- result:Vec4)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `a` | `Vec4` | First vector |
| `b` | `Vec4` | Second vector |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec4` | Sum of a and b |

**Example:**

```qd
v1 v2 vec4::add -> v3
```

---

### `fn` subtract

Subtract two Vec4 vectors.

**Signature:** `(a:Vec4 b:Vec4 -- result:Vec4)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `a` | `Vec4` | First vector |
| `b` | `Vec4` | Second vector |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec4` | Difference of a and b |

**Example:**

```qd
v1 v2 vec4::subtract -> v3
```

---

### `fn` scale

Multiply Vec4 by scalar.

**Signature:** `(v:Vec4 s:f64 -- result:Vec4)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `v` | `Vec4` | Vector to scale |
| `s` | `f64` | Scalar multiplier |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec4` | Scaled vector |

**Example:**

```qd
v 2.0 vec4::scale -> v2
```

---

### `fn` dot

Dot product of two Vec4 vectors.

**Signature:** `(a:Vec4 b:Vec4 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `a` | `Vec4` | First vector |
| `b` | `Vec4` | Second vector |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Dot product |

**Example:**

```qd
v1 v2 vec4::dot -> d
```

---

### `fn` length

Length (magnitude) of a Vec4.

**Signature:** `(v:Vec4 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `v` | `Vec4` | Input vector |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Length of vector |

**Example:**

```qd
v vec4::length -> len
```

---

### `fn` normalize

Normalize a Vec4 to unit length.

**Signature:** `(v:Vec4 -- result:Vec4)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `v` | `Vec4` | Input vector |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec4` | Unit vector in same direction |

**Example:**

```qd
v vec4::normalize -> unit
```
