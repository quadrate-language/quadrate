# Links API

A bookmark store as a JSON HTTP API.

## Run

```bash
quad run links-api.qd
```

## Usage

```bash
# Service info
curl http://localhost:8080/

# List all links
curl http://localhost:8080/links

# Get a specific link
curl http://localhost:8080/links/0

# Search by title
curl http://localhost:8080/search?q=quadrate
```

## Features

- HTTP server with routing and path parameters (`http` module)
- JSON building with key-value pairs (`json` module)
- String builder for efficient concatenation (`sb` module)
- Query parameter parsing
- Switch-based dispatch
- Error responses with status codes (400, 404)
