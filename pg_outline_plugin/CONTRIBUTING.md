# Contributing to PostgreSQL Outline Plugin

Thank you for your interest in contributing to the PostgreSQL Outline Plugin!

## Getting Started

1. Fork the repository
2. Clone your fork: `git clone https://github.com/YOUR_USERNAME/pg_outline_plugin.git`
3. Create a branch: `git checkout -b feature/your-feature-name`

## Development Setup

### Prerequisites

- PostgreSQL 12 or later
- PostgreSQL development headers (`postgresql-server-dev-XX`)
- GCC or Clang compiler
- Make

### Building

```bash
make PG_CONFIG=/usr/lib/postgresql/12/bin/pg_config
sudo make install PG_CONFIG=/usr/lib/postgresql/12/bin/pg_config
```

### Testing

```bash
# Run validation tests
bash validate.sh

# Run functional tests
psql -U postgres -d testdb -f test/test_outline.sql
psql -U postgres -d testdb -f test/test_multi_block.sql
```

## Code Style

- Follow PostgreSQL coding conventions
- Use tabs for indentation (not spaces)
- Keep lines under 80 characters where possible
- Add comments for complex logic
- Use PostgreSQL memory management functions (`palloc`, `pfree`)

## Commit Messages

- Use clear, descriptive commit messages
- Start with a verb (Add, Fix, Update, Remove, etc.)
- Reference issues when applicable

Example:
```
Add support for UNION query blocks

- Implement SET$ prefix for set operations
- Add tests for UNION, INTERSECT, EXCEPT
- Update documentation with examples

Fixes #123
```

## Pull Request Process

1. Update documentation for any new features
2. Add tests for new functionality
3. Ensure all tests pass
4. Update CHANGELOG.md with your changes
5. Submit PR with clear description

## Testing Guidelines

- Write tests for all new features
- Include edge cases and error conditions
- Test with different PostgreSQL versions (12-16)
- Verify memory management (no leaks)

## Documentation

- Update README.md for user-facing changes
- Add technical details to doc/ directory
- Include examples for new features
- Keep documentation in sync with code

## Reporting Bugs

When reporting bugs, include:

- PostgreSQL version
- Plugin version
- Steps to reproduce
- Expected vs actual behavior
- Relevant logs and error messages

## Feature Requests

We welcome feature requests! Please:

- Check if feature already exists
- Describe use case clearly
- Provide examples if possible
- Be open to discussion

## Code Review

All contributions go through code review:

- Be responsive to feedback
- Ask questions if unclear
- Make requested changes promptly
- Be respectful and constructive

## License

By contributing, you agree that your contributions will be licensed under the PostgreSQL License.

## Questions?

- Open an issue for questions
- Join discussions
- Check existing documentation

Thank you for contributing!
