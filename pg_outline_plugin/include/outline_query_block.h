/*-------------------------------------------------------------------------
 *
 * outline_query_block.h
 *    Query block identification for multi-level hint support
 *
 * 支持多查询块的 Hint 定位功能
 *
 *-------------------------------------------------------------------------
 */

#ifndef OUTLINE_QUERY_BLOCK_H
#define OUTLINE_QUERY_BLOCK_H

#include "postgres.h"
#include "nodes/parsenodes.h"

/* 查询类型 */
typedef enum QueryBlockType
{
    QB_TYPE_SELECT,
    QB_TYPE_INSERT,
    QB_TYPE_UPDATE,
    QB_TYPE_DELETE,
    QB_TYPE_SET_UNION,
    QB_TYPE_SET_INTERSECT,
    QB_TYPE_SET_EXCEPT
} QueryBlockType;

/* 查询块信息 */
typedef struct QueryBlockInfo
{
    int32               block_id;       /* 查询块序号 */
    char                qb_name[32];    /* QB_NAME: SEL$12345678 */
    int32               parent_id;      /* 父查询块 ID */
    QueryBlockType      block_type;     /* 查询类型 */
    bool                is_subquery;    /* 是否为子查询 */
    Query              *query;          /* 对应的 Query 节点 */
    List               *child_blocks;   /* 子查询块列表 */

    /* 用于生成 QB_NAME */
    char               *query_source;   /* 查询源文本 */
    int32               depth;          /* 嵌套深度 */
} QueryBlockInfo;

/* 查询块上下文 */
typedef struct QueryBlockContext
{
    int32       block_counter;          /* 块计数器 */
    int32       depth;                  /* 当前深度 */
    List       *all_blocks;             /* 所有识别的块 */
    MemoryContext mcxt;                 /* 内存上下文 */
} QueryBlockContext;

/* Hint 与查询块关联 */
typedef struct QueryBlockHint
{
    char        qb_name[32];            /* 目标查询块名 */
    char       *hint_text;              /* Hint 文本 */
    bool        is_global;              /* 是否为全局 hint */
    bool        applied;                /* 是否已应用 */
} QueryBlockHint;

/* Function declarations */

/* 查询块识别 */
extern List *IdentifyQueryBlocks(Query *query);
extern QueryBlockInfo *CreateQueryBlock(Query *query,
                                        QueryBlockInfo *parent,
                                        QueryBlockContext *context);
extern void GenerateQBName(QueryBlockInfo *block, QueryBlockContext *context);

/* 递归遍历 */
extern List *TraverseQueryTree(Query *query,
                              QueryBlockInfo *parent,
                              QueryBlockContext *context);
extern List *ProcessExprForSubqueries(Node *node,
                                     QueryBlockInfo *parent,
                                     QueryBlockContext *context);

/* Hint 解析 */
extern List *ParseMultiBlockHints(const char *hint_content);
extern QueryBlockHint *ParseSingleHint(const char *hint_str);
extern char *ExtractQBName(const char *hint_str);
extern char *RemoveQBName(const char *hint_str);

/* Hint 应用 */
extern void ApplyQueryBlockHints(Query *query,
                                List *query_blocks,
                                List *hints);
extern List *FindHintsForBlock(const char *qb_name, List *all_hints);

/* 工具函数 */
extern const char *GetQBPrefix(QueryBlockType type);
extern const char *GetQueryBlockTypeName(QueryBlockType type);
extern char *GenerateComplexSignature(const char *query_string, List *query_blocks);

/* 调试和诊断 */
extern void PrintQueryBlocks(List *blocks);
extern char *QueryBlockToString(QueryBlockInfo *block);

#endif /* OUTLINE_QUERY_BLOCK_H */
