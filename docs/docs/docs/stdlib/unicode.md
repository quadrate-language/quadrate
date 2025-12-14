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
| `dollar` | `36` | Punctuation: Dollar sign (it). |
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
48 unicode::is_digit .  // 1
```

