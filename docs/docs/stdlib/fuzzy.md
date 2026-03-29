# `use` fuzzy

## Functions

### `fn` best

Find the best matching items from a list, sorted by score descending. Items with score 0 are excluded.

**Signature:** `(query:str items:ptr max_results:i64 -- results:ptr count:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `query` | `str` | Query string to match |
| `items` | `ptr` | Array of strings to search |
| `max_results` | `i64` | Maximum number of results to return |

| Output | Type | Description |
|--------|------|-------------|
| `results` | `ptr` | Array of matching strings (caller must free) |
| `count` | `i64` | Number of results |
---

### `fn` contains_ci

Check if a string contains a substring (case-insensitive).

**Signature:** `(str str -- i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | String to search in |
| `substr` | `str` | Substring to look for |

| Output | Type | Description |
|--------|------|-------------|
| `contains` | `i64` | 1 if s contains substr, 0 otherwise |
---

### `fn` distance

Compute Levenshtein edit distance between two strings. The edit distance is the minimum number of single-character edits (insertions, deletions, substitutions) required to transform one string into another.

**Signature:** `(a:str b:str -- dist:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `a` | `str` | First string |
| `b` | `str` | Second string |

| Output | Type | Description |
|--------|------|-------------|
| `dist` | `i64` | Edit distance (0 = identical) |

**Example:**

```qd
"kitten" "sitting" fuzzy::distance  // 3
"hello" "hello" fuzzy::distance  // 0
"" "abc" fuzzy::distance  // 3
```
---

### `fn` matches

Check if query approximately matches target within a similarity threshold.

**Signature:** `(str str f64 -- i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `query` | `str` | Query string to search for |
| `target` | `str` | Target string to match against |
| `threshold` | `f64` | Minimum similarity (0.0 to 1.0) |

| Output | Type | Description |
|--------|------|-------------|
| `matches` | `i64` | 1 if similarity >= threshold, 0 otherwise |

**Example:**

```qd
"helo" "hello" 0.7 fuzzy::matches  // 1
"xyz" "hello" 0.7 fuzzy::matches  // 0
```
---

### `fn` score

Compute a fuzzy match score combining multiple matching strategies. Higher scores indicate better matches. Returns 0 if no match. Scoring:   - Exact match: 1000   - Prefix match: 500 + (query_len / target_len * 100)   - Substring match: 300 + (query_len / target_len * 100)   - Word boundary match (after _ or at start): 200   - Similarity match (>= 0.6): similarity * 100

**Signature:** `(query:str target:str -- score:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `query` | `str` | Query string (typically shorter) |
| `target` | `str` | Target string to match against |

| Output | Type | Description |
|--------|------|-------------|
| `score` | `i64` | Match score (0 = no match, higher = better) |

**Example:**

```qd
"sin" "sin" fuzzy::score  // 1000
"sin" "asin" fuzzy::score  // ~350 (substring)
"rf" "read_file" fuzzy::score  // 200 (word boundary)
```
---

### `fn` similarity

Compute similarity between two strings as a value from 0.0 to 1.0. 1.0 means identical, 0.0 means completely different. Based on normalized Levenshtein distance: 1 - (distance / max_length).

**Signature:** `(a:str b:str -- sim:f64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `a` | `str` | First string |
| `b` | `str` | Second string |

| Output | Type | Description |
|--------|------|-------------|
| `sim` | `f64` | Similarity score (0.0 to 1.0) |

**Example:**

```qd
"hello" "hello" fuzzy::similarity  // 1.0
"hello" "hallo" fuzzy::similarity  // 0.8
"abc" "xyz" fuzzy::similarity  // 0.0
```
---

### `fn` starts_with_ci

Check if a string starts with a given prefix (case-insensitive).

**Signature:** `(str str -- i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | String to check |
| `prefix` | `str` | Prefix to look for |

| Output | Type | Description |
|--------|------|-------------|
| `starts` | `i64` | 1 if s starts with prefix, 0 otherwise |
