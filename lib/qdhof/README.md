# qdhof - Higher-Order Function Combinators

Pure Quadrate library providing higher-order function combinators for functional programming.

## Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `apply` | `( x f -- r )` | Apply function f to x |
| `bi` | `( x f g -- a b )` | Apply f and g to same x |
| `tri` | `( x f g h -- a b c )` | Apply f, g, h to same x |
| `keep` | `( x f -- r x )` | Apply f, keep original x |
| `dip` | `( x y f -- r y )` | Apply f to x, preserve y |
| `both` | `( x y f -- a b )` | Apply f to both x and y |
| `bi_star` | `( x y f g -- a b )` | Apply f to x, g to y |
| `when` | `( x cond f -- r )` | Apply f if cond is true |
| `unless` | `( x cond f -- r )` | Apply f if cond is false |
| `times` | `( x n f -- r )` | Apply f n times |

## Example

```quadrate
use hof

fn main() {
    // Apply two functions to same value
    5 fn (x:i64 -- r:i64) { 2 * } fn (x:i64 -- r:i64) { 3 + } hof::bi
    // Stack: 10 8

    // Apply but keep original
    5 fn (x:i64 -- r:i64) { 2 * } hof::keep
    // Stack: 10 5
}
```
