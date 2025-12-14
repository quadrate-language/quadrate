# `use` uuid

UUID generation (version 4 random UUIDs).
Format: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx

## Functions

### `fn` is_valid

Check if a string is a valid UUID format.

**Signature:** `( s:str -- valid:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | String to check |

| Output | Type | Description |
|--------|------|-------------|
| `valid` | `i64` | 1 if valid format, 0 otherwise |

**Example:**

```qd
"550e8400-e29b-41d4-a716-446655440000" uuid::is_valid  // result
```

---

### `fn` v4_seeded

Generate UUID with specific seed (for reproducibility).

**Signature:** `( seed:i64 -- uuid:str )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `seed` | `i64` | Seed value |

| Output | Type | Description |
|--------|------|-------------|
| `uuid` | `str` | UUID string (36 chars) |

**Example:**

```qd
12345 uuid::v4_seeded  // id
```

---

### `fn` v4

Generate a new random UUIDv4 string.

**Signature:** `(  -- uuid:str )`

| Output | Type | Description |
|--------|------|-------------|
| `uuid` | `str` | UUID string (36 chars) |

**Example:**

```qd
uuid::v4  // id
```

