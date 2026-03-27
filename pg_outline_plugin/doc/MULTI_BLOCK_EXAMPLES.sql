/*-------------------------------------------------------------------------
 *
 * 多查询块 Outline 功能演示
 *
 * 本文件展示如何处理包含多个 SELECT 的复杂 SQL 查询，
 * 并为每个查询块应用不同的 hints。
 *
 *-------------------------------------------------------------------------
 */

-- ============================================================================
-- 示例 1: 包含子查询的 SELECT
-- ============================================================================

-- 原始查询（没有 outline）
SELECT u.user_id, u.username,
       (SELECT COUNT(*) FROM orders o WHERE o.user_id = u.user_id) as order_count
FROM users u
WHERE u.status = 'active'
  AND u.created_at > '2024-01-01';

-- 查询块识别结果：
-- Block 0 (SEL$AABBCCDD): 主查询
-- Block 1 (SEL$11223344): SELECT COUNT 子查询

-- 创建 outline，为不同查询块指定不同的 hints
SELECT pg_outline_create(
    'user_order_count_outline',
    'SELECT u.user_id, u.username,
           (SELECT COUNT(*) FROM orders o WHERE o.user_id = u.user_id) as order_count
     FROM users u
     WHERE u.status = ''active''
       AND u.created_at > ''2024-01-01''',
    '/*+
        INDEX(@SEL$AABBCCDD u idx_users_status_created)  -- 主查询使用复合索引
        INDEX(@SEL$11223344 o idx_orders_user_id)        -- 子查询使用用户索引
        ROWS(@SEL$11223344 #1)                           -- 预估子查询返回1行
    */'
);


-- ============================================================================
-- 示例 2: 包含 EXISTS 子查询
-- ============================================================================

-- 原始查询
SELECT p.product_id, p.product_name, p.price
FROM products p
WHERE p.category = 'electronics'
  AND EXISTS (
    SELECT 1
    FROM inventory i
    WHERE i.product_id = p.product_id
      AND i.quantity > 0
  )
  AND NOT EXISTS (
    SELECT 1
    FROM discontinued d
    WHERE d.product_id = p.product_id
  );

-- 查询块识别：
-- Block 0 (SEL$MAIN0001): 主查询
-- Block 1 (SEL$EXISTS01): 第一个 EXISTS 子查询
-- Block 2 (SEL$EXISTS02): 第二个 EXISTS 子查询

SELECT pg_outline_create(
    'available_products_outline',
    'SELECT p.product_id, p.product_name, p.price
     FROM products p
     WHERE p.category = ''electronics''
       AND EXISTS (
         SELECT 1 FROM inventory i
         WHERE i.product_id = p.product_id AND i.quantity > 0
       )
       AND NOT EXISTS (
         SELECT 1 FROM discontinued d WHERE d.product_id = p.product_id
       )',
    '/*+
        INDEX(@SEL$MAIN0001 p idx_products_category)    -- 主查询按类别索引
        INDEX(@SEL$EXISTS01 i idx_inventory_product)    -- 库存查询
        INDEX(@SEL$EXISTS02 d idx_discontinued_product) -- 停产查询
        SEMI_TO_INNER(@SEL$MAIN0001)                    -- EXISTS 转 INNER JOIN
    */'
);


-- ============================================================================
-- 示例 3: 包含 IN 子查询
-- ============================================================================

-- 原始查询
SELECT c.customer_id, c.customer_name
FROM customers c
WHERE c.customer_id IN (
    SELECT o.customer_id
    FROM orders o
    WHERE o.order_date >= '2024-01-01'
      AND o.total_amount > 1000
);

-- 查询块识别：
-- Block 0 (SEL$OUTER000): 主查询
-- Block 1 (SEL$INNER001): IN 子查询

SELECT pg_outline_create(
    'high_value_customers_outline',
    'SELECT c.customer_id, c.customer_name
     FROM customers c
     WHERE c.customer_id IN (
       SELECT o.customer_id
       FROM orders o
       WHERE o.order_date >= ''2024-01-01''
         AND o.total_amount > 1000
     )',
    '/*+
        HASH_SJ(@SEL$OUTER000)                          -- 主查询使用 Hash Semi-Join
        INDEX(@SEL$INNER001 o idx_orders_date_amount)   -- 子查询复合索引
        LEADING(@SEL$OUTER000 o c)                      -- 先访问 orders
    */'
);


-- ============================================================================
-- 示例 4: 多层嵌套子查询
-- ============================================================================

-- 原始查询
SELECT d.department_id, d.department_name,
       (SELECT AVG(e.salary)
        FROM employees e
        WHERE e.department_id = d.department_id
          AND e.hire_date > (
            SELECT MIN(hire_date)
            FROM employees
            WHERE department_id = e.department_id
          )
       ) as avg_new_salary
FROM departments d
WHERE d.active = true;

-- 查询块识别：
-- Block 0 (SEL$L1_0001): 最外层查询
-- Block 1 (SEL$L2_0001): SELECT AVG 子查询
-- Block 2 (SEL$L3_0001): SELECT MIN 子查询（最深层）

SELECT pg_outline_create(
    'dept_avg_salary_outline',
    'SELECT d.department_id, d.department_name,
           (SELECT AVG(e.salary)
            FROM employees e
            WHERE e.department_id = d.department_id
              AND e.hire_date > (
                SELECT MIN(hire_date)
                FROM employees
                WHERE department_id = e.department_id
              )
           ) as avg_new_salary
     FROM departments d
     WHERE d.active = true',
    '/*+
        FULL(@SEL$L1_0001 d)                             -- 外层全表扫描
        INDEX(@SEL$L2_0001 e idx_emp_dept_hire)          -- 中层复合索引
        INDEX(@SEL$L3_0001 employees idx_emp_dept_hire)  -- 内层索引
        NO_MERGE(@SEL$L2_0001)                           -- 不合并子查询
        MATERIALIZE(@SEL$L2_0001)                        -- 物化子查询结果
    */'
);


-- ============================================================================
-- 示例 5: WITH (CTE) 子句
-- ============================================================================

-- 原始查询
WITH recent_orders AS (
    SELECT customer_id, SUM(total_amount) as total
    FROM orders
    WHERE order_date >= '2024-01-01'
    GROUP BY customer_id
),
vip_customers AS (
    SELECT customer_id
    FROM recent_orders
    WHERE total > 10000
)
SELECT c.customer_id, c.customer_name, r.total
FROM customers c
JOIN vip_customers v ON c.customer_id = v.customer_id
JOIN recent_orders r ON c.customer_id = r.customer_id;

-- 查询块识别：
-- Block 0 (SEL$MAIN_CTE): 主查询
-- Block 1 (SEL$CTE_0001): recent_orders CTE
-- Block 2 (SEL$CTE_0002): vip_customers CTE

SELECT pg_outline_create(
    'vip_customer_summary_outline',
    'WITH recent_orders AS (
       SELECT customer_id, SUM(total_amount) as total
       FROM orders WHERE order_date >= ''2024-01-01''
       GROUP BY customer_id
     ),
     vip_customers AS (
       SELECT customer_id FROM recent_orders WHERE total > 10000
     )
     SELECT c.customer_id, c.customer_name, r.total
     FROM customers c
     JOIN vip_customers v ON c.customer_id = v.customer_id
     JOIN recent_orders r ON c.customer_id = r.customer_id',
    '/*+
        HASH_AGG(@SEL$CTE_0001)                          -- CTE1 使用 Hash 聚合
        INDEX(@SEL$CTE_0001 orders idx_orders_date)      -- CTE1 索引
        MATERIALIZE(@SEL$CTE_0001)                       -- 物化 CTE1
        MATERIALIZE(@SEL$CTE_0002)                       -- 物化 CTE2
        LEADING(@SEL$MAIN_CTE c v r)                     -- 主查询 JOIN 顺序
        USE_HASH(@SEL$MAIN_CTE c v r)                    -- 使用 Hash Join
    */'
);


-- ============================================================================
-- 示例 6: UNION 查询
-- ============================================================================

-- 原始查询
SELECT product_id, product_name, 'in_stock' as status
FROM products
WHERE quantity > 0
UNION ALL
SELECT product_id, product_name, 'out_of_stock' as status
FROM products
WHERE quantity = 0
UNION ALL
SELECT product_id, product_name, 'discontinued' as status
FROM discontinued_products;

-- 查询块识别：
-- Block 0 (SET$UNION001): UNION 操作
-- Block 1 (SEL$UNION_01): 第一个 SELECT
-- Block 2 (SEL$UNION_02): 第二个 SELECT
-- Block 3 (SEL$UNION_03): 第三个 SELECT

SELECT pg_outline_create(
    'product_status_union_outline',
    'SELECT product_id, product_name, ''in_stock'' as status
     FROM products WHERE quantity > 0
     UNION ALL
     SELECT product_id, product_name, ''out_of_stock'' as status
     FROM products WHERE quantity = 0
     UNION ALL
     SELECT product_id, product_name, ''discontinued'' as status
     FROM discontinued_products',
    '/*+
        INDEX(@SEL$UNION_01 products idx_products_quantity)  -- 第1分支索引
        INDEX(@SEL$UNION_02 products idx_products_quantity)  -- 第2分支索引
        FULL(@SEL$UNION_03 discontinued_products)            -- 第3分支全表
        PARALLEL(@SET$UNION001 4)                            -- UNION 并行度4
    */'
);


-- ============================================================================
-- 示例 7: 复杂的 JOIN 与子查询组合
-- ============================================================================

-- 原始查询
SELECT
    o.order_id,
    c.customer_name,
    (SELECT SUM(oi.quantity * oi.price)
     FROM order_items oi
     WHERE oi.order_id = o.order_id) as order_total,
    (SELECT COUNT(*)
     FROM shipments s
     WHERE s.order_id = o.order_id
       AND s.status = 'delivered') as delivered_items
FROM orders o
JOIN customers c ON o.customer_id = c.customer_id
WHERE o.order_date BETWEEN '2024-01-01' AND '2024-12-31'
  AND o.customer_id IN (
    SELECT customer_id
    FROM customer_segments
    WHERE segment = 'premium'
  );

-- 查询块识别：
-- Block 0 (SEL$MAIN0000): 主查询
-- Block 1 (SEL$SUB_0001): SUM 子查询
-- Block 2 (SEL$SUB_0002): COUNT 子查询
-- Block 3 (SEL$IN_00001): IN 子查询

SELECT pg_outline_create(
    'order_summary_complex_outline',
    'SELECT
       o.order_id, c.customer_name,
       (SELECT SUM(oi.quantity * oi.price)
        FROM order_items oi WHERE oi.order_id = o.order_id) as order_total,
       (SELECT COUNT(*) FROM shipments s
        WHERE s.order_id = o.order_id AND s.status = ''delivered'') as delivered_items
     FROM orders o
     JOIN customers c ON o.customer_id = c.customer_id
     WHERE o.order_date BETWEEN ''2024-01-01'' AND ''2024-12-31''
       AND o.customer_id IN (
         SELECT customer_id FROM customer_segments WHERE segment = ''premium''
       )',
    '/*+
        LEADING(@SEL$MAIN0000 o c)                        -- 先访问 orders
        USE_NL(@SEL$MAIN0000 c)                           -- 使用 Nested Loop Join
        INDEX(@SEL$MAIN0000 o idx_orders_date_customer)   -- 主查询复合索引
        INDEX(@SEL$SUB_0001 oi idx_order_items_order)     -- 子查询1索引
        INDEX(@SEL$SUB_0002 s idx_shipments_order_status) -- 子查询2复合索引
        HASH_SJ(@SEL$IN_00001)                            -- IN 子查询转 Hash Semi-Join
        INDEX(@SEL$IN_00001 customer_segments idx_segment) -- IN 子查询索引
    */'
);


-- ============================================================================
-- 查询 Outline 信息
-- ============================================================================

-- 查看所有包含多查询块的 outlines
SELECT
    outline_name,
    sql_id,
    enabled,
    usage_count,
    LENGTH(outline_content) as hint_length,
    CASE
        WHEN outline_content LIKE '%@SEL$%' THEN 'Multi-Block'
        ELSE 'Single-Block'
    END as complexity
FROM pg_outline_info
WHERE outline_content LIKE '%@SEL$%'
ORDER BY usage_count DESC;

-- 查看特定 outline 的详细信息
SELECT
    outline_name,
    sql_text,
    outline_content
FROM pg_outline
WHERE outline_name = 'order_summary_complex_outline';


-- ============================================================================
-- 测试验证
-- ============================================================================

-- 启用 debug 日志查看查询块识别过程
SET pg_outline.debug_log = on;

-- 执行查询，验证 outline 匹配和应用
EXPLAIN (VERBOSE, COSTS, BUFFERS)
SELECT o.order_id, c.customer_name
FROM orders o
JOIN customers c ON o.customer_id = c.customer_id
WHERE o.customer_id IN (
    SELECT customer_id
    FROM customer_segments
    WHERE segment = 'premium'
);

-- 查看匹配的 outline 和应用的 hints
SELECT * FROM pg_outline_info
WHERE outline_name = 'order_summary_complex_outline';
