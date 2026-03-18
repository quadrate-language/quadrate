# `use` os

Operating system interface.
Error codes: Ok=1 (success), specific errors start at 2

## Constants

| Name | Value | Description |
|------|-------|-------------|
| `ErrExists` | `4` | Error: File already exists. |
| `ErrInvalidArg` | `12` | Error: Invalid argument. |
| `ErrIo` | `7` | Error: I/O error. |
| `ErrIsDirectory` | `6` | Error: Path is a directory. |
| `ErrNameTooLong` | `10` | Error: File name too long. |
| `ErrNoSpace` | `8` | Error: No space left on device. |
| `ErrNotDirectory` | `5` | Error: Path is not a directory. |
| `ErrNotFound` | `2` | Error: No such file or directory. |
| `ErrOutOfMemory` | `11` | Error: Out of memory. |
| `ErrPermission` | `3` | Error: Permission denied. |
| `ErrReadOnly` | `9` | Error: Read-only file system. |
| `ExitFailure` | `1` | Exit code for failed termination. |
| `ExitSuccess` | `0` | Exit code for successful termination. |

## Functions

### `fn` chdir

Change the current working directory.

**Signature:** `(path:str -- )!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | `str` | Directory path |

| Error | Description |
|-------|-------------|
| `os::ErrNotFound` | Directory not found |
| `os::ErrPermission` | Permission denied |
| `os::ErrNotDirectory` | Path is not a directory |

**Example:**

```qd
"/tmp" os::chdir!
```
---

### `fn` chmod

Change file permissions. Mode is specified as Unix permission bits (e.g., 0o644, 0o755).

**Signature:** `(path:str mode:i64 -- )!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | `str` | File or directory path |
| `mode` | `i64` | Permission mode (e.g., 420 for 0o644) |

| Error | Description |
|-------|-------------|
| `os::ErrNotFound` | File not found |
| `os::ErrPermission` | Permission denied |

**Example:**

```qd
"/tmp/script.sh" 0o755 os::chmod!
```
---

### `fn` chown

Change file owner and group. Use -1 for uid or gid to leave unchanged.

**Signature:** `(path:str uid:i64 gid:i64 -- )!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | `str` | File or directory path |
| `uid` | `i64` | User ID (-1 to not change) |
| `gid` | `i64` | Group ID (-1 to not change) |

| Error | Description |
|-------|-------------|
| `os::ErrNotFound` | File not found |
| `os::ErrPermission` | Permission denied (usually requires root) |

**Example:**

```qd
"/tmp/myfile" 1000 1000 os::chown!
```
---

### `fn` copy

Copy a file.

**Signature:** `(srcpath:str dstpath:str -- )!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `srcpath` | `str` | Source path |
| `dstpath` | `str` | Destination path |

| Error | Description |
|-------|-------------|
| `os::ErrNotFound` | File not found |
| `os::ErrPermission` | Permission denied |

**Example:**

```qd
"src.txt" "dst.txt" os::copy!
```
---

### `fn` cwd

Get current working directory.

**Signature:** `( -- path:str)!`

| Output | Type | Description |
|--------|------|-------------|
| `path` | `str` | Current working directory path |

| Error | Description |
|-------|-------------|
| `os::ErrIo` | Failed to get directory |

**Example:**

```qd
os::cwd!  // dir
```
---

### `fn` delete

Delete a file or empty directory.

**Signature:** `(path:str -- )!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | `str` | Path to delete |

| Error | Description |
|-------|-------------|
| `os::ErrNotFound` | File not found |
| `os::ErrPermission` | Permission denied |

**Example:**

```qd
"/tmp/test.txt" os::delete!
```
---

### `fn` exec

Execute a command and capture all output. Simpler alternative to os::popen when you just need the full output.

**Signature:** `(cmd:str -- stdout:str exitcode:i64)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `cmd` | `str` | Command to execute |

| Output | Type | Description |
|--------|------|-------------|
| `stdout` | `str` | All output from command |
| `exitcode` | `i64` | Command exit status |

| Error | Description |
|-------|-------------|
| `os::ErrIo` | Failed to execute command |
| `os::ErrOutOfMemory` | Out of memory |

**Example:**

```qd
"echo hello" os::exec! -> exitcode  // stdout
```
---

### `fn` exists

Check if path exists.

**Signature:** `(path:str -- exists:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | `str` | File or directory path |

| Output | Type | Description |
|--------|------|-------------|
| `exists` | `i64` | 1 if exists, 0 otherwise |

**Example:**

```qd
"/tmp" os::exists print  // 1
```
---

### `fn` exit

Exit the program with status code.

**Signature:** `(code:i64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `code` | `i64` | Exit status (os::ExitSuccess or os::ExitFailure) |

**Example:**

```qd
os::ExitSuccess os::exit
```
---

### `fn` getenv

Get environment variable value.

**Signature:** `(name:str -- value:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `name` | `str` | Variable name |

| Output | Type | Description |
|--------|------|-------------|
| `value` | `str` | Variable value (empty if not set) |

**Example:**

```qd
"HOME" os::getenv  // home
```
---

### `fn` getgid

Get the current group ID.

**Signature:** `( -- gid:i64)`

| Output | Type | Description |
|--------|------|-------------|
| `gid` | `i64` | Real group ID of the calling process |

**Example:**

```qd
os::getgid print
```
---

### `fn` getpid

Get the current process ID.

**Signature:** `( -- pid:i64)`

| Output | Type | Description |
|--------|------|-------------|
| `pid` | `i64` | Process ID |

**Example:**

```qd
os::getpid print nl
```
---

### `fn` getuid

Get the current user ID.

**Signature:** `( -- uid:i64)`

| Output | Type | Description |
|--------|------|-------------|
| `uid` | `i64` | Real user ID of the calling process |

**Example:**

```qd
os::getuid print
```
---

### `fn` glob

Match files using glob pattern. Supports *, ?, and ** for recursive matching.

**Signature:** `(pattern:str -- entries:ptr count:i64)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `pattern` | `str` | Glob pattern |

| Output | Type | Description |
|--------|------|-------------|
| `entries` | `ptr` | Array of matching paths |
| `count` | `i64` | Number of matches |

**Example:**

```qd
"*.qd" os::glob! -> count  // entries
```
---

### `fn` hostname

Get the system hostname.

**Signature:** `( -- hostname:str)!`

| Output | Type | Description |
|--------|------|-------------|
| `hostname` | `str` | Hostname of the current system |

| Error | Description |
|-------|-------------|
| `os::ErrIo` | Failed to get hostname |

**Example:**

```qd
os::hostname! print
```
---

### `fn` is_dir

Check if path is a directory.

**Signature:** `(path:str -- is_dir:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | `str` | Path to check |

| Output | Type | Description |
|--------|------|-------------|
| `is_dir` | `i64` | 1 if directory, 0 otherwise |

**Example:**

```qd
"/tmp" os::is_dir print  // 1
```
---

### `fn` is_file

Check if path is a regular file.

**Signature:** `(path:str -- is_file:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | `str` | Path to check |

| Output | Type | Description |
|--------|------|-------------|
| `is_file` | `i64` | 1 if regular file, 0 otherwise |

**Example:**

```qd
"/etc/passwd" os::is_file print  // 1
```
---

### `fn` is_symlink

Check if path is a symbolic link. Does not follow the link to check if the target exists.

**Signature:** `(path:str -- is_symlink:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | `str` | Path to check |

| Output | Type | Description |
|--------|------|-------------|
| `is_symlink` | `i64` | 1 if symbolic link, 0 otherwise |

**Example:**

```qd
"/tmp/mylink" os::is_symlink print
```
---

### `fn` list

List directory contents.

**Signature:** `(path:str -- entries:ptr count:i64)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | `str` | Directory path |

| Output | Type | Description |
|--------|------|-------------|
| `entries` | `ptr` | Array of entry names |
| `count` | `i64` | Number of entries |

| Error | Description |
|-------|-------------|
| `os::ErrNotFound` | File not found |
| `os::ErrNotDirectory` | Path is not a directory |

**Example:**

```qd
"/tmp" os::list! -> entries  // count
```
---

### `fn` mkdir

Create a directory and all parent directories.

**Signature:** `(path:str -- )!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | `str` | Directory path |

| Error | Description |
|-------|-------------|
| `os::ErrPermission` | Permission denied |

**Example:**

```qd
"/tmp/a/b/c" os::mkdir!
```
---

### `fn` mktemp

Create a unique temporary directory. Creates a directory in /tmp with a unique name. Caller is responsible for removing it when done (use os::rmdir).

**Signature:** `( -- path:str)!`

| Output | Type | Description |
|--------|------|-------------|
| `path` | `str` | Path to the created directory |

| Error | Description |
|-------|-------------|
| `os::ErrIo` | Failed to create directory |

**Example:**

```qd
os::mktemp!  // tmpdir
```
---

### `fn` popen

Execute a command and stream output line-by-line to a callback. The callback is called synchronously for each line of output (without trailing newline). The call blocks until the command completes. Closures with captured variables are supported.

**Signature:** `(cmd:str callback:ptr -- exitcode:i64)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `cmd` | `str` | Command to execute |
| `callback` | `ptr` | Callback function (line:str -- ) |

| Output | Type | Description |
|--------|------|-------------|
| `exitcode` | `i64` | Command exit status |

| Error | Description |
|-------|-------------|
| `os::ErrIo` | Failed to execute command |

**Example:**

```qd
"ls -la" fn (line:str -- ) { print nl } os::popen!  // code
```
---

### `fn` readlink

Read the target of a symbolic link.

**Signature:** `(path:str -- target:str)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | `str` | Path to the symbolic link |

| Output | Type | Description |
|--------|------|-------------|
| `target` | `str` | Path that the link points to |

| Error | Description |
|-------|-------------|
| `os::ErrNotFound` | Link not found |
| `os::ErrInvalidArg` | Path is not a symbolic link |

**Example:**

```qd
"/tmp/mylink" os::readlink!  // target
```
---

### `fn` rename

Rename or move a file.

**Signature:** `(oldpath:str newpath:str -- )!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `oldpath` | `str` | Current path |
| `newpath` | `str` | New path |

| Error | Description |
|-------|-------------|
| `os::ErrNotFound` | File not found |
| `os::ErrPermission` | Permission denied |

**Example:**

```qd
"old.txt" "new.txt" os::rename!
```
---

### `fn` rmdir

Remove directory and all contents recursively.

**Signature:** `(path:str -- )!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | `str` | Directory path to remove |

| Error | Description |
|-------|-------------|
| `os::ErrNotFound` | Path not found |
| `os::ErrPermission` | Permission denied |

**Example:**

```qd
"/tmp/mydir" os::rmdir!
```
---

### `fn` setenv

Set environment variable.

**Signature:** `(name:str value:str -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `name` | `str` | Variable name |
| `value` | `str` | Variable value |

**Example:**

```qd
"MY_VAR" "hello" os::setenv
```
---

### `fn` symlink

Create a symbolic link.

**Signature:** `(target:str linkpath:str -- )!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `target` | `str` | Path that the link points to |
| `linkpath` | `str` | Path where the link will be created |

| Error | Description |
|-------|-------------|
| `os::ErrExists` | Link path already exists |
| `os::ErrPermission` | Permission denied |

**Example:**

```qd
"/usr/bin/python3" "/tmp/python" os::symlink!
```
---

### `fn` system

Execute a shell command.

**Signature:** `(cmd:str -- exitcode:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `cmd` | `str` | Command to execute |

| Output | Type | Description |
|--------|------|-------------|
| `exitcode` | `i64` | Command exit status |

**Example:**

```qd
"ls -la" os::system  // code
```
---

### `fn` unsetenv

Unset environment variable.

**Signature:** `(name:str -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `name` | `str` | Variable name to remove |

**Example:**

```qd
"MY_VAR" os::unsetenv
```
---

### `fn` walk

Walk a directory tree recursively. Calls callback for each file and directory.

**Signature:** `(path:str callback:ptr -- )!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | `str` | Directory path to walk |
| `callback` | `ptr` | Callback function (path:str is_dir:i64 depth:i64 -- ) |

| Error | Description |
|-------|-------------|
| `os::ErrNotFound` | Path not found |
| `os::ErrNotDirectory` | Path is not a directory |

**Example:**

```qd
"/tmp" fn (p:str is_dir:i64 depth:i64 -- ) { p print nl } os::walk!
```
