# Decimal

Exact decimal arithmetic with `d128`: money that adds up.

## Run

```bash
quad run decimal.qd
```

## Features

- `d128` literals (`0.1d`) and parameters
- An `i64` widening to `d128` (`1 rate +`, `1000` as the principal)
- `decimal::pow` and `decimal::quantize`, rounding to cents with ties to even
