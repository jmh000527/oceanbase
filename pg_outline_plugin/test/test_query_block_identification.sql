-- PostgreSQL Outline Plugin - Query Block Identification Test
--
-- Tests for query block identification and QB_NAME generation
--

-- Load extension
CREATE EXTENSION IF NOT EXISTS pg_outline;

-- Enable debug logging
SET pg_outline.enabled = true;
SET pg_outline.debug_log = true;
SET client_min_messages = DEBUG1;

-- =============================================================================
-- Test 1: Single SELECT (1 block expected)
-- =============================================================================
\echo '=== Test 1: Single SELECT ==='
\echo 'Expected: 1 query block (SEL$xxxxx_0)'

SELECT * FROM users WHERE status = 'active';

-- =============================================================================
-- Test 2: WHERE Subquery (2 blocks expected)
-- =============================================================================
\echo '=== Test 2: WHERE Subquery ==='
\echo 'Expected: 2 query blocks (main + subquery)'

SELECT * FROM users
WHERE id IN (SELECT user_id FROM orders WHERE amount > 100);

-- =============================================================================
-- Test 3: FROM Subquery (2 blocks expected)
-- =============================================================================
\echo '=== Test 3: FROM Subquery ==='
\echo 'Expected: 2 query blocks (main + derived table)'

SELECT u.name, o.total
FROM users u
JOIN (SELECT user_id, SUM(amount) as total FROM orders GROUP BY user_id) o
ON u.id = o.user_id;

-- =============================================================================
-- Test 4: SELECT List Subquery (2 blocks expected)
-- =============================================================================
\echo '=== Test 4: SELECT List Subquery ==='
\echo 'Expected: 2 query blocks (main + scalar subquery)'

SELECT name,
       (SELECT COUNT(*) FROM orders WHERE user_id = users.id) as order_count
FROM users;

-- =============================================================================
-- Test 5: Multiple Subqueries (4 blocks expected)
-- =============================================================================
\echo '=== Test 5: Multiple Subqueries ==='
\echo 'Expected: 4 query blocks (main + 3 subqueries)'

SELECT name,
       (SELECT COUNT(*) FROM orders WHERE user_id = users.id) as order_count,
       (SELECT MAX(amount) FROM orders WHERE user_id = users.id) as max_amount
FROM users
WHERE id IN (SELECT user_id FROM orders WHERE status = 'completed');

-- =============================================================================
-- Test 6: Nested Subqueries (3 blocks expected)
-- =============================================================================
\echo '=== Test 6: Nested Subqueries ==='
\echo 'Expected: 3 query blocks (main + level 1 + level 2)'

SELECT * FROM users
WHERE id IN (
  SELECT user_id FROM orders
  WHERE product_id IN (
    SELECT id FROM products WHERE category = 'electronics'
  )
);

-- =============================================================================
-- Test 7: EXISTS Subquery (2 blocks expected)
-- =============================================================================
\echo '=== Test 7: EXISTS Subquery ==='
\echo 'Expected: 2 query blocks (main + EXISTS subquery)'

SELECT * FROM users u
WHERE EXISTS (
  SELECT 1 FROM orders o
  WHERE o.user_id = u.id AND o.status = 'completed'
);

-- =============================================================================
-- Test 8: Simple CTE (2 blocks expected)
-- =============================================================================
\echo '=== Test 8: Simple CTE ==='
\echo 'Expected: 2 query blocks (CTE + main)'

WITH active_users AS (
  SELECT id, name FROM users WHERE status = 'active'
)
SELECT * FROM active_users WHERE id > 100;

-- =============================================================================
-- Test 9: Multiple CTEs (3 blocks expected)
-- =============================================================================
\echo '=== Test 9: Multiple CTEs ==='
\echo 'Expected: 3 query blocks (2 CTEs + main)'

WITH
  active_users AS (
    SELECT id, name FROM users WHERE status = 'active'
  ),
  recent_orders AS (
    SELECT user_id, COUNT(*) as order_count FROM orders
    WHERE created_at > CURRENT_DATE - 30
    GROUP BY user_id
  )
SELECT au.name, ro.order_count
FROM active_users au
JOIN recent_orders ro ON au.id = ro.user_id;

-- =============================================================================
-- Test 10: CTE with Subquery (3 blocks expected)
-- =============================================================================
\echo '=== Test 10: CTE with Subquery ==='
\echo 'Expected: 3 query blocks (CTE with subquery + main)'

WITH high_value_users AS (
  SELECT user_id FROM orders
  WHERE amount > (SELECT AVG(amount) FROM orders)
  GROUP BY user_id
)
SELECT * FROM users WHERE id IN (SELECT user_id FROM high_value_users);

-- =============================================================================
-- Test 11: UNION (3 blocks expected)
-- =============================================================================
\echo '=== Test 11: UNION ==='
\echo 'Expected: 3 query blocks (set op + 2 branches)'

SELECT user_id, 'order' as type FROM orders WHERE amount > 100
UNION
SELECT user_id, 'refund' as type FROM refunds WHERE amount > 50;

-- =============================================================================
-- Test 12: Complex Multi-Level (6+ blocks expected)
-- =============================================================================
\echo '=== Test 12: Complex Multi-Level ==='
\echo 'Expected: 6+ query blocks'

WITH top_products AS (
  SELECT id, name FROM products
  WHERE id IN (
    SELECT product_id FROM orders
    WHERE amount > 1000
  )
)
SELECT u.name,
       (SELECT COUNT(*) FROM orders WHERE user_id = u.id) as total_orders,
       (SELECT COUNT(*) FROM orders o
        WHERE o.user_id = u.id
          AND o.product_id IN (SELECT id FROM top_products)) as premium_orders
FROM users u
WHERE EXISTS (
  SELECT 1 FROM orders WHERE user_id = u.id
);

-- =============================================================================
-- Test 13: HAVING with Subquery (2 blocks expected)
-- =============================================================================
\echo '=== Test 13: HAVING with Subquery ==='
\echo 'Expected: 2 query blocks (main + HAVING subquery)'

SELECT user_id, COUNT(*) as order_count
FROM orders
GROUP BY user_id
HAVING COUNT(*) > (SELECT AVG(cnt) FROM (SELECT COUNT(*) as cnt FROM orders GROUP BY user_id) x);

-- =============================================================================
-- Test 14: INSERT with Subquery (2 blocks expected)
-- =============================================================================
\echo '=== Test 14: INSERT with Subquery ==='
\echo 'Expected: 2 query blocks (INS$ + SEL$)'

-- Note: This is just to test query block identification, not actual execution
EXPLAIN INSERT INTO user_stats (user_id, order_count)
SELECT user_id, COUNT(*) FROM orders GROUP BY user_id;

-- =============================================================================
-- Test 15: UPDATE with Subquery (2 blocks expected)
-- =============================================================================
\echo '=== Test 15: UPDATE with Subquery ==='
\echo 'Expected: 2 query blocks (UPD$ + SEL$)'

-- Note: This is just to test query block identification
EXPLAIN UPDATE users
SET status = 'inactive'
WHERE id NOT IN (SELECT DISTINCT user_id FROM orders WHERE created_at > CURRENT_DATE - 365);

-- =============================================================================
-- Test 16: DELETE with Subquery (2 blocks expected)
-- =============================================================================
\echo '=== Test 16: DELETE with Subquery ==='
\echo 'Expected: 2 query blocks (DEL$ + SEL$)'

-- Note: This is just to test query block identification
EXPLAIN DELETE FROM temp_data
WHERE id IN (SELECT id FROM temp_data WHERE created_at < CURRENT_DATE - 90);

\echo '===================================='
\echo 'Query block identification tests completed!'
\echo 'Review the DEBUG1 logs above to verify query block identification'
\echo '===================================='
