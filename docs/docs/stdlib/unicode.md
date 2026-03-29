# `use` unicode

## Constants

| Name | Value | Description |
|------|-------|-------------|
| `A` | `65` | Letter: Uppercase A. |
| `a` | `97` | Letter: Lowercase a. |
| `ampersand` | `38` | Punctuation: Ampersand (&). |
| `asterisk` | `42` | Punctuation: Asterisk (*). |
| `at` | `64` | Punctuation: At sign (@). |
| `B` | `66` | Letter: Uppercase B. |
| `b` | `98` | Letter: Lowercase b. |
| `backslash` | `92` | Punctuation: Backslash (\). |
| `backtick` | `96` | Punctuation: Backtick (`). |
| `C` | `67` | Letter: Uppercase C. |
| `c` | `99` | Letter: Lowercase c. |
| `caret` | `94` | Punctuation: Caret (^). |
| `colon` | `58` | Punctuation: Colon (:). |
| `comma` | `44` | Punctuation: Comma (,). |
| `cr` | `13` | Control: Carriage return. |
| `d` | `100` | Letter: Lowercase d. |
| `D` | `68` | Letter: Uppercase D. |
| `digit0` | `48` | Digit = 0. |
| `digit1` | `49` | Digit = 1. |
| `digit2` | `50` | Digit = 2. |
| `digit3` | `51` | Digit = 3. |
| `digit4` | `52` | Digit = 4. |
| `digit5` | `53` | Digit = 5. |
| `digit6` | `54` | Digit = 6. |
| `digit7` | `55` | Digit = 7. |
| `digit8` | `56` | Digit = 8. |
| `digit9` | `57` | Digit = 9. |
| `dollar` | `36` | Punctuation: Dollar sign ($). |
| `dot` | `46` | Punctuation: Period/dot (.). |
| `dquote` | `34` | Punctuation: Double quote ("). |
| `e` | `101` | Letter: Lowercase e. |
| `E` | `69` | Letter: Uppercase E. |
| `equals` | `61` | Punctuation: Equals (=). |
| `esc` | `27` | Control: Escape character. |
| `exclaim` | `33` | Punctuation: Exclamation mark (!). |
| `f` | `102` | Letter: Lowercase f. |
| `F` | `70` | Letter: Uppercase F. |
| `g` | `103` | Letter: Lowercase g. |
| `G` | `71` | Letter: Uppercase G. |
| `gt` | `62` | Punctuation: Greater than (>). |
| `h` | `104` | Letter: Lowercase h. |
| `H` | `72` | Letter: Uppercase H. |
| `hash` | `35` | Punctuation: Hash (#). |
| `i` | `105` | Letter: Lowercase i. |
| `I` | `73` | Letter: Uppercase I. |
| `j` | `106` | Letter: Lowercase j. |
| `J` | `74` | Letter: Uppercase J. |
| `k` | `107` | Letter: Lowercase k. |
| `K` | `75` | Letter: Uppercase K. |
| `l` | `108` | Letter: Lowercase l. |
| `L` | `76` | Letter: Uppercase L. |
| `lbrace` | `123` | Punctuation: Left brace ({). |
| `lbracket` | `91` | Punctuation: Left bracket ([). |
| `lparen` | `40` | Punctuation: Left parenthesis. |
| `lt` | `60` | Punctuation: Less than (<). |
| `m` | `109` | Letter: Lowercase m. |
| `M` | `77` | Letter: Uppercase M. |
| `minus` | `45` | Punctuation: Minus/hyphen (-). |
| `n` | `110` | Letter: Lowercase n. |
| `N` | `78` | Letter: Uppercase N. |
| `newline` | `10` | Control: Newline (line feed). |
| `nul` | `0` | Unicode character constants and classification. Control: Null character. |
| `o` | `111` | Letter: Lowercase o. |
| `O` | `79` | Letter: Uppercase O. |
| `p` | `112` | Letter: Lowercase p. |
| `P` | `80` | Letter: Uppercase P. |
| `percent` | `37` | Punctuation: Percent (%). |
| `pipe` | `124` | Punctuation: Pipe (|). |
| `plus` | `43` | Punctuation: Plus sign (+). |
| `q` | `113` | Letter: Lowercase q. |
| `Q` | `81` | Letter: Uppercase Q. |
| `question` | `63` | Punctuation: Question mark (?). |
| `r` | `114` | Letter: Lowercase r. |
| `R` | `82` | Letter: Uppercase R. |
| `rbrace` | `125` | Punctuation: Right brace (}). |
| `rbracket` | `93` | Punctuation: Right bracket (]). |
| `rparen` | `41` | Punctuation: Right parenthesis. |
| `s` | `115` | Letter: Lowercase s. |
| `S` | `83` | Letter: Uppercase S. |
| `semicolon` | `59` | Punctuation: Semicolon (;). |
| `slash` | `47` | Punctuation: Forward slash (/). |
| `space` | `32` | Punctuation: Space. |
| `squote` | `39` | Punctuation: Single quote ('). |
| `t` | `116` | Letter: Lowercase t. |
| `T` | `84` | Letter: Uppercase T. |
| `tab` | `9` | Control: Tab character. |
| `tilde` | `126` | Punctuation: Tilde (~). |
| `u` | `117` | Letter: Lowercase u. |
| `U` | `85` | Letter: Uppercase U. |
| `underscore` | `95` | Punctuation: Underscore (_). |
| `v` | `118` | Letter: Lowercase v. |
| `V` | `86` | Letter: Uppercase V. |
| `w` | `119` | Letter: Lowercase w. |
| `W` | `87` | Letter: Uppercase W. |
| `x` | `120` | Letter: Lowercase x. |
| `X` | `88` | Letter: Uppercase X. |
| `y` | `121` | Letter: Lowercase y. |
| `Y` | `89` | Letter: Uppercase Y. |
| `z` | `122` | Letter: Lowercase z. |
| `Z` | `90` | Letter: Uppercase Z. |

## Functions

### `fn` digit_value

Convert digit character to its numeric value.

**Signature:** `(c:i64 -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `c` | `i64` | Character code (must be a digit) |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | Numeric value (0-9), or -1 if not a digit |

**Example:**

```qd
53 unicode::digit_value print  // 5
```
---

### `fn` hex_digit_value

Convert hex digit character to its numeric value.

**Signature:** `(c:i64 -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `c` | `i64` | Character code (must be a hex digit) |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | Numeric value (0-15), or -1 if not a hex digit |

**Example:**

```qd
65 unicode::hex_digit_value print  // 10 (A)
```
---

### `fn` is_alnum

Check if character is alphanumeric (A-Z, a-z, or 0-9).

**Signature:** `(c:i64 -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `c` | `i64` | Character code |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | 1 if alphanumeric, 0 otherwise |

**Example:**

```qd
65 unicode::is_alnum print  // 1
```
---

### `fn` is_alpha

Check if character is an alphabetic letter (A-Z or a-z).

**Signature:** `(c:i64 -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `c` | `i64` | Character code |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | 1 if alphabetic, 0 otherwise |

**Example:**

```qd
65 unicode::is_alpha print  // 1
```
---

### `fn` is_ascii

Check if character is ASCII (0-127).

**Signature:** `(c:i64 -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `c` | `i64` | Character code |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | 1 if ASCII, 0 otherwise |

**Example:**

```qd
65 unicode::is_ascii print  // 1
```
---

### `fn` is_control

Check if character is a control character (ASCII 0-31 and 127).

**Signature:** `(c:i64 -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `c` | `i64` | Character code |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | 1 if control character, 0 otherwise |

**Example:**

```qd
10 unicode::is_control print  // 1 (newline)
```
---

### `fn` is_digit

Check if character is a digit (0-9).

**Signature:** `(c:i64 -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `c` | `i64` | Character code |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | 1 if digit, 0 otherwise |

**Example:**

```qd
48 unicode::is_digit print  // 1
```
---

### `fn` is_hex_digit

Check if character is a hexadecimal digit (0-9, A-F, a-f).

**Signature:** `(c:i64 -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `c` | `i64` | Character code |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | 1 if hex digit, 0 otherwise |

**Example:**

```qd
65 unicode::is_hex_digit print  // 1 (A)
```
---

### `fn` is_ident_cont

Check if character is a valid identifier continuation (letter, digit, or underscore).

**Signature:** `(c:i64 -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `c` | `i64` | Character code |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | 1 if valid identifier continuation, 0 otherwise |

**Example:**

```qd
95 unicode::is_ident_cont print  // 1 (_)
```
---

### `fn` is_ident_start

Check if character is a valid identifier start (letter or underscore).

**Signature:** `(c:i64 -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `c` | `i64` | Character code |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | 1 if valid identifier start, 0 otherwise |

**Example:**

```qd
95 unicode::is_ident_start print  // 1 (_)
```
---

### `fn` is_lower

Check if character is a lowercase letter (a-z).

**Signature:** `(c:i64 -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `c` | `i64` | Character code |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | 1 if lowercase, 0 otherwise |

**Example:**

```qd
97 unicode::is_lower print  // 1
```
---

### `fn` is_print

Check if character is printable (space through tilde, ASCII 32-126).

**Signature:** `(c:i64 -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `c` | `i64` | Character code |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | 1 if printable, 0 otherwise |

**Example:**

```qd
65 unicode::is_print print  // 1
```
---

### `fn` is_punct

Check if character is punctuation (printable but not alphanumeric or space).

**Signature:** `(c:i64 -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `c` | `i64` | Character code |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | 1 if punctuation, 0 otherwise |

**Example:**

```qd
33 unicode::is_punct print  // 1 (!)
```
---

### `fn` is_space

Check if character is whitespace (space, tab, newline, carriage return).

**Signature:** `(c:i64 -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `c` | `i64` | Character code |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | 1 if whitespace, 0 otherwise |

**Example:**

```qd
32 unicode::is_space print  // 1
```
---

### `fn` is_upper

Check if character is an uppercase letter (A-Z).

**Signature:** `(c:i64 -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `c` | `i64` | Character code |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | 1 if uppercase, 0 otherwise |

**Example:**

```qd
65 unicode::is_upper print  // 1
```
---

### `fn` is_utf8_cont

Check if byte is a UTF-8 continuation byte (10xxxxxx).

**Signature:** `(b:i64 -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `b` | `i64` | Byte value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | 1 if continuation byte, 0 otherwise |

**Example:**

```qd
0x80 unicode::is_utf8_cont print  // 1
```
---

### `fn` is_utf8_start

Check if byte is the start of a UTF-8 sequence.

**Signature:** `(b:i64 -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `b` | `i64` | Byte value |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | 1 if start byte, 0 otherwise |

**Example:**

```qd
0xC2 unicode::is_utf8_start print  // 1
```
---

### `fn` is_valid_codepoint

Check if a codepoint is valid Unicode.

**Signature:** `(cp:i64 -- valid:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `cp` | `i64` | Codepoint to check |

| Output | Type | Description |
|--------|------|-------------|
| `valid` | `i64` | 1 if valid, 0 otherwise |

**Example:**

```qd
0x1F600 unicode::is_valid_codepoint print  // 1 (😀)
```
---

### `fn` to_lower

Convert uppercase letter to lowercase.

**Signature:** `(c:i64 -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `c` | `i64` | Character code |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | Lowercase character code (unchanged if not uppercase) |

**Example:**

```qd
65 unicode::to_lower print  // 97
```
---

### `fn` to_upper

Convert lowercase letter to uppercase.

**Signature:** `(c:i64 -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `c` | `i64` | Character code |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | Uppercase character code (unchanged if not lowercase) |

**Example:**

```qd
97 unicode::to_upper print  // 65
```
---

### `fn` utf8_encode_len

Get the number of UTF-8 bytes needed to encode a codepoint.

**Signature:** `(cp:i64 -- len:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `cp` | `i64` | Unicode codepoint |

| Output | Type | Description |
|--------|------|-------------|
| `len` | `i64` | Number of bytes needed (1-4), or 0 if invalid |

**Example:**

```qd
0x00E9 unicode::utf8_encode_len print  // 2 (é)
```
---

### `fn` utf8_seq_len

Get the number of bytes in a UTF-8 sequence based on the first byte. Returns 1 for ASCII, 2-4 for multi-byte sequences, 0 for invalid.

**Signature:** `(b:i64 -- len:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `b` | `i64` | First byte of UTF-8 sequence |

| Output | Type | Description |
|--------|------|-------------|
| `len` | `i64` | Number of bytes (1-4), or 0 if invalid |

**Example:**

```qd
0xC2 unicode::utf8_seq_len print  // 2
```
