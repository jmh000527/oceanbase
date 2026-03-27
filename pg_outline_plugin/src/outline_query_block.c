/*-------------------------------------------------------------------------
 *
 * outline_query_block.c
 *    Query block identification and management for multi-level hint support
 *
 * This module implements the query block identification mechanism similar to
 * OceanBase's approach, enabling precise hint targeting in complex SQL queries
 * with subqueries, CTEs, and set operations.
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "outline_query_block.h"
#include "outline_normalize.h"

#include "nodes/nodeFuncs.h"
#include "nodes/parsenodes.h"
#include "nodes/primnodes.h"
#include "parser/parser.h"
#include "utils/builtins.h"
#include "catalog/pg_type.h"
#include "common/md5.h"

#include <string.h>

/* Global query block context */
static QueryBlockContext *current_context = NULL;

/*
 * GetQBPrefix - Get QB_NAME prefix based on query block type
 */
const char *
GetQBPrefix(QueryBlockType type)
{
    switch (type)
    {
        case QB_TYPE_SELECT:
            return "SEL$";
        case QB_TYPE_INSERT:
            return "INS$";
        case QB_TYPE_UPDATE:
            return "UPD$";
        case QB_TYPE_DELETE:
            return "DEL$";
        case QB_TYPE_SET_UNION:
            return "SET$";
        case QB_TYPE_SET_INTERSECT:
            return "INT$";
        case QB_TYPE_SET_EXCEPT:
            return "EXC$";
        default:
            return "QRY$";
    }
}

/*
 * GetQueryBlockTypeName - Get human-readable name for query block type
 */
const char *
GetQueryBlockTypeName(QueryBlockType type)
{
    switch (type)
    {
        case QB_TYPE_SELECT:
            return "SELECT";
        case QB_TYPE_INSERT:
            return "INSERT";
        case QB_TYPE_UPDATE:
            return "UPDATE";
        case QB_TYPE_DELETE:
            return "DELETE";
        case QB_TYPE_SET_UNION:
            return "UNION";
        case QB_TYPE_SET_INTERSECT:
            return "INTERSECT";
        case QB_TYPE_SET_EXCEPT:
            return "EXCEPT";
        default:
            return "QUERY";
    }
}

/*
 * GenerateQBName - Generate unique QB_NAME for a query block
 *
 * Format: PREFIX$HASH_COUNTER
 * Example: SEL$ABCD1234_1
 */
void
GenerateQBName(QueryBlockInfo *block, QueryBlockContext *context)
{
    pg_md5_ctx  md5_ctx;
    uint8       digest[MD5_DIGEST_LENGTH];
    char        hash_str[9];
    const char *prefix;
    int         counter;

    if (block == NULL || context == NULL)
        return;

    /* Get prefix for this block type */
    prefix = GetQBPrefix(block->block_type);

    /* Generate MD5 hash from query source */
    if (block->query_source && *block->query_source)
    {
        pg_md5_init(&md5_ctx);
        pg_md5_update(&md5_ctx, (uint8 *) block->query_source,
                      strlen(block->query_source));
        pg_md5_final(digest, &md5_ctx);

        /* Convert first 4 bytes to hex string */
        snprintf(hash_str, 9, "%02X%02X%02X%02X",
                 digest[0], digest[1], digest[2], digest[3]);
    }
    else
    {
        /* Fallback if no query source */
        snprintf(hash_str, 9, "%08X", block->block_id);
    }

    /* Combine prefix, hash, and counter */
    counter = context->block_counter;
    snprintf(block->qb_name, 32, "%s%s_%d", prefix, hash_str, counter);
}

/*
 * CreateQueryBlock - Create a new query block information structure
 */
QueryBlockInfo *
CreateQueryBlock(Query *query, QueryBlockInfo *parent,
                QueryBlockContext *context)
{
    QueryBlockInfo *block;
    MemoryContext   oldcontext;

    if (context == NULL || context->mcxt == NULL)
        return NULL;

    oldcontext = MemoryContextSwitchTo(context->mcxt);

    block = (QueryBlockInfo *) palloc0(sizeof(QueryBlockInfo));

    /* Assign block ID */
    block->block_id = context->block_counter++;

    /* Set parent */
    block->parent_id = parent ? parent->block_id : -1;

    /* Set depth */
    block->depth = context->depth;

    /* Store query reference */
    block->query = query;

    /* Determine query block type */
    if (query)
    {
        switch (query->commandType)
        {
            case CMD_SELECT:
                block->block_type = QB_TYPE_SELECT;
                break;
            case CMD_INSERT:
                block->block_type = QB_TYPE_INSERT;
                break;
            case CMD_UPDATE:
                block->block_type = QB_TYPE_UPDATE;
                break;
            case CMD_DELETE:
                block->block_type = QB_TYPE_DELETE;
                break;
            default:
                block->block_type = QB_TYPE_SELECT;
                break;
        }

        /* Check if this is a subquery */
        block->is_subquery = (parent != NULL);

        /* Generate query source for hashing */
        block->query_source = nodeToString((Node *) query);
    }

    /* Initialize child blocks list */
    block->child_blocks = NIL;

    /* Generate QB_NAME */
    GenerateQBName(block, context);

    /* Add to context's all_blocks list */
    context->all_blocks = lappend(context->all_blocks, block);

    MemoryContextSwitchTo(oldcontext);

    return block;
}

/*
 * ProcessExprForSubqueries - Recursively find subqueries in expressions
 */
List *
ProcessExprForSubqueries(Node *node, QueryBlockInfo *parent,
                        QueryBlockContext *context)
{
    List *blocks = NIL;

    if (node == NULL)
        return NIL;

    /* Handle SubLink nodes (subqueries) */
    if (IsA(node, SubLink))
    {
        SubLink *sublink = (SubLink *) node;
        Query   *subquery;

        /* Get the subquery */
        if (IsA(sublink->subselect, Query))
        {
            subquery = (Query *) sublink->subselect;

            /* Recursively traverse the subquery */
            context->depth++;
            blocks = TraverseQueryTree(subquery, parent, context);
            context->depth--;
        }
    }
    /* Handle Query nodes directly */
    else if (IsA(node, Query))
    {
        context->depth++;
        blocks = TraverseQueryTree((Query *) node, parent, context);
        context->depth--;
    }
    /* Recursively check child nodes */
    else if (IsA(node, List))
    {
        ListCell *lc;
        foreach(lc, (List *) node)
        {
            blocks = list_concat(blocks,
                               ProcessExprForSubqueries((Node *) lfirst(lc),
                                                       parent, context));
        }
    }

    return blocks;
}

/*
 * TraverseQueryTree - Recursively traverse query tree and identify all blocks
 */
List *
TraverseQueryTree(Query *query, QueryBlockInfo *parent,
                 QueryBlockContext *context)
{
    QueryBlockInfo *block;
    List           *blocks = NIL;
    ListCell       *lc;

    if (query == NULL || context == NULL)
        return NIL;

    /* Create block for this query */
    block = CreateQueryBlock(query, parent, context);
    blocks = lappend(blocks, block);

    /* Process SELECT target list for subqueries */
    if (query->targetList)
    {
        foreach(lc, query->targetList)
        {
            TargetEntry *te = (TargetEntry *) lfirst(lc);
            List *sub_blocks = ProcessExprForSubqueries((Node *) te->expr,
                                                       block, context);
            blocks = list_concat(blocks, sub_blocks);

            /* Add to parent's children */
            foreach(lc, sub_blocks)
            {
                QueryBlockInfo *child = (QueryBlockInfo *) lfirst(lc);
                if (child->parent_id == block->block_id)
                    block->child_blocks = lappend(block->child_blocks, child);
            }
        }
    }

    /* Process FROM clause (RangeTblEntry with subqueries) */
    if (query->rtable)
    {
        foreach(lc, query->rtable)
        {
            RangeTblEntry *rte = (RangeTblEntry *) lfirst(lc);

            if (rte->rtekind == RTE_SUBQUERY && rte->subquery)
            {
                List *sub_blocks;

                context->depth++;
                sub_blocks = TraverseQueryTree(rte->subquery, block, context);
                context->depth--;

                blocks = list_concat(blocks, sub_blocks);
                block->child_blocks = list_concat(block->child_blocks, sub_blocks);
            }
        }
    }

    /* Process WHERE clause for subqueries */
    if (query->jointree && query->jointree->quals)
    {
        List *sub_blocks = ProcessExprForSubqueries(query->jointree->quals,
                                                   block, context);
        blocks = list_concat(blocks, sub_blocks);

        /* Add to parent's children */
        ListCell *lc2;
        foreach(lc2, sub_blocks)
        {
            QueryBlockInfo *child = (QueryBlockInfo *) lfirst(lc2);
            if (child->parent_id == block->block_id)
                block->child_blocks = lappend(block->child_blocks, child);
        }
    }

    /* Process HAVING clause */
    if (query->havingQual)
    {
        List *sub_blocks = ProcessExprForSubqueries(query->havingQual,
                                                   block, context);
        blocks = list_concat(blocks, sub_blocks);
    }

    /* Process CTE (WITH clause) */
    if (query->cteList)
    {
        foreach(lc, query->cteList)
        {
            CommonTableExpr *cte = (CommonTableExpr *) lfirst(lc);

            if (IsA(cte->ctequery, Query))
            {
                List *sub_blocks;

                context->depth++;
                sub_blocks = TraverseQueryTree((Query *) cte->ctequery,
                                             block, context);
                context->depth--;

                blocks = list_concat(blocks, sub_blocks);
                block->child_blocks = list_concat(block->child_blocks, sub_blocks);
            }
        }
    }

    /* Process set operations (UNION, INTERSECT, EXCEPT) */
    if (query->setOperations)
    {
        /* Note: Full set operation handling would be more complex */
        /* This is a simplified version */
        elog(DEBUG1, "pg_outline: Set operations detected in query block %s",
             block->qb_name);
    }

    return blocks;
}

/*
 * IdentifyQueryBlocks - Main entry point for query block identification
 */
List *
IdentifyQueryBlocks(Query *query)
{
    QueryBlockContext context;
    List             *blocks;
    MemoryContext     oldcontext;

    if (query == NULL)
        return NIL;

    /* Initialize context */
    MemSet(&context, 0, sizeof(QueryBlockContext));
    context.block_counter = 0;
    context.depth = 0;
    context.all_blocks = NIL;
    context.mcxt = AllocSetContextCreate(CurrentMemoryContext,
                                        "QueryBlockContext",
                                        ALLOCSET_DEFAULT_SIZES);

    oldcontext = MemoryContextSwitchTo(context.mcxt);

    /* Set global context */
    current_context = &context;

    /* Traverse the query tree */
    blocks = TraverseQueryTree(query, NULL, &context);

    MemoryContextSwitchTo(oldcontext);

    /* Clear global context */
    current_context = NULL;

    return blocks;
}

/*
 * QueryBlockToString - Convert query block to string for debugging
 */
char *
QueryBlockToString(QueryBlockInfo *block)
{
    StringInfoData str;

    if (block == NULL)
        return pstrdup("(null)");

    initStringInfo(&str);

    appendStringInfo(&str, "Block[%d]: %s", block->block_id, block->qb_name);
    appendStringInfo(&str, " Type=%s", GetQueryBlockTypeName(block->block_type));
    appendStringInfo(&str, " Parent=%d", block->parent_id);
    appendStringInfo(&str, " Depth=%d", block->depth);
    appendStringInfo(&str, " Children=%d", list_length(block->child_blocks));

    if (block->is_subquery)
        appendStringInfo(&str, " [SUBQUERY]");

    return str.data;
}

/*
 * PrintQueryBlocks - Print all query blocks for debugging
 */
void
PrintQueryBlocks(List *blocks)
{
    ListCell *lc;
    int       count = 0;

    if (blocks == NIL)
    {
        elog(DEBUG1, "pg_outline: No query blocks identified");
        return;
    }

    elog(DEBUG1, "pg_outline: Identified %d query blocks:", list_length(blocks));

    foreach(lc, blocks)
    {
        QueryBlockInfo *block = (QueryBlockInfo *) lfirst(lc);
        char           *block_str = QueryBlockToString(block);

        elog(DEBUG1, "  %s", block_str);
        pfree(block_str);
        count++;
    }
}
