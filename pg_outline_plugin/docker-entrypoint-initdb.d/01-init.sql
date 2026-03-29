-- Initialize pg_outline extension
CREATE EXTENSION pg_outline;

-- Configure default settings
ALTER SYSTEM SET pg_outline.enabled = on;
ALTER SYSTEM SET pg_outline.cache_size = 1000;
ALTER SYSTEM SET pg_outline.reload_interval = 60;
ALTER SYSTEM SET pg_outline.debug_log = off;

-- Reload configuration
SELECT pg_reload_conf();

-- Show extension info
SELECT * FROM pg_outline_info;
