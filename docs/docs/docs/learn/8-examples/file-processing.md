# Example: File Processing

Working with files in Quadrate.

## Reading a File

```qd
use io
use mem

fn read_entire_file(path:str -- content:str ok:i64) {
	-> path

	path io::Read io::open if {
		-> file
		defer { file io::close }

		// Get file size by seeking to end
		file 0 io::SeekEnd io::seek if {
			-> size
			file 0 io::SeekSet io::seek if {
				drop

				size mem::alloc -> buf
				defer { buf mem::free }

				file buf size io::read if {
					-> bytes_read
					buf bytes_read mem::to_string 1
				} else {
					"" 0
				}
			} else {
				"" 0
			}
		} else {
			"" 0
		}
	} else {
		"" 0
	}
}

fn main( -- ) {
	"test.txt" read_entire_file if {
		-> content
		"File contents:" print nl
		content print nl
	} else {
		drop
		"Could not read file" print nl
	}
}
```

## Writing a File

```qd
use io
use mem

fn write_file(path:str content:str -- ok:i64) {
	-> content -> path

	path io::Write io::open if {
		-> file

		content mem::from_string -> size -> buf

		file buf size io::write if {
			drop
			buf mem::free
			file io::close
			1
		} else {
			buf mem::free
			file io::close
			0
		}
	} else {
		0
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
use mem
use str

fn read_entire_file(path:str -- content:str)! {
	-> path

	path io::Read io::open! -> file
	defer { file io::close }

	file 0 io::SeekEnd io::seek! -> size
	file 0 io::SeekSet io::seek! drop

	size mem::alloc -> buf
	defer { buf mem::free }

	file buf size io::read! -> bytes_read
	buf bytes_read mem::to_string
}

fn process_lines(path:str -- ) {
	-> path

	path read_entire_file if {
		-> content
		content "\n" str::split if {
			-> count -> lines

			0 count 1 for i {
				i 1 + lines i 8 * mem::get_ptr casts process_line
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
use mem
use str

fn read_entire_file(path:str -- content:str ok:i64) {
	-> path

	path io::Read io::open if {
		-> file
		defer { file io::close }

		file 0 io::SeekEnd io::seek if {
			-> size
			file 0 io::SeekSet io::seek if {
				drop

				size mem::alloc -> buf
				defer { buf mem::free }

				file buf size io::read if {
					-> bytes_read
					buf bytes_read mem::to_string 1
				} else {
					"" 0
				}
			} else {
				"" 0
			}
		} else {
			"" 0
		}
	} else {
		"" 0
	}
}

fn count_words(path:str -- words:i64 lines:i64 chars:i64 ok:i64) {
	-> path

	path read_entire_file if {
		-> content

		content str::len -> chars

		// Count lines
		content "\n" str::split if {
			-> line_count -> line_parts

			line_count -> lines

			// Count words
			0 -> words
			0 line_count 1 for i {
				line_parts i 8 * mem::get_i64 casts " " str::split if {
					-> word_count drop
					words word_count + -> words
				}
			}

			words lines chars 1
		} else {
			0 0 0 0
		}
	} else {
		drop
		0 0 0 0
	}
}

fn main( -- ) {
	"document.txt" count_words if {
		-> chars -> lines -> words
		"Words: " print words print nl
		"Lines: " print lines print nl
		"Chars: " print chars print nl
	} else {
		drop drop drop
		"Could not count" print nl
	}
}
```

## Copy File

```qd
use io
use mem

const BUFFER_SIZE = 4096

fn copy_file(src:str dst:str -- ok:i64) {
	-> dst -> src

	src io::ReadBinary io::open if {
		-> src_file
		defer { src_file io::close }

		dst io::WriteBinary io::open if {
			-> dst_file
			defer { dst_file io::close }

			BUFFER_SIZE mem::alloc -> buf
			defer { buf mem::free }

			0 -> total_copied
			1 -> copying

			copying while {
				src_file buf BUFFER_SIZE io::read if {
					-> bytes_read
					bytes_read 0 == if {
						0 -> copying
					} else {
						dst_file buf bytes_read io::write if {
							drop
							total_copied bytes_read + -> total_copied
						} else {
							0 -> copying
						}
					}
				} else {
					0 -> copying
				}
			}

			"Copied " print total_copied print " bytes" print nl
			1
		} else {
			0
		}
	} else {
		0
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
use mem
use str
use strconv

fn read_entire_file(path:str -- content:str ok:i64) {
	-> path

	path io::Read io::open if {
		-> file
		defer { file io::close }

		file 0 io::SeekEnd io::seek if {
			-> size
			file 0 io::SeekSet io::seek if {
				drop

				size mem::alloc -> buf
				defer { buf mem::free }

				file buf size io::read if {
					-> bytes_read
					buf bytes_read mem::to_string 1
				} else {
					"" 0
				}
			} else {
				"" 0
			}
		} else {
			"" 0
		}
	} else {
		"" 0
	}
}

struct Record {
	name:str
	age:i64
	city:str
}

fn get_str(arr:ptr idx:i64 -- s:str) {
	-> idx -> arr
	arr idx 8 * mem::get_i64 casts
}

fn parse_csv_line(line:str -- record:ptr ok:i64) {
	-> line

	line "," str::split if {
		-> count -> fields
		count 3 != if {
			0 0
		} else {
			fields 0 get_str -> name
			fields 1 get_str strconv::atoi -> age
			fields 2 get_str -> city

			Record { name = name age = age city = city } 1
		}
	} else {
		0 0
	}
}

fn process_csv(path:str -- ) {
	-> path

	path read_entire_file if {
		-> content
		content "\n" str::split if {
			-> line_count -> lines
			1 -> first  // Skip header

			0 line_count 1 for i {
				first if {
					0 -> first
				} else {
					lines i get_str parse_csv_line if {
						-> record
						record @name print
						" is " print
						record @age print
						" years old from " print
						record @city print nl
					} else {
						drop
					}
				}
			}
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
