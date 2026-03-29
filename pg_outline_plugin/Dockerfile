FROM postgres:12

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    postgresql-server-dev-12 \
    git \
    && rm -rf /var/lib/apt/lists/*

# Copy plugin source code
COPY . /tmp/pg_outline_plugin

# Build and install the extension
WORKDIR /tmp/pg_outline_plugin
RUN make PG_CONFIG=/usr/lib/postgresql/12/bin/pg_config && \
    make install PG_CONFIG=/usr/lib/postgresql/12/bin/pg_config && \
    rm -rf /tmp/pg_outline_plugin

# Switch back to postgres user
USER postgres

# Initialize database with extension
COPY docker-entrypoint-initdb.d/* /docker-entrypoint-initdb.d/

EXPOSE 5432
