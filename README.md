# Quadrate

## Table of Contents
- [Description](#description)
- [Installation](#installation)
- [Usage](#usage)
- [Maintainers](#maintainers)
- [Contributing](#contributing)

## Description
Quadrate is a stack-based programming language under development.
See https://quad.r8.rs for more information and documentation.

## Installation

```yaml
$ git clone --branch 1.0.0 https://git.sr.ht/~klahr/quadrate
$ cd quadrate
$ make && sudo -E make install
```

### Standrad library
The standard library is included in the repository and is installed by default to `/$HOME/quadrate`. You may overrie by setting QUADRATE_ROOT env.

```bash
$ export QUADRATE_ROOT=~/custom-quadrate
```

## Usage
```
// main.qd
use fmt

fn main() {
	push 1.0
	fmt::print
}
```
```bash
$ quadc -o one main.qd && ./one
```

## Maintainers
[~klahr](https://sr.ht/~klahr)

## Contributing
We enthusiastically encourage and welcome contributions.
