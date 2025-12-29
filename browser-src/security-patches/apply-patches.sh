#!/bin/bash
#
# Apply webOS WebKit Security Patches
#
# Usage: ./apply-patches.sh [--dry-run]
#

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BROWSER_SRC="$(dirname "$SCRIPT_DIR")"

DRY_RUN=""
if [ "$1" = "--dry-run" ]; then
    DRY_RUN="--dry-run"
    echo "=== DRY RUN MODE ==="
fi

echo "Browser source: $BROWSER_SRC"
echo ""

cd "$BROWSER_SRC"

PATCHES=(
    "001-rendering-null-checks.patch"
    "002-svg-uaf-protection.patch"
    "003-css-memory-safety.patch"
    "004-xss-dom-hardening.patch"
    "005-rtl-text-uaf.patch"
)

FAILED=0
APPLIED=0

for patch in "${PATCHES[@]}"; do
    PATCH_FILE="$SCRIPT_DIR/$patch"

    if [ ! -f "$PATCH_FILE" ]; then
        echo "[SKIP] $patch - file not found"
        continue
    fi

    echo -n "[APPLY] $patch... "

    if patch -p1 $DRY_RUN < "$PATCH_FILE" > /dev/null 2>&1; then
        echo "OK"
        ((APPLIED++))
    else
        echo "FAILED"
        ((FAILED++))

        # Show what went wrong
        echo "  Details:"
        patch -p1 $DRY_RUN < "$PATCH_FILE" 2>&1 | head -20 | sed 's/^/    /'
    fi
done

echo ""
echo "=== Summary ==="
echo "Applied: $APPLIED"
echo "Failed:  $FAILED"

if [ $FAILED -gt 0 ]; then
    echo ""
    echo "Some patches failed. This may be due to:"
    echo "  - Patches already applied"
    echo "  - Source code differences"
    echo "  - Missing files"
    echo ""
    echo "Try applying patches manually with:"
    echo "  patch -p1 < security-patches/XXX.patch"
    exit 1
fi

if [ -z "$DRY_RUN" ]; then
    echo ""
    echo "All patches applied successfully!"
    echo "Rebuild WebKit to include security fixes."
fi
