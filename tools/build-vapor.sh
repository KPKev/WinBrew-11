#!/usr/bin/env bash
# Build the complete combined application; no console deployment or publication.
set -euo pipefail
exec bash "$(dirname "$0")/build-vapor-cs2.sh" "$@"
