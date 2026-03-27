-- PostgreSQL Outline Plugin - Multi-Query Block Test Suite
--
-- Tests for complex queries with multiple query blocks and @QB_NAME hints
--

-- Load extension
CREATE EXTENSION IF NOT EXISTS pg_outline;

-- Enable debug logging for testing
SET pg_outline.enabled = true;
SET pg_outline.debug_log = true;

-- =============================================================================
-- Test 1: Simple Subquery in WHERE Clause
-- =============================================================================
\echo '=== Test 1: Simple Subquery in WHERE ==='

-- Create test outline for query with subquery in WHERE
SELECT pg_outline_create(
    'test_where_subquery',
    $$SELECT * FROM users WHERE id IN (SELECT user_id FROM orders WHERE amount > 100)$$,
    $$/*+ INDEX(@SEL$ABCD1234_1 users idx_users_id) FULL(@SEL$ABCD1234_2 orders) */$$,
    'Test outline for WHERE subquery',
    true
);

-- Execute the query (should match and apply hints)
SELECT * FROM users WHERE id IN (SELECT user_id FROM orders WHERE amount > 100);

-- =============================================================================
-- Test 2: Subquery in FROM Clause (Derived Table)
-- =============================================================================
\echo '=== Test 2: Subquery in FROM Clause ==='

-- Create outline for derived table query
SELECT pg_outline_create(
    'test_from_subquery',
    $$SELECT u.name, o.total
      FROM users u
      JOIN (SELECT user_id, SUM(amount) as total FROM orders GROUP BY user_id) o
      ON u.id = o.user_id$$,
    $$/*+ HASHJOIN(@SEL$12345678_1 u o) INDEX(@SEL$87654321_2 orders idx_orders_user) */$$,
    'Test outline for FROM subquery',
    true
);

-- Execute the query
SELECT u.name, o.total
FROM users u
JOIN (SELECT user_id, SUM(amount) as total FROM orders GROUP BY user_id) o
ON u.id = o.user_id;

-- =============================================================================
-- Test 3: Subquery in SELECT List
-- =============================================================================
\echo '=== Test 3: Subquery in SELECT List ==='

-- Create outline for scalar subquery in SELECT
SELECT pg_outline_create(
    'test_select_subquery',
    $$SELECT u.name,
             (SELECT COUNT(*) FROM orders WHERE user_id = u.id) as order_count
      FROM users u$$,
    $$/*+ SEQSCAN(@SEL$AAAA1111_1 u) INDEX(@SEL$BBBB2222_2 orders idx_orders_user) */$$,
    'Test outline for SELECT subquery',
    true
);

-- Execute the query
SELECT u.name,
       (SELECT COUNT(*) FROM orders WHERE user_id = u.id) as order_count
FROM users u;

-- =============================================================================
-- Test 4: EXISTS Subquery
-- =============================================================================
\echo '=== Test 4: EXISTS Subquery ==='

-- Create outline for EXISTS query
SELECT pg_outline_create(
    'test_exists_subquery',
    $$SELECT * FROM users u
      WHERE EXISTS (SELECT 1 FROM orders o WHERE o.user_id = u.id AND o.status = 'completed')$$,
    $$/*+ NESTLOOP(@SEL$CCCC3333_1 u o) INDEX(@SEL$DDDD4444_2 orders idx_orders_user_status) */$$,
    'Test outline for EXISTS subquery',
    true
);

-- Execute the query
SELECT * FROM users u
WHERE EXISTS (SELECT 1 FROM orders o WHERE o.user_id = u.id AND o.status = 'completed');

-- =============================================================================
-- Test 5: NOT EXISTS Subquery
-- =============================================================================
\echo '=== Test 5: NOT EXISTS Subquery ==='

-- Create outline for NOT EXISTS query
SELECT pg_outline_create(
    'test_not_exists',
    $$SELECT * FROM users u
      WHERE NOT EXISTS (SELECT 1 FROM orders o WHERE o.user_id = u.id)$$,
    $$/*+ HASHJOIN(@SEL$EEEE5555_1 u o) FULL(@SEL$FFFF6666_2 orders) */$$,
    'Test outline for NOT EXISTS subquery',
    true
);

-- Execute the query
SELECT * FROM users u
WHERE NOT EXISTS (SELECT 1 FROM orders o WHERE o.user_id = u.id);

-- =============================================================================
-- Test 6: Nested Subqueries (Multi-Level)
-- =============================================================================
\echo '=== Test 6: Nested Subqueries ==='

-- Create outline for nested subqueries
SELECT pg_outline_create(
    'test_nested_subquery',
    $$SELECT * FROM users
      WHERE id IN (
        SELECT user_id FROM orders
        WHERE product_id IN (
          SELECT id FROM products WHERE category = 'electronics'
        )
      )$$,
    $$/*+ INDEX(@SEL$1111AAAA_1 users idx_users_id)
         INDEX(@SEL$2222BBBB_2 orders idx_orders_user_product)
         INDEX(@SEL$3333CCCC_3 products idx_products_category) */$$,
    'Test outline for nested subqueries',
    true
);

-- Execute the query
SELECT * FROM users
WHERE id IN (
  SELECT user_id FROM orders
  WHERE product_id IN (
    SELECT id FROM products WHERE category = 'electronics'
  )
);

-- =============================================================================
-- Test 7: Common Table Expression (CTE)
-- =============================================================================
\echo '=== Test 7: Common Table Expression (CTE) ==='

-- Create outline for CTE query
SELECT pg_outline_create(
    'test_cte',
    $$WITH recent_orders AS (
        SELECT user_id, SUM(amount) as total
        FROM orders
        WHERE created_at > CURRENT_DATE - INTERVAL '30 days'
        GROUP BY user_id
      )
      SELECT u.name, ro.total
      FROM users u
      JOIN recent_orders ro ON u.id = ro.user_id$$,
    $$/*+ INDEX(@SEL$AAAA7777_2 orders idx_orders_created)
         HASHJOIN(@SEL$BBBB8888_1 u ro) */$$,
    'Test outline for CTE',
    true
);

-- Execute the query
WITH recent_orders AS (
  SELECT user_id, SUM(amount) as total
  FROM orders
  WHERE created_at > CURRENT_DATE - INTERVAL '30 days'
  GROUP BY user_id
)
SELECT u.name, ro.total
FROM users u
JOIN recent_orders ro ON u.id = ro.user_id;

-- =============================================================================
-- Test 8: Multiple CTEs
-- =============================================================================
\echo '=== Test 8: Multiple CTEs ==='

-- Create outline for multiple CTEs
SELECT pg_outline_create(
    'test_multiple_ctes',
    $$WITH
      active_users AS (
        SELECT id, name FROM users WHERE status = 'active'
      ),
      high_value_orders AS (
        SELECT user_id, COUNT(*) as order_count
        FROM orders
        WHERE amount > 1000
        GROUP BY user_id
      )
      SELECT au.name, hvo.order_count
      FROM active_users au
      JOIN high_value_orders hvo ON au.id = hvo.user_id$$,
    $$/*+ INDEX(@SEL$1234ABCD_2 users idx_users_status)
         INDEX(@SEL$5678EFGH_3 orders idx_orders_amount)
         HASHJOIN(@SEL$9012IJKL_1 au hvo) */$$,
    'Test outline for multiple CTEs',
    true
);

-- Execute the query
WITH
  active_users AS (
    SELECT id, name FROM users WHERE status = 'active'
  ),
  high_value_orders AS (
    SELECT user_id, COUNT(*) as order_count
    FROM orders
    WHERE amount > 1000
    GROUP BY user_id
  )
SELECT au.name, hvo.order_count
FROM active_users au
JOIN high_value_orders hvo ON au.id = hvo.user_id;

-- =============================================================================
-- Test 9: UNION Query
-- =============================================================================
\echo '=== Test 9: UNION Query ==='

-- Create outline for UNION query
SELECT pg_outline_create(
    'test_union',
    $$SELECT user_id, 'order' as type FROM orders WHERE amount > 100
      UNION
      SELECT user_id, 'refund' as type FROM refunds WHERE amount > 50$$,
    $$/*+ INDEX(@SET$AAAA1111_1 orders idx_orders_amount)
         INDEX(@SET$BBBB2222_2 refunds idx_refunds_amount) */$$,
    'Test outline for UNION',
    true
);

-- Execute the query
SELECT user_id, 'order' as type FROM orders WHERE amount > 100
UNION
SELECT user_id, 'refund' as type FROM refunds WHERE amount > 50;

-- =============================================================================
-- Test 10: Complex Multi-Level Query with Multiple Subqueries
-- =============================================================================
\echo '=== Test 10: Complex Multi-Level Query ==='

-- Create outline for complex multi-level query
SELECT pg_outline_create(
    'test_complex_multilevel',
    $$SELECT u.name,
             (SELECT COUNT(*) FROM orders WHERE user_id = u.id) as order_count,
             (SELECT MAX(amount) FROM orders WHERE user_id = u.id) as max_amount
      FROM users u
      WHERE u.id IN (
        SELECT DISTINCT user_id
        FROM orders
        WHERE status = 'completed'
          AND product_id IN (
            SELECT id FROM products WHERE category = 'premium'
          )
      )
      ORDER BY order_count DESC$$,
    $$/*+ INDEX(@SEL$MAIN0001_1 u idx_users_id)
         INDEX(@SEL$SUB10002_2 orders idx_orders_user)
         INDEX(@SEL$SUB20003_3 orders idx_orders_user)
         INDEX(@SEL$SUB30004_4 orders idx_orders_status_product)
         INDEX(@SEL$SUB40005_5 products idx_products_category) */$$,
    'Test outline for complex multi-level query',
    true
);

-- Execute the query
SELECT u.name,
       (SELECT COUNT(*) FROM orders WHERE user_id = u.id) as order_count,
       (SELECT MAX(amount) FROM orders WHERE user_id = u.id) as max_amount
FROM users u
WHERE u.id IN (
  SELECT DISTINCT user_id
  FROM orders
  WHERE status = 'completed'
    AND product_id IN (
      SELECT id FROM products WHERE category = 'premium'
    )
)
ORDER BY order_count DESC;

-- =============================================================================
-- Test 11: Global Hints (No @QB_NAME)
-- =============================================================================
\echo '=== Test 11: Global Hints ==='

-- Create outline with global hints (no @QB_NAME targeting)
SELECT pg_outline_create(
    'test_global_hints',
    $$SELECT * FROM users u JOIN orders o ON u.id = o.user_id$$,
    $$/*+ LEADING(u o) HASHJOIN(u o) */$$,
    'Test outline with global hints',
    true
);

-- Execute the query
SELECT * FROM users u JOIN orders o ON u.id = o.user_id;

-- =============================================================================
-- Test 12: Mixed Global and Block-Specific Hints
-- =============================================================================
\echo '=== Test 12: Mixed Global and Block-Specific Hints ==='

-- Create outline with both global and block-specific hints
SELECT pg_outline_create(
    'test_mixed_hints',
    $$SELECT u.name,
             (SELECT COUNT(*) FROM orders WHERE user_id = u.id) as order_count
      FROM users u
      WHERE u.status = 'active'$$,
    $$/*+ SEQSCAN(u)
         INDEX(@SEL$NESTED01_2 orders idx_orders_user)
         SET(enable_hashjoin off) */$$,
    'Test outline with mixed hints',
    true
);

-- Execute the query
SELECT u.name,
       (SELECT COUNT(*) FROM orders WHERE user_id = u.id) as order_count
FROM users u
WHERE u.status = 'active';

-- =============================================================================
-- Validation Tests
-- =============================================================================
\echo '=== Validation Tests ==='

-- List all created outlines
SELECT outline_name, enabled, format_outline
FROM pg_outline
WHERE outline_name LIKE 'test_%'
ORDER BY outline_name;

-- Check outline usage statistics
SELECT outline_name, used_count, last_used
FROM pg_outline
WHERE outline_name LIKE 'test_%' AND used_count > 0
ORDER BY used_count DESC;

-- =============================================================================
-- Cleanup
-- =============================================================================
\echo '=== Cleanup ==='

-- Drop test outlines
SELECT pg_outline_drop('test_where_subquery');
SELECT pg_outline_drop('test_from_subquery');
SELECT pg_outline_drop('test_select_subquery');
SELECT pg_outline_drop('test_exists_subquery');
SELECT pg_outline_drop('test_not_exists');
SELECT pg_outline_drop('test_nested_subquery');
SELECT pg_outline_drop('test_cte');
SELECT pg_outline_drop('test_multiple_ctes');
SELECT pg_outline_drop('test_union');
SELECT pg_outline_drop('test_complex_multilevel');
SELECT pg_outline_drop('test_global_hints');
SELECT pg_outline_drop('test_mixed_hints');

\echo 'Multi-block tests completed!'
