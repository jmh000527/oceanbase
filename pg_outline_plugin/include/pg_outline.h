/*-------------------------------------------------------------------------
 *
 * pg_outline.h
 *    Core header file for PostgreSQL Outline Plugin
 *    Based on OceanBase outline feature implementation
 *
 * This plugin provides execution plan stabilization by binding optimizer
 * hints to SQL queries without modifying application code.
 *
 *-------------------------------------------------------------------------
 */

#ifndef PG_OUTLINE_H
#define PG_OUTLINE_H

#include "postgres.h"
#include "nodes/pg_list.h"
#include "utils/hsearch.h"
#include "storage/lwlock.h"

/* Version */
#define PG_OUTLINE_VERSION "1.0.0"

/* Configuration parameters */
extern bool pg_outline_enabled;
extern int pg_outline_cache_size;
extern int pg_outline_reload_interval;
extern bool pg_outline_debug_log;

/* Outline information structure */
typedef struct OutlineInfo
{
    int32       outline_id;
    char        outline_name[128];
    char        *signature;          /* Normalized SQL */
    char        sql_id[65];          /* MD5 hash of signature */
    char        *outline_content;    /* Hint string */
    char        *sql_text;           /* Original SQL text */
    bool        enabled;
    bool        format_outline;      /* Format outline flag */
    Oid         owner;
    TimestampTz created_at;
    TimestampTz modified_at;

    /* Statistics */
    int64       usage_count;
    TimestampTz last_used_at;
} OutlineInfo;

/* Outline cache entry */
typedef struct OutlineCacheEntry
{
    char        key[65];             /* signature hash or sql_id */
    OutlineInfo *outline;
    bool        valid;
} OutlineCacheEntry;

/* Function declarations */

/* Initialization and cleanup */
extern void InitOutlineManager(void);
extern void ShutdownOutlineManager(void);
extern void RefreshOutlineCache(void);

/* Outline lookup */
extern OutlineInfo *LookupOutlineBySignature(const char *signature);
extern OutlineInfo *LookupOutlineBySqlId(const char *sql_id);
extern OutlineInfo *MatchOutline(const char *query_string);

/* Outline management */
extern bool CreateOutline(const char *name, const char *sql_text,
                         const char *hint_content);
extern bool AlterOutline(const char *name, const char *hint_content);
extern bool DropOutline(const char *name);
extern bool EnableOutline(const char *name, bool enable);

/* Statistics */
extern void RecordOutlineUsage(int32 outline_id);

/* Utility functions */
extern char *ComputeSqlId(const char *signature);
extern void LogOutlineMatch(OutlineInfo *outline, const char *query);

#endif /* PG_OUTLINE_H */
