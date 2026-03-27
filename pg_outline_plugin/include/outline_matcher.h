/*-------------------------------------------------------------------------
 *
 * outline_matcher.h
 *    Outline matching logic for SQL queries
 *    Implements multi-strategy matching based on OceanBase design
 *
 *-------------------------------------------------------------------------
 */

#ifndef OUTLINE_MATCHER_H
#define OUTLINE_MATCHER_H

#include "postgres.h"
#include "pg_outline.h"

/* Match strategy enumeration */
typedef enum OutlineMatchStrategy
{
    MATCH_BY_SIGNATURE,
    MATCH_BY_SQL_ID,
    MATCH_BY_FORMAT_SIGNATURE,
    MATCH_BY_FORMAT_SQL_ID
} OutlineMatchStrategy;

/* Match result structure */
typedef struct OutlineMatchResult
{
    OutlineInfo         *outline;
    OutlineMatchStrategy strategy;
    char                *normalized_sql;
    char                *sql_id;
    char                *signature;
    List                *query_blocks;  /* List of QueryBlockInfo */
    bool                matched;
} OutlineMatchResult;

/* Function declarations */

/* Main matching function */
extern OutlineMatchResult *MatchOutlineForQuery(const char *query_string,
                                                Query *parse);

/* Strategy-specific matching */
extern OutlineInfo *MatchBySignature(const char *signature, bool format_outline);
extern OutlineInfo *MatchBySqlId(const char *sql_id, bool format_outline);

/* Match validation */
extern bool ValidateOutlineMatch(OutlineInfo *outline, Query *parse);
extern bool ValidateComplexOutlineMatch(OutlineInfo *outline, Query *parse,
                                       List *query_blocks);
extern bool CheckOutlineEnabled(OutlineInfo *outline);

/* Cleanup */
extern void FreeMatchResult(OutlineMatchResult *result);

#endif /* OUTLINE_MATCHER_H */
