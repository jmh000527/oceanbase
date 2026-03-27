/*-------------------------------------------------------------------------
 *
 * outline_hints.h
 *    Hint application interface for outlines
 *    Integrates with pg_hint_plan for applying hints to queries
 *
 *-------------------------------------------------------------------------
 */

#ifndef OUTLINE_HINTS_H
#define OUTLINE_HINTS_H

#include "postgres.h"
#include "nodes/parsenodes.h"

/* Hint state structure */
typedef struct OutlineHintState
{
    char        *hint_string;
    List        *parsed_hints;
    bool        applied;
    bool        valid;
} OutlineHintState;

/* Function declarations */

/* Hint parsing and application */
extern OutlineHintState *ParseOutlineHints(const char *hint_content);
extern bool ApplyOutlineHints(OutlineHintState *hstate, Query *query);
extern void SetHintForQuery(const char *hint_string);

/* Hint validation */
extern bool ValidateHintString(const char *hint_content);
extern char *ExtractHintsFromComment(const char *sql_text);

/* Cleanup */
extern void FreeHintState(OutlineHintState *hstate);

/* Integration with pg_hint_plan */
extern void InitHintPlanIntegration(void);
extern bool IsPgHintPlanAvailable(void);

#endif /* OUTLINE_HINTS_H */
