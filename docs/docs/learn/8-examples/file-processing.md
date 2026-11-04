# Example: file processing

Working with files in Quadrate.

## Reading a file

The simplest way to read a file is with `io::read_file`:

```qd
use io

fn main() {
	"/etc/hostname" io::read_file switch {
		Ok { -> content "Hostname: " print content print }
		_ { drop err drop drop "Could not read file" print nl }
	}
}
```

## Writing a file

Use `io::write_file` to write string contents:

```qd
use io

fn main() {
	"/tmp/hello.txt" "Hello, World!\n" io::write_file switch {
		Ok { "File written successfully" print nl }
		_ { err drop drop "Failed to write file" print nl }
	}
}
```

## Reading with low-level API

For more control, use `io::open`, `io::read` and the rest. Inside a fallible function, call
them with `?`, so a failure is passed on to the caller and the defers still run:

<!-- doccheck: compile-only reads a file that only exists on the reader's machine -->
```qd
use io
use mem

def read_file(path:str -- content:str)! {
	path io::Read io::open? -> file
	defer { file io::close }

	file 0 io::SeekEnd io::seek? -> size
	file 0 io::SeekSet io::seek? drop

	size mem::alloc? -> buf
	defer { buf mem::free }

	file buf size io::read? -> bytes_read
	buf bytes_read mem::to_string
}

fn main() {
	"test.txt" read_file switch {
		Ok { -> content "File contents:" print nl content print nl }
		_ { drop err drop drop "Could not read file" print nl }
	}
}
```

## Writing with low-level API

```qd
use io
use mem

def write_file(path:str content:str -- )! {
	path io::Write io::open? -> file
	defer { file io::close }

	content mem::from_string -> size -> buf
	defer { buf mem::free }

	file buf size io::write? drop
}

fn main() {
	"output.txt" "Hello, World!\n" write_file switch {
		Ok { "File written successfully" print nl }
		_ { err drop drop "Failed to write file" print nl }
	}
}
```

## Line-by-line processing

```qd
use io
use strings

def process_line(num:i64 line:str -- ) {
	num print ": " print line print nl
}

fn main() {
	"data.txt" io::read_file switch {
		Ok {
			-> content
			content strings::lines! -> lines
			0 lines len 1 for i {
				i 1 + lines i nth process_line
			}
			lines free
		}
		_ { drop err drop drop "Could not read file" print nl }
	}
}
```

`strings::lines` splits on `\n`, so a file that ends in a newline, as most do, gives one
more, empty, line at the end.

## Word count

<!-- doccheck: compile-only reads a file that only exists on the reader's machine -->
```qd
use io
use strings

def count_words(path:str -- words:i64 lines:i64 chars:i64)! {
	path io::read_file? -> content

	content len -> chars
	content strings::lines? -> line_array
	line_array len -> lines

	0 -> words
	0 lines 1 for i {
		line_array i nth -> line
		line len 0 > if {
			line strings::words? -> word_arr
			words word_arr len + -> words
			word_arr free
		}
	}
	line_array free

	words lines chars
}

fn main() {
	"document.txt" count_words switch {
		Ok {
			-> chars -> lines -> words
			"Words: " print words print nl
			"Lines: " print lines print nl
			"Chars: " print chars print nl
		}
		_ { drop drop drop err drop drop "Could not count" print nl }
	}
}
```

## Copy file

<!-- doccheck: compile-only reads a file that only exists on the reader's machine -->
```qd
use io
use mem

const BufferSize = 4096

def copy_file(src:str dst:str -- total:i64)! {
	src io::ReadBinary io::open? -> src_file
	defer { src_file io::close }

	dst io::WriteBinary io::open? -> dst_file
	defer { dst_file io::close }

	BufferSize mem::alloc? -> buf
	defer { buf mem::free }

	0 -> total_copied
	1 -> copying

	loop {
		copying 0 == if { break }
		src_file buf BufferSize io::read? -> bytes_read
		bytes_read 0 == if {
			0 -> copying
		} else {
			dst_file buf bytes_read io::write? drop
			total_copied bytes_read + -> total_copied
		}
	}

	total_copied
}

fn main() {
	"input.txt" "output.txt" copy_file switch {
		Ok { -> total "Copied " print total print " bytes" print nl }
		_ { drop err drop drop "Copy failed" print nl }
	}
}
```

## Key concepts

1. **Fallible functions** - Mark with `!` after the signature; inside one, call other fallible functions with `?` to pass their errors to the caller
2. **defer for cleanup** - Files and buffers are cleaned up on return, on a panic and on `?`; only a `!` abort skips the defers
3. **Error handling** - Use `switch { Ok { } _ { } }` to handle success/failure
4. **High-level vs low-level** - Use `io::read_file`/`io::write_file` for simple cases

## What's next?

Learn about [User Input](user-input.md) to build interactive programs.
