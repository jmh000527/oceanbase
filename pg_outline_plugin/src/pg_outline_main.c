/*-------------------------------------------------------------------------
 *
 * pg_outline_main.c
 *    Main entry point for PostgreSQL Outline Plugin
 *
 * This module implements the extension initialization and hooks
 * into PostgreSQL's planner to apply outlines.
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "pg_outline.h"
#include "outline_matcher.h"
#include "outline_hints.h"

#include "commands/explain.h"
#include "optimizer/planner.h"
#include "parser/analyze.h"
#include "tcop/utility.h"
#include "utils/guc.h"

PG_MODULE_MAGIC;

/* Saved hook values */
static planner_hook_type prev_planner_hook = NULL;
static post_parse_analyze_hook_type prev_post_parse_analyze_hook = NULL;

/* Function declarations */
void _PG_init(void);
void _PG_fini(void);

static PlannedStmt *pg_outline_planner_hook(Query *parse,
                                           const char *query_string,
                                           int cursorOptions,
                                           ParamListInfo boundParams);

static void pg_outline_post_parse_hook(ParseState *pstate,
                                      Query *query,
                                      JumbleState *jstate);

/*-*-
 * _PG_init - Extension initialization
 *
 * Called when the extension is loaded. Sets up hooks and configuration.
 */
void
_PG_init(void)
{
    elog(LOG, "pg_outline: Initializing extension version %s", PG_OUTLINE_VERSION);

    /* Define custom GUC variables */
    DefineCustomBoolVariable("pg_outline.enabled",
                            "Enable outline feature",
                            "When enabled, queries will be matched against defined outlines.",
                            &pg_outline_enabled,
                            true,
                            PGC_SUSET,
                            0,
                            NULL, NULL, NULL);

    DefineCustomIntVariable("pg_outline.cache_size",
                           "Maximum number of outlines to cache",
                           "Controls the size of the outline cache.",
                           &pg_outline_cache_size,
                           1000,
                           100,
                           10000,
                           PGC_SUSET,
                           0,
                           NULL, NULL, NULL);

    DefineCustomIntVariable("pg_outline.reload_interval",
                           "Outline cache reload interval in seconds",
                           "How often to reload the outline cache from the database.",
                           &pg_outline_reload_interval,
                           60,
                           0,
                           3600,
                           PGC_SUSET,
                           0,
                           NULL, NULL, NULL);

    DefineCustomBoolVariable("pg_outline.debug_log",
                            "Enable debug logging",
                            "When enabled, logs detailed information about outline matching.",
                            &pg_outline_debug_log,
                            false,
                            PGC_SUSET,
                            0,
                            NULL, NULL, NULL);

    /* Initialize outline manager */
    InitOutlineManager();

    /* Initialize hint plan integration */
    InitHintPlanIntegration();

    /* Install hooks */
    prev_planner_hook = planner_hook;
    planner_hook = pg_outline_planner_hook;

    prev_post_parse_analyze_hook = post_parse_analyze_hook;
    post_parse_analyze_hook = pg_outline_post_parse_hook;

    elog(LOG, "pg_outline: Extension initialization complete");
}

/*-*-
 * _PG_fini - Extension cleanup
 *
 * Called when the extension is unloaded. Restores hooks and cleans up.
 */
void
_PG_fini(void)
{
    elog(LOG, "pg_outline: Shutting down extension");

    /* Restore hooks */
    planner_hook = prev_planner_hook;
    post_parse_analyze_hook = prev_post_parse_analyze_hook;

    /* Shutdown outline manager */
    ShutdownOutlineManager();

    elog(LOG, "pg_outline: Extension shutdown complete");
}

/*-*-
 * pg_outline_planner_hook - Planner hook for outline application
 *
 * This hook is called during query planning. It matches the query against
 * outlines and applies hints if a match is found.
 */
static PlannedStmt *
pg_outline_planner_hook(Query *parse,
                       const char *query_string,
                       int cursorOptions,
                       ParamListInfo boundParams)
{
    OutlineMatchResult *match_result = NULL;
    PlannedStmt        *result = NULL;
    bool                outline_applied = false;

    /* Try to match an outline if feature is enabled */
    if (pg_outline_enabled && parse && query_string)
    {
        /* Match outline for this query */
        match_result = MatchOutlineForQuery(query_string, parse);

        if (match_result && match_result->matched && match_result->outline)
        {
            OutlineInfo *outline = match_result->outline;

            /* Log the match */
            LogOutlineMatch(outline, query_string);

            /* Apply outline hints */
            if (outline->outline_content && *outline->outline_content)
            {
                /* Set hints for pg_hint_plan or our hint system */
                SetHintForQuery(outline->outline_content);
                outline_applied = true;

                if (pg_outline_debug_log)
                {
                    elog(LOG, "pg_outline: Applied hints: %s",
                         outline->outline_content);
                }
            }

            /* Record usage statistics */
            RecordOutlineUsage(outline->outline_id);
        }

        /* Clean up match result */
        if (match_result)
            FreeMatchResult(match_result);
    }

    /* Call the previous hook or standard planner */
    if (prev_planner_hook)
    {
        result = prev_planner_hook(parse, query_string,
                                  cursorOptions, boundParams);
    }
    else
    {
        result = standard_planner(parse, query_string,
                                 cursorOptions, boundParams);
    }

    /* Add outline information to plan if applied */
    if (outline_applied && result)
    {
        /* Could add custom fields to track outline usage */
        if (pg_outline_debug_log)
        {
            elog(DEBUG1, "pg_outline: Plan generated with outline hints");
        }
    }

    return result;
}

/*-*-
 * pg_outline_post_parse_hook - Post-parse analysis hook
 *
 * This hook is called after query parsing. Can be used for additional
 * validation or preprocessing.
 */
static void
pg_outline_post_parse_hook(ParseState *pstate,
                          Query *query,
                          JumbleState *jstate)
{
    /* Call previous hook if exists */
    if (prev_post_parse_analyze_hook)
    {
        prev_post_parse_analyze_hook(pstate, query, jstate);
    }

    /* Additional post-parse processing could be added here */
    /* For example: early outline matching, query validation, etc. */
}

/*-*-
 * MatchOutline - High-level API for outline matching
 *
 * This function provides a simple interface for matching outlines
 * from outside the planner hook.
 */
OutlineInfo *
MatchOutline(const char *query_string)
{
    OutlineMatchResult *match_result;
    OutlineInfo        *outline = NULL;

    if (!pg_outline_enabled || query_string == NULL)
        return NULL;

    match_result = MatchOutlineForQuery(query_string, NULL);

    if (match_result && match_result->matched)
    {
        outline = match_result->outline;
    }

    if (match_result)
        FreeMatchResult(match_result);

    return outline;
}
