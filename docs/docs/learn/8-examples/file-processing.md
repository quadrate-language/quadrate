# Example: file processing

Working with files in Quadrate.

## Reading a file

The simplest way to read a file is with `io::read_file`:

```qd
use io

fn main() {
	"/etc/hostname" io::read_file switch {
		Ok { -> content "Hostname: " print content print }
		_ { err drop drop "Could not read file" print nl }
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

For more control, use `io::open!`, `io::read!`, etc:

```qd
use io
use mem

fn read_file(path:str -- content:str)! {
	-> path
	path io::Read io::open! -> file
	defer { file io::close }

	file 0 io::SeekEnd io::seek! -> size
	file 0 io::SeekSet io::seek! drop

	size mem::alloc! -> buf
	defer { buf mem::free }

	file buf size io::read! -> bytes_read
	buf bytes_read mem::to_string
}

fn main() {
	"test.txt" read_file switch {
		Ok { -> content "File contents:" print nl content print nl }
		_ { err drop drop "Could not read file" print nl }
	}
}
```

## Writing with low-level API

```qd
use io
use mem

fn write_file(path:str content:str -- )! {
	-> content -> path
	path io::Write io::open! -> file
	defer { file io::close }

	content mem::from_string -> size -> buf
	defer { buf mem::free }

	file buf size io::write! drop
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
use mem
use strings

fn process_line(num:i64 line:str -- ) {
	-> line -> num
	num print ": " print line print nl
}

fn main() {
	"data.txt" io::read_file switch {
		Ok {
			-> content
			content strings::lines! -> count -> lines
			0 count 1 for i {
				i 1 + lines i 8 * mem::get_ptr cast<str> process_line
			}
			lines mem::free
		}
		_ { err drop drop "Could not read file" print nl }
	}
}
```

## Word count

```qd
use io
use mem
use strings

fn count_words(path:str -- words:i64 lines:i64 chars:i64)! {
	-> path
	path io::read_file! -> content

	content strings::len -> chars
	content strings::lines! -> line_count -> line_array
	line_count -> lines

	0 -> words
	0 line_count 1 for i {
		line_array i 8 * mem::get_ptr cast<str> -> line
		line strings::len 0 > if {
			line strings::words! -> word_count -> word_arr
			words word_count + -> words
			word_arr mem::free
		}
	}
	line_array mem::free

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
		_ { err drop drop "Could not count" print nl }
	}
}
```

## Copy file

```qd
use io
use mem

const BufferSize = 4096

fn copy_file(src:str dst:str -- total:i64)! {
	-> dst -> src
	src io::ReadBinary io::open! -> src_file
	defer { src_file io::close }

	dst io::WriteBinary io::open! -> dst_file
	defer { dst_file io::close }

	BufferSize mem::alloc! -> buf
	defer { buf mem::free }

	0 -> total_copied
	1 -> copying

	loop {
		copying 0 == if { break }
		src_file buf BufferSize io::read! -> bytes_read
		bytes_read 0 == if {
			0 -> copying
		} else {
			dst_file buf bytes_read io::write! drop
			total_copied bytes_read + -> total_copied
		}
	}

	total_copied
}

fn main() {
	"input.txt" "output.txt" copy_file switch {
		Ok { -> total "Copied " print total print " bytes" print nl }
		_ { err drop drop "Copy failed" print nl }
	}
}
```

## Key concepts

1. **Fallible functions** - Mark with `!` suffix, use `switch { Ok { } _ { err panic } }` to propagate errors
2. **defer for cleanup** - Files and buffers always cleaned up, even on error
3. **Error handling** - Use `switch { Ok { } _ { } }` to handle success/failure
4. **Error propagation** - Use `err panic` to re-throw errors to caller
5. **High-level vs low-level** - Use `io::read_file`/`io::write_file` for simple cases

## What's next?

Learn about [User Input](user-input.md) to build interactive programs.
