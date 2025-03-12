# Quadrate

## Table of Contents
- [Description](#description)
- [Installation](#installation)
- [Usage](#usage)
- [Maintainers](#maintainers)
- [Contributing](#contributing)

## Description
Quadrate is a stack-based programming language under development.
See https://quad.r8.rs for more information.

## Installation

```yaml
$ git clone https://git.sr.ht:~/klahr/quadrate
$ cd quadrate
$ go install
```

## Usage
```
// main.qd
use std

fn main() {
	push 1.0
	std::print
}
```
```bash
$ quadc -o one main.qd && ./one
```

## Maintainers
[~klahr](https://sr.ht/~klahr)

## Contributing
We enthusiastically encourage and welcome contributions.
