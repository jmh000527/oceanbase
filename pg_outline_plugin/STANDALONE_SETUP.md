# Standalone Repository Setup Guide

This guide helps you set up the PostgreSQL Outline Plugin as a standalone repository.

## Quick Start

### Option 1: Create New Repository on GitHub

1. **Create a new repository on GitHub:**
   ```bash
   # On GitHub, create a new repository named "pg-outline-plugin"
   # Don't initialize with README, .gitignore, or license
   ```

2. **Initialize git in the plugin directory:**
   ```bash
   cd pg_outline_plugin
   git init
   git add .
   git commit -m "Initial commit: PostgreSQL Outline Plugin v1.0.0"
   ```

3. **Add remote and push:**
   ```bash
   git remote add origin https://github.com/YOUR_USERNAME/pg-outline-plugin.git
   git branch -M main
   git push -u origin main
   ```

### Option 2: Copy to New Location

1. **Copy the plugin directory:**
   ```bash
   cp -r pg_outline_plugin /path/to/new/location/pg-outline-plugin
   cd /path/to/new/location/pg-outline-plugin
   ```

2. **Initialize git:**
   ```bash
   git init
   git add .
   git commit -m "Initial commit: PostgreSQL Outline Plugin v1.0.0"
   ```

3. **Create GitHub repository and push:**
   ```bash
   # Create repo on GitHub first
   git remote add origin https://github.com/YOUR_USERNAME/pg-outline-plugin.git
   git branch -M main
   git push -u origin main
   ```

## What's Included

The standalone repository includes:

### Core Files
- ✅ Source code (`src/*.c`)
- ✅ Headers (`include/*.h`)
- ✅ SQL definitions (`sql/*.sql`)
- ✅ Makefile (with PGXS build system)
- ✅ Extension control file (`pg_outline.control`)

### Documentation
- ✅ README.md - User guide
- ✅ INSTALL.md - Build instructions
- ✅ CHANGELOG.md - Version history
- ✅ CONTRIBUTING.md - Contribution guide
- ✅ SECURITY.md - Security policy
- ✅ LICENSE - PostgreSQL License
- ✅ Technical documentation in `doc/`

### Testing
- ✅ Test suite (`test/*.sql`)
- ✅ Validation script (`validate.sh`)
- ✅ Docker support (`Dockerfile`, `docker-compose.yml`)

### CI/CD
- ✅ GitHub Actions workflow (`.github/workflows/ci.yml`)
- ✅ Issue templates (`.github/ISSUE_TEMPLATE/`)
- ✅ Pull request template

### Configuration
- ✅ `.gitignore` - Git ignore patterns
- ✅ `VERSION` - Version tracking

## Repository Structure

```
pg-outline-plugin/
├── .github/
│   ├── workflows/
│   │   └── ci.yml                    # CI/CD pipeline
│   ├── ISSUE_TEMPLATE/
│   │   ├── bug_report.md
│   │   └── feature_request.md
│   └── pull_request_template.md
├── doc/
│   ├── INSTALL.md                    # Installation guide
│   ├── MULTI_QUERY_BLOCK_DESIGN.md  # Technical design
│   ├── MULTI_BLOCK_EXAMPLES.sql     # Usage examples
│   ├── MULTI_BLOCK_FAQ.md           # FAQ
│   ├── MULTI_BLOCK_QUICKREF.md      # Quick reference
│   ├── QUERY_BLOCK_CONCEPT.txt      # Concept explanation
│   └── IMPLEMENTATION_SUMMARY.md    # Implementation details
├── docker-entrypoint-initdb.d/
│   └── 01-init.sql                  # Docker init script
├── include/
│   ├── pg_outline.h                 # Main header
│   ├── outline_manager.h
│   ├── outline_normalize.h
│   ├── outline_matcher.h
│   ├── outline_hints.h
│   ├── outline_query_block.h
│   └── outline_hint_parser.h
├── src/
│   ├── pg_outline_main.c            # Main plugin
│   ├── outline_manager.c            # Storage management
│   ├── outline_normalize.c          # SQL normalization
│   ├── outline_matcher.c            # Matching logic
│   ├── outline_hints.c              # Hint application
│   ├── outline_query_block.c        # Query block identification
│   └── outline_hint_parser.c        # Hint parsing
├── sql/
│   └── pg_outline--1.0.sql          # Extension SQL
├── test/
│   ├── test_outline.sql             # Basic tests
│   ├── test_multi_block.sql         # Multi-block tests
│   ├── test_query_block_identification.sql
│   └── test_hint_parser.sql
├── .gitignore                       # Git ignore
├── CHANGELOG.md                     # Version history
├── CONTRIBUTING.md                  # Contribution guide
├── Dockerfile                       # Docker image
├── docker-compose.yml               # Docker compose
├── LICENSE                          # PostgreSQL License
├── Makefile                         # Build system
├── pg_outline.control               # Extension control
├── README.md                        # Main documentation
├── SECURITY.md                      # Security policy
├── validate.sh                      # Validation script
└── VERSION                          # Version number
```

## Build and Test

### Local Build
```bash
make PG_CONFIG=/usr/lib/postgresql/12/bin/pg_config
sudo make install PG_CONFIG=/usr/lib/postgresql/12/bin/pg_config
```

### Run Validation
```bash
bash validate.sh
```

### Docker Build
```bash
docker build -t pg_outline:latest .
docker-compose up -d
```

### Run Tests
```bash
psql -U postgres -d testdb -c "CREATE EXTENSION pg_outline;"
psql -U postgres -d testdb -f test/test_outline.sql
```

## CI/CD Setup

The GitHub Actions workflow automatically:
- Builds on PostgreSQL 12-16
- Tests on Ubuntu 20.04 and 22.04
- Runs validation and functional tests
- Builds Docker images
- Checks code quality

To enable:
1. Push to GitHub
2. GitHub Actions will run automatically
3. View results in the "Actions" tab

## Versioning

This project uses [Semantic Versioning](https://semver.org/):
- **MAJOR**: Incompatible API changes
- **MINOR**: Backward-compatible functionality
- **PATCH**: Backward-compatible bug fixes

Current version: **1.0.0**

## Distribution

### GitHub Releases

1. Tag a release:
   ```bash
   git tag -a v1.0.0 -m "Release version 1.0.0"
   git push origin v1.0.0
   ```

2. Create release on GitHub:
   - Go to "Releases" → "Create a new release"
   - Select the tag
   - Add release notes from CHANGELOG.md
   - Attach artifacts if needed

### PGXN (PostgreSQL Extension Network)

To publish on PGXN:

1. Create `META.json`:
   ```json
   {
      "name": "pg_outline",
      "abstract": "SQL execution plan stabilization",
      "version": "1.0.0",
      "maintainer": "Your Name <email@example.com>",
      "license": "postgresql",
      "provides": {
         "pg_outline": {
            "file": "sql/pg_outline--1.0.sql",
            "version": "1.0.0"
         }
      }
   }
   ```

2. Submit to PGXN: https://manager.pgxn.org/

### Docker Hub

1. Tag and push:
   ```bash
   docker build -t your-username/pg_outline:1.0.0 .
   docker tag your-username/pg_outline:1.0.0 your-username/pg_outline:latest
   docker push your-username/pg_outline:1.0.0
   docker push your-username/pg_outline:latest
   ```

## Maintenance

### Regular Tasks

- Update CHANGELOG.md for each release
- Respond to issues and pull requests
- Test with new PostgreSQL versions
- Review and merge contributions
- Update documentation as needed

### Security

- Monitor security advisories
- Apply patches promptly
- Follow disclosure process in SECURITY.md

## Support

- **Issues**: GitHub Issues
- **Discussions**: GitHub Discussions
- **Email**: [your-email]

## Next Steps

1. ✅ Repository is ready for standalone use
2. ✅ All files configured
3. ✅ Documentation complete
4. ✅ CI/CD configured
5. ✅ Docker support ready

**To deploy:**
1. Create GitHub repository
2. Push code
3. Configure repository settings
4. Enable GitHub Actions
5. Create first release
6. Announce to community

## Resources

- [PostgreSQL Extension Building](https://www.postgresql.org/docs/current/extend-pgxs.html)
- [PGXN How-to](https://manager.pgxn.org/howto)
- [GitHub Actions Documentation](https://docs.github.com/en/actions)
- [Docker Documentation](https://docs.docker.com/)

---

**Ready to go!** The plugin is now fully configured as a standalone repository.
