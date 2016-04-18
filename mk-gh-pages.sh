#!/bin/sh
set -ex

SYNTH_SRC_ROOT="`dirname $0`"
SYNTH_BIN_ROOT=`pwd`
SYNTH_PG_ROOT="$SYNTH_SRC_ROOT/../synth-pages"

LLVM_LIB_ROOT="/usr/lib/llvm-3.9"
STDTAGS="$SYNTH_BIN_ROOT/cppreference-doxygen-web.tag.xml"

if [ ! -d "$SYNTH_PG_ROOT" ]; then
    SYNTH_PG_ROOT="$SYNTH_BIN_ROOT/../synth-pages"
fi

cd "$SYNTH_SRC_ROOT"
SRC_REV=`git rev-parse HEAD`

cd "$SYNTH_PG_ROOT"

rm -r * || echo "Nothing to remove."

"$SYNTH_BIN_ROOT/src/synth" \
    "$SYNTH_SRC_ROOT" "$LLVM_LIB_ROOT" -o lib-llvm/ \
    --doxytags  "$STDTAGS" http://en.cppreference.com/w/ \
    --db "$SYNTH_BIN_ROOT"
"$SYNTH_SRC_ROOT/dir2html.py" . index.html
cp -R "$SYNTH_SRC_ROOT/html-resources/"* .
git add -A
git commit --amend -m "Match $SRC_REV."
