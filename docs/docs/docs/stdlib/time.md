# `use` time

## Constants

| Name | Value | Description |
|------|-------|-------------|
| `April` | `4` |  |
| `August` | `8` |  |
| `Day` | `86400000000000` | Duration = 1 day (24 hours). |
| `December` | `12` |  |
| `ErrFormat` | `2` | Error: Format operation failed. |
| `ErrParse` | `3` | Error: Parse operation failed. |
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

### `fn` add_days

Add days to a timestamp.

**Signature:** `(ts:i64 days:i64 -- new_ts:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `ts` | `i64` | Unix timestamp in seconds |
| `days` | `i64` | Number of days to add |

| Output | Type | Description |
|--------|------|-------------|
| `new_ts` | `i64` | New timestamp |

**Example:**

```qd
time::unix 7 time::add_days .
```
---

### `fn` add_hours

Add hours to a timestamp.

**Signature:** `(ts:i64 hours:i64 -- new_ts:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `ts` | `i64` | Unix timestamp in seconds |
| `hours` | `i64` | Number of hours to add |

| Output | Type | Description |
|--------|------|-------------|
| `new_ts` | `i64` | New timestamp |

**Example:**

```qd
time::unix 2 time::add_hours .
```
---

### `fn` add_minutes

Add minutes to a timestamp.

**Signature:** `(ts:i64 mins:i64 -- new_ts:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `ts` | `i64` | Unix timestamp in seconds |
| `mins` | `i64` | Number of minutes to add |

| Output | Type | Description |
|--------|------|-------------|
| `new_ts` | `i64` | New timestamp |

**Example:**

```qd
time::unix 30 time::add_minutes .
```
---

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

### `fn` days_between

Get number of full days between two timestamps.

**Signature:** `(ts1:i64 ts2:i64 -- days:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `ts1` | `i64` | First timestamp in seconds |
| `ts2` | `i64` | Second timestamp in seconds |

| Output | Type | Description |
|--------|------|-------------|
| `days` | `i64` | Number of full days between timestamps (absolute) |

**Example:**

```qd
end_ts start_ts time::days_between .
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

### `fn` days_in_year

Get number of days in a year.

**Signature:** `(yr:i64 -- days:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `yr` | `i64` | Year |

| Output | Type | Description |
|--------|------|-------------|
| `days` | `i64` | 365 or 366 for leap year |

**Example:**

```qd
2024 time::days_in_year .  // 366
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

### `fn` end_of_day

Get end of day (23:59:59) for a timestamp.

**Signature:** `(ts:i64 -- end_ts:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `ts` | `i64` | Unix timestamp in seconds |

| Output | Type | Description |
|--------|------|-------------|
| `end_ts` | `i64` | Timestamp at 23:59:59 of same day |

**Example:**

```qd
time::unix time::end_of_day .
```
---

### `fn` format

Format timestamp using strftime format string. Common format specifiers: - %Y: 4-digit year, %m: month (01-12), %d: day (01-31) - %H: hour (00-23), %M: minute (00-59), %S: second (00-59) - %F: date (YYYY-MM-DD), %T: time (HH:MM:SS) - %A: weekday name, %B: month name

**Signature:** `(timestamp:i64 format:str -- result:str)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `timestamp` | `i64` | Unix timestamp in seconds |
| `format` | `str` | strftime format string |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `str` | Formatted time string |

| Error | Description |
|-------|-------------|
| `time::ErrFormat` | Format error or invalid timestamp |

**Example:**

```qd
time::unix "%Y-%m-%d %H:%M:%S" time::format! print
```
---

### `fn` hours_between

Get number of full hours between two timestamps.

**Signature:** `(ts1:i64 ts2:i64 -- hours:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `ts1` | `i64` | First timestamp in seconds |
| `ts2` | `i64` | Second timestamp in seconds |

| Output | Type | Description |
|--------|------|-------------|
| `hours` | `i64` | Number of full hours between timestamps (absolute) |

**Example:**

```qd
end_ts start_ts time::hours_between .
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

### `fn` iso_week

Get ISO week number (1-53).

**Signature:** `(ts:i64 -- week:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `ts` | `i64` | Unix timestamp in seconds |

| Output | Type | Description |
|--------|------|-------------|
| `week` | `i64` | ISO week number |

**Example:**

```qd
time::unix time::iso_week .
```
---

### `fn` is_weekday

Check if weekday is a weekday (Monday-Friday).

**Signature:** `(wd:i64 -- is_wkday:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `wd` | `i64` | Weekday (0=Sunday, 6=Saturday) |

| Output | Type | Description |
|--------|------|-------------|
| `is_wkday` | `i64` | 1 if weekday, 0 otherwise |

**Example:**

```qd
time::unix time::weekday time::is_weekday .
```
---

### `fn` is_weekend

Check if weekday is a weekend day (Saturday or Sunday).

**Signature:** `(wd:i64 -- is_wknd:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `wd` | `i64` | Weekday (0=Sunday, 6=Saturday) |

| Output | Type | Description |
|--------|------|-------------|
| `is_wknd` | `i64` | 1 if weekend, 0 otherwise |

**Example:**

```qd
time::unix time::weekday time::is_weekend .
```
---

### `fn` micros

Get current time in microseconds since epoch.

**Signature:** `( -- microseconds:i64)`

| Output | Type | Description |
|--------|------|-------------|
| `microseconds` | `i64` | Microseconds since 1970-01-01 00:00:00 UTC |

**Example:**

```qd
time::micros  // us
```
---

### `fn` millis

Get current time in milliseconds since epoch.

**Signature:** `( -- milliseconds:i64)`

| Output | Type | Description |
|--------|------|-------------|
| `milliseconds` | `i64` | Milliseconds since 1970-01-01 00:00:00 UTC |

**Example:**

```qd
time::millis  // ms
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

### `fn` parse

Parse string to timestamp using strptime format string.

**Signature:** `(input:str format:str -- timestamp:i64)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `input` | `str` | String to parse |
| `format` | `str` | strptime format string |

| Output | Type | Description |
|--------|------|-------------|
| `timestamp` | `i64` | Unix timestamp in seconds |

| Error | Description |
|-------|-------------|
| `time::ErrParse` | Parse failed or invalid format |

**Example:**

```qd
"2024-01-15" "%Y-%m-%d" time::parse!  // ts
```
---

### `fn` same_day

Check if two timestamps are on the same day.

**Signature:** `(ts1:i64 ts2:i64 -- same:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `ts1` | `i64` | First timestamp |
| `ts2` | `i64` | Second timestamp |

| Output | Type | Description |
|--------|------|-------------|
| `same` | `i64` | 1 if same day, 0 otherwise |

**Example:**

```qd
ts1 ts2 time::same_day .
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

### `fn` start_of_day

Get start of day (midnight) for a timestamp.

**Signature:** `(ts:i64 -- midnight_ts:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `ts` | `i64` | Unix timestamp in seconds |

| Output | Type | Description |
|--------|------|-------------|
| `midnight_ts` | `i64` | Timestamp at midnight of same day |

**Example:**

```qd
time::unix time::start_of_day .
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
