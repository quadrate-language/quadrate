# `use` vec2

2D vector math operations.

## Structs

### `struct` Vec2

2D vector with x and y components.

| Field | Type | Description |
|-------|------|-------------|
| `x` | `f64` | X component |
| `y` | `f64` | Y component |

## Functions

### `fn` new

Create a new Vec2.

**Signature:** `(x:f64 y:f64 -- v:Vec2)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `x` | `f64` | X component |
| `y` | `f64` | Y component |

| Output | Type | Description |
|--------|------|-------------|
| `v` | `Vec2` | New 2D vector |

**Example:**

```qd
1.0 2.0 vec2::new -> v
```

---

### `fn` zero

Create a Vec2 with all components set to zero.

**Signature:** `( -- v:Vec2)`

| Output | Type | Description |
|--------|------|-------------|
| `v` | `Vec2` | Zero vector (0, 0) |

**Example:**

```qd
vec2::zero -> v
```

---

### `fn` one

Create a Vec2 with all components set to one.

**Signature:** `( -- v:Vec2)`

| Output | Type | Description |
|--------|------|-------------|
| `v` | `Vec2` | One vector (1, 1) |

**Example:**

```qd
vec2::one -> v
```

---

### `fn` add

Add two Vec2 vectors.

**Signature:** `(a:Vec2 b:Vec2 -- result:Vec2)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `a` | `Vec2` | First vector |
| `b` | `Vec2` | Second vector |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec2` | Sum of a and b |

**Example:**

```qd
v1 v2 vec2::add -> v3
```

---

### `fn` subtract

Subtract two Vec2 vectors.

**Signature:** `(a:Vec2 b:Vec2 -- result:Vec2)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `a` | `Vec2` | First vector |
| `b` | `Vec2` | Second vector |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec2` | Difference of a and b |

**Example:**

```qd
v1 v2 vec2::subtract -> v3
```

---

### `fn` scale

Multiply Vec2 by scalar.

**Signature:** `(v:Vec2 s:f64 -- result:Vec2)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `v` | `Vec2` | Vector to scale |
| `s` | `f64` | Scalar multiplier |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec2` | Scaled vector |

**Example:**

```qd
v 2.0 vec2::scale -> v2
```

---

### `fn` dot

Dot product of two Vec2 vectors.

**Signature:** `(a:Vec2 b:Vec2 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `a` | `Vec2` | First vector |
| `b` | `Vec2` | Second vector |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Dot product |

**Example:**

```qd
v1 v2 vec2::dot -> d
```

---

### `fn` length

Length (magnitude) of a Vec2.

**Signature:** `(v:Vec2 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `v` | `Vec2` | Input vector |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Length of vector |

**Example:**

```qd
v vec2::length -> len
```

---

### `fn` length_sq

Squared length of a Vec2 (avoids sqrt).

**Signature:** `(v:Vec2 -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `v` | `Vec2` | Input vector |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Squared length of vector |

**Example:**

```qd
v vec2::length_sq -> len_sq
```

---

### `fn` normalize

Normalize a Vec2 to unit length.

**Signature:** `(v:Vec2 -- result:Vec2)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `v` | `Vec2` | Input vector |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec2` | Unit vector in same direction |

**Example:**

```qd
v vec2::normalize -> unit
```

---

### `fn` neg

Negate a Vec2.

**Signature:** `(v:Vec2 -- result:Vec2)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `v` | `Vec2` | Input vector |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec2` | Negated vector |

**Example:**

```qd
v vec2::neg -> neg_v
```

---

### `fn` lerp

Linear interpolation between two Vec2 vectors.

**Signature:** `(a:Vec2 b:Vec2 t:f64 -- result:Vec2)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `a` | `Vec2` | Start vector |
| `b` | `Vec2` | End vector |
| `t` | `f64` | Interpolation factor (0.0 to 1.0) |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Vec2` | Interpolated vector |

**Example:**

```qd
v1 v2 0.5 vec2::lerp -> mid
```
