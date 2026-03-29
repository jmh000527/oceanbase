#!/bin/bash
# Setup script for standalone repository

set -e

echo "=========================================="
echo "PostgreSQL Outline Plugin"
echo "Standalone Repository Setup"
echo "=========================================="
echo ""

# Check if we're in the right directory
if [ ! -f "pg_outline.control" ]; then
    echo "Error: Must run from pg_outline_plugin directory"
    exit 1
fi

echo "Step 1: Checking prerequisites..."

# Check for git
if ! command -v git &> /dev/null; then
    echo "Error: git is not installed"
    echo "Install with: sudo apt-get install git"
    exit 1
fi

echo "✓ Git is installed"

# Check for PostgreSQL
if ! command -v pg_config &> /dev/null; then
    echo "Warning: pg_config not found in PATH"
    echo "You may need to install postgresql-server-dev-XX"
else
    PG_VERSION=$(pg_config --version | grep -oP '\d+' | head -1)
    echo "✓ PostgreSQL $PG_VERSION detected"
fi

echo ""
echo "Step 2: Repository statistics..."
echo "  Source files: $(find src -name '*.c' | wc -l)"
echo "  Header files: $(find include -name '*.h' | wc -l)"
echo "  Test files: $(find test -name '*.sql' | wc -l)"
echo "  Documentation files: $(find doc -name '*.md' | wc -l)"
echo "  Total lines of code: $(find src include -name '*.c' -o -name '*.h' | xargs wc -l | tail -1 | awk '{print $1}')"

echo ""
echo "Step 3: Validating plugin structure..."
bash validate.sh > /dev/null 2>&1
if [ $? -eq 0 ]; then
    echo "✓ All validation tests passed"
else
    echo "⚠ Some validation tests failed"
    echo "  Run 'bash validate.sh' for details"
fi

echo ""
echo "Step 4: Checking for git repository..."
if [ -d ".git" ]; then
    echo "⚠ Git repository already initialized"
    echo "  Current branch: $(git branch --show-current 2>/dev/null || echo 'none')"
    echo "  Remote: $(git remote get-url origin 2>/dev/null || echo 'none')"
else
    echo "ℹ No git repository found"
    read -p "Initialize git repository? (y/n) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        git init
        echo "✓ Git repository initialized"

        # Create initial commit
        git add .
        git commit -m "Initial commit: PostgreSQL Outline Plugin v1.0.0

- Complete multi-query block support implementation
- Query block identification with recursive traversal
- Multi-block hint parsing with @QB_NAME syntax
- Comprehensive test suite (27 tests)
- Full documentation and examples
- Docker and CI/CD support"

        echo "✓ Initial commit created"
        echo ""
        echo "Next steps:"
        echo "1. Create a repository on GitHub"
        echo "2. Add remote: git remote add origin https://github.com/YOUR_USERNAME/pg-outline-plugin.git"
        echo "3. Push: git push -u origin main"
    fi
fi

echo ""
echo "=========================================="
echo "Setup complete!"
echo "=========================================="
echo ""
echo "Repository is ready for standalone use."
echo ""
echo "Quick commands:"
echo "  Build:     make PG_CONFIG=/usr/lib/postgresql/12/bin/pg_config"
echo "  Install:   sudo make install"
echo "  Validate:  bash validate.sh"
echo "  Docker:    docker-compose up -d"
echo ""
echo "Documentation:"
echo "  Main guide:      README.md"
echo "  Setup guide:     STANDALONE_SETUP.md"
echo "  Installation:    doc/INSTALL.md"
echo "  Contributing:    CONTRIBUTING.md"
echo ""
echo "For detailed setup instructions, see:"
echo "  STANDALONE_SETUP.md"
echo ""
