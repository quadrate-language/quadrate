# time

Time operations and duration constants.

## Constants

| Name | Value | Description |
|------|-------|-------------|
| `Nanosecond` | `1` | Duration: 1 nanosecond. |
| `Microsecond` | `1000` | Duration: 1 microsecond (1000 nanoseconds). |
| `Millisecond` | `1000000` | Duration: 1 millisecond (1,000,000 nanoseconds). |
| `Second` | `1000000000` | Duration: 1 second (1,000,000,000 nanoseconds). |
| `Minute` | `60000000000` | Duration: 1 minute (60 seconds). |
| `Hour` | `3600000000000` | Duration: 1 hour (60 minutes). |

## Functions

### unix

Get Unix timestamp in seconds.

**Signature:** `( -- timestamp:i64 )`

| Return | Type | Description |
|--------|------|-------------|
| `timestamp` | `i64` | Seconds since 1970-01-01 00:00:00 UTC |

**Example:**

```qd
time::unix .  // 1700000000
```

---

### now

Get current time in nanoseconds.

**Signature:** `( -- nanoseconds:i64 )`

| Return | Type | Description |
|--------|------|-------------|
| `nanoseconds` | `i64` | Monotonic nanosecond timestamp |

**Example:**

```qd
time::now -> start
```

---

### sleep

Sleep for duration in nanoseconds.

**Signature:** `( duration:i64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `duration` | `i64` | Duration to sleep (use time constants) |

**Example:**

```qd
time::Second time::sleep
```
