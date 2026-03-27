# 多查询块 Hint 支持 - 问题回答

## 您的问题

> "oceanbase对于一个SQL中存在多处可以指定Hint，那么Outline是怎么匹配到这多处可以指定Hint的位置的呢。按照你现在实现的逻辑，不是只能处理只有一个SELECT关键字的SQL吗？"

## 简短回答

**您说得完全正确！**

当前实现确实过于简化，只能处理单个 SELECT 的简单情况。OceanBase 的 outline 功能要复杂得多，它使用 **查询块命名机制（QB_NAME）** 来精确定位复杂 SQL 中的每个查询块（主查询、子查询、CTE、UNION 等），并为每个块单独指定 hints。

## 详细说明

### 1. OceanBase 是如何做的

#### 查询块识别
OceanBase 递归遍历整个查询树，为每个查询块分配唯一的 QB_NAME：

```sql
-- 示例：包含子查询的复杂 SQL
SELECT t1.id, t1.name,
       (SELECT COUNT(*) FROM orders o WHERE o.user_id = t1.id) as cnt
FROM users t1
WHERE t1.id IN (SELECT user_id FROM subscriptions WHERE active = true)
```

**查询块识别结果：**
- `SEL$12345678` - 主查询（外层 SELECT）
- `SEL$87654321` - SELECT COUNT 子查询
- `SEL$ABCDEF00` - IN 子查询

#### Hint 定位
使用 `@QB_NAME` 语法指定每个 hint 的目标：

```sql
/*+
    INDEX(@SEL$12345678 t1 idx_id)       -- 主查询使用索引
    FULL(@SEL$87654321 o)                 -- 子查询1全表扫描
    INDEX(@SEL$ABCDEF00 subscriptions idx_user) -- 子查询2索引
*/
```

#### QB_NAME 生成规则
- **前缀**：`SEL$`（SELECT）、`SET$`（UNION）、`INS$`（INSERT）等
- **后缀**：8位十六进制 MD5 哈希 + 序号避免冲突
- **格式**：`SEL$12345678_1`

### 2. 当前实现的局限

我们的 PostgreSQL 插件只实现了：

```c
// 当前的简化实现
char* NormalizeQueryString(const char *query_string)
{
    // 只是简单地替换常量
    // 没有识别查询块结构
    // 没有处理子查询、CTE、UNION
}
```

**问题：**
- ❌ 不能识别多个查询块
- ❌ 不能为子查询单独指定 hint
- ❌ 不能处理 CTE（WITH 子句）
- ❌ 不能处理 UNION/INTERSECT
- ❌ Hint 只能应用到整个查询

### 3. 需要增强的功能

#### 核心组件

**A. 查询块识别器**
```c
List *IdentifyQueryBlocks(Query *query)
{
    // 递归遍历查询树
    // 识别：主查询、子查询、CTE、UNION
    // 为每个块生成 QB_NAME
}
```

**B. Hint 解析器**
```c
List *ParseMultiBlockHints(const char *hint_content)
{
    // 解析 /*+ ... */ 中的多个 hints
    // 提取每个 hint 的 @QB_NAME
    // 关联 hint 与目标查询块
}
```

**C. Hint 应用器**
```c
void ApplyQueryBlockHints(Query *query, List *blocks, List *hints)
{
    // 遍历所有查询块
    // 找到每个块对应的 hints
    // 分别应用到各个查询块
}
```

### 4. 实现示例

#### 创建支持多查询块的 Outline

```sql
-- 复杂查询
SELECT u.id, u.name,
       (SELECT COUNT(*) FROM orders o WHERE o.user_id = u.id) as order_count
FROM users u
WHERE u.status = 'active'
  AND EXISTS (SELECT 1 FROM subscriptions s WHERE s.user_id = u.id);

-- 创建 outline，指定多个查询块的 hints
SELECT pg_outline_create(
    'complex_user_query',
    'SELECT u.id, u.name, ... (完整 SQL)',
    '/*+
        INDEX(@SEL$MAIN0001 u idx_users_status)    -- 主查询
        INDEX(@SEL$SUB_0001 o idx_orders_user)     -- 子查询1
        INDEX(@SEL$SUB_0002 s idx_subscriptions)   -- 子查询2
        LEADING(@SEL$MAIN0001 u)                   -- 主查询访问顺序
    */'
);
```

### 5. 数据结构设计

#### 查询块信息
```c
typedef struct QueryBlockInfo
{
    int32       block_id;       /* 0, 1, 2... */
    char        qb_name[32];    /* SEL$12345678 */
    int32       parent_id;      /* 父查询块 ID */
    QueryType   query_type;     /* SELECT, INSERT, UPDATE... */
    Query      *query;          /* PostgreSQL Query 节点 */
    List       *child_blocks;   /* 子查询列表 */
} QueryBlockInfo;
```

#### Hint 与查询块关联
```c
typedef struct QueryBlockHint
{
    char        qb_name[32];    /* 目标查询块名 */
    char       *hint_text;      /* Hint 内容 */
    bool        applied;        /* 是否已应用 */
} QueryBlockHint;
```

### 6. 实现难度

| 功能 | 难度 | 说明 |
|------|------|------|
| 单层子查询识别 | ⭐⭐ | 中等 - 需要遍历 RTE |
| QB_NAME 生成 | ⭐⭐ | 中等 - MD5 哈希 |
| Hint 字符串解析 | ⭐ | 简单 - 字符串处理 |
| CTE 处理 | ⭐⭐⭐ | 较难 - 递归结构 |
| UNION 处理 | ⭐⭐⭐ | 较难 - SetOperation 节点 |
| 深度嵌套子查询 | ⭐⭐⭐⭐ | 困难 - 复杂递归 |
| 查询变换后追踪 | ⭐⭐⭐⭐⭐ | 很难 - 与优化器深度集成 |

### 7. 实现路线图

#### 阶段 1：基础支持（推荐先做）
- ✅ 识别 WHERE 子句中的子查询
- ✅ 识别 FROM 子句中的子查询
- ✅ 基本的 QB_NAME 生成
- ✅ 简单的 @QB_NAME 语法支持

#### 阶段 2：增强支持
- 📋 支持 SELECT 列表中的子查询
- 📋 支持 EXISTS/IN 子查询
- 📋 支持相关子查询
- 📋 完整的 Hint 解析

#### 阶段 3：完整支持
- 📋 支持 CTE (WITH 子句)
- 📋 支持 UNION/INTERSECT/EXCEPT
- 📋 支持深度嵌套
- 📋 查询变换后的块追踪

### 8. 参考资料

已创建的文档：
- **`doc/MULTI_QUERY_BLOCK_DESIGN.md`** - 完整设计方案
- **`doc/MULTI_BLOCK_EXAMPLES.sql`** - 7个使用示例
- **`include/outline_query_block.h`** - 数据结构定义

OceanBase 源码参考：
- `src/sql/resolver/dml/ob_sql_hint.cpp` - QB_NAME 生成
- `src/sql/resolver/dml/ob_select_resolver.cpp` - 查询块识别
- `src/sql/optimizer/ob_log_subplan_filter.cpp` - 子查询处理

## 总结

您的观察非常敏锐！**确实，当前实现只是一个基础框架，只能处理最简单的单 SELECT 查询。**

要达到 OceanBase 的水平，需要：

1. **查询块识别** - 递归遍历整个查询树
2. **QB_NAME 机制** - 为每个块生成唯一标识
3. **Hint 定位** - 使用 @QB_NAME 精确指定目标
4. **递归应用** - 分别为每个块应用 hints

我已经创建了：
- ✅ 完整的设计文档
- ✅ 数据结构定义
- ✅ 7个实用示例
- ✅ 实现路线图

这个增强功能需要大量工作，但提供了清晰的实现路径。建议从简单的子查询支持开始，逐步增强到完整的多查询块支持。

---

**关键点：** 这不是 bug，而是功能的简化实现。要支持复杂 SQL，需要大幅增强查询树遍历和 hint 定位机制。
