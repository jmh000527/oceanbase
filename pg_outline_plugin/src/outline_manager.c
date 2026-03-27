/*-------------------------------------------------------------------------
 *
 * outline_manager.c
 *    Outline storage, retrieval and cache management
 *
 * This module manages outline definitions in memory and provides
 * efficient lookup mechanisms using hash tables.
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "pg_outline.h"
#include "outline_normalize.h"

#include "access/htup_details.h"
#include "access/xact.h"
#include "catalog/pg_type.h"
#include "executor/spi.h"
#include "funcapi.h"
#include "lib/stringinfo.h"
#include "storage/ipc.h"
#include "storage/lwlock.h"
#include "storage/shmem.h"
#include "utils/builtins.h"
#include "utils/hsearch.h"
#include "utils/memutils.h"
#include "common/md5.h"

#include <string.h>

/* Global variables */
bool pg_outline_enabled = true;
int pg_outline_cache_size = 1000;
int pg_outline_reload_interval = 60;
bool pg_outline_debug_log = false;

/* Hash tables for outline cache */
static HTAB *outline_signature_hash = NULL;
static HTAB *outline_sqlid_hash = NULL;
static HTAB *outline_name_hash = NULL;

/* Memory context for outlines */
static MemoryContext OutlineContext = NULL;

/* Statistics counters */
static int outline_cache_hits = 0;
static int outline_cache_misses = 0;

/*
 * ComputeSqlId - Compute MD5 hash of signature for SQL ID
 */
char *
ComputeSqlId(const char *signature)
{
    pg_md5_ctx  md5_context;
    uint8       digest[MD5_DIGEST_LENGTH];
    char        *result;
    int         i;

    if (signature == NULL)
        return NULL;

    result = (char *) palloc(MD5_DIGEST_STRING_LENGTH);

    pg_md5_init(&md5_context);
    pg_md5_update(&md5_context, (uint8 *) signature, strlen(signature));
    pg_md5_final(digest, &md5_context);

    /* Convert binary digest to hex string */
    for (i = 0; i < MD5_DIGEST_LENGTH; i++)
        sprintf(result + i * 2, "%02x", digest[i]);
    result[MD5_DIGEST_STRING_LENGTH - 1] = '\0';

    return result;
}

/*
 * InitOutlineManager - Initialize outline manager
 */
void
InitOutlineManager(void)
{
    HASHCTL     hash_ctl;

    /* Create memory context */
    if (OutlineContext == NULL)
    {
        OutlineContext = AllocSetContextCreate(TopMemoryContext,
                                              "OutlineContext",
                                              ALLOCSET_DEFAULT_SIZES);
    }

    /* Initialize signature hash table */
    MemSet(&hash_ctl, 0, sizeof(hash_ctl));
    hash_ctl.keysize = 65;  /* MD5 hash size + 1 */
    hash_ctl.entrysize = sizeof(OutlineCacheEntry);
    hash_ctl.hcxt = OutlineContext;

    outline_signature_hash = hash_create("Outline Signature Hash",
                                        pg_outline_cache_size,
                                        &hash_ctl,
                                        HASH_ELEM | HASH_CONTEXT);

    /* Initialize SQL ID hash table */
    outline_sqlid_hash = hash_create("Outline SQL ID Hash",
                                    pg_outline_cache_size,
                                    &hash_ctl,
                                    HASH_ELEM | HASH_CONTEXT);

    /* Initialize name hash table */
    hash_ctl.keysize = 128;
    outline_name_hash = hash_create("Outline Name Hash",
                                   pg_outline_cache_size,
                                   &hash_ctl,
                                   HASH_ELEM | HASH_CONTEXT);

    elog(LOG, "pg_outline: Outline manager initialized (cache_size=%d)",
         pg_outline_cache_size);
}

/*
 * ShutdownOutlineManager - Shutdown outline manager
 */
void
ShutdownOutlineManager(void)
{
    if (outline_signature_hash)
    {
        hash_destroy(outline_signature_hash);
        outline_signature_hash = NULL;
    }

    if (outline_sqlid_hash)
    {
        hash_destroy(outline_sqlid_hash);
        outline_sqlid_hash = NULL;
    }

    if (outline_name_hash)
    {
        hash_destroy(outline_name_hash);
        outline_name_hash = NULL;
    }

    if (OutlineContext)
    {
        MemoryContextDelete(OutlineContext);
        OutlineContext = NULL;
    }

    elog(LOG, "pg_outline: Outline manager shutdown (hits=%d, misses=%d)",
         outline_cache_hits, outline_cache_misses);
}

/*
 * LoadOutlineFromTable - Load outline from pg_outline table
 */
static OutlineInfo *
LoadOutlineFromTable(const char *key, const char *key_column)
{
    int             ret;
    StringInfoData  query;
    OutlineInfo    *outline = NULL;
    MemoryContext   oldcontext;

    if (!pg_outline_enabled)
        return NULL;

    /* Connect to SPI */
    if ((ret = SPI_connect()) < 0)
        elog(ERROR, "pg_outline: SPI_connect failed: %d", ret);

    /* Build query */
    initStringInfo(&query);
    appendStringInfo(&query,
                    "SELECT outline_id, outline_name, signature, sql_id, "
                    "outline_content, sql_text, enabled, format_outline, "
                    "owner, created_at, modified_at "
                    "FROM pg_outline WHERE %s = $1 AND enabled = true",
                    key_column);

    /* Execute query */
    ret = SPI_execute_with_args(query.data, 1,
                               (Oid[]) { TEXTOID },
                               (Datum[]) { CStringGetTextDatum(key) },
                               NULL, true, 1);

    if (ret == SPI_OK_SELECT && SPI_processed > 0)
    {
        HeapTuple   tup = SPI_tuptable->vals[0];
        TupleDesc   tupdesc = SPI_tuptable->tupdesc;
        bool        isnull;

        /* Switch to OutlineContext */
        oldcontext = MemoryContextSwitchTo(OutlineContext);

        /* Allocate outline structure */
        outline = (OutlineInfo *) palloc0(sizeof(OutlineInfo));

        /* Extract fields */
        outline->outline_id = DatumGetInt32(SPI_getbinval(tup, tupdesc, 1, &isnull));

        strncpy(outline->outline_name,
               TextDatumGetCString(SPI_getbinval(tup, tupdesc, 2, &isnull)),
               127);
        outline->outline_name[127] = '\0';

        outline->signature = TextDatumGetCString(SPI_getbinval(tup, tupdesc, 3, &isnull));

        strncpy(outline->sql_id,
               TextDatumGetCString(SPI_getbinval(tup, tupdesc, 4, &isnull)),
               64);
        outline->sql_id[64] = '\0';

        outline->outline_content = TextDatumGetCString(SPI_getbinval(tup, tupdesc, 5, &isnull));
        outline->sql_text = TextDatumGetCString(SPI_getbinval(tup, tupdesc, 6, &isnull));
        outline->enabled = DatumGetBool(SPI_getbinval(tup, tupdesc, 7, &isnull));
        outline->format_outline = DatumGetBool(SPI_getbinval(tup, tupdesc, 8, &isnull));

        outline->owner = DatumGetObjectId(SPI_getbinval(tup, tupdesc, 9, &isnull));
        outline->created_at = DatumGetTimestampTz(SPI_getbinval(tup, tupdesc, 10, &isnull));
        outline->modified_at = DatumGetTimestampTz(SPI_getbinval(tup, tupdesc, 11, &isnull));

        outline->usage_count = 0;
        outline->last_used_at = 0;

        MemoryContextSwitchTo(oldcontext);
    }

    SPI_finish();
    pfree(query.data);

    return outline;
}

/*
 * CacheOutline - Add outline to cache
 */
static void
CacheOutline(OutlineInfo *outline)
{
    OutlineCacheEntry *entry;
    bool            found;

    if (outline == NULL || outline_signature_hash == NULL)
        return;

    /* Cache by signature */
    if (outline->signature && *outline->signature)
    {
        char sig_key[65];
        char *sig_hash = ComputeSqlId(outline->signature);

        strncpy(sig_key, sig_hash, 64);
        sig_key[64] = '\0';

        entry = (OutlineCacheEntry *) hash_search(outline_signature_hash,
                                                  sig_key,
                                                  HASH_ENTER,
                                                  &found);
        if (entry)
        {
            entry->outline = outline;
            entry->valid = true;
        }

        pfree(sig_hash);
    }

    /* Cache by SQL ID */
    if (outline->sql_id && *outline->sql_id)
    {
        entry = (OutlineCacheEntry *) hash_search(outline_sqlid_hash,
                                                  outline->sql_id,
                                                  HASH_ENTER,
                                                  &found);
        if (entry)
        {
            entry->outline = outline;
            entry->valid = true;
        }
    }

    /* Cache by name */
    if (outline->outline_name && *outline->outline_name)
    {
        entry = (OutlineCacheEntry *) hash_search(outline_name_hash,
                                                  outline->outline_name,
                                                  HASH_ENTER,
                                                  &found);
        if (entry)
        {
            entry->outline = outline;
            entry->valid = true;
        }
    }
}

/*
 * LookupOutlineBySignature - Lookup outline by SQL signature
 */
OutlineInfo *
LookupOutlineBySignature(const char *signature)
{
    OutlineCacheEntry *entry;
    OutlineInfo      *outline;
    char              *sig_hash;
    char              sig_key[65];

    if (signature == NULL || *signature == '\0')
        return NULL;

    if (!pg_outline_enabled || outline_signature_hash == NULL)
        return NULL;

    /* Compute signature hash */
    sig_hash = ComputeSqlId(signature);
    strncpy(sig_key, sig_hash, 64);
    sig_key[64] = '\0';

    /* Check cache first */
    entry = (OutlineCacheEntry *) hash_search(outline_signature_hash,
                                             sig_key,
                                             HASH_FIND,
                                             NULL);

    if (entry && entry->valid)
    {
        outline_cache_hits++;
        pfree(sig_hash);
        return entry->outline;
    }

    outline_cache_misses++;

    /* Load from database */
    outline = LoadOutlineFromTable(signature, "signature");

    if (outline)
        CacheOutline(outline);

    pfree(sig_hash);
    return outline;
}

/*
 * LookupOutlineBySqlId - Lookup outline by SQL ID
 */
OutlineInfo *
LookupOutlineBySqlId(const char *sql_id)
{
    OutlineCacheEntry *entry;
    OutlineInfo      *outline;

    if (sql_id == NULL || *sql_id == '\0')
        return NULL;

    if (!pg_outline_enabled || outline_sqlid_hash == NULL)
        return NULL;

    /* Check cache first */
    entry = (OutlineCacheEntry *) hash_search(outline_sqlid_hash,
                                             sql_id,
                                             HASH_FIND,
                                             NULL);

    if (entry && entry->valid)
    {
        outline_cache_hits++;
        return entry->outline;
    }

    outline_cache_misses++;

    /* Load from database */
    outline = LoadOutlineFromTable(sql_id, "sql_id");

    if (outline)
        CacheOutline(outline);

    return outline;
}

/*
 * RefreshOutlineCache - Clear and reload outline cache
 */
void
RefreshOutlineCache(void)
{
    HASH_SEQ_STATUS status;
    OutlineCacheEntry *entry;

    if (!pg_outline_enabled)
        return;

    elog(DEBUG1, "pg_outline: Refreshing outline cache");

    /* Clear all hash tables */
    if (outline_signature_hash)
    {
        hash_seq_init(&status, outline_signature_hash);
        while ((entry = (OutlineCacheEntry *) hash_seq_search(&status)) != NULL)
        {
            entry->valid = false;
        }
    }

    if (outline_sqlid_hash)
    {
        hash_seq_init(&status, outline_sqlid_hash);
        while ((entry = (OutlineCacheEntry *) hash_seq_search(&status)) != NULL)
        {
            entry->valid = false;
        }
    }

    if (outline_name_hash)
    {
        hash_seq_init(&status, outline_name_hash);
        while ((entry = (OutlineCacheEntry *) hash_seq_search(&status)) != NULL)
        {
            entry->valid = false;
        }
    }

    /* Reset statistics */
    outline_cache_hits = 0;
    outline_cache_misses = 0;

    elog(DEBUG1, "pg_outline: Cache refreshed");
}

/*
 * RecordOutlineUsage - Record outline usage statistics
 */
void
RecordOutlineUsage(int32 outline_id)
{
    int             ret;
    StringInfoData  query;

    if (!pg_outline_enabled || outline_id <= 0)
        return;

    /* Connect to SPI */
    if ((ret = SPI_connect()) < 0)
        return;

    /* Update usage statistics */
    initStringInfo(&query);
    appendStringInfo(&query,
                    "UPDATE pg_outline_stats "
                    "SET usage_count = usage_count + 1, "
                    "last_used_at = NOW() "
                    "WHERE outline_id = $1");

    SPI_execute_with_args(query.data, 1,
                         (Oid[]) { INT4OID },
                         (Datum[]) { Int32GetDatum(outline_id) },
                         NULL, false, 0);

    SPI_finish();
    pfree(query.data);
}

/*
 * LogOutlineMatch - Log outline match for debugging
 */
void
LogOutlineMatch(OutlineInfo *outline, const char *query)
{
    if (!pg_outline_debug_log || outline == NULL)
        return;

    elog(LOG, "pg_outline: Matched outline '%s' (id=%d) for query: %.100s...",
         outline->outline_name,
         outline->outline_id,
         query ? query : "(null)");
}
