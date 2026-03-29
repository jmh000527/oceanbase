# Changelog

All notable changes to the PostgreSQL Outline Plugin will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2026-03-29

### Added
- Initial release of PostgreSQL Outline Plugin
- SQL execution plan stabilization with optimizer hints
- Multi-query block support with @QB_NAME syntax
- Query block identification for complex queries
  - Subqueries in WHERE, FROM, SELECT, HAVING
  - Common Table Expressions (CTEs)
  - Set operations (UNION, INTERSECT, EXCEPT)
  - Nested subqueries (multi-level)
- QB_NAME generation using MD5 hashing
- Multi-block hint parser with @QB_NAME targeting
- SQL normalization and matching
- Multi-strategy outline matching
  - Match by signature
  - Match by SQL ID
  - Format outline support
- Efficient in-memory caching with hash tables
- Usage statistics tracking
- Import/export functionality (JSON format)
- Comprehensive test suite
  - Functional tests
  - Query block identification tests
  - Hint parser tests
  - Multi-block integration tests
- Complete documentation
  - User guide and quick reference
  - Technical design documents
  - Multi-block examples and FAQ
  - Installation and build instructions
- Docker support for testing
- GitHub Actions CI/CD pipeline
- Validation script for code quality

### Features
- **Query Block Identification**: Recursive traversal of query trees
- **Hint Targeting**: Apply hints to specific query blocks using @QB_NAME
- **Global Hints**: Support for hints that apply to entire query
- **Mixed Hints**: Combine global and block-specific hints
- **Plan Stability**: Ensure consistent execution plans across runs
- **Memory Safety**: Proper PostgreSQL memory context management
- **Debug Logging**: Detailed logging for troubleshooting

### Compatibility
- PostgreSQL 12+
- Tested on Ubuntu 20.04 and 22.04
- Compatible with pg_hint_plan (optional integration)

### Documentation
- README.md: Overview and usage
- INSTALL.md: Build and installation instructions
- MULTI_QUERY_BLOCK_DESIGN.md: Technical architecture
- MULTI_BLOCK_EXAMPLES.sql: Practical examples
- MULTI_BLOCK_FAQ.md: Common questions
- MULTI_BLOCK_QUICKREF.md: Quick reference guide
- IMPLEMENTATION_SUMMARY.md: Implementation details

### Testing
- 27 validation tests (all passing)
- 12 multi-block functional tests
- 16 query block identification tests
- 12 hint parser tests
- Docker-based integration tests

## [Unreleased]

### Planned
- Integration with pg_hint_plan for actual hint execution
- Plan verification and comparison tools
- Automatic outline generation from execution history
- Web-based management interface
- Query block visualization tools
- Hint effectiveness analytics
- Support for additional PostgreSQL versions
- Performance optimizations
- Extended hint syntax support

---

[1.0.0]: https://github.com/YOUR_USERNAME/pg_outline_plugin/releases/tag/v1.0.0
