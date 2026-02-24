# log

Structured logging with levels, rotation, and multiple outputs for [Quadrate](https://git.sr.ht/~klahr/quadrate).

## Installation

```bash
quadpm get https://git.sr.ht/~klahr/qdlog
```

**Note**: This module requires native compilation. The C source files in `src/` must be compiled and linked.

## Usage

```quadrate
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

    // JSON format
    logger log::FormatJson log::set_format

    // Cleanup
    logger log::free
}
```

## Log Levels

| Constant | Value | Description |
|----------|-------|-------------|
| `LevelDebug` | 0 | Debug messages (most verbose) |
| `LevelInfo` | 1 | Informational messages |
| `LevelWarn` | 2 | Warning messages |
| `LevelError` | 3 | Error messages only |
| `LevelOff` | 4 | Disable all logging |

## Output Formats

| Constant | Description |
|----------|-------------|
| `FormatText` | Human-readable text format |
| `FormatJson` | JSON lines format |

## Rotation Modes

| Constant | Description |
|----------|-------------|
| `RotateNone` | No rotation |
| `RotateSize` | Rotate by file size |
| `RotateDaily` | Rotate daily |
| `RotateHourly` | Rotate hourly |

## Functions

| Function | Description |
|----------|-------------|
| `new!` | Create a new logger |
| `free` | Free logger resources |
| `set_level` | Set minimum log level |
| `get_level` | Get current log level |
| `set_format` | Set output format |
| `enable_stdout` | Enable stdout output |
| `disable_stdout` | Disable stdout output |
| `add_file!` | Add file output |
| `add_file_rotate!` | Add file output with rotation |
| `debug` | Log debug message |
| `info` | Log info message |
| `warn` | Log warning message |
| `error` | Log error message |
| `log` | Log at specified level |
| `flush` | Flush all outputs |

## License

Apache-2.0 - See [LICENSE](LICENSE) for details.

## Contributing

Contributions welcome! Please open an issue or submit a patch on [SourceHut](https://git.sr.ht/~klahr/qdlog).
