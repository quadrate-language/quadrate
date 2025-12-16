# `use` quat

Quaternion math operations for 3D rotations.

## Structs

### `struct` Quat

Quaternion for representing rotations. Stored as (w, x, y, z) where w is the scalar part.

| Field | Type | Description |
|-------|------|-------------|
| `w` | `f64` | Scalar (real) component |
| `x` | `f64` | X component of vector part |
| `y` | `f64` | Y component of vector part |
| `z` | `f64` | Z component of vector part |

**Creating a Quat:**

```qd
Quat { w = 1.0 x = 0.0 y = 0.0 z = 0.0 } -> q
```

## Functions

### `fn` identity

Create identity quaternion (no rotation).

**Signature:** `( -- q:Quat)`

| Output | Type | Description |
|--------|------|-------------|
| `q` | `Quat` | Identity quaternion |

**Example:**

```qd
quat::identity -> q
```

---

### `fn` mul

Quaternion multiplication.

**Signature:** `(a:Quat b:Quat -- result:Quat)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `a` | `Quat` | First quaternion |
| `b` | `Quat` | Second quaternion |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Quat` | Product of a and b |

**Example:**

```qd
q1 q2 quat::mul -> q3
```

---

### `fn` conjugate

Conjugate of quaternion.

**Signature:** `(q:Quat -- result:Quat)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `q` | `Quat` | Input quaternion |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Quat` | Conjugate quaternion |

**Example:**

```qd
q quat::conjugate -> q_conj
```

---

### `fn` length

Length (magnitude) of quaternion.

**Signature:** `(q:Quat -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `q` | `Quat` | Input quaternion |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Length of quaternion |

**Example:**

```qd
q quat::length -> len
```

---

### `fn` normalize

Normalize quaternion to unit length.

**Signature:** `(q:Quat -- result:Quat)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `q` | `Quat` | Input quaternion |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `Quat` | Unit quaternion |

**Example:**

```qd
q quat::normalize -> unit_q
```

---

### `fn` dot

Dot product of two quaternions.

**Signature:** `(a:Quat b:Quat -- result:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `a` | `Quat` | First quaternion |
| `b` | `Quat` | Second quaternion |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `f64` | Dot product |

**Example:**

```qd
q1 q2 quat::dot -> d
```
