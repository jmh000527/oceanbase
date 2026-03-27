# PostgreSQL Outline Plugin - Installation Guide

## System Requirements

- **PostgreSQL Version**: 12, 13, 14, 15, or later
- **Operating System**: Linux (Ubuntu, CentOS, Debian, etc.) or macOS
- **Compiler**: GCC 4.9+ or Clang 3.4+
- **Build Tools**: make, cmake (optional)
- **Disk Space**: ~10 MB for source and build artifacts

## Step-by-Step Installation

### 1. Install PostgreSQL Development Headers

#### Ubuntu/Debian
```bash
sudo apt-get update
sudo apt-get install postgresql-server-dev-12 build-essential
```

#### CentOS/RHEL
```bash
sudo yum install postgresql12-devel gcc make
```

#### macOS (with Homebrew)
```bash
brew install postgresql@12
```

### 2. Download the Plugin

```bash
cd /path/to/oceanbase/repository
cd pg_outline_plugin
```

### 3. Build the Extension

```bash
make clean  # Clean any previous builds
make        # Build the extension
```

Expected output:
```
gcc -Wall -Wmissing-prototypes ... -c -o src/pg_outline_main.o src/pg_outline_main.c
gcc -Wall -Wmissing-prototypes ... -c -o src/outline_manager.o src/outline_manager.c
...
gcc -shared ... -o pg_outline.so
```

### 4. Install the Extension

```bash
sudo make install
```

This will:
- Copy `pg_outline.so` to PostgreSQL's library directory
- Copy `pg_outline.control` to the extension directory
- Copy SQL files to the share directory

### 5. Verify Installation

```bash
# Check if files are installed
ls -l $(pg_config --pkglibdir)/pg_outline.so
ls -l $(pg_config --sharedir)/extension/pg_outline*
```

### 6. Create Extension in Database

```sql
-- Connect to your database
psql -d your_database

-- Create the extension
CREATE EXTENSION pg_outline;

-- Verify extension is loaded
\dx pg_outline

-- Check version
SELECT extname, extversion FROM pg_extension WHERE extname = 'pg_outline';
```

### 7. Configure PostgreSQL

Add to `postgresql.conf`:
```ini
# pg_outline configuration
shared_preload_libraries = 'pg_outline'  # Add to existing libraries
pg_outline.enabled = on
pg_outline.cache_size = 1000
pg_outline.reload_interval = 60
pg_outline.debug_log = off
```

### 8. Restart PostgreSQL

```bash
# Linux (systemd)
sudo systemctl restart postgresql

# Linux (init.d)
sudo service postgresql restart

# macOS
brew services restart postgresql@12
```

### 9. Verify Configuration

```sql
-- Check current settings
SHOW pg_outline.enabled;
SHOW pg_outline.cache_size;

-- View all pg_outline settings
SELECT name, setting, unit, context
FROM pg_settings
WHERE name LIKE 'pg_outline.%';
```

## Troubleshooting Installation

### Problem: "pg_config: command not found"

**Solution**: Add PostgreSQL bin directory to PATH
```bash
export PATH=/usr/pgsql-12/bin:$PATH  # Adjust version as needed
# Or find pg_config
find /usr -name pg_config 2>/dev/null
```

### Problem: "Permission denied" during make install

**Solution**: Use sudo
```bash
sudo make install
```

### Problem: Extension files not found

**Solution**: Verify installation paths
```bash
pg_config --pkglibdir
pg_config --sharedir
ls -l $(pg_config --sharedir)/extension/pg_outline*
```

### Problem: CREATE EXTENSION fails with "could not open extension control file"

**Solution**: Check file permissions and ownership
```bash
sudo chown postgres:postgres $(pg_config --sharedir)/extension/pg_outline.control
sudo chmod 644 $(pg_config --sharedir)/extension/pg_outline.control
```

### Problem: "undefined symbol" errors when loading

**Solution**: Rebuild with correct PostgreSQL version
```bash
make clean
PG_CONFIG=/path/to/correct/pg_config make
sudo make install
```

### Problem: Extension loads but outlines don't match

**Solution**:
1. Enable debug logging:
   ```sql
   ALTER SYSTEM SET pg_outline.debug_log = on;
   SELECT pg_reload_conf();
   ```

2. Check PostgreSQL logs for matching attempts

3. Verify outline is enabled:
   ```sql
   SELECT * FROM pg_outline WHERE enabled = true;
   ```

## Upgrading

### From 1.0 to Future Versions

1. Build new version:
   ```bash
   cd pg_outline_plugin
   git pull
   make clean
   make
   sudo make install
   ```

2. Update extension in database:
   ```sql
   ALTER EXTENSION pg_outline UPDATE TO '1.1';
   ```

3. Restart PostgreSQL if required

## Uninstalling

### 1. Drop Extension from Databases

```sql
-- In each database using pg_outline
DROP EXTENSION pg_outline CASCADE;
```

### 2. Remove Files

```bash
sudo rm $(pg_config --pkglibdir)/pg_outline.so
sudo rm $(pg_config --sharedir)/extension/pg_outline*
```

### 3. Remove Configuration

Edit `postgresql.conf` and remove:
- `pg_outline` from `shared_preload_libraries`
- All `pg_outline.*` parameters

### 4. Restart PostgreSQL

```bash
sudo systemctl restart postgresql
```

## Docker Installation

### Dockerfile Example

```dockerfile
FROM postgres:12

# Install build dependencies
RUN apt-get update && apt-get install -y \
    postgresql-server-dev-12 \
    build-essential \
    git

# Copy plugin source
COPY pg_outline_plugin /tmp/pg_outline_plugin

# Build and install
WORKDIR /tmp/pg_outline_plugin
RUN make && make install

# Configure
RUN echo "shared_preload_libraries = 'pg_outline'" >> /usr/share/postgresql/postgresql.conf.sample
RUN echo "pg_outline.enabled = on" >> /usr/share/postgresql/postgresql.conf.sample

# Cleanup
RUN apt-get remove -y postgresql-server-dev-12 build-essential git
RUN apt-get autoremove -y && apt-get clean
RUN rm -rf /tmp/pg_outline_plugin
```

Build and run:
```bash
docker build -t postgres-outline .
docker run -d --name pg-outline -e POSTGRES_PASSWORD=secret postgres-outline
docker exec -it pg-outline psql -U postgres -c "CREATE EXTENSION pg_outline;"
```

## Platform-Specific Notes

### Ubuntu 20.04 LTS
- Default PostgreSQL version: 12
- Install: `sudo apt install postgresql-12 postgresql-server-dev-12`

### CentOS 8 / RHEL 8
- Enable PostgreSQL repository first
- Install: `sudo dnf install postgresql12-server postgresql12-devel`

### macOS Big Sur / Monterey
- Use Homebrew for easiest installation
- PostgreSQL.app also supported

### Windows (WSL2)
- Install Ubuntu in WSL2
- Follow Ubuntu instructions above
- Or use native Windows PostgreSQL build (more complex)

## Security Considerations

### File Permissions

Ensure proper permissions on shared library:
```bash
sudo chmod 755 $(pg_config --pkglibdir)/pg_outline.so
sudo chown root:root $(pg_config --pkglibdir)/pg_outline.so
```

### Database Permissions

Grant appropriate permissions:
```sql
-- Allow specific user to manage outlines
GRANT ALL ON pg_outline TO outline_admin;
GRANT ALL ON pg_outline_stats TO outline_admin;

-- Read-only access for monitoring
GRANT SELECT ON pg_outline_info TO monitoring_user;
```

## Performance Considerations

### Memory Usage

- Each cached outline: ~1-2 KB
- Default cache (1000 outlines): ~2 MB
- Adjust based on workload:
  ```sql
  ALTER SYSTEM SET pg_outline.cache_size = 5000;
  ```

### CPU Overhead

- Minimal overhead when no outlines match (<1%)
- Outline matching: typically <100 microseconds
- Enable only for databases that need it

## Support and Resources

- **Documentation**: README.md in plugin directory
- **Examples**: test/test_outline.sql
- **Source Code**: pg_outline_plugin/src/
- **Issues**: Report via GitHub or email

## Next Steps

After installation:
1. Read the [README.md](README.md) for usage guide
2. Run test suite: `make test`
3. Create your first outline
4. Monitor usage with `pg_outline_info` view
5. Adjust configuration based on workload

## Quick Start Example

```sql
-- 1. Create extension
CREATE EXTENSION pg_outline;

-- 2. Create a test table
CREATE TABLE products (id INT, name TEXT, price NUMERIC);
CREATE INDEX idx_price ON products(price);

-- 3. Create an outline
SELECT pg_outline_create(
    'force_index_scan',
    'SELECT * FROM products WHERE price > 100',
    '/*+ IndexScan(products idx_price) */'
);

-- 4. Test it
EXPLAIN SELECT * FROM products WHERE price > 100;
EXPLAIN SELECT * FROM products WHERE price > 500;  -- Also matches!

-- 5. Check stats
SELECT * FROM pg_outline_info;
```

Congratulations! You're now ready to use pg_outline for execution plan control.
