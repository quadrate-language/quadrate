# Quadrate

## Table of Contents
- [Description](#description)
- [Installation](#installation)
- [Usage](#usage)
- [License](#license)
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

## License
This repository contains source code generated with assistance from an AI model (Anthropic Claude).
It may include or resemble material that is licensed under the GNU General Public License (GPL), version 3 or later.

Therefore, all files in this repository are distributed **under the GPL v3**.

The distributor makes **no claim of authorship or copyright ownership** of the generated content.
This repository is provided solely for research and demonstration purposes.

See the [`LICENSE`](./LICENSE) file for full terms.

## Maintainers
[~klahr](https://sr.ht/~klahr)

## Contributing
We enthusiastically encourage and welcome contributions.
