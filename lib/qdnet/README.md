# qdnet

TCP networking (`net::` module).

## Overview

Socket operations for network programming.

## Key Functions

- `listen` - Create server socket
- `accept` - Accept connection
- `connect` - Connect to server
- `send`, `receive` - Data transfer
- `shutdown`, `close` - Connection management

## Platform Support

Socket operations are abstracted in `src/platform/` for portability.
