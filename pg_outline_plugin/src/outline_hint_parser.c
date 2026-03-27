/*-------------------------------------------------------------------------
 *
 * outline_hint_parser.c
 *    Multi-block hint parsing with @QB_NAME support
 *
 * This module parses outline hints that target specific query blocks
 * using the @QB_NAME syntax similar to OceanBase.
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "outline_query_block.h"

#include "utils/builtins.h"
#include "lib/stringinfo.h"

#include <string.h>
#include <ctype.h>

/*
 * ExtractQBName - Extract QB_NAME from hint string
 *
 * Input: "INDEX(@SEL$12345678 t1 idx)" or "FULL(@SEL$ABCD1234_1 t2)"
 * Output: "SEL$12345678" or "SEL$ABCD1234_1"
 */
char *
ExtractQBName(const char *hint_str)
{
    const char *start;
    const char *end;
    size_t      len;
    char       *qb_name;

    if (hint_str == NULL || *hint_str == '\0')
        return NULL;

    /* Find '@' character */
    start = strchr(hint_str, '@');
    if (start == NULL)
        return NULL;

    start++; /* Skip '@' */

    /* Find end of QB_NAME (space, comma, or close paren) */
    end = start;
    while (*end && !isspace((unsigned char) *end) &&
           *end != ')' && *end != ',')
        end++;

    len = end - start;
    if (len == 0)
        return NULL;

    qb_name = (char *) palloc(len + 1);
    memcpy(qb_name, start, len);
    qb_name[len] = '\0';

    return qb_name;
}

/*
 * RemoveQBName - Remove @QB_NAME from hint string
 *
 * Input: "INDEX(@SEL$12345678 t1 idx)"
 * Output: "INDEX(t1 idx)"
 */
char *
RemoveQBName(const char *hint_str)
{
    StringInfoData result;
    const char    *at_pos;
    const char    *space_after_qb;

    if (hint_str == NULL || *hint_str == '\0')
        return NULL;

    initStringInfo(&result);

    /* Find '@' character */
    at_pos = strchr(hint_str, '@');
    if (at_pos == NULL)
    {
        /* No QB_NAME, return copy */
        return pstrdup(hint_str);
    }

    /* Copy part before '@' */
    appendBinaryStringInfo(&result, hint_str, at_pos - hint_str);

    /* Find space or delimiter after QB_NAME */
    space_after_qb = at_pos + 1;
    while (*space_after_qb && !isspace((unsigned char) *space_after_qb) &&
           *space_after_qb != ')' && *space_after_qb != ',')
        space_after_qb++;

    /* Skip whitespace */
    while (*space_after_qb && isspace((unsigned char) *space_after_qb))
        space_after_qb++;

    /* Append rest of hint */
    appendStringInfoString(&result, space_after_qb);

    return result.data;
}

/*
 * ParseSingleHint - Parse a single hint and extract QB_NAME if present
 */
QueryBlockHint *
ParseSingleHint(const char *hint_str)
{
    QueryBlockHint *hint;
    char           *qb_name;
    char           *content;

    if (hint_str == NULL || *hint_str == '\0')
        return NULL;

    hint = (QueryBlockHint *) palloc0(sizeof(QueryBlockHint));

    /* Try to extract QB_NAME */
    qb_name = ExtractQBName(hint_str);

    if (qb_name != NULL)
    {
        /* Has @QB_NAME syntax */
        strncpy(hint->qb_name, qb_name, 31);
        hint->qb_name[31] = '\0';
        hint->is_global = false;

        /* Remove QB_NAME from hint content */
        content = RemoveQBName(hint_str);

        pfree(qb_name);
    }
    else
    {
        /* No QB_NAME - global hint */
        hint->qb_name[0] = '\0';
        hint->is_global = true;
        content = pstrdup(hint_str);
    }

    hint->hint_text = content;
    hint->applied = false;

    return hint;
}

/*
 * ParseMultiBlockHints - Parse outline content with multiple hints
 *
 * Input: "/*+ INDEX(@SEL$1 t1 idx) FULL(@SEL$2 t2) LEADING(t1 t2) *\/"
 * Output: List of QueryBlockHint structures
 */
List *
ParseMultiBlockHints(const char *hint_content)
{
    List           *hints = NIL;
    char           *hint_copy;
    char           *cleaned;
    char           *token;
    char           *saveptr;
    StringInfoData  current_hint;
    int             paren_depth;

    if (hint_content == NULL || *hint_content == '\0')
        return NIL;

    /* Remove comment markers /*+ and */ */
    hint_copy = pstrdup(hint_content);

    /* Find start of hints */
    cleaned = strstr(hint_copy, "/*+");
    if (cleaned)
        cleaned += 3;
    else
        cleaned = hint_copy;

    /* Find end marker */
    char *end_marker = strstr(cleaned, "*/");
    if (end_marker)
        *end_marker = '\0';

    /* Parse hints - they may contain parentheses */
    initStringInfo(&current_hint);
    paren_depth = 0;

    for (char *p = cleaned; *p; p++)
    {
        if (*p == '(')
        {
            paren_depth++;
            appendStringInfoChar(&current_hint, *p);
        }
        else if (*p == ')')
        {
            paren_depth--;
            appendStringInfoChar(&current_hint, *p);

            /* If we closed all parens, this hint is complete */
            if (paren_depth == 0 && current_hint.len > 0)
            {
                QueryBlockHint *hint = ParseSingleHint(current_hint.data);
                if (hint != NULL)
                    hints = lappend(hints, hint);

                /* Reset for next hint */
                resetStringInfo(&current_hint);

                /* Skip whitespace */
                while (*(p + 1) && isspace((unsigned char) *(p + 1)))
                    p++;
            }
        }
        else if (paren_depth > 0)
        {
            /* Inside parentheses, keep everything */
            appendStringInfoChar(&current_hint, *p);
        }
        else if (!isspace((unsigned char) *p))
        {
            /* Start of new hint name */
            appendStringInfoChar(&current_hint, *p);
        }
    }

    /* Handle any remaining hint */
    if (current_hint.len > 0)
    {
        QueryBlockHint *hint = ParseSingleHint(current_hint.data);
        if (hint != NULL)
            hints = lappend(hints, hint);
    }

    pfree(hint_copy);
    pfree(current_hint.data);

    return hints;
}

/*
 * FindHintsForBlock - Find all hints targeting a specific query block
 */
List *
FindHintsForBlock(const char *qb_name, List *all_hints)
{
    List     *block_hints = NIL;
    ListCell *lc;

    if (qb_name == NULL || all_hints == NIL)
        return NIL;

    foreach(lc, all_hints)
    {
        QueryBlockHint *hint = (QueryBlockHint *) lfirst(lc);

        /* Match QB_NAME or include global hints */
        if (hint->is_global ||
            (hint->qb_name[0] != '\0' &&
             strcmp(hint->qb_name, qb_name) == 0))
        {
            block_hints = lappend(block_hints, hint);
        }
    }

    return block_hints;
}

/*
 * ApplyQueryBlockHints - Apply hints to their respective query blocks
 */
void
ApplyQueryBlockHints(Query *query, List *query_blocks, List *hints)
{
    ListCell *block_lc;

    if (query == NULL || query_blocks == NIL || hints == NIL)
        return;

    /* Iterate through all query blocks */
    foreach(block_lc, query_blocks)
    {
        QueryBlockInfo *block = (QueryBlockInfo *) lfirst(block_lc);
        List           *block_hints;

        /* Find hints for this block */
        block_hints = FindHintsForBlock(block->qb_name, hints);

        if (block_hints != NIL)
        {
            ListCell *hint_lc;

            elog(DEBUG1, "pg_outline: Applying %d hints to block %s",
                 list_length(block_hints), block->qb_name);

            foreach(hint_lc, block_hints)
            {
                QueryBlockHint *hint = (QueryBlockHint *) lfirst(hint_lc);

                /* Mark as applied */
                hint->applied = true;

                /* Log hint application */
                elog(DEBUG2, "pg_outline: Applied hint '%s' to block %s",
                     hint->hint_text, block->qb_name);

                /*
                 * Actual hint application to PostgreSQL planner would happen here
                 * This would involve:
                 * - Parsing hint type (INDEX, FULL, LEADING, etc.)
                 * - Modifying Query structure
                 * - Setting planner parameters
                 * - Injecting path hints
                 */
            }
        }
    }
}

/*
 * GenerateComplexSignature - Generate signature for complex query with blocks
 */
char *
GenerateComplexSignature(const char *query_string, List *query_blocks)
{
    StringInfoData signature;
    ListCell      *lc;
    int            block_count;

    if (query_string == NULL)
        return NULL;

    initStringInfo(&signature);

    /* Add main query signature */
    appendStringInfo(&signature, "MAIN:");

    /* Normalize main query (simplified) */
    char *normalized = pstrdup(query_string);
    /* Remove extra whitespace */
    char *p = normalized;
    while (*p)
    {
        if (isspace((unsigned char) *p))
            *p = ' ';
        p++;
    }

    appendStringInfo(&signature, "%s", normalized);

    /* Add block structure information */
    block_count = list_length(query_blocks);
    appendStringInfo(&signature, "|BLOCKS:%d", block_count);

    /* Add each block's QB_NAME */
    foreach(lc, query_blocks)
    {
        QueryBlockInfo *block = (QueryBlockInfo *) lfirst(lc);
        appendStringInfo(&signature, "|%s", block->qb_name);
    }

    pfree(normalized);
    return signature.data;
}
