#!/bin/sh
set -eu

preview_directory=$(dirname "$0")
preview_directory=$(cd "$preview_directory" >/dev/null && pwd -P)
exec "$preview_directory/angrybirds-preview.app/Contents/MacOS/angrybirds-preview" "$@"
