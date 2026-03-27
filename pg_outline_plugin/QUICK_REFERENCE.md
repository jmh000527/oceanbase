# pg_outline - Quick Reference Card

## Installation (3 Commands)

```bash
make && sudo make install                           # Build and install
psql -d mydb -c "CREATE EXTENSION pg_outline;"     # Enable extension
psql -d mydb -c "SET pg_outline.enabled = on;"     # Activate feature
```

## Basic Operations

### Create Outline
```sql
SELECT pg_outline_create(
    'outline_name',           -- Unique name
    'SELECT * FROM t WHERE id = 1',  -- Target SQL
    '/*+ SeqScan(t) */'      -- Hints to apply
);
```

### Modify Outline
```sql
SELECT pg_outline_alter('outline_name', '/*+ IndexScan(t idx) */');
```

### Enable/Disable
```sql
SELECT pg_outline_enable('outline_name', true);   -- Enable
SELECT pg_outline_enable('outline_name', false);  -- Disable
```

### Delete Outline
```sql
SELECT pg_outline_drop('outline_name');
```

## Query Outlines

### List All Outlines
```sql
SELECT * FROM pg_outline_list();
SELECT * FROM pg_outline_list(false);  -- Include disabled
```

### View Details
```sql
SELECT * FROM pg_outline_info WHERE outline_name = 'outline_name';
```

### Check Usage Statistics
```sql
SELECT outline_name, usage_count, last_used_at
FROM pg_outline_info
ORDER BY usage_count DESC;
```

## Import/Export

### Export All
```sql
\o outlines.json
SELECT pg_outline_export();
\o
```

### Export One
```sql
SELECT pg_outline_export('outline_name');
```

### Import
```sql
SELECT pg_outline_import('file content here'::json);
```

## Common Hints

### Table Scan Methods
```sql
'/*+ SeqScan(table_name) */'           -- Force sequential scan
'/*+ IndexScan(table_name idx) */'     -- Force index scan
'/*+ IndexOnlyScan(table_name idx) */' -- Force index-only scan
'/*+ BitmapScan(table_name idx) */'    -- Force bitmap scan
```

### Join Methods
```sql
'/*+ NestLoop(t1 t2) */'    -- Force nested loop join
'/*+ HashJoin(t1 t2) */'    -- Force hash join
'/*+ MergeJoin(t1 t2) */'   -- Force merge join
```

### Join Order
```sql
'/*+ Leading(t1 t2 t3) */'  -- Force join order: t1→t2→t3
```

### Parallel Query
```sql
'/*+ Parallel(table_name 4) */'   -- Use 4 workers
'/*+ Parallel(table_name 0) */'   -- Disable parallel
```

### Aggregation
```sql
'/*+ HashAggregate */'    -- Force hash aggregation
'/*+ GroupAggregate */'   -- Force group aggregation
```

## Configuration

### In postgresql.conf
```ini
shared_preload_libraries = 'pg_outline'
pg_outline.enabled = on
pg_outline.cache_size = 1000
pg_outline.reload_interval = 60
pg_outline.debug_log = off
```

### Runtime Changes
```sql
ALTER SYSTEM SET pg_outline.enabled = on;
SELECT pg_reload_conf();

-- Or for current session only
SET pg_outline.enabled = on;
```

## Troubleshooting

### Enable Debug Logging
```sql
ALTER SYSTEM SET pg_outline.debug_log = on;
SELECT pg_reload_conf();
-- Check PostgreSQL logs
```

### Check Outline Matching
```sql
-- See what signature your query generates
SELECT MD5(UPPER(REGEXP_REPLACE('your sql', '\s+', ' ', 'g')));

-- Compare with outline
SELECT sql_id FROM pg_outline WHERE outline_name = 'your_outline';
```

### Verify Outline is Active
```sql
SELECT outline_name, enabled
FROM pg_outline
WHERE outline_name = 'your_outline';
```

### Clear Cache
```sql
-- Outlines are reloaded automatically
-- Or restart PostgreSQL to force reload
```

## Examples

### Example 1: Force Sequential Scan
```sql
-- Create
SELECT pg_outline_create(
    'force_seq',
    'SELECT * FROM users WHERE status = ''active''',
    '/*+ SeqScan(users) */'
);

-- Test (both will use SeqScan)
EXPLAIN SELECT * FROM users WHERE status = 'active';
EXPLAIN SELECT * FROM users WHERE status = 'inactive';
```

### Example 2: Force Index Usage
```sql
-- Create
SELECT pg_outline_create(
    'force_idx',
    'SELECT * FROM orders WHERE customer_id = 123',
    '/*+ IndexScan(orders idx_customer) */'
);

-- Test
EXPLAIN SELECT * FROM orders WHERE customer_id = 456;
```

### Example 3: Control Join Order
```sql
-- Create
SELECT pg_outline_create(
    'join_order',
    'SELECT * FROM t1 JOIN t2 ON t1.id = t2.id JOIN t3 ON t2.id = t3.id',
    '/*+ Leading(t3 t2 t1) HashJoin(t2 t3) */'
);

-- Test
EXPLAIN SELECT * FROM t1 JOIN t2 ON t1.id = t2.id JOIN t3 ON t2.id = t3.id;
```

### Example 4: Disable Parallel Query
```sql
-- Create
SELECT pg_outline_create(
    'no_parallel',
    'SELECT COUNT(*) FROM large_table WHERE date > ''2024-01-01''',
    '/*+ Parallel(large_table 0) */'
);

-- Test
EXPLAIN SELECT COUNT(*) FROM large_table WHERE date > '2024-01-01';
```

## Schema

### Tables
- `pg_outline` - Outline definitions
- `pg_outline_stats` - Usage statistics

### Views
- `pg_outline_info` - Combined outline and statistics

### Functions
- `pg_outline_create()` - Create outline
- `pg_outline_alter()` - Modify outline
- `pg_outline_drop()` - Delete outline
- `pg_outline_enable()` - Enable/disable
- `pg_outline_list()` - List outlines
- `pg_outline_export()` - Export JSON
- `pg_outline_import()` - Import JSON

## Performance Tips

1. **Cache Size**: Set based on number of distinct queries
   ```sql
   ALTER SYSTEM SET pg_outline.cache_size = 5000;
   ```

2. **Selective Use**: Only outline problematic queries

3. **Monitor Usage**: Regularly check pg_outline_info

4. **Clean Up**: Drop unused outlines
   ```sql
   SELECT pg_outline_drop(outline_name)
   FROM pg_outline_info
   WHERE usage_count = 0 AND created_at < NOW() - INTERVAL '30 days';
   ```

5. **Test First**: Always EXPLAIN before deploying

## Common Patterns

### Pattern: Lock Current Plan
```sql
-- 1. Run EXPLAIN to see current plan
EXPLAIN (FORMAT JSON) SELECT ...;

-- 2. Extract hints from plan (manual process)

-- 3. Create outline with those hints
SELECT pg_outline_create('my_outline', 'SELECT ...', '/*+ hints */');
```

### Pattern: A/B Testing Plans
```sql
-- Create two outlines for same query
SELECT pg_outline_create('plan_a', 'SELECT ...', '/*+ hints_a */');
SELECT pg_outline_create('plan_b', 'SELECT ...', '/*+ hints_b */');

-- Test plan A
SELECT pg_outline_enable('plan_a', true);
SELECT pg_outline_enable('plan_b', false);
-- Run tests...

-- Test plan B
SELECT pg_outline_enable('plan_a', false);
SELECT pg_outline_enable('plan_b', true);
-- Run tests...

-- Compare results from pg_outline_stats
```

### Pattern: Temporary Override
```sql
-- Save current hints
SELECT outline_content FROM pg_outline WHERE outline_name = 'my_outline' \gset old_

-- Apply temporary hints
SELECT pg_outline_alter('my_outline', '/*+ new hints */');

-- Test...

-- Restore
SELECT pg_outline_alter('my_outline', :'old_outline_content');
```

## Resources

- **Full Documentation**: README.md
- **Installation Guide**: doc/INSTALL.md
- **Test Suite**: test/test_outline.sql
- **Implementation Details**: IMPLEMENTATION_SUMMARY.md

## Quick Diagnosis

| Problem | Check | Solution |
|---------|-------|----------|
| Outline not matching | Enable debug_log | Verify signature |
| Hints not applied | Check enabled status | Verify hint syntax |
| Performance overhead | Monitor cache hit ratio | Increase cache_size |
| Stale cache | Check reload_interval | Decrease interval |

---
**pg_outline v1.0.0** | Based on OceanBase outline feature
