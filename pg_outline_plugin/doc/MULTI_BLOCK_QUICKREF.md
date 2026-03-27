# PostgreSQL Outline Plugin - Multi-Query Block Quick Reference

## QB_NAME Format

```
PREFIX$HASH_COUNTER

PREFIX:
  SEL$  - SELECT statement
  INS$  - INSERT statement
  UPD$  - UPDATE statement
  DEL$  - DELETE statement
  SET$  - Set operation (UNION, INTERSECT, EXCEPT)

HASH:  8-character hex from MD5 of query source
COUNTER: Sequential number starting from 0
```

**Examples:**
- `SEL$ABCD1234_0` - Main SELECT query
- `SEL$EFGH5678_1` - First subquery in SELECT
- `INS$1234ABCD_0` - INSERT statement
- `SET$UNION001_1` - First branch of UNION

## Common Query Patterns

### 1. WHERE Subquery
```sql
SELECT * FROM users
WHERE id IN (SELECT user_id FROM orders);

-- Hints:
/*+ INDEX(@SEL$MAIN_1 users idx)
    FULL(@SEL$SUB_2 orders) */
```

### 2. FROM Subquery (Derived Table)
```sql
SELECT u.name, o.total
FROM users u
JOIN (SELECT user_id, SUM(amt) FROM orders GROUP BY user_id) o
ON u.id = o.user_id;

-- Hints:
/*+ HASHJOIN(@SEL$MAIN_1 u o)
    INDEX(@SEL$DERIVED_2 orders idx) */
```

### 3. SELECT List Subquery
```sql
SELECT name,
       (SELECT COUNT(*) FROM orders WHERE user_id = users.id)
FROM users;

-- Hints:
/*+ SEQSCAN(@SEL$MAIN_1 users)
    INDEX(@SEL$SUB_2 orders idx_user) */
```

### 4. EXISTS Subquery
```sql
SELECT * FROM users u
WHERE EXISTS (SELECT 1 FROM orders WHERE user_id = u.id);

-- Hints:
/*+ NESTLOOP(@SEL$MAIN_1 u orders)
    INDEX(@SEL$EXISTS_2 orders idx_user) */
```

### 5. Nested Subqueries (3 Levels)
```sql
SELECT * FROM users
WHERE id IN (
  SELECT user_id FROM orders
  WHERE product_id IN (
    SELECT id FROM products WHERE category = 'electronics'
  )
);

-- Hints:
/*+ INDEX(@SEL$L1_1 users pk)
    INDEX(@SEL$L2_2 orders idx_user_product)
    INDEX(@SEL$L3_3 products idx_category) */
```

### 6. CTE (WITH Clause)
```sql
WITH recent_orders AS (
  SELECT user_id, SUM(amount) FROM orders
  WHERE created_at > CURRENT_DATE - 30
  GROUP BY user_id
)
SELECT u.name, ro.total
FROM users u
JOIN recent_orders ro ON u.id = ro.user_id;

-- Hints:
/*+ INDEX(@SEL$CTE_2 orders idx_created)
    HASHJOIN(@SEL$MAIN_1 u ro) */
```

### 7. Multiple CTEs
```sql
WITH
  active_users AS (SELECT id FROM users WHERE status = 'active'),
  recent_orders AS (SELECT user_id FROM orders WHERE created_at > '2024-01-01')
SELECT * FROM active_users au
JOIN recent_orders ro ON au.id = ro.user_id;

-- Hints:
/*+ INDEX(@SEL$CTE1_2 users idx_status)
    INDEX(@SEL$CTE2_3 orders idx_created)
    HASHJOIN(@SEL$MAIN_1 au ro) */
```

### 8. UNION Query
```sql
SELECT user_id FROM orders WHERE amount > 100
UNION
SELECT user_id FROM refunds WHERE amount > 50;

-- Hints:
/*+ INDEX(@SET$UNION_1 orders idx_amount)
    INDEX(@SET$UNION_2 refunds idx_amount) */
```

### 9. Complex Multi-Level
```sql
SELECT u.name,
       (SELECT COUNT(*) FROM orders WHERE user_id = u.id),
       (SELECT MAX(amount) FROM orders WHERE user_id = u.id)
FROM users u
WHERE id IN (
  SELECT user_id FROM orders WHERE status = 'completed'
    AND product_id IN (SELECT id FROM products WHERE category = 'premium')
);

-- Hints:
/*+ INDEX(@SEL$MAIN_1 u pk)
    INDEX(@SEL$SUB1_2 orders idx_user)
    INDEX(@SEL$SUB2_3 orders idx_user)
    INDEX(@SEL$SUB3_4 orders idx_status_product)
    INDEX(@SEL$SUB4_5 products idx_category) */
```

## Global vs Block-Specific Hints

### Global Hints (No @QB_NAME)
Apply to the query as a whole:
```sql
/*+ LEADING(t1 t2 t3) HASHJOIN(t1 t2) */
```

### Block-Specific Hints (With @QB_NAME)
Apply only to specific query blocks:
```sql
/*+ INDEX(@SEL$12345678_1 t1 idx1)
    FULL(@SEL$87654321_2 t2) */
```

### Mixed Hints
Combine both types:
```sql
/*+ SEQSCAN(main_table)
    INDEX(@SEL$SUBQ_2 orders idx_user)
    SET(enable_hashjoin off) */
```

## Best Practices

1. **Identify Query Blocks First**
   - Enable `pg_outline.debug_log = on`
   - Run query to see QB_NAMEs in logs
   - Use identified QB_NAMEs in hints

2. **Start Simple**
   - Begin with global hints
   - Add block-specific hints only when needed
   - Test incrementally

3. **Use Meaningful Prefixes**
   - While QB_NAMEs are auto-generated, document them clearly
   - Add comments to outline descriptions

4. **Monitor Performance**
   - Check `pg_outline` table for usage statistics
   - Review `last_used` and `used_count` columns
   - Adjust hints based on actual performance

5. **Handle Edge Cases**
   - Some queries may have dynamic block counts
   - Test with representative data
   - Verify hints apply correctly

## Debugging Tips

```sql
-- Enable detailed logging
SET pg_outline.debug_log = on;
SET client_min_messages = DEBUG1;

-- Run query to see block identification
SELECT * FROM users WHERE id IN (SELECT user_id FROM orders);

-- Check logs for:
-- "pg_outline: Identified X query blocks"
-- "pg_outline: Block[0]: SEL$xxxxx_0 Type=SELECT Parent=-1 Depth=0"
-- "pg_outline: Block[1]: SEL$xxxxx_1 Type=SELECT Parent=0 Depth=1"
```

## See Also

- [Multi-Block Design Document](MULTI_QUERY_BLOCK_DESIGN.md)
- [Multi-Block Examples](MULTI_BLOCK_EXAMPLES.sql)
- [Multi-Block FAQ](MULTI_BLOCK_FAQ.md)
- [Query Block Concept](QUERY_BLOCK_CONCEPT.txt)
