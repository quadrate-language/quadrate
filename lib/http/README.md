# http

HTTP client and server module for the Quadrate programming language.

## Installation

```bash
quadpm get https://git.sr.ht/~klahr/qdhttp
```

## Dependencies

This module depends on:
- [tls](https://git.sr.ht/~klahr/qdtls) - TLS/SSL encryption
- OpenSSL (libssl, libcrypto) - System libraries

## Usage

### HTTP Client

```qd
use http

fn main() {
    // Simple GET request
    "https://example.com" http::get! -> resp
    resp <<status print nl
    resp <<body print nl
    resp http::close
}
```

### HTTP Client with Custom Headers

```qd
use http

fn main() {
    "https://api.example.com/data" http::new -> req
    req "POST" http::method
    req "Content-Type" "application/json" http::header
    req "Authorization" "Bearer token123" http::header
    req "{\"key\":\"value\"}" http::body

    req http::send! -> resp
    resp <<status print nl
    resp <<body print nl

    req http::free_request
    resp http::close
}
```

### HTTP Server (Gin-inspired API)

```qd
use http

fn main() {
    http::engine -> e

    // Register routes
    e "/" fn(c:http::Ctx -- ) {
        c 200 "Hello, World!" http::string
    } http::GET

    e "/users/:id" fn(c:http::Ctx -- ) {
        c "id" http::param -> id
        c 200 id http::string
    } http::GET

    e "/api/data" fn(c:http::Ctx -- ) {
        c 200 "{\"ok\":true}" http::json
    } http::POST

    // Start server
    e ":8080" http::run!

    e http::free_engine
}
```

### Route Groups and Middleware

```qd
use http

fn main() {
    http::engine -> e

    // Global middleware
    e fn(c:http::Ctx -- ) {
        c "X-Server" "Quadrate" http::set_header
    } http::use

    // API group
    e "/api" http::group -> api

    api "/users" fn(c:http::Ctx -- ) {
        c 200 "[]" http::json
    } http::group_GET

    e ":8080" http::run!
    e http::free_engine
}
```

### Static File Serving

```qd
use http

fn main() {
    http::engine -> e

    // Serve files from ./public at /static/*
    e "/static/*" fn(c:http::Ctx -- ) {
        c "/static" "./public" http::static
    } http::GET

    e ":8080" http::run!
    e http::free_engine
}
```

## API Reference

### Client Functions

- `http::new(url:str -- req:ptr)` - Create new request builder
- `http::method(req:ptr method:str -- )` - Set HTTP method
- `http::header(req:ptr name:str value:str -- )` - Add header
- `http::body(req:ptr body:str -- )` - Set request body
- `http::cert(req:ptr cert:str key:str -- )` - Set client certificate for mTLS
- `http::send(req:ptr -- resp:Response)!` - Execute request
- `http::get(url:str -- resp:Response)!` - Simple GET request
- `http::free_request(req:ptr -- )` - Free request object
- `http::close(resp:Response -- )` - Close response

### Server Functions

- `http::engine( -- engine:ptr)` - Create HTTP engine
- `http::GET/POST/PUT/DELETE/ANY(engine:ptr path:str handler:ptr -- )` - Register routes
- `http::use(engine:ptr middleware:ptr -- )` - Add middleware
- `http::run(engine:ptr addr:str -- )!` - Start server (blocking)
- `http::handle_one(engine:ptr -- handled:i64)` - Handle single request (non-blocking)
- `http::stop(engine:ptr -- )` - Stop server
- `http::free_engine(engine:ptr -- )` - Free engine

### Context Methods

- `http::param(c:Ctx name:str -- value:str)` - Get path parameter
- `http::query_param(c:Ctx name:str -- value:str)` - Get query parameter
- `http::get_header(c:Ctx name:str -- value:str)` - Get request header
- `http::string(c:Ctx status:i64 body:str -- )` - Send text response
- `http::json(c:Ctx status:i64 body:str -- )` - Send JSON response
- `http::html(c:Ctx status:i64 body:str -- )` - Send HTML response
- `http::set_header(c:Ctx name:str value:str -- )` - Set response header
- `http::abort(c:Ctx status:i64 body:str -- )` - Abort with error

### Route Groups

- `http::group(engine:ptr prefix:str -- group:ptr)` - Create route group
- `http::group_use(group:ptr middleware:ptr -- )` - Add middleware to group
- `http::group_GET/POST/PUT/DELETE(group:ptr path:str handler:ptr -- )` - Register group routes

### Static Files

- `http::static_file(c:Ctx filepath:str -- )` - Serve single file
- `http::static(c:Ctx prefix:str fs_path:str -- )` - Serve directory

## Error Codes

- `http::ErrConnect (2)` - Connection failed
- `http::ErrTls (3)` - TLS handshake failed
- `http::ErrTimeout (4)` - Request timeout
- `http::ErrParse (5)` - Response parse error
- `http::ErrInvalidUrl (6)` - Invalid URL format
- `http::ErrRedirect (7)` - Too many redirects
- `http::ErrMemory (8)` - Memory allocation failed
- `http::ErrSend (9)` - Send failed
- `http::ErrReceive (10)` - Receive failed
- `http::ErrBind (20)` - Server bind failed
- `http::ErrAccept (21)` - Accept failed

## HTTP Status Constants

All standard HTTP status codes are available as constants:
- `http::StatusOK (200)`
- `http::StatusCreated (201)`
- `http::StatusNotFound (404)`
- `http::StatusInternalServerError (500)`
- ... and many more

## License

Apache 2.0

## Contributing

Contributions welcome! Please open an issue or submit a patch on [SourceHut](https://git.sr.ht/~klahr/qdhttp).
