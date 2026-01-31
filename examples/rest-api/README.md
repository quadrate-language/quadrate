# REST API

HTTP REST API server with route groups and middleware.

**Note:** Requires the external `http` module.

## Run

```bash
quad run rest-api.qd
```

## Endpoints

- `GET /` - Welcome message
- `GET /search?q=...` - Search
- `GET /api/users` - List users
- `GET /api/users/:id` - Get user by ID
- `POST /api/users` - Create user
- `DELETE /api/users/:id` - Delete user
- `GET /api/posts` - List posts
- `GET /api/posts/:id` - Get post by ID

## Features

- Route groups (`/api/*`)
- Middleware (logging, API auth)
- Path parameters (`:id`)
- Query parameters
- JSON responses
