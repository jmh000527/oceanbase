# PostgreSQL Outline Plugin

## Overview

The PostgreSQL Outline Plugin (`pg_outline`) provides SQL execution plan stabilization by binding optimizer hints to SQL queries without modifying application code. This implementation is based on the outline feature from OceanBase, bringing similar plan control capabilities to PostgreSQL.

## Features

- **Plan Stabilization**: Fix execution plans for specific SQL patterns
- **SQL Normalization**: Automatic parameter extraction for matching queries with different constants
- **Multi-Strategy Matching**: Match outlines by signature or SQL ID
- **Multi-Query Block Support**: Target hints to specific query blocks using @QB_NAME syntax
- **Complex Query Support**: Handle subqueries, CTEs, UNION, and nested queries
- **Efficient Caching**: In-memory hash-based lookup for fast outline retrieval
- **Usage Statistics**: Track outline usage and performance metrics
- **Format Outlines**: Support for flexible outline matching patterns
- **Easy Management**: SQL functions for outline CRUD operations
- **Import/Export**: JSON-based outline backup and migration

## Installation

### Prerequisites

- PostgreSQL 12 or later
- PostgreSQL development headers
- Standard C compiler (gcc or clang)
- Make

### Build and Install

```bash
cd pg_outline_plugin
make
sudo make install
```

### Enable the Extension

```sql
CREATE EXTENSION pg_outline;
```

## Configuration

Add these parameters to `postgresql.conf`:

```ini
# Enable outline feature
pg_outline.enabled = on

# Cache size (number of outlines)
pg_outline.cache_size = 1000

# Cache reload interval in seconds
pg_outline.reload_interval = 60

# Debug logging
pg_outline.debug_log = off
```

Reload configuration:
```sql
SELECT pg_reload_conf();
```

## Usage

### Basic Outline Creation

```sql
-- Create a simple outline to force a sequential scan
SELECT pg_outline_create(
    'outline_test1',                           -- outline name
    'SELECT * FROM users WHERE id = 100',      -- target SQL
    '/*+ SeqScan(users) */',                   -- hints to apply
    'Force seqscan on users table',            -- description (optional)
    true                                        -- enabled (default: true)
);
```

### Multi-Query Block Outlines

For complex queries with subqueries, use @QB_NAME syntax to target specific query blocks:

```sql
-- Create outline for query with subquery
SELECT pg_outline_create(
    'outline_complex',
    $$SELECT * FROM users
      WHERE id IN (SELECT user_id FROM orders WHERE amount > 100)$$,
    $$/*+ INDEX(@SEL$ABCD1234_1 users idx_users_id)
         INDEX(@SEL$EFGH5678_2 orders idx_orders_user_amount) */$$,
    'Outline with multi-block hints'
);
```

**Understanding Query Blocks:**

Each query block in a SQL statement gets a unique QB_NAME:
- Main query: `SEL$xxxxx_0` (SELECT), `INS$xxxxx_0` (INSERT), etc.
- Subqueries: `SEL$xxxxx_1`, `SEL$xxxxx_2`, etc.
- CTEs: Numbered sequentially
- Set operations: `SET$xxxxx_1`, `SET$xxxxx_2`

**Common Scenarios:**

```sql
-- Subquery in WHERE clause
/*+ INDEX(@SEL$MAIN_1 users pk)
    INDEX(@SEL$SUB_2 orders idx_user) */

-- Subquery in FROM (derived table)
/*+ HASHJOIN(@SEL$MAIN_1 u o)
    SEQSCAN(@SEL$DERIVED_2 orders) */

-- Multiple subqueries in SELECT list
/*+ INDEX(@SEL$MAIN_1 users pk)
    INDEX(@SEL$SUB1_2 orders idx1)
    INDEX(@SEL$SUB2_3 orders idx2) */

-- CTE (WITH clause)
/*+ INDEX(@SEL$CTE_2 orders idx_created)
    HASHJOIN(@SEL$MAIN_1 u ro) */

-- Nested subqueries (3 levels)
/*+ INDEX(@SEL$L1_1 users pk)
    INDEX(@SEL$L2_2 orders idx_user)
    INDEX(@SEL$L3_3 products idx_cat) */

-- Global hints (apply to all blocks)
/*+ LEADING(u o) HASHJOIN(u o) */
```

For detailed examples, see [Multi-Block Examples](doc/MULTI_BLOCK_EXAMPLES.sql)

### Listing Outlines

```sql
-- List all enabled outlines
SELECT * FROM pg_outline_list(true);

-- View detailed information
SELECT * FROM pg_outline_info;
```

### Modifying an Outline

```sql
-- Change hint content
SELECT pg_outline_alter(
    'outline_test1',
    '/*+ IndexScan(users users_pkey) */'
);

-- Disable an outline
SELECT pg_outline_enable('outline_test1', false);

-- Enable an outline
SELECT pg_outline_enable('outline_test1', true);
```

### Dropping an Outline

```sql
SELECT pg_outline_drop('outline_test1');
```

### Exporting and Importing Outlines

```sql
-- Export all outlines
SELECT pg_outline_export();

-- Export specific outline
SELECT pg_outline_export('outline_test1');

-- Import outlines
SELECT pg_outline_import('[
    {
        "outline_name": "outline_test2",
        "sql_text": "SELECT * FROM orders WHERE status = ''pending''",
        "outline_content": "/*+ SeqScan(orders) */",
        "enabled": true,
        "format_outline": false
    }
]'::json);
```

## How It Works

### 1. SQL Normalization

When a query is executed, pg_outline:
1. Parses the SQL statement
2. Replaces constants with parameter placeholders
3. Generates a normalized signature
4. Computes an SQL ID (MD5 hash)

Example:
```sql
Original:  SELECT * FROM users WHERE id = 123 AND status = 'active'
Signature: SELECT * FROM USERS WHERE ID = ? AND STATUS = ?
SQL ID:    a1b2c3d4e5f6... (MD5 hash)
```

### 2. Outline Matching

The plugin attempts multiple matching strategies in order:
1. Match by signature (normal outline)
2. Match by SQL ID (normal outline)
3. Match by signature (format outline)
4. Match by SQL ID (format outline)

### 3. Hint Application

When a match is found:
1. The outline's hint content is extracted
2. Hints are parsed and validated
3. Hints are applied to the query planner
4. Usage statistics are recorded

### 4. Plan Generation

PostgreSQL's planner generates the execution plan with the applied hints, ensuring consistent plan behavior across executions.

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    SQL Query Input                       │
└────────────────────┬────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────┐
│           Planner Hook (pg_outline_planner_hook)         │
└────────────────────┬────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────┐
│               SQL Normalization Module                   │
│  • Parse query tree                                      │
│  • Replace constants with placeholders                   │
│  • Generate signature                                    │
└────────────────────┬────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────┐
│                 Outline Matcher                          │
│  • Compute SQL ID                                        │
│  • Try signature match                                   │
│  • Try SQL ID match                                      │
│  • Validate match                                        │
└────────────────────┬────────────────────────────────────┘
                     │
         ┌───────────┴───────────┐
         │ Match Found?          │
         │                       │
      Yes│                       │No
         ▼                       ▼
┌─────────────────────┐  ┌──────────────────┐
│  Outline Manager    │  │  Standard        │
│  • Lookup cache     │  │  Planner         │
│  • Load from DB     │  │  Flow            │
│  • Extract hints    │  └──────────────────┘
└─────────┬───────────┘
          │
          ▼
┌─────────────────────────────────────────────────────────┐
│                  Hint Applicator                         │
│  • Parse hint string                                     │
│  • Apply to query                                        │
│  • Integrate with pg_hint_plan (if available)            │
└────────────────────┬────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────┐
│               PostgreSQL Planner                         │
│  • Generate execution plan with hints                    │
└────────────────────┬────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────┐
│                  Execute Query                           │
└─────────────────────────────────────────────────────────┘
```

## Database Schema

### pg_outline Table

Stores outline definitions:

| Column | Type | Description |
|--------|------|-------------|
| outline_id | SERIAL | Unique identifier |
| outline_name | VARCHAR(128) | Human-readable name |
| signature | TEXT | Normalized SQL signature |
| sql_id | VARCHAR(64) | MD5 hash of signature |
| outline_content | TEXT | Hint content |
| sql_text | TEXT | Original SQL text |
| enabled | BOOLEAN | Active status |
| format_outline | BOOLEAN | Format outline flag |
| owner | OID | Owner user ID |
| created_at | TIMESTAMPTZ | Creation time |
| modified_at | TIMESTAMPTZ | Last modification time |

### pg_outline_stats Table

Tracks usage statistics:

| Column | Type | Description |
|--------|------|-------------|
| outline_id | INTEGER | References pg_outline |
| usage_count | BIGINT | Number of times used |
| last_used_at | TIMESTAMPTZ | Last usage time |
| total_execution_time_ms | BIGINT | Total execution time |
| avg_execution_time_ms | NUMERIC | Average execution time |

## Examples

### Example 1: Forcing Index Usage

```sql
-- Create table and index
CREATE TABLE products (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100),
    price NUMERIC(10,2),
    category VARCHAR(50)
);

CREATE INDEX idx_products_category ON products(category);

-- Query normally uses sequential scan for small tables
EXPLAIN SELECT * FROM products WHERE category = 'Electronics';

-- Create outline to force index scan
SELECT pg_outline_create(
    'force_category_index',
    'SELECT * FROM products WHERE category = ''Electronics''',
    '/*+ IndexScan(products idx_products_category) */'
);

-- Now query uses index scan
EXPLAIN SELECT * FROM products WHERE category = 'Electronics';
EXPLAIN SELECT * FROM products WHERE category = 'Books';  -- Also matches!
```

### Example 2: Controlling Join Order

```sql
-- Create outline for join order
SELECT pg_outline_create(
    'orders_join_order',
    'SELECT * FROM orders o JOIN customers c ON o.customer_id = c.id WHERE c.status = ''active''',
    '/*+ Leading(c o) NestLoop(o c) */'
);
```

### Example 3: Disabling Parallel Query

```sql
-- Create outline to disable parallel execution
SELECT pg_outline_create(
    'no_parallel_scan',
    'SELECT COUNT(*) FROM large_table WHERE date > ''2024-01-01''',
    '/*+ Parallel(large_table 0) */'
);
```

## Monitoring

### Check Outline Usage

```sql
SELECT
    outline_name,
    usage_count,
    last_used_at,
    avg_execution_time_ms
FROM pg_outline_info
WHERE enabled = true
ORDER BY usage_count DESC;
```

### Cache Statistics

Check PostgreSQL logs for cache hit/miss ratios:
```
pg_outline: Outline manager shutdown (hits=1234, misses=56)
```

## Troubleshooting

### Outline Not Matching

1. Enable debug logging:
   ```sql
   ALTER SYSTEM SET pg_outline.debug_log = on;
   SELECT pg_reload_conf();
   ```

2. Check PostgreSQL logs for matching attempts

3. Verify signature generation:
   ```sql
   SELECT signature, sql_id FROM pg_outline WHERE outline_name = 'your_outline';
   ```

### Hints Not Applied

1. Verify outline is enabled:
   ```sql
   SELECT enabled FROM pg_outline WHERE outline_name = 'your_outline';
   ```

2. Check hint syntax in outline_content

3. Ensure pg_outline.enabled = on in configuration

### Performance Issues

1. Increase cache size:
   ```sql
   ALTER SYSTEM SET pg_outline.cache_size = 5000;
   ```

2. Reduce reload interval:
   ```sql
   ALTER SYSTEM SET pg_outline.reload_interval = 300;
   ```

3. Monitor cache hit ratio in logs

## Integration with pg_hint_plan

pg_outline can integrate with the pg_hint_plan extension for comprehensive hint support:

1. Install pg_hint_plan:
   ```bash
   # Download and install pg_hint_plan
   git clone https://github.com/ossc-db/pg_hint_plan.git
   cd pg_hint_plan
   make
   sudo make install
   ```

2. Load both extensions:
   ```sql
   CREATE EXTENSION pg_hint_plan;
   CREATE EXTENSION pg_outline;
   ```

3. pg_outline will automatically detect and use pg_hint_plan's hint system

## Limitations

- SQL normalization is simplified and may not handle all complex query patterns
- Some PostgreSQL-specific syntax may not be normalized correctly
- Hint support depends on PostgreSQL's planner capabilities
- Cross-database outline migration requires careful validation

## Future Enhancements

- [ ] Enhanced SQL normalization for complex queries
- [ ] Automatic outline generation from plan cache
- [ ] Concurrent execution limits per outline
- [ ] Plan comparison and validation tools
- [ ] Web-based management interface
- [ ] Outline recommendation system based on workload analysis

## Contributing

Contributions are welcome! Please:
1. Fork the repository
2. Create a feature branch
3. Add tests for new functionality
4. Submit a pull request

## License

This plugin is based on concepts from OceanBase's outline feature and is provided under the same license terms as PostgreSQL.

## Support

For issues, questions, or contributions:
- GitHub Issues: [your-repo]/issues
- Documentation: [your-repo]/wiki
- Email: support@example.com

## References

- [PostgreSQL Extension Documentation](https://www.postgresql.org/docs/current/extend-extensions.html)
- [OceanBase Outline Feature](https://www.oceanbase.com/docs/outline)
- [pg_hint_plan](https://github.com/ossc-db/pg_hint_plan)
- [PostgreSQL Planner Hooks](https://www.postgresql.org/docs/current/planner-optimizer.html)
