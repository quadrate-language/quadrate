# `use` log

Logging module - structured logging with levels and rotation.

Provides:
- Log levels: Debug, Info, Warn, Error
- Structured key-value logging
- Multiple outputs (stdout, files)
- Log rotation (by size, daily, hourly)
- Text and JSON formats

## Example

    use log

    fn main() {
        // Create logger
        log::new! -> logger

        // Basic logging
        logger "server started" log::info
        logger "connection failed" log::error

        // Set minimum level
        logger log::LevelWarn log::set_level

        // Add file output with rotation
        logger "/var/log/app.log" log::RotateDaily 0 7 log::add_file_rotate!

        // Structured logging with key-value pairs
        logger "user login" "user" "alice" "ip" "192.168.1.1" 2 log::info_kv

        // JSON format
        logger log::FormatJson log::set_format

        // Cleanup
        logger log::free
    }

## Constants

| Name | Value | Description |
|------|-------|-------------|
| `ErrAlloc` | `2` | Error: Memory allocation failed. |
| `ErrFile` | `3` | Error: File operation failed. |
| `ErrInvalid` | `4` | Error: Invalid argument. |
| `FormatJson` | `1` | Output format: JSON lines. |
| `FormatText` | `0` | Output format: human-readable text. |
| `LevelDebug` | `0` | Log level: Debug (most verbose). |
| `LevelError` | `3` | Log level: Error (errors only). |
| `LevelInfo` | `1` | Log level: Info (normal messages). |
| `LevelOff` | `4` | Log level: Off (no logging). |
| `LevelWarn` | `2` | Log level: Warn (warnings). |
| `RotateDaily` | `2` | Rotation mode: rotate daily. |
| `RotateHourly` | `3` | Rotation mode: rotate hourly. |
| `RotateNone` | `0` | Rotation mode: no rotation. |
| `RotateSize` | `1` | Rotation mode: rotate by file size. |

## Logger

Logger handle.

### Struct

| Field | Type | Description |
|-------|------|-------------|
| `handle` | `ptr` |  |

### Constructors

#### `fn` new

Create a new logger.  By default logs to stdout at Info level in text format. 

**Signature:** `( -- logger:Logger)!`

| Output | Type | Description |
|--------|------|-------------|
| `logger` | `Logger` | New logger handle |

**Example:**

```qd
log::new!  // logger
```

### Methods

#### `fn` add_file

Add file output (no rotation). 

**Signature:** `(logger:Logger) add_file(path:str -- )!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | `str` | Log file path |

| Error | Description |
|-------|-------------|
| `log::ErrFile` | Failed to open file |

**Example:**

```qd
logger "/var/log/app.log" log::add_file!
```
---

#### `fn` add_file_rotate

Add file output with rotation. 

**Signature:** `(logger:Logger) add_file_rotate(path:str mode:i64 max_size:i64 max_files:i64 -- )!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | `str` | Base log file path |
| `mode` | `i64` | Rotation mode (RotateNone, RotateSize, RotateDaily, RotateHourly) |
| `max_size` | `i64` | Max file size for RotateSize (bytes), 0 for time-based |
| `max_files` | `i64` | Max rotated files to keep (0 = unlimited) |

| Error | Description |
|-------|-------------|
| `log::ErrFile` | Failed to open file |

**Example:**

```qd
logger "/var/log/app.log" log::RotateDaily 0 7 log::add_file_rotate!
```
---

#### `fn` check_rotate

Check and perform rotation if needed.  Called automatically before each log, but can be called manually. 

**Signature:** `(logger:Logger) check_rotate( -- )`

**Example:**

```qd
logger log::check_rotate
```
---

#### `fn` debug

Log a debug message. 

**Signature:** `(logger:Logger) debug(msg:str -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `msg` | `str` | Message to log |

**Example:**

```qd
logger "processing item" log::debug
```
---

#### `fn` disable_stdout

Disable stdout output. 

**Signature:** `(logger:Logger) disable_stdout( -- )`

**Example:**

```qd
logger log::disable_stdout
```
---

#### `fn` enable_stdout

Enable stdout output (default). 

**Signature:** `(logger:Logger) enable_stdout( -- )`

**Example:**

```qd
logger log::enable_stdout
```
---

#### `fn` error

Log an error message. 

**Signature:** `(logger:Logger) error(msg:str -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `msg` | `str` | Message to log |

**Example:**

```qd
logger "connection failed" log::error
```
---

#### `fn` flush

Flush all outputs. 

**Signature:** `(logger:Logger) flush( -- )`

**Example:**

```qd
logger log::flush
```
---

#### `fn` free

Free logger resources. 

**Signature:** `(logger:Logger) free( -- )`

**Example:**

```qd
logger log::free
```
---

#### `fn` get_level

Get current log level. 

**Signature:** `(logger:Logger) get_level( -- level:i64)`

| Output | Type | Description |
|--------|------|-------------|
| `level` | `i64` | Current minimum level |

**Example:**

```qd
logger log::get_level  // level
```
---

#### `fn` info

Log an info message. 

**Signature:** `(logger:Logger) info(msg:str -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `msg` | `str` | Message to log |

**Example:**

```qd
logger "server started" log::info
```
---

#### `fn` log

Log a message at specified level. 

**Signature:** `(logger:Logger) log(level:i64 msg:str -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `level` | `i64` | Log level |
| `msg` | `str` | Message to log |

**Example:**

```qd
logger log::LevelInfo "custom message" log::log
```
---

#### `fn` set_format

Set output format. 

**Signature:** `(logger:Logger) set_format(format:i64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `format` | `i64` | Format (FormatText or FormatJson) |

**Example:**

```qd
logger log::FormatJson log::set_format
```
---

#### `fn` set_level

Set minimum log level.  Messages below this level are ignored. 

**Signature:** `(logger:Logger) set_level(level:i64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `level` | `i64` | Minimum level (LevelDebug, LevelInfo, LevelWarn, LevelError, LevelOff) |

**Example:**

```qd
logger log::LevelWarn log::set_level
```
---

#### `fn` warn

Log a warning message. 

**Signature:** `(logger:Logger) warn(msg:str -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `msg` | `str` | Message to log |

**Example:**

```qd
logger "connection timeout" log::warn
```

