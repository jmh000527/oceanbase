/*-------------------------------------------------------------------------
 *
 * pg_outline--1.0.sql
 *    SQL schema and functions for PostgreSQL Outline Plugin
 *
 * This file creates the necessary tables, functions, and views for
 * managing outlines in PostgreSQL.
 *
 *-------------------------------------------------------------------------
 */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pg_outline" to load this file. \quit

--
-- Outline storage table
--
CREATE TABLE pg_outline (
    outline_id      SERIAL PRIMARY KEY,
    outline_name    VARCHAR(128) UNIQUE NOT NULL,
    signature       TEXT NOT NULL,
    sql_id          VARCHAR(64) NOT NULL,
    outline_content TEXT NOT NULL,
    sql_text        TEXT,
    enabled         BOOLEAN DEFAULT TRUE NOT NULL,
    format_outline  BOOLEAN DEFAULT FALSE NOT NULL,
    owner           OID DEFAULT CURRENT_USER NOT NULL,
    created_at      TIMESTAMP WITH TIME ZONE DEFAULT NOW() NOT NULL,
    modified_at     TIMESTAMP WITH TIME ZONE DEFAULT NOW() NOT NULL,
    CONSTRAINT outline_content_not_empty CHECK (outline_content <> '')
);

-- Indexes for efficient lookup
CREATE INDEX idx_outline_signature ON pg_outline(signature) WHERE enabled = TRUE;
CREATE INDEX idx_outline_sql_id ON pg_outline(sql_id) WHERE enabled = TRUE;
CREATE INDEX idx_outline_enabled ON pg_outline(enabled) WHERE enabled = TRUE;
CREATE INDEX idx_outline_format ON pg_outline(format_outline, enabled) WHERE enabled = TRUE;

-- Comments
COMMENT ON TABLE pg_outline IS 'Storage for SQL execution plan outlines';
COMMENT ON COLUMN pg_outline.outline_id IS 'Unique outline identifier';
COMMENT ON COLUMN pg_outline.outline_name IS 'Human-readable outline name';
COMMENT ON COLUMN pg_outline.signature IS 'Normalized SQL signature for matching';
COMMENT ON COLUMN pg_outline.sql_id IS 'MD5 hash of signature';
COMMENT ON COLUMN pg_outline.outline_content IS 'Hint content to apply';
COMMENT ON COLUMN pg_outline.sql_text IS 'Original SQL text';
COMMENT ON COLUMN pg_outline.enabled IS 'Whether outline is active';
COMMENT ON COLUMN pg_outline.format_outline IS 'Whether this is a format outline';

--
-- Outline usage statistics table
--
CREATE TABLE pg_outline_stats (
    outline_id              INTEGER PRIMARY KEY REFERENCES pg_outline(outline_id) ON DELETE CASCADE,
    usage_count             BIGINT DEFAULT 0 NOT NULL,
    last_used_at            TIMESTAMP WITH TIME ZONE,
    total_execution_time_ms BIGINT DEFAULT 0,
    avg_execution_time_ms   NUMERIC(10, 2),
    CONSTRAINT usage_count_positive CHECK (usage_count >= 0)
);

COMMENT ON TABLE pg_outline_stats IS 'Usage statistics for outlines';

--
-- Outline management view
--
CREATE VIEW pg_outline_info AS
SELECT
    o.outline_id,
    o.outline_name,
    o.sql_id,
    o.enabled,
    o.format_outline,
    o.created_at,
    o.modified_at,
    o.owner,
    LENGTH(o.sql_text) as sql_length,
    LENGTH(o.outline_content) as hint_length,
    COALESCE(s.usage_count, 0) as usage_count,
    s.last_used_at,
    s.avg_execution_time_ms
FROM pg_outline o
LEFT JOIN pg_outline_stats s ON o.outline_id = s.outline_id;

COMMENT ON VIEW pg_outline_info IS 'Comprehensive outline information with statistics';

--
-- Function: pg_outline_create
-- Create a new outline
--
CREATE OR REPLACE FUNCTION pg_outline_create(
    p_outline_name VARCHAR,
    p_sql_text TEXT,
    p_hint_content TEXT,
    p_format_outline BOOLEAN DEFAULT FALSE
) RETURNS INTEGER AS $$
DECLARE
    v_signature TEXT;
    v_sql_id VARCHAR(64);
    v_outline_id INTEGER;
BEGIN
    -- Validate inputs
    IF p_outline_name IS NULL OR p_outline_name = '' THEN
        RAISE EXCEPTION 'Outline name cannot be empty';
    END IF;

    IF p_sql_text IS NULL OR p_sql_text = '' THEN
        RAISE EXCEPTION 'SQL text cannot be empty';
    END IF;

    IF p_hint_content IS NULL OR p_hint_content = '' THEN
        RAISE EXCEPTION 'Hint content cannot be empty';
    END IF;

    -- Check if outline name already exists
    IF EXISTS (SELECT 1 FROM pg_outline WHERE outline_name = p_outline_name) THEN
        RAISE EXCEPTION 'Outline % already exists', p_outline_name;
    END IF;

    -- Generate signature (simplified - in real implementation would call C function)
    v_signature := UPPER(REGEXP_REPLACE(p_sql_text, '\s+', ' ', 'g'));

    -- Generate SQL ID
    v_sql_id := MD5(v_signature);

    -- Insert outline
    INSERT INTO pg_outline (
        outline_name, signature, sql_id, outline_content,
        sql_text, enabled, format_outline, owner
    ) VALUES (
        p_outline_name, v_signature, v_sql_id, p_hint_content,
        p_sql_text, TRUE, p_format_outline, CURRENT_USER
    ) RETURNING outline_id INTO v_outline_id;

    -- Create stats entry
    INSERT INTO pg_outline_stats (outline_id) VALUES (v_outline_id);

    -- Refresh cache (would call C function)
    -- PERFORM pg_outline_refresh_cache();

    RAISE NOTICE 'Created outline % with ID %', p_outline_name, v_outline_id;

    RETURN v_outline_id;
END;
$$ LANGUAGE plpgsql;

COMMENT ON FUNCTION pg_outline_create IS 'Create a new outline for SQL execution plan control';

--
-- Function: pg_outline_alter
-- Modify an existing outline
--
CREATE OR REPLACE FUNCTION pg_outline_alter(
    p_outline_name VARCHAR,
    p_hint_content TEXT DEFAULT NULL,
    p_enabled BOOLEAN DEFAULT NULL
) RETURNS BOOLEAN AS $$
DECLARE
    v_updated BOOLEAN := FALSE;
BEGIN
    -- Validate outline exists
    IF NOT EXISTS (SELECT 1 FROM pg_outline WHERE outline_name = p_outline_name) THEN
        RAISE EXCEPTION 'Outline % does not exist', p_outline_name;
    END IF;

    -- Update hint content if provided
    IF p_hint_content IS NOT NULL THEN
        UPDATE pg_outline
        SET outline_content = p_hint_content,
            modified_at = NOW()
        WHERE outline_name = p_outline_name;
        v_updated := TRUE;
    END IF;

    -- Update enabled status if provided
    IF p_enabled IS NOT NULL THEN
        UPDATE pg_outline
        SET enabled = p_enabled,
            modified_at = NOW()
        WHERE outline_name = p_outline_name;
        v_updated := TRUE;
    END IF;

    IF v_updated THEN
        -- Refresh cache (would call C function)
        -- PERFORM pg_outline_refresh_cache();
        RAISE NOTICE 'Updated outline %', p_outline_name;
    END IF;

    RETURN v_updated;
END;
$$ LANGUAGE plpgsql;

COMMENT ON FUNCTION pg_outline_alter IS 'Modify an existing outline';

--
-- Function: pg_outline_drop
-- Drop an outline
--
CREATE OR REPLACE FUNCTION pg_outline_drop(
    p_outline_name VARCHAR
) RETURNS BOOLEAN AS $$
DECLARE
    v_deleted BOOLEAN := FALSE;
BEGIN
    DELETE FROM pg_outline WHERE outline_name = p_outline_name;

    GET DIAGNOSTICS v_deleted = ROW_COUNT;

    IF v_deleted THEN
        -- Refresh cache (would call C function)
        -- PERFORM pg_outline_refresh_cache();
        RAISE NOTICE 'Dropped outline %', p_outline_name;
    ELSE
        RAISE NOTICE 'Outline % does not exist', p_outline_name;
    END IF;

    RETURN v_deleted;
END;
$$ LANGUAGE plpgsql;

COMMENT ON FUNCTION pg_outline_drop IS 'Drop an outline';

--
-- Function: pg_outline_enable
-- Enable or disable an outline
--
CREATE OR REPLACE FUNCTION pg_outline_enable(
    p_outline_name VARCHAR,
    p_enabled BOOLEAN DEFAULT TRUE
) RETURNS BOOLEAN AS $$
BEGIN
    RETURN pg_outline_alter(p_outline_name, NULL, p_enabled);
END;
$$ LANGUAGE plpgsql;

COMMENT ON FUNCTION pg_outline_enable IS 'Enable or disable an outline';

--
-- Function: pg_outline_list
-- List all outlines with optional filters
--
CREATE OR REPLACE FUNCTION pg_outline_list(
    p_enabled_only BOOLEAN DEFAULT TRUE
) RETURNS TABLE (
    outline_name VARCHAR,
    sql_id VARCHAR,
    enabled BOOLEAN,
    usage_count BIGINT,
    last_used_at TIMESTAMP WITH TIME ZONE,
    created_at TIMESTAMP WITH TIME ZONE
) AS $$
BEGIN
    RETURN QUERY
    SELECT
        o.outline_name,
        o.sql_id,
        o.enabled,
        COALESCE(s.usage_count, 0),
        s.last_used_at,
        o.created_at
    FROM pg_outline o
    LEFT JOIN pg_outline_stats s ON o.outline_id = s.outline_id
    WHERE (NOT p_enabled_only OR o.enabled = TRUE)
    ORDER BY o.created_at DESC;
END;
$$ LANGUAGE plpgsql;

COMMENT ON FUNCTION pg_outline_list IS 'List outlines with optional filtering';

--
-- Function: pg_outline_export
-- Export outline definitions as JSON
--
CREATE OR REPLACE FUNCTION pg_outline_export(
    p_outline_name VARCHAR DEFAULT NULL
) RETURNS JSON AS $$
BEGIN
    RETURN (
        SELECT json_agg(
            json_build_object(
                'outline_name', outline_name,
                'sql_text', sql_text,
                'outline_content', outline_content,
                'enabled', enabled,
                'format_outline', format_outline,
                'created_at', created_at
            )
        )
        FROM pg_outline
        WHERE (p_outline_name IS NULL OR outline_name = p_outline_name)
    );
END;
$$ LANGUAGE plpgsql;

COMMENT ON FUNCTION pg_outline_export IS 'Export outline definitions as JSON';

--
-- Function: pg_outline_import
-- Import outline definitions from JSON
--
CREATE OR REPLACE FUNCTION pg_outline_import(
    p_json JSON
) RETURNS INTEGER AS $$
DECLARE
    v_outline JSON;
    v_count INTEGER := 0;
BEGIN
    FOR v_outline IN SELECT json_array_elements(p_json)
    LOOP
        -- Create outline from JSON
        PERFORM pg_outline_create(
            (v_outline->>'outline_name')::VARCHAR,
            v_outline->>'sql_text',
            v_outline->>'outline_content',
            COALESCE((v_outline->>'format_outline')::BOOLEAN, FALSE)
        );
        v_count := v_count + 1;
    END LOOP;

    RAISE NOTICE 'Imported % outlines', v_count;
    RETURN v_count;
EXCEPTION
    WHEN OTHERS THEN
        RAISE EXCEPTION 'Failed to import outlines: %', SQLERRM;
END;
$$ LANGUAGE plpgsql;

COMMENT ON FUNCTION pg_outline_import IS 'Import outline definitions from JSON';

--
-- Grant permissions (adjust as needed)
--
GRANT SELECT, INSERT, UPDATE, DELETE ON pg_outline TO PUBLIC;
GRANT SELECT, INSERT, UPDATE, DELETE ON pg_outline_stats TO PUBLIC;
GRANT USAGE, SELECT ON SEQUENCE pg_outline_outline_id_seq TO PUBLIC;
GRANT SELECT ON pg_outline_info TO PUBLIC;
