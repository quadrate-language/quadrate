# Example: File Processing

Working with files in Quadrate.

## Reading a File

```qd
use io
use mem

fn read_entire_file(path:str -- content:str)! {
	-> path

	path io::ReadOnly io::open if {
		-> file
		defer { file io::close }

		// Get file size
		file io::size if {
			-> size

			size mem::alloc -> buf
			defer { buf mem::free }

			file buf size io::read if {
				-> bytes_read
				buf bytes_read mem::to_string
			} else {
				""
				1 "read failed" panic
			}
		} else {
			""
			1 "size failed" panic
		}
	} else {
		""
		1 "open failed" panic
	}
}

fn main( -- ) {
	"test.txt" read_entire_file if {
		-> content
		"File contents:" print nl
		content print nl
	} else {
		"Could not read file" print nl
	}
}
```

## Writing a File

```qd
use io
use str

fn write_file(path:str content:str -- )! {
	-> content -> path

	path io::WriteOnly io::create if {
		-> file
		defer { file io::close }

		content str::len -> len
		file content len io::write if {
			drop  // bytes written
		} else {
			1 "write failed" panic
		}
	} else {
		1 "create failed" panic
	}
}

fn main( -- ) {
	"output.txt" "Hello, World!\n" write_file if {
		"File written successfully" print nl
	} else {
		"Failed to write file" print nl
	}
}
```

## Line-by-Line Processing

```qd
use io
use str

fn process_lines(path:str -- ) {
	-> path

	path read_entire_file if {
		-> content
		content "\n" str::split if {
			-> lines
			0 -> line_num

			0 lines len 1 for i {
				line_num 1 + -> line_num
				line_num lines i nth process_line
			}
		}
	} else {
		"Could not read file" print nl
	}
}

fn process_line(num:i64 line:str -- ) {
	-> line -> num
	num print ": " print line print nl
}

fn main( -- ) {
	"data.txt" process_lines
}
```

## Word Count

```qd
use io
use str

fn count_words(path:str -- words:i64 lines:i64 chars:i64)! {
	-> path

	path read_entire_file if {
		-> content

		content str::len -> chars

		// Count lines
		content "\n" str::split if {
			-> line_arr
			line_arr len -> lines

			// Count words
			0 -> words
			0 line_arr len 1 for i {
				line_arr i nth " " str::split if {
					-> word_arr
					words word_arr len + -> words
				}
			}

			words lines chars
		} else {
			0 0 0
			1 "split failed" panic
		}
	} else {
		0 0 0
		1 "read failed" panic
	}
}

fn main( -- ) {
	"document.txt" count_words if {
		-> chars -> lines -> words
		"Words: " print words print nl
		"Lines: " print lines print nl
		"Chars: " print chars print nl
	} else {
		"Could not count" print nl
	}
}
```

## Copy File

```qd
use io
use mem

const BUFFER_SIZE 4096

fn copy_file(src:str dst:str -- )! {
	-> dst -> src

	src io::ReadOnly io::open if {
		-> src_file
		defer { src_file io::close }

		dst io::WriteOnly io::create if {
			-> dst_file
			defer { dst_file io::close }

			BUFFER_SIZE mem::alloc -> buf
			defer { buf mem::free }

			0 -> total_copied
			1 -> continue

			continue while {
				src_file buf BUFFER_SIZE io::read if {
					-> bytes_read
					bytes_read 0 == if {
						0 -> continue
					} else {
						dst_file buf bytes_read io::write if {
							drop
							total_copied bytes_read + -> total_copied
						} else {
							0 -> continue
						}
					}
				} else {
					0 -> continue
				}
			}

			"Copied " print total_copied print " bytes" print nl
		} else {
			1 "create failed" panic
		}
	} else {
		1 "open failed" panic
	}
}

fn main( -- ) {
	"input.txt" "output.txt" copy_file if {
		"Copy successful" print nl
	} else {
		"Copy failed" print nl
	}
}
```

## CSV Processing

```qd
use io
use str

struct Record {
	name:str
	age:i64
	city:str
}

fn parse_csv_line(line:str -- record:ptr)! {
	-> line

	line "," str::split if {
		-> fields
		fields len 3 != if {
			0
			1 "invalid field count" panic
		}

		fields 0 nth -> name
		fields 1 nth str::to_i64 if {
			-> age
			fields 2 nth -> city

			Record { name = name age = age city = city }
		} else {
			0
			1 "invalid age" panic
		}
	} else {
		0
		1 "split failed" panic
	}
}

fn process_csv(path:str -- ) {
	-> path

	path read_entire_file if {
		-> content
		content "\n" str::split if {
			-> lines
			1 -> first  // Skip header

			0 lines len 1 for i {
				first if {
					0 -> first
				} else {
					lines i nth parse_csv_line if {
						-> record
						record @name print
						" is " print
						record @age print
						" years old from " print
						record @city print nl
					}
				}
			}
		}
	}
}

fn main( -- ) {
	"data.csv" process_csv
}
```

## Key Concepts

1. **defer for cleanup** - Files and buffers always cleaned up
2. **Error handling** - Every I/O operation can fail
3. **Buffer management** - Read in chunks for efficiency
4. **String operations** - Split, parse, transform

## What's Next?

See [Data Structures](data-structures.md) for more complex examples.
