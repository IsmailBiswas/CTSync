# Use a small base image
FROM debian:bookworm-slim AS build-stage

# Set working directory
WORKDIR /app

# Install dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake libssl-dev uuid-dev uthash-dev libsqlite3-dev libcjson-dev openssl \
    && rm -rf /var/lib/apt/lists/*

# Copy backend source code
COPY . /app/backend

# Build the server
RUN cd /app/backend && mkdir -p build && cd build && cmake .. && make

RUN /app/backend/compose/gen_server_files.sh

# Second stage with a minimal runtime environment
FROM debian:bookworm-slim AS runtime-stage

# Set working directory
WORKDIR /app

# Install runtime dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    libssl3 sqlite3 uuid-runtime libcjson-dev tini \
    && rm -rf /var/lib/apt/lists/*

# Copy the built server binary from the first stage
COPY --from=build-stage /app/backend/build/CTSync-server /app/CTSync-server

# Copies pre-built server files and the **certificate** for immediate use if no custom files are mounted.
COPY --from=build-stage /app/server_files /app/server_files

# Run the compiled server binary
ENTRYPOINT ["tini", "--", "stdbuf", "-oL", "-eL", "/app/CTSync-server"]
