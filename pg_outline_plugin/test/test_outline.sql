-- PostgreSQL Outline Plugin - Test Examples
-- This file contains test cases for the pg_outline extension

-- Enable the extension
CREATE EXTENSION IF NOT EXISTS pg_outline;

-- Set configuration
SET pg_outline.enabled = on;
SET pg_outline.debug_log = on;

-- Create test tables
CREATE TABLE test_users (
    id INTEGER PRIMARY KEY,
    name VARCHAR(100),
    email VARCHAR(100),
    status VARCHAR(20),
    created_at TIMESTAMP
);

CREATE INDEX idx_users_status ON test_users(status);
CREATE INDEX idx_users_email ON test_users(email);

-- Insert test data
INSERT INTO test_users SELECT
    generate_series(1, 1000),
    'User ' || generate_series(1, 1000),
    'user' || generate_series(1, 1000) || '@example.com',
    CASE WHEN random() < 0.5 THEN 'active' ELSE 'inactive' END,
    NOW() - (random() * 365 || ' days')::INTERVAL;

-- Test 1: Basic outline creation
SELECT pg_outline_create(
    'test_force_seqscan',
    'SELECT * FROM test_users WHERE status = ''active''',
    '/*+ SeqScan(test_users) */'
);

-- Test 2: Verify outline was created
SELECT * FROM pg_outline_info WHERE outline_name = 'test_force_seqscan';

-- Test 3: Run query that should match outline
EXPLAIN SELECT * FROM test_users WHERE status = 'active';

-- Test 4: Run query with different parameter (should still match)
EXPLAIN SELECT * FROM test_users WHERE status = 'inactive';

-- Test 5: Modify outline
SELECT pg_outline_alter(
    'test_force_seqscan',
    '/*+ IndexScan(test_users idx_users_status) */'
);

-- Test 6: Verify outline was modified
SELECT outline_content FROM pg_outline WHERE outline_name = 'test_force_seqscan';

-- Test 7: Run query again to verify new hint is applied
EXPLAIN SELECT * FROM test_users WHERE status = 'active';

-- Test 8: Create outline for join query
CREATE TABLE test_orders (
    id INTEGER PRIMARY KEY,
    user_id INTEGER,
    amount NUMERIC(10,2),
    order_date TIMESTAMP
);

INSERT INTO test_orders SELECT
    generate_series(1, 5000),
    (random() * 999 + 1)::INTEGER,
    (random() * 1000)::NUMERIC(10,2),
    NOW() - (random() * 180 || ' days')::INTERVAL;

SELECT pg_outline_create(
    'test_join_order',
    'SELECT * FROM test_users u JOIN test_orders o ON u.id = o.user_id WHERE u.status = ''active''',
    '/*+ Leading(u o) NestLoop(u o) */'
);

-- Test 9: Check join query plan
EXPLAIN SELECT * FROM test_users u
JOIN test_orders o ON u.id = o.user_id
WHERE u.status = 'active';

-- Test 10: Disable outline
SELECT pg_outline_enable('test_force_seqscan', false);

-- Test 11: Verify query no longer uses outline
EXPLAIN SELECT * FROM test_users WHERE status = 'active';

-- Test 12: Re-enable outline
SELECT pg_outline_enable('test_force_seqscan', true);

-- Test 13: List all outlines
SELECT * FROM pg_outline_list();

-- Test 14: Check usage statistics
SELECT * FROM pg_outline_info ORDER BY usage_count DESC;

-- Test 15: Export outlines
SELECT pg_outline_export();

-- Test 16: Export specific outline
SELECT pg_outline_export('test_force_seqscan');

-- Test 17: Drop an outline
SELECT pg_outline_drop('test_join_order');

-- Test 18: Verify outline was dropped
SELECT * FROM pg_outline WHERE outline_name = 'test_join_order';

-- Test 19: Import outline from JSON
SELECT pg_outline_import('[
    {
        "outline_name": "imported_outline",
        "sql_text": "SELECT COUNT(*) FROM test_users WHERE created_at > ''2024-01-01''",
        "outline_content": "/*+ SeqScan(test_users) */",
        "enabled": true,
        "format_outline": false
    }
]'::json);

-- Test 20: Verify imported outline
SELECT * FROM pg_outline WHERE outline_name = 'imported_outline';

-- Test 21: Test parameterized queries
PREPARE test_query(VARCHAR) AS SELECT * FROM test_users WHERE status = $1;
EXPLAIN EXECUTE test_query('active');
EXECUTE test_query('inactive');

-- Test 22: Complex query outline
SELECT pg_outline_create(
    'test_complex_query',
    'SELECT u.name, COUNT(o.id) FROM test_users u
     LEFT JOIN test_orders o ON u.id = o.user_id
     WHERE u.status = ''active''
     GROUP BY u.name HAVING COUNT(o.id) > 5',
    '/*+ HashJoin(u o) HashAggregate */'
);

-- Test 23: Cache refresh
-- Note: This would call a C function in the real implementation
-- For now, we can manually test by querying after modifications

-- Cleanup test 1: Drop test outlines
SELECT pg_outline_drop('test_force_seqscan');
SELECT pg_outline_drop('test_complex_query');
SELECT pg_outline_drop('imported_outline');

-- Cleanup test 2: Verify all test outlines are dropped
SELECT COUNT(*) FROM pg_outline WHERE outline_name LIKE 'test_%' OR outline_name = 'imported_outline';

-- Summary report
SELECT
    'Total Outlines' as metric,
    COUNT(*)::TEXT as value
FROM pg_outline
UNION ALL
SELECT
    'Enabled Outlines',
    COUNT(*)::TEXT
FROM pg_outline
WHERE enabled = true
UNION ALL
SELECT
    'Total Usage',
    SUM(usage_count)::TEXT
FROM pg_outline_stats;

-- Reset configuration
RESET pg_outline.debug_log;

COMMENT ON EXTENSION pg_outline IS 'Tests completed successfully';
