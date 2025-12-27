# `use` time

## Constants

| Name | Value | Description |
|------|-------|-------------|
| `April` | `4` |  |
| `August` | `8` |  |
| `Day` | `86400000000000` | Duration = 1 day (24 hours). |
| `December` | `12` |  |
| `February` | `2` |  |
| `Friday` | `5` |  |
| `Hour` | `3600000000000` | Duration = 1 hour (60 minutes). |
| `January` | `1` | Month constants (1-indexed) |
| `July` | `7` |  |
| `June` | `6` |  |
| `March` | `3` |  |
| `May` | `5` |  |
| `Microsecond` | `1000` | Duration = 1 microsecond (1000 nanoseconds). |
| `Millisecond` | `1000000` | Duration = 1 millisecond (1,000,000 nanoseconds). |
| `Minute` | `60000000000` | Duration = 1 minute (60 seconds). |
| `Monday` | `1` |  |
| `Nanosecond` | `1` | Time operations and duration constants. Duration = 1 nanosecond. |
| `November` | `11` |  |
| `October` | `10` |  |
| `Saturday` | `6` |  |
| `Second` | `1000000000` | Duration = 1 second (1,000,000,000 nanoseconds). |
| `September` | `9` |  |
| `Sunday` | `0` | Weekday constants (Sunday=0) |
| `Thursday` | `4` |  |
| `Tuesday` | `2` |  |
| `Wednesday` | `3` |  |
| `Week` | `604800000000000` | Duration = 1 week (7 days). |

## Functions

### `fn` add

Add duration to a timestamp.

**Signature:** `(ts:i64 duration:i64 -- new_ts:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `ts` | `i64` | Timestamp (use time::now for nanoseconds) |
| `duration` | `i64` | Duration to add (use time constants) |

| Output | Type | Description |
|--------|------|-------------|
| `new_ts` | `i64` | New timestamp |

**Example:**

```qd
time::now time::Day 4 * time::add .
```
---

### `fn` after

Check if timestamp is after another.

**Signature:** `(ts1:i64 ts2:i64 -- after:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `ts1` | `i64` | First timestamp |
| `ts2` | `i64` | Second timestamp |

| Output | Type | Description |
|--------|------|-------------|
| `after` | `i64` | 1 if ts1 > ts2, 0 otherwise |

**Example:**

```qd
ts1 ts2 time::after .
```
---

### `fn` before

Check if timestamp is before another.

**Signature:** `(ts1:i64 ts2:i64 -- before:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `ts1` | `i64` | First timestamp |
| `ts2` | `i64` | Second timestamp |

| Output | Type | Description |
|--------|------|-------------|
| `before` | `i64` | 1 if ts1 < ts2, 0 otherwise |

**Example:**

```qd
ts1 ts2 time::before .
```
---

### `fn` date

Create Unix timestamp from date components.

**Signature:** `(yr:i64 mon:i64 d:i64 h:i64 m:i64 s:i64 -- ts:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `yr` | `i64` | Year |
| `mon` | `i64` | Month (1-12) |
| `d` | `i64` | Day (1-31) |
| `h` | `i64` | Hour (0-23) |
| `m` | `i64` | Minute (0-59) |
| `s` | `i64` | Second (0-59) |

| Output | Type | Description |
|--------|------|-------------|
| `ts` | `i64` | Unix timestamp in seconds |

**Example:**

```qd
2024 1 15 12 30 0 time::date .
```
---

### `fn` days_in_month

Get number of days in a month.

**Signature:** `(yr:i64 mon:i64 -- days:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `yr` | `i64` | Year (needed for February) |
| `mon` | `i64` | Month (1-12) |

| Output | Type | Description |
|--------|------|-------------|
| `days` | `i64` | Number of days in the month |

**Example:**

```qd
2024 2 time::days_in_month print  // 29
```
---

### `fn` day

Extract day of month from Unix timestamp (1-31).

**Signature:** `(ts:i64 -- d:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `ts` | `i64` | Unix timestamp in seconds |

| Output | Type | Description |
|--------|------|-------------|
| `d` | `i64` | Day of month |

**Example:**

```qd
time::unix time::day .
```
---

### `fn` hour

Extract hour from Unix timestamp (0-23).

**Signature:** `(ts:i64 -- h:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `ts` | `i64` | Unix timestamp in seconds |

| Output | Type | Description |
|--------|------|-------------|
| `h` | `i64` | Hour |

**Example:**

```qd
time::unix time::hour .
```
---

### `fn` is_leap_year

Check if year is a leap year.

**Signature:** `(yr:i64 -- is_leap:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `yr` | `i64` | Year to check |

| Output | Type | Description |
|--------|------|-------------|
| `is_leap` | `i64` | 1 if leap year, 0 otherwise |

**Example:**

```qd
2024 time::is_leap_year print  // 1
```
---

### `fn` minute

Extract minute from Unix timestamp (0-59).

**Signature:** `(ts:i64 -- m:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `ts` | `i64` | Unix timestamp in seconds |

| Output | Type | Description |
|--------|------|-------------|
| `m` | `i64` | Minute |

**Example:**

```qd
time::unix time::minute .
```
---

### `fn` month

Extract month from Unix timestamp (1-12).

**Signature:** `(ts:i64 -- mon:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `ts` | `i64` | Unix timestamp in seconds |

| Output | Type | Description |
|--------|------|-------------|
| `mon` | `i64` | Month (1=January, 12=December) |

**Example:**

```qd
time::unix time::month .
```
---

### `fn` now

Get current time in nanoseconds since epoch.

**Signature:** `( -- nanoseconds:i64)`

| Output | Type | Description |
|--------|------|-------------|
| `nanoseconds` | `i64` | Nanoseconds since 1970-01-01 00:00:00 UTC |

**Example:**

```qd
time::now  // start
```
---

### `fn` second

Extract second from Unix timestamp (0-59).

**Signature:** `(ts:i64 -- s:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `ts` | `i64` | Unix timestamp in seconds |

| Output | Type | Description |
|--------|------|-------------|
| `s` | `i64` | Second |

**Example:**

```qd
time::unix time::second .
```
---

### `fn` sleep

Sleep for duration in nanoseconds.

**Signature:** `(duration:i64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `duration` | `i64` | Duration to sleep (use time constants) |

**Example:**

```qd
time::Second time::sleep
```
---

### `fn` sub

Get difference between two timestamps in seconds.

**Signature:** `(ts1:i64 ts2:i64 -- diff:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `ts1` | `i64` | First timestamp |
| `ts2` | `i64` | Second timestamp |

| Output | Type | Description |
|--------|------|-------------|
| `diff` | `i64` | ts1 - ts2 in seconds |

**Example:**

```qd
end_time start_time time::sub .
```
---

### `fn` unix

Get Unix timestamp in seconds.

**Signature:** `( -- timestamp:i64)`

| Output | Type | Description |
|--------|------|-------------|
| `timestamp` | `i64` | Seconds since 1970-01-01 00:00:00 UTC |

**Example:**

```qd
time::unix print  // 1700000000
```
---

### `fn` weekday

Get day of week from Unix timestamp (0=Sunday, 6=Saturday).

**Signature:** `(ts:i64 -- wd:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `ts` | `i64` | Unix timestamp in seconds |

| Output | Type | Description |
|--------|------|-------------|
| `wd` | `i64` | Weekday (0=Sunday) |

**Example:**

```qd
time::unix time::weekday .
```
---

### `fn` year_day

Get day of year from Unix timestamp (1-366).

**Signature:** `(ts:i64 -- yd:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `ts` | `i64` | Unix timestamp in seconds |

| Output | Type | Description |
|--------|------|-------------|
| `yd` | `i64` | Day of year |

**Example:**

```qd
time::unix time::year_day .
```
---

### `fn` year

Extract year from Unix timestamp.

**Signature:** `(ts:i64 -- yr:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `ts` | `i64` | Unix timestamp in seconds |

| Output | Type | Description |
|--------|------|-------------|
| `yr` | `i64` | Year (e.g., 2024) |

**Example:**

```qd
time::unix time::year .
```
