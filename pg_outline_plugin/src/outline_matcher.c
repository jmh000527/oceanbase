/*-------------------------------------------------------------------------
 *
 * outline_matcher.c
 *    Outline matching logic implementation
 *
 * Implements multi-strategy matching of outlines to SQL queries
 * based on OceanBase's outline matching approach.
 * Enhanced with multi-query block support.
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "outline_matcher.h"
#include "outline_normalize.h"
#include "outline_query_block.h"
#include "pg_outline.h"

#include "nodes/parsenodes.h"
#include "utils/memutils.h"

#include <string.h>

/*
 * MatchBySignature - Match outline by SQL signature
 */
OutlineInfo *
MatchBySignature(const char *signature, bool format_outline)
{
    OutlineInfo *outline;

    if (signature == NULL || *signature == '\0')
        return NULL;

    /* Lookup by signature */
    outline = LookupOutlineBySignature(signature);

    /* Check if format matches request */
    if (outline && outline->format_outline != format_outline)
        return NULL;

    return outline;
}

/*
 * MatchBySqlId - Match outline by SQL ID
 */
OutlineInfo *
MatchBySqlId(const char *sql_id, bool format_outline)
{
    OutlineInfo *outline;

    if (sql_id == NULL || *sql_id == '\0')
        return NULL;

    /* Lookup by SQL ID */
    outline = LookupOutlineBySqlId(sql_id);

    /* Check if format matches request */
    if (outline && outline->format_outline != format_outline)
        return NULL;

    return outline;
}

/*
 * CheckOutlineEnabled - Check if outline is enabled
 */
bool
CheckOutlineEnabled(OutlineInfo *outline)
{
    if (outline == NULL)
        return false;

    return outline->enabled;
}

/*
 * ValidateOutlineMatch - Validate that outline matches the query
 */
bool
ValidateOutlineMatch(OutlineInfo *outline, Query *parse)
{
    if (outline == NULL)
        return false;

    /* Basic validation - check if enabled */
    if (!outline->enabled)
        return false;

    /* Check if hint content is valid */
    if (outline->outline_content == NULL || *outline->outline_content == '\0')
        return false;

    /* Additional validation could be added here:
     * - Check query type matches
     * - Validate table/column references
     * - Check user permissions
     */

    return true;
}

/*
 * ValidateComplexOutlineMatch - Validate outline for complex query with blocks
 */
bool
ValidateComplexOutlineMatch(OutlineInfo *outline, Query *parse, List *query_blocks)
{
    if (!ValidateOutlineMatch(outline, parse))
        return false;

    /* If query has multiple blocks, outline should support multi-block hints */
    if (query_blocks != NIL && list_length(query_blocks) > 1)
    {
        /* Check if outline content contains @QB_NAME syntax */
        if (outline->outline_content && strchr(outline->outline_content, '@'))
        {
            if (pg_outline_debug_log)
            {
                elog(DEBUG2, "pg_outline: Complex query with %d blocks, "
                     "outline contains @QB_NAME hints",
                     list_length(query_blocks));
            }
        }
    }

    return true;
}

/*
 * MatchOutlineForQuery - Main outline matching function
 *
 * Implements the matching strategy from OceanBase:
 * 1. Try normal outline with signature
 * 2. Try normal outline with sql_id
 * 3. Try format outline with signature
 * 4. Try format outline with sql_id
 *
 * Enhanced with multi-query block support - generates complex signature
 * for queries with multiple query blocks.
 */
OutlineMatchResult *
MatchOutlineForQuery(const char *query_string, Query *parse)
{
    OutlineMatchResult *result;
    char               *normalized_sql = NULL;
    char               *signature = NULL;
    char               *sql_id = NULL;
    OutlineInfo        *outline = NULL;
    List               *query_blocks = NIL;
    bool                is_complex_query = false;

    /* Allocate result structure */
    result = (OutlineMatchResult *) palloc0(sizeof(OutlineMatchResult));
    result->matched = false;
    result->outline = NULL;
    result->strategy = MATCH_BY_SIGNATURE;
    result->query_blocks = NIL;

    /* Check if outline feature is enabled */
    if (!pg_outline_enabled)
    {
        return result;
    }

    /* Identify query blocks if we have parsed query */
    if (parse)
    {
        query_blocks = IdentifyQueryBlocks(parse);
        result->query_blocks = query_blocks;

        if (query_blocks != NIL && list_length(query_blocks) > 1)
        {
            is_complex_query = true;

            if (pg_outline_debug_log)
            {
                elog(DEBUG1, "pg_outline: Complex query detected with %d blocks",
                     list_length(query_blocks));
            }
        }
    }

    /* Normalize the query */
    if (query_string && *query_string)
    {
        normalized_sql = NormalizeQueryString(query_string);
    }
    else if (parse)
    {
        normalized_sql = NormalizeQuery(parse);
    }

    if (normalized_sql == NULL || *normalized_sql == '\0')
    {
        return result;
    }

    /* Generate signature based on query complexity */
    if (is_complex_query)
    {
        /* Generate complex signature with query block structure */
        signature = GenerateComplexSignature(query_string, query_blocks);

        if (pg_outline_debug_log)
        {
            elog(DEBUG2, "pg_outline: Complex signature: %s", signature);
        }
    }
    else
    {
        /* Generate simple signature */
        signature = GenerateSignature(normalized_sql);
    }

    if (signature == NULL)
    {
        pfree(normalized_sql);
        return result;
    }

    /* Compute SQL ID */
    sql_id = ComputeSqlId(signature);
    if (sql_id == NULL)
    {
        pfree(normalized_sql);
        pfree(signature);
        return result;
    }

    /* Store in result for potential use */
    result->normalized_sql = pstrdup(normalized_sql);
    result->sql_id = pstrdup(sql_id);
    result->signature = pstrdup(signature);

    /*
     * Strategy 1: Try normal outline with signature
     */
    outline = MatchBySignature(signature, false);
    if (outline && CheckOutlineEnabled(outline))
    {
        if (is_complex_query)
        {
            if (ValidateComplexOutlineMatch(outline, parse, query_blocks))
            {
                result->outline = outline;
                result->strategy = MATCH_BY_SIGNATURE;
                result->matched = true;

                if (pg_outline_debug_log)
                {
                    elog(LOG, "pg_outline: Matched complex query by signature (normal): %s",
                         outline->outline_name);
                }

                goto cleanup;
            }
        }
        else if (ValidateOutlineMatch(outline, parse))
        {
            result->outline = outline;
            result->strategy = MATCH_BY_SIGNATURE;
            result->matched = true;

            if (pg_outline_debug_log)
            {
                elog(LOG, "pg_outline: Matched by signature (normal): %s",
                     outline->outline_name);
            }

            goto cleanup;
        }
    }

    /*
     * Strategy 2: Try normal outline with sql_id
     */
    outline = MatchBySqlId(sql_id, false);
    if (outline && CheckOutlineEnabled(outline))
    {
        if (is_complex_query)
        {
            if (ValidateComplexOutlineMatch(outline, parse, query_blocks))
            {
                result->outline = outline;
                result->strategy = MATCH_BY_SQL_ID;
                result->matched = true;

                if (pg_outline_debug_log)
                {
                    elog(LOG, "pg_outline: Matched complex query by sql_id (normal): %s",
                         outline->outline_name);
                }

                goto cleanup;
            }
        }
        else if (ValidateOutlineMatch(outline, parse))
        {
            result->outline = outline;
            result->strategy = MATCH_BY_SQL_ID;
            result->matched = true;

            if (pg_outline_debug_log)
            {
                elog(LOG, "pg_outline: Matched by sql_id (normal): %s",
                     outline->outline_name);
            }

            goto cleanup;
        }
    }

    /*
     * Strategy 3: Try format outline with signature
     */
    outline = MatchBySignature(signature, true);
    if (outline && CheckOutlineEnabled(outline))
    {
        if (is_complex_query)
        {
            if (ValidateComplexOutlineMatch(outline, parse, query_blocks))
            {
                result->outline = outline;
                result->strategy = MATCH_BY_FORMAT_SIGNATURE;
                result->matched = true;

                if (pg_outline_debug_log)
                {
                    elog(LOG, "pg_outline: Matched complex query by signature (format): %s",
                         outline->outline_name);
                }

                goto cleanup;
            }
        }
        else if (ValidateOutlineMatch(outline, parse))
        {
            result->outline = outline;
            result->strategy = MATCH_BY_FORMAT_SIGNATURE;
            result->matched = true;

            if (pg_outline_debug_log)
            {
                elog(LOG, "pg_outline: Matched by signature (format): %s",
                     outline->outline_name);
            }

            goto cleanup;
        }
    }

    /*
     * Strategy 4: Try format outline with sql_id
     */
    outline = MatchBySqlId(sql_id, true);
    if (outline && CheckOutlineEnabled(outline))
    {
        if (is_complex_query)
        {
            if (ValidateComplexOutlineMatch(outline, parse, query_blocks))
            {
                result->outline = outline;
                result->strategy = MATCH_BY_FORMAT_SQL_ID;
                result->matched = true;

                if (pg_outline_debug_log)
                {
                    elog(LOG, "pg_outline: Matched complex query by sql_id (format): %s",
                         outline->outline_name);
                }

                goto cleanup;
            }
        }
        else if (ValidateOutlineMatch(outline, parse))
        {
            result->outline = outline;
            result->strategy = MATCH_BY_FORMAT_SQL_ID;
            result->matched = true;

            if (pg_outline_debug_log)
            {
                elog(LOG, "pg_outline: Matched by sql_id (format): %s",
                     outline->outline_name);
            }

            goto cleanup;
        }
    }

    /* No match found */
    if (pg_outline_debug_log)
    {
        elog(DEBUG1, "pg_outline: No outline matched for query: %.100s",
             query_string ? query_string : "(null)");
    }

cleanup:
    pfree(normalized_sql);
    pfree(signature);
    pfree(sql_id);

    return result;
}

/*
 * FreeMatchResult - Free match result structure
 */
void
FreeMatchResult(OutlineMatchResult *result)
{
    if (result == NULL)
        return;

    if (result->normalized_sql)
        pfree(result->normalized_sql);

    if (result->sql_id)
        pfree(result->sql_id);

    if (result->signature)
        pfree(result->signature);

    /* Note: query_blocks are managed in their own memory context */

    pfree(result);
}
