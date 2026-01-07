# Security policy

## Reporting a vulnerability

If you discover a security vulnerability in Quadrate, please report it responsibly:

1. **Email**: Send details to ~klahr/quadrate@lists.sr.ht with subject prefix `[SECURITY]`
2. **Do not** open public issues for security vulnerabilities

## Response timeline

- **Acknowledgment**: Within 48 hours
- **Initial assessment**: Within 7 days
- **Fix timeline**: Depends on severity, typically within 30 days

## Supported versions

Only the latest release receives security updates.

| Version | Supported |
|---------|-----------|
| Latest  | ✓         |
| Older   | ✗         |

## Scope

Security issues in:
- The quadc compiler
- Runtime library (libqdrt)
- Embedding API (libqd)
- Standard library modules
- Language server (quadlsp)

## Out of scope

- Vulnerabilities in user-written Quadrate code
- Issues in third-party dependencies (report to their maintainers)
- Denial of service via resource exhaustion in user code
