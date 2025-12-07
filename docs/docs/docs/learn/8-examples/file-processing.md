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
				drop ""
				"read failed" 1 error
			}
		} else {
			drop ""
			"size failed" 1 error
		}
	} else {
		drop ""
		"open failed" 1 error
	}
}

fn main( -- ) {
	"test.txt" read_entire_file if {
		-> content
		"File contents:" io::println
		content io::println
	} else {
		drop
		"Could not read file" io::println
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
			drop
			"write failed" 1 error
		}
	} else {
		drop
		"create failed" 1 error
	}
}

fn main( -- ) {
	"output.txt" "Hello, World!\n" write_file if {
		"File written successfully" io::println
	} else {
		drop
		"Failed to write file" io::println
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

			lines iter for line {
				line_num 1 + -> line_num
				line_num line process_line
			}
		} else {
			drop
		}
	} else {
		drop
		"Could not read file" io::println
	}
}

fn process_line(num:i64 line:str -- ) {
	-> line -> num
	num print ": " print line io::println
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
			line_arr @len -> lines

			// Count words
			0 -> words
			line_arr iter for line {
				line " " str::split if {
					-> word_arr
					words word_arr @len + -> words
				} else {
					drop
				}
			}

			words lines chars
		} else {
			drop
			0 0 0
			"split failed" 1 error
		}
	} else {
		drop
		0 0 0
		"read failed" 1 error
	}
}

fn main( -- ) {
	"document.txt" count_words if {
		-> chars -> lines -> words
		"Words: " io::print words io::println
		"Lines: " io::print lines io::println
		"Chars: " io::print chars io::println
	} else {
		drop
		"Could not count" io::println
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
							drop 0 -> continue
						}
					}
				} else {
					drop
					0 -> continue
				}
			}

			"Copied " io::print total_copied io::print " bytes" io::println
		} else {
			drop
			"create failed" 1 error
		}
	} else {
		drop
		"open failed" 1 error
	}
}

fn main( -- ) {
	"input.txt" "output.txt" copy_file if {
		"Copy successful" io::println
	} else {
		drop
		"Copy failed" io::println
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
		fields @len 3 != if {
			0
			"invalid field count" 1 error
		}

		fields 0 @[] -> name
		fields 1 @[] str::to_i64 if {
			-> age
			fields 2 @[] -> city

			Record { name = name age = age city = city }
		} else {
			drop 0
			"invalid age" 1 error
		}
	} else {
		drop 0
		"split failed" 1 error
	}
}

fn process_csv(path:str -- ) {
	-> path

	path read_entire_file if {
		-> content
		content "\n" str::split if {
			-> lines
			1 -> first  // Skip header

			lines iter for line {
				first if {
					0 -> first
				} else {
					line parse_csv_line if {
						-> record
						record @name io::print
						" is " io::print
						record @age io::print
						" years old from " io::print
						record @city io::println
					} else {
						drop
					}
				}
			}
		} else {
			drop
		}
	} else {
		drop
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
