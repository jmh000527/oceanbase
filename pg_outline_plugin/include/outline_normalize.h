/*-------------------------------------------------------------------------
 *
 * outline_normalize.h
 *    SQL normalization for outline matching
 *    Removes constants and generates parameterized SQL signature
 *
 *-------------------------------------------------------------------------
 */

#ifndef OUTLINE_NORMALIZE_H
#define OUTLINE_NORMALIZE_H

#include "postgres.h"
#include "nodes/parsenodes.h"

/* Normalization context */
typedef struct NormalizeContext
{
    StringInfo  result;
    int         param_count;
    bool        in_function;
    List        *param_types;
} NormalizeContext;

/* Function declarations */

/* Main normalization entry point */
extern char *NormalizeQueryString(const char *query_string);
extern char *NormalizeQuery(Query *query);

/* Node normalization */
extern void NormalizeNode(Node *node, NormalizeContext *context);
extern void NormalizeExpr(Expr *expr, NormalizeContext *context);
extern void NormalizeConst(Const *const_node, NormalizeContext *context);

/* Utility functions */
extern bool IsNormalizableConst(Const *const_node);
extern char *GenerateSignature(const char *normalized_sql);
extern void InitNormalizeContext(NormalizeContext *context);
extern void CleanupNormalizeContext(NormalizeContext *context);

#endif /* OUTLINE_NORMALIZE_H */
