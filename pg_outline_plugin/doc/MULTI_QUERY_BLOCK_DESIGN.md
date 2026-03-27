# PostgreSQL Outline 插件 - 多查询块 Hint 支持设计方案

## 问题分析

当前实现的局限性：
1. **只能处理单个 SELECT** - 没有识别多个查询块
2. **没有查询块命名机制** - 无法区分主查询和子查询
3. **Hint 定位不精确** - 无法指定 hint 应用到哪个查询块
4. **缺少递归遍历** - 不处理嵌套查询、CTE、UNION 等

## OceanBase 的解决方案

### 1. 查询块命名（QB_NAME）

OceanBase 为每个查询块生成唯一标识：

```
主查询:     SEL$12345678
子查询1:    SEL$87654321
子查询2:    SEL$ABCDEF00
UNION块:    SET$11111111
```

命名规则：
- 前缀：`SEL$`(SELECT), `SET$`(UNION), `INS$`(INSERT), `UPD$`(UPDATE), `DEL$`(DELETE)
- 后缀：8位十六进制 MD5 哈希 + 序号

### 2. Hint 与查询块关联

```sql
SELECT /*+
    INDEX(@SEL$1 t1 idx_id)          -- 主查询使用索引
    FULL(@SEL$2 t2)                   -- 子查询使用全表扫描
    LEADING(@SEL$1 t1 t3)             -- 主查询 JOIN 顺序
*/
    t1.*, t3.*
FROM t1
JOIN t3 ON t1.id = t3.id
WHERE t1.status IN (
    SELECT /*+ @SEL$2 */ status      -- 子查询块标识
    FROM t2
    WHERE t2.active = true
)
```

### 3. 数据结构

```c
/* 查询块信息 */
typedef struct QueryBlockInfo
{
    int32       block_id;           /* 查询块 ID (0, 1, 2...) */
    char        qb_name[32];        /* QB_NAME: SEL$12345678 */
    int32       parent_id;          /* 父查询块 ID (-1 表示顶层) */
    QueryType   query_type;         /* SELECT, INSERT, UPDATE, DELETE, SET */
    bool        is_subquery;        /* 是否为子查询 */
    List       *child_blocks;       /* 子查询块列表 */
} QueryBlockInfo;

/* Hint 与查询块的关联 */
typedef struct QueryBlockHint
{
    char        qb_name[32];        /* 目标查询块 */
    char       *hint_string;        /* Hint 内容 */
    bool        applied;            /* 是否已应用 */
} QueryBlockHint;

/* Outline 信息扩展 */
typedef struct OutlineInfoEx
{
    /* 原有字段... */

    /* 新增：多查询块支持 */
    List       *query_blocks;       /* QueryBlockInfo 列表 */
    List       *block_hints;        /* QueryBlockHint 列表 */
    bool        is_complex_query;   /* 是否为复杂查询 */
} OutlineInfoEx;
```

## 实现方案

### 阶段 1: 查询块识别

**文件**: `src/outline_query_block.c`

```c
/*
 * IdentifyQueryBlocks - 递归识别所有查询块
 */
List *
IdentifyQueryBlocks(Query *query)
{
    QueryBlockContext context;
    List *blocks = NIL;

    InitQueryBlockContext(&context);

    /* 遍历主查询 */
    blocks = TraverseQueryTree(query, NULL, &context);

    return blocks;
}

/*
 * TraverseQueryTree - 递归遍历查询树
 */
static List *
TraverseQueryTree(Query *query, QueryBlockInfo *parent,
                  QueryBlockContext *context)
{
    QueryBlockInfo *block;
    List *blocks = NIL;

    /* 创建当前查询块 */
    block = CreateQueryBlock(query, parent, context);
    blocks = lappend(blocks, block);

    /* 递归处理子查询 */
    if (query->hasSubLinks)
    {
        ListCell *lc;
        foreach(lc, query->targetList)
        {
            TargetEntry *te = (TargetEntry *) lfirst(lc);
            blocks = list_concat(blocks,
                               ProcessExprForSubqueries(te->expr, block, context));
        }
    }

    /* 处理 FROM 子句中的子查询 */
    foreach(lc, query->rtable)
    {
        RangeTblEntry *rte = (RangeTblEntry *) lfirst(lc);
        if (rte->rtekind == RTE_SUBQUERY)
        {
            blocks = list_concat(blocks,
                               TraverseQueryTree(rte->subquery, block, context));
        }
    }

    /* 处理 WHERE 子句中的子查询 */
    if (query->jointree && query->jointree->quals)
    {
        blocks = list_concat(blocks,
                           ProcessExprForSubqueries(query->jointree->quals,
                                                   block, context));
    }

    /* 处理 CTE (WITH 子句) */
    if (query->cteList)
    {
        foreach(lc, query->cteList)
        {
            CommonTableExpr *cte = (CommonTableExpr *) lfirst(lc);
            blocks = list_concat(blocks,
                               TraverseQueryTree((Query *) cte->ctequery,
                                               block, context));
        }
    }

    /* 处理 UNION/INTERSECT/EXCEPT */
    if (query->setOperations)
    {
        blocks = list_concat(blocks,
                           ProcessSetOperations(query->setOperations,
                                              block, context));
    }

    return blocks;
}

/*
 * GenerateQBName - 生成查询块名称
 */
static void
GenerateQBName(QueryBlockInfo *block, QueryBlockContext *context)
{
    pg_md5_ctx  md5_ctx;
    uint8       digest[MD5_DIGEST_LENGTH];
    char        hash_str[9];

    /* 生成查询源的 MD5 哈希 */
    pg_md5_init(&md5_ctx);
    pg_md5_update(&md5_ctx, (uint8 *) block->query_source,
                  strlen(block->query_source));
    pg_md5_final(digest, &md5_ctx);

    /* 转换为 8 位十六进制 */
    snprintf(hash_str, 9, "%02X%02X%02X%02X",
             digest[0], digest[1], digest[2], digest[3]);

    /* 组合前缀和哈希 */
    snprintf(block->qb_name, 32, "%s%s_%d",
             GetQBPrefix(block->query_type),
             hash_str,
             context->block_counter++);
}
```

### 阶段 2: Hint 解析与关联

**文件**: `src/outline_hint_parser.c`

```c
/*
 * ParseOutlineHints - 解析 outline 中的 hints
 */
List *
ParseOutlineHints(const char *hint_content)
{
    List       *hints = NIL;
    char       *hint_copy;
    char       *token;
    char       *saveptr;

    /* 移除注释标记 */
    hint_copy = ExtractHintContent(hint_content);

    /* 按空格分割 hints */
    token = strtok_r(hint_copy, " \t\n", &saveptr);
    while (token != NULL)
    {
        QueryBlockHint *hint = ParseSingleHint(token);
        if (hint != NULL)
            hints = lappend(hints, hint);

        token = strtok_r(NULL, " \t\n", &saveptr);
    }

    pfree(hint_copy);
    return hints;
}

/*
 * ParseSingleHint - 解析单个 hint
 */
static QueryBlockHint *
ParseSingleHint(const char *hint_str)
{
    QueryBlockHint *hint;
    char           *qb_name = NULL;
    char           *content;

    hint = (QueryBlockHint *) palloc0(sizeof(QueryBlockHint));

    /* 查找 @QB_NAME 标识 */
    if (strstr(hint_str, "@SEL$") ||
        strstr(hint_str, "@SET$") ||
        strstr(hint_str, "@INS$"))
    {
        /* 提取 QB_NAME */
        qb_name = ExtractQBName(hint_str);
        strncpy(hint->qb_name, qb_name, 31);
        hint->qb_name[31] = '\0';

        /* 提取 hint 内容 */
        content = RemoveQBName(hint_str);
    }
    else
    {
        /* 默认应用到主查询块 */
        hint->qb_name[0] = '\0';
        content = pstrdup(hint_str);
    }

    hint->hint_string = content;
    hint->applied = false;

    return hint;
}

/*
 * ExtractQBName - 从 hint 中提取 QB_NAME
 * 例如: "INDEX(@SEL$12345678 t1 idx)" -> "SEL$12345678"
 */
static char *
ExtractQBName(const char *hint_str)
{
    const char *start;
    const char *end;
    size_t      len;
    char       *qb_name;

    start = strchr(hint_str, '@');
    if (start == NULL)
        return NULL;

    start++; /* 跳过 @ */

    /* 查找 QB_NAME 结束位置（空格或括号） */
    end = start;
    while (*end && !isspace(*end) && *end != ')' && *end != ',')
        end++;

    len = end - start;
    qb_name = (char *) palloc(len + 1);
    memcpy(qb_name, start, len);
    qb_name[len] = '\0';

    return qb_name;
}
```

### 阶段 3: Hint 应用

**文件**: `src/outline_hint_applier.c`

```c
/*
 * ApplyQueryBlockHints - 应用 hints 到特定查询块
 */
void
ApplyQueryBlockHints(Query *query, List *query_blocks, List *hints)
{
    ListCell *block_lc;

    /* 遍历所有查询块 */
    foreach(block_lc, query_blocks)
    {
        QueryBlockInfo *block = (QueryBlockInfo *) lfirst(block_lc);

        /* 查找该块的 hints */
        List *block_hints = FindHintsForBlock(block->qb_name, hints);

        if (block_hints != NIL)
        {
            /* 应用 hints 到该查询块 */
            ApplyHintsToQuery(block->query, block_hints);
        }
    }
}

/*
 * FindHintsForBlock - 查找特定查询块的 hints
 */
static List *
FindHintsForBlock(const char *qb_name, List *all_hints)
{
    List     *block_hints = NIL;
    ListCell *lc;

    foreach(lc, all_hints)
    {
        QueryBlockHint *hint = (QueryBlockHint *) lfirst(lc);

        /* 匹配 QB_NAME 或应用到所有块 */
        if (hint->qb_name[0] == '\0' ||
            strcmp(hint->qb_name, qb_name) == 0)
        {
            block_hints = lappend(block_hints, hint);
        }
    }

    return block_hints;
}
```

### 阶段 4: Outline 匹配增强

**文件**: `src/outline_matcher_ex.c`

```c
/*
 * MatchOutlineForComplexQuery - 匹配复杂查询的 outline
 */
OutlineMatchResult *
MatchOutlineForComplexQuery(const char *query_string, Query *parse)
{
    OutlineMatchResult *result;
    List               *query_blocks;
    char               *signature;

    result = (OutlineMatchResult *) palloc0(sizeof(OutlineMatchResult));

    /* 识别所有查询块 */
    query_blocks = IdentifyQueryBlocks(parse);

    /* 生成复合签名（包含所有查询块） */
    signature = GenerateComplexSignature(query_string, query_blocks);

    /* 匹配 outline */
    result->outline = LookupOutlineBySignature(signature);

    if (result->outline && result->outline->is_complex_query)
    {
        /* 解析多查询块 hints */
        List *hints = ParseOutlineHints(result->outline->outline_content);

        /* 应用到各个查询块 */
        ApplyQueryBlockHints(parse, query_blocks, hints);

        result->matched = true;
    }

    pfree(signature);
    return result;
}

/*
 * GenerateComplexSignature - 生成复杂查询的签名
 */
static char *
GenerateComplexSignature(const char *query_string, List *query_blocks)
{
    StringInfoData signature;
    ListCell      *lc;

    initStringInfo(&signature);

    /* 主查询签名 */
    appendStringInfo(&signature, "MAIN:");

    /* 规范化主查询 */
    char *main_normalized = NormalizeQueryString(query_string);
    appendStringInfo(&signature, "%s", main_normalized);

    /* 添加查询块结构信息 */
    appendStringInfo(&signature, "|BLOCKS:%d", list_length(query_blocks));

    /* 为每个块添加类型信息 */
    foreach(lc, query_blocks)
    {
        QueryBlockInfo *block = (QueryBlockInfo *) lfirst(lc);
        appendStringInfo(&signature, "|%s:%d",
                        GetQueryTypeString(block->query_type),
                        block->block_id);
    }

    pfree(main_normalized);
    return signature.data;
}
```

## 使用示例

### 创建复杂查询的 Outline

```sql
-- 复杂查询示例
SELECT t1.id, t1.name,
       (SELECT COUNT(*) FROM orders o WHERE o.user_id = t1.id) as order_count
FROM users t1
WHERE t1.status = 'active'
  AND EXISTS (
    SELECT 1 FROM subscriptions s
    WHERE s.user_id = t1.id
      AND s.expired_at > NOW()
  );

-- 创建 outline 时指定多个查询块的 hints
SELECT pg_outline_create(
    'complex_user_query',
    'SELECT t1.id, t1.name,
           (SELECT COUNT(*) FROM orders o WHERE o.user_id = t1.id) as order_count
     FROM users t1
     WHERE t1.status = ''active''
       AND EXISTS (
         SELECT 1 FROM subscriptions s
         WHERE s.user_id = t1.id
           AND s.expired_at > NOW()
       )',
    '/*+
        INDEX(@SEL$1 t1 idx_users_status)           -- 主查询使用索引
        FULL(@SEL$2 o)                               -- 子查询1全表扫描
        INDEX(@SEL$3 s idx_subscriptions_user)       -- 子查询2使用索引
        LEADING(@SEL$1 t1)                           -- 主查询访问顺序
    */'
);
```

### Outline 匹配过程

1. **解析查询** → 识别 3 个查询块
   - SEL$1: 主查询
   - SEL$2: SELECT COUNT 子查询
   - SEL$3: EXISTS 子查询

2. **生成签名** → `MAIN:SELECT...BLOCKS:3|SELECT:0|SELECT:1|SELECT:2`

3. **匹配 Outline** → 找到 'complex_user_query'

4. **解析 Hints** → 提取每个 @SEL$N 的 hint

5. **应用 Hints** → 分别应用到对应的查询块

## 实现难度评估

### 简单部分 ✅
- 查询块信息结构定义
- Hint 字符串解析
- QB_NAME 提取

### 中等难度 ⚠️
- 递归遍历查询树
- 查询块命名（哈希生成）
- Hint 与查询块关联

### 高难度 🔴
- 完整的子查询识别（所有类型）
- CTE 递归处理
- UNION/INTERSECT/EXCEPT 处理
- 与 PostgreSQL 优化器深度集成
- 查询变换后的 QB_NAME 维护

## 下一步建议

### 基础实现（推荐先做）
1. 实现单层子查询的识别
2. 支持简单的 @QB_NAME 语法
3. 处理 WHERE 和 FROM 子句中的子查询

### 进阶实现
1. 支持 CTE (WITH 子句)
2. 支持 UNION/INTERSECT
3. 支持相关子查询
4. 完整的查询块树结构

### 完整实现
1. 与 PostgreSQL 查询重写器集成
2. 查询变换后的 QB_NAME 追踪
3. 复杂嵌套查询的完整支持
4. 性能优化和测试

## 总结

您提出的问题非常关键！OceanBase 的 outline 功能确实需要：

1. **查询块识别** - 递归识别所有 SELECT/子查询/CTE/UNION
2. **QB_NAME 机制** - 为每个块生成唯一标识
3. **Hint 定位** - 使用 @QB_NAME 精确指定 hint 目标
4. **递归应用** - 遍历查询树分别应用 hints

当前的简化实现只是一个起点，要达到 OceanBase 的水平，还需要大量工作。但这个增强方案提供了清晰的实现路径。
