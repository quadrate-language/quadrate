# fuzzy

Fuzzy string matching library for Quadrate.

## Installation

```bash
quadpm get https://git.sr.ht/~klahr/qdfuzzy
```

## Usage

```qd
use fuzzy

// Levenshtein edit distance
"kitten" "sitting" fuzzy::distance  // 3

// Similarity score (0.0 to 1.0)
"hello" "hallo" fuzzy::similarity  // 0.8

// Check if strings match within threshold
"helo" "hello" 0.7 fuzzy::matches  // 1

// Fuzzy match score (higher = better match)
"sin" "math::sin" fuzzy::score  // 300+ (substring match)
"rf" "read_file" fuzzy::score   // 200 (word boundary match)

// Find best matches from a list
"app" items 5 fuzzy::best -> count -> results
```

## Functions

### `distance(a:str b:str -- dist:i64)`
Compute Levenshtein edit distance between two strings.

### `similarity(a:str b:str -- sim:f64)`
Compute similarity as normalized Levenshtein distance (0.0 to 1.0).

### `matches(query:str target:str threshold:f64 -- matches:i64)`
Check if similarity meets threshold.

### `score(query:str target:str -- score:i64)`
Compute fuzzy match score using multiple strategies:
- Exact match: 1000
- Prefix match: 500+
- Substring match: 300+
- Word boundary match: 200
- Similarity match: 0-100

### `best(query:str items:ptr max_results:i64 -- results:ptr count:i64)`
Find best matching items from an array, sorted by score descending.

### `starts_with_ci(s:str prefix:str -- starts:i64)`
Case-insensitive prefix check.

### `contains_ci(s:str substr:str -- contains:i64)`
Case-insensitive substring check.

## License

Apache 2.0

## Contributing

Contributions welcome! Please open an issue or submit a patch on [SourceHut](https://git.sr.ht/~klahr/qdfuzzy).
