/*-------------------------------------------------------------------------
 *
 * outline_normalize.c
 *    SQL normalization implementation for outline matching
 *
 * This module implements SQL normalization by replacing constants with
 * parameter placeholders, generating a signature that can match queries
 * with different constant values.
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "outline_normalize.h"
#include "nodes/nodeFuncs.h"
#include "nodes/parsenodes.h"
#include "nodes/primnodes.h"
#include "parser/parser.h"
#include "utils/builtins.h"
#include "catalog/pg_type.h"
#include "mb/pg_wchar.h"
#include "common/md5.h"

#include <string.h>

/*
 * InitNormalizeContext - Initialize normalization context
 */
void
InitNormalizeContext(NormalizeContext *context)
{
    context->result = makeStringInfo();
    context->param_count = 0;
    context->in_function = false;
    context->param_types = NIL;
}

/*
 * CleanupNormalizeContext - Cleanup normalization context
 */
void
CleanupNormalizeContext(NormalizeContext *context)
{
    if (context->result)
    {
        pfree(context->result->data);
        pfree(context->result);
    }
    if (context->param_types)
        list_free(context->param_types);
}

/*
 * IsNormalizableConst - Check if a constant should be normalized
 */
bool
IsNormalizableConst(Const *const_node)
{
    if (const_node == NULL)
        return false;

    /* Don't normalize NULL constants */
    if (const_node->constisnull)
        return false;

    /* Normalize most types, but be careful with special cases */
    switch (const_node->consttype)
    {
        case BOOLOID:
            /* Keep boolean constants as-is in some contexts */
            return true;
        case INT2OID:
        case INT4OID:
        case INT8OID:
        case FLOAT4OID:
        case FLOAT8OID:
        case NUMERICOID:
        case TEXTOID:
        case VARCHAROID:
        case BPCHAROID:
        case DATEOID:
        case TIMESTAMPOID:
        case TIMESTAMPTZOID:
            return true;
        default:
            /* Normalize most other types */
            return true;
    }
}

/*
 * NormalizeConst - Replace constant with parameter placeholder
 */
void
NormalizeConst(Const *const_node, NormalizeContext *context)
{
    if (!IsNormalizableConst(const_node))
    {
        /* Keep the constant as-is */
        appendStringInfo(context->result, "%s",
                        nodeToString((Node *) const_node));
        return;
    }

    /* Replace with parameter placeholder */
    context->param_count++;
    appendStringInfo(context->result, "?");

    /* Track parameter type for potential future use */
    context->param_types = lappend_oid(context->param_types,
                                      const_node->consttype);
}

/*
 * NormalizeExpr - Normalize an expression node
 */
void
NormalizeExpr(Expr *expr, NormalizeContext *context)
{
    if (expr == NULL)
        return;

    switch (nodeTag(expr))
    {
        case T_Const:
            NormalizeConst((Const *) expr, context);
            break;

        case T_Var:
        case T_Param:
        case T_Aggref:
        case T_WindowFunc:
        case T_FuncExpr:
        case T_OpExpr:
        case T_BoolExpr:
        case T_SubLink:
        case T_CaseExpr:
        case T_CoalesceExpr:
        case T_NullTest:
        case T_BooleanTest:
            /* For other expressions, keep structure but normalize sub-nodes */
            NormalizeNode((Node *) expr, context);
            break;

        default:
            /* Keep as-is for unknown types */
            appendStringInfo(context->result, "%s", nodeToString((Node *) expr));
            break;
    }
}

/*
 * NormalizeNode - Recursively normalize a parse tree node
 */
void
NormalizeNode(Node *node, NormalizeContext *context)
{
    if (node == NULL)
        return;

    /* Special handling for constants */
    if (IsA(node, Const))
    {
        NormalizeConst((Const *) node, context);
        return;
    }

    /* For other nodes, we need more complex handling */
    /* This is a simplified version - full implementation would handle
     * all node types recursively */

    switch (nodeTag(node))
    {
        case T_Query:
        {
            Query *query = (Query *) node;
            appendStringInfo(context->result, "SELECT");

            /* Normalize target list */
            if (query->targetList)
            {
                ListCell *lc;
                foreach(lc, query->targetList)
                {
                    TargetEntry *te = (TargetEntry *) lfirst(lc);
                    NormalizeExpr(te->expr, context);
                }
            }

            /* Normalize FROM clause */
            if (query->rtable)
            {
                appendStringInfo(context->result, " FROM ");
                /* Simplification - full implementation would handle RTEs */
            }

            /* Normalize WHERE clause */
            if (query->jointree && query->jointree->quals)
            {
                appendStringInfo(context->result, " WHERE ");
                NormalizeNode(query->jointree->quals, context);
            }
            break;
        }

        default:
            /* For other node types, use a simplified representation */
            break;
    }
}

/*
 * NormalizeQueryString - Normalize a query string
 *
 * This is the main entry point for normalization.
 * It parses the query and normalizes the parse tree.
 */
char *
NormalizeQueryString(const char *query_string)
{
    List            *parse_tree_list;
    RawStmt         *raw_stmt;
    NormalizeContext context;
    char            *result;

    if (query_string == NULL || *query_string == '\0')
        return NULL;

    /* Parse the query */
    parse_tree_list = raw_parser(query_string);
    if (parse_tree_list == NIL)
        return NULL;

    /* Get first statement */
    raw_stmt = linitial_node(RawStmt, parse_tree_list);
    if (raw_stmt == NULL)
        return NULL;

    /* Initialize context */
    InitNormalizeContext(&context);

    /* Normalize the parse tree */
    NormalizeNode(raw_stmt->stmt, &context);

    /* Get result */
    result = pstrdup(context.result->data);

    /* Cleanup */
    CleanupNormalizeContext(&context);

    return result;
}

/*
 * NormalizeQuery - Normalize a Query node
 */
char *
NormalizeQuery(Query *query)
{
    NormalizeContext context;
    char            *result;

    if (query == NULL)
        return NULL;

    /* Initialize context */
    InitNormalizeContext(&context);

    /* Normalize the query */
    NormalizeNode((Node *) query, &context);

    /* Get result */
    result = pstrdup(context.result->data);

    /* Cleanup */
    CleanupNormalizeContext(&context);

    return result;
}

/*
 * GenerateSignature - Generate a canonical signature from normalized SQL
 *
 * This function takes normalized SQL and produces a clean signature
 * by removing extra whitespace and standardizing format.
 */
char *
GenerateSignature(const char *normalized_sql)
{
    StringInfoData  sig;
    const char     *p;
    bool            in_space = false;
    bool            last_was_space = false;

    if (normalized_sql == NULL)
        return NULL;

    initStringInfo(&sig);

    /* Normalize whitespace and convert to uppercase */
    for (p = normalized_sql; *p; p++)
    {
        if (isspace((unsigned char) *p))
        {
            if (!last_was_space && sig.len > 0)
            {
                appendStringInfoChar(&sig, ' ');
                last_was_space = true;
            }
        }
        else
        {
            /* Convert to uppercase for case-insensitive matching */
            appendStringInfoChar(&sig, pg_toupper((unsigned char) *p));
            last_was_space = false;
        }
    }

    /* Trim trailing space */
    if (sig.len > 0 && sig.data[sig.len - 1] == ' ')
        sig.data[--sig.len] = '\0';

    return sig.data;
}
