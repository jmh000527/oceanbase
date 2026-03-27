-- PostgreSQL Outline Plugin - Hint Parser Test
--
-- Tests for multi-block hint parsing with @QB_NAME syntax
--

-- Load extension
CREATE EXTENSION IF NOT EXISTS pg_outline;

-- Enable debug logging
SET pg_outline.enabled = true;
SET pg_outline.debug_log = true;
SET client_min_messages = DEBUG2;

-- =============================================================================
-- Test 1: Single Hint with @QB_NAME
-- =============================================================================
\echo '=== Test 1: Single Hint with @QB_NAME ==='

SELECT pg_outline_create(
    'test_single_qbname',
    $$SELECT * FROM users WHERE id IN (SELECT user_id FROM orders)$$,
    $$/*+ INDEX(@SEL$ABCD1234_1 users idx_users_id) */$$,
    'Single hint with QB_NAME',
    true
);

SELECT * FROM users WHERE id IN (SELECT user_id FROM orders);

-- =============================================================================
-- Test 2: Multiple Hints with Different @QB_NAMEs
-- =============================================================================
\echo '=== Test 2: Multiple Hints with Different QB_NAMEs ==='

SELECT pg_outline_create(
    'test_multiple_qbnames',
    $$SELECT * FROM users WHERE id IN (SELECT user_id FROM orders WHERE amount > 100)$$,
    $$/*+ INDEX(@SEL$11111111_1 users idx_users_id)
         INDEX(@SEL$22222222_2 orders idx_orders_user_amount) */$$,
    'Multiple hints with different QB_NAMEs',
    true
);

SELECT * FROM users WHERE id IN (SELECT user_id FROM orders WHERE amount > 100);

-- =============================================================================
-- Test 3: Global Hint (No @QB_NAME)
-- =============================================================================
\echo '=== Test 3: Global Hint (No QB_NAME) ==='

SELECT pg_outline_create(
    'test_global_hint',
    $$SELECT * FROM users u JOIN orders o ON u.id = o.user_id$$,
    $$/*+ HASHJOIN(u o) */$$,
    'Global hint without QB_NAME',
    true
);

SELECT * FROM users u JOIN orders o ON u.id = o.user_id;

-- =============================================================================
-- Test 4: Mixed Global and Block-Specific Hints
-- =============================================================================
\echo '=== Test 4: Mixed Global and Block-Specific Hints ==='

SELECT pg_outline_create(
    'test_mixed_hints',
    $$SELECT u.name FROM users u WHERE id IN (SELECT user_id FROM orders)$$,
    $$/*+ SEQSCAN(u) INDEX(@SEL$BBBBBBBB_2 orders idx_orders_user) */$$,
    'Mixed global and block-specific hints',
    true
);

SELECT u.name FROM users u WHERE id IN (SELECT user_id FROM orders);

-- =============================================================================
-- Test 5: Hints with Multiple Parameters
-- =============================================================================
\echo '=== Test 5: Hints with Multiple Parameters ==='

SELECT pg_outline_create(
    'test_multi_param',
    $$SELECT * FROM users u JOIN orders o ON u.id = o.user_id
      WHERE o.product_id IN (SELECT id FROM products)$$,
    $$/*+ LEADING(@SEL$AAAAAAAA_1 u o p)
         NESTLOOP(@SEL$AAAAAAAA_1 u o)
         HASHJOIN(@SEL$AAAAAAAA_1 o p)
         INDEX(@SEL$BBBBBBBB_2 products idx_products_id) */$$,
    'Hints with multiple parameters',
    true
);

SELECT * FROM users u JOIN orders o ON u.id = o.user_id
WHERE o.product_id IN (SELECT id FROM products);

-- =============================================================================
-- Test 6: Nested Query with 3 Levels of Hints
-- =============================================================================
\echo '=== Test 6: Three-Level Nested Query ==='

SELECT pg_outline_create(
    'test_three_level',
    $$SELECT * FROM users
      WHERE id IN (
        SELECT user_id FROM orders
        WHERE product_id IN (
          SELECT id FROM products WHERE category = 'electronics'
        )
      )$$,
    $$/*+ INDEX(@SEL$LEVEL001_1 users idx_users_id)
         INDEX(@SEL$LEVEL002_2 orders idx_orders_user_product)
         INDEX(@SEL$LEVEL003_3 products idx_products_category) */$$,
    'Three-level nested query with hints',
    true
);

SELECT * FROM users
WHERE id IN (
  SELECT user_id FROM orders
  WHERE product_id IN (
    SELECT id FROM products WHERE category = 'electronics'
  )
);

-- =============================================================================
-- Test 7: CTE with Block-Specific Hints
-- =============================================================================
\echo '=== Test 7: CTE with Block-Specific Hints ==='

SELECT pg_outline_create(
    'test_cte_hints',
    $$WITH active_users AS (
        SELECT id, name FROM users WHERE status = 'active'
      )
      SELECT * FROM active_users WHERE id > 100$$,
    $$/*+ INDEX(@SEL$CTE00001_2 users idx_users_status)
         SEQSCAN(@SEL$MAIN0001_1 active_users) */$$,
    'CTE with block-specific hints',
    true
);

WITH active_users AS (
  SELECT id, name FROM users WHERE status = 'active'
)
SELECT * FROM active_users WHERE id > 100;

-- =============================================================================
-- Test 8: Complex Hints with Special Characters
-- =============================================================================
\echo '=== Test 8: Complex Hints with Special Characters ==='

SELECT pg_outline_create(
    'test_complex_syntax',
    $$SELECT u.name, o.amount
      FROM users u
      JOIN orders o ON u.id = o.user_id
      WHERE o.amount > (SELECT AVG(amount) FROM orders)$$,
    $$/*+ USE_NL(@SEL$MAIN0001_1 u o)
         INDEX(@SEL$MAIN0001_1 u "idx_users_name")
         INDEX(@SEL$MAIN0001_1 o idx_orders_user_amount)
         FULL(@SEL$SUBQ0002_2 orders) */$$,
    'Complex hints with special characters',
    true
);

SELECT u.name, o.amount
FROM users u
JOIN orders o ON u.id = o.user_id
WHERE o.amount > (SELECT AVG(amount) FROM orders);

-- =============================================================================
-- Test 9: Set Operation Hints (UNION)
-- =============================================================================
\echo '=== Test 9: Set Operation Hints ==='

SELECT pg_outline_create(
    'test_set_op_hints',
    $$SELECT user_id FROM orders WHERE amount > 100
      UNION
      SELECT user_id FROM refunds WHERE amount > 50$$,
    $$/*+ INDEX(@SET$UNION001_1 orders idx_orders_amount)
         INDEX(@SET$UNION002_2 refunds idx_refunds_amount) */$$,
    'Set operation with hints',
    true
);

SELECT user_id FROM orders WHERE amount > 100
UNION
SELECT user_id FROM refunds WHERE amount > 50;

-- =============================================================================
-- Test 10: Very Long Hint String
-- =============================================================================
\echo '=== Test 10: Very Long Hint String ==='

SELECT pg_outline_create(
    'test_long_hints',
    $$SELECT u.name, o.amount, p.name as product
      FROM users u
      JOIN orders o ON u.id = o.user_id
      JOIN products p ON o.product_id = p.id
      WHERE o.amount > (SELECT AVG(amount) FROM orders)$$,
    $$/*+ LEADING(@SEL$MAIN0001_1 u o p)
         USE_NL(@SEL$MAIN0001_1 u o)
         USE_HASH(@SEL$MAIN0001_1 o p)
         INDEX(@SEL$MAIN0001_1 u idx_users_id)
         INDEX(@SEL$MAIN0001_1 o idx_orders_user_product)
         INDEX(@SEL$MAIN0001_1 p idx_products_id)
         FULL(@SEL$SUBQ0002_2 orders)
         PARALLEL(@SEL$MAIN0001_1 4)
         PARALLEL(@SEL$SUBQ0002_2 2) */$$,
    'Very long hint string with multiple hints',
    true
);

SELECT u.name, o.amount, p.name as product
FROM users u
JOIN orders o ON u.id = o.user_id
JOIN products p ON o.product_id = p.id
WHERE o.amount > (SELECT AVG(amount) FROM orders);

-- =============================================================================
-- Test 11: Hint Parsing Edge Cases
-- =============================================================================
\echo '=== Test 11: Hint Parsing Edge Cases ==='

-- Hint with extra whitespace
SELECT pg_outline_create(
    'test_whitespace',
    $$SELECT * FROM users WHERE id > 0$$,
    $$/*+    INDEX(   @SEL$TEST0001_1    users    idx_users_id   )    */$$,
    'Hint with extra whitespace',
    true
);

-- Hint with tabs and newlines
SELECT pg_outline_create(
    'test_newlines',
    $$SELECT * FROM orders WHERE amount > 0$$,
    $$/*+
         INDEX(@SEL$TEST0002_1 orders idx_orders_amount)
         FULL(@SEL$TEST0003_2 products)
       */$$,
    'Hint with newlines',
    true
);

-- =============================================================================
-- Test 12: Invalid Hint Syntax (Should Handle Gracefully)
-- =============================================================================
\echo '=== Test 12: Invalid Hint Syntax ==='

-- Missing closing parenthesis (should handle gracefully)
SELECT pg_outline_create(
    'test_invalid_1',
    $$SELECT * FROM users WHERE id > 0$$,
    $$/*+ INDEX(@SEL$TEST0001_1 users idx_users_id */$$,
    'Invalid hint - missing paren',
    true
);

-- Malformed QB_NAME (should handle gracefully)
SELECT pg_outline_create(
    'test_invalid_2',
    $$SELECT * FROM users WHERE id > 0$$,
    $$/*+ INDEX(@ users idx_users_id) */$$,
    'Invalid hint - malformed QB_NAME',
    true
);

-- =============================================================================
-- Validation: Check Hint Parsing Results
-- =============================================================================
\echo '=== Validation: Check Created Outlines ==='

SELECT outline_name,
       LENGTH(outline_content) as hint_length,
       enabled
FROM pg_outline
WHERE outline_name LIKE 'test_%'
ORDER BY outline_name;

-- =============================================================================
-- Cleanup
-- =============================================================================
\echo '=== Cleanup ==='

SELECT pg_outline_drop(outline_name)
FROM pg_outline
WHERE outline_name LIKE 'test_%';

\echo '===================================='
\echo 'Hint parser tests completed!'
\echo 'Review the DEBUG2 logs to verify hint parsing'
\echo '===================================='
