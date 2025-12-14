# unicode

## Constants

| Name | Value | Description |
|------|-------|-------------|
| `A` | `65` | Letter: Uppercase A. |
| `B` | `66` | Letter: Uppercase B. |
| `C` | `67` | Letter: Uppercase C. |
| `D` | `68` | Letter: Uppercase D. |
| `E` | `69` | Letter: Uppercase E. |
| `F` | `70` | Letter: Uppercase F. |
| `G` | `71` | Letter: Uppercase G. |
| `H` | `72` | Letter: Uppercase H. |
| `I` | `73` | Letter: Uppercase I. |
| `J` | `74` | Letter: Uppercase J. |
| `K` | `75` | Letter: Uppercase K. |
| `L` | `76` | Letter: Uppercase L. |
| `M` | `77` | Letter: Uppercase M. |
| `N` | `78` | Letter: Uppercase N. |
| `O` | `79` | Letter: Uppercase O. |
| `P` | `80` | Letter: Uppercase P. |
| `Q` | `81` | Letter: Uppercase Q. |
| `R` | `82` | Letter: Uppercase R. |
| `S` | `83` | Letter: Uppercase S. |
| `T` | `84` | Letter: Uppercase T. |
| `U` | `85` | Letter: Uppercase U. |
| `V` | `86` | Letter: Uppercase V. |
| `W` | `87` | Letter: Uppercase W. |
| `X` | `88` | Letter: Uppercase X. |
| `Y` | `89` | Letter: Uppercase Y. |
| `Z` | `90` | Letter: Uppercase Z. |
| `a` | `97` | Letter: Lowercase a. |
| `ampersand` | `38` | Punctuation: Ampersand (&). |
| `asterisk` | `42` | Punctuation: Asterisk (*). |
| `at` | `64` | Punctuation: At sign (@). |
| `b` | `98` | Letter: Lowercase b. |
| `backslash` | `92` | Punctuation: Backslash (\). |
| `backtick` | `96` | Punctuation: Backtick (`). |
| `c` | `99` | Letter: Lowercase c. |
| `caret` | `94` | Punctuation: Caret (^). |
| `colon` | `58` | Punctuation: Colon (:). |
| `comma` | `44` | Punctuation: Comma (,). |
| `cr` | `13` | Control: Carriage return. |
| `d` | `100` | Letter: Lowercase d. |
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
| `equals` | `61` | Punctuation: Equals (=). |
| `esc` | `27` | Control: Escape character. |
| `exclaim` | `33` | Punctuation: Exclamation mark (!). |
| `f` | `102` | Letter: Lowercase f. |
| `g` | `103` | Letter: Lowercase g. |
| `gt` | `62` | Punctuation: Greater than (>). |
| `h` | `104` | Letter: Lowercase h. |
| `hash` | `35` | Punctuation: Hash (#). |
| `i` | `105` | Letter: Lowercase i. |
| `j` | `106` | Letter: Lowercase j. |
| `k` | `107` | Letter: Lowercase k. |
| `l` | `108` | Letter: Lowercase l. |
| `lbrace` | `123` | Punctuation: Left brace ({). |
| `lbracket` | `91` | Punctuation: Left bracket ([). |
| `lparen` | `40` | Punctuation: Left parenthesis. |
| `lt` | `60` | Punctuation: Less than (<). |
| `m` | `109` | Letter: Lowercase m. |
| `minus` | `45` | Punctuation: Minus/hyphen (-). |
| `n` | `110` | Letter: Lowercase n. |
| `newline` | `10` | Control: Newline (line feed). |
| `nul` | `0` | Unicode character constants and classification. Control: Null character. |
| `o` | `111` | Letter: Lowercase o. |
| `p` | `112` | Letter: Lowercase p. |
| `percent` | `37` | Punctuation: Percent (%). |
| `pipe` | `124` | Punctuation: Pipe (|). |
| `plus` | `43` | Punctuation: Plus sign (+). |
| `q` | `113` | Letter: Lowercase q. |
| `question` | `63` | Punctuation: Question mark (?). |
| `r` | `114` | Letter: Lowercase r. |
| `rbrace` | `125` | Punctuation: Right brace (}). |
| `rbracket` | `93` | Punctuation: Right bracket (]). |
| `rparen` | `41` | Punctuation: Right parenthesis. |
| `s` | `115` | Letter: Lowercase s. |
| `semicolon` | `59` | Punctuation: Semicolon (;). |
| `slash` | `47` | Punctuation: Forward slash (/). |
| `space` | `32` | Punctuation: Space. |
| `squote` | `39` | Punctuation: Single quote ('). |
| `t` | `116` | Letter: Lowercase t. |
| `tab` | `9` | Control: Tab character. |
| `tilde` | `126` | Punctuation: Tilde (~). |
| `u` | `117` | Letter: Lowercase u. |
| `underscore` | `95` | Punctuation: Underscore (_). |
| `v` | `118` | Letter: Lowercase v. |
| `w` | `119` | Letter: Lowercase w. |
| `x` | `120` | Letter: Lowercase x. |
| `y` | `121` | Letter: Lowercase y. |
| `z` | `122` | Letter: Lowercase z. |

## Functions

### `fn` is_digit

Check if character is a digit (0-9).

**Signature:** `( c:i64 -- result:i64 )`

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
