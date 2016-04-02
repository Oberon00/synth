#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys
import os
from os import path
from html import escape

OUT_TEMPLATE = """<!DOCTYPE html>
<html>
    <head>
        <meta charset="utf-8">
        <meta name="viewport" content="width=device-width">
        <title>Directory listing</title>
        <link rel="stylesheet" href="dirlisting.css" />
    </head>
    <body>
        <h1>Directory listing</h1>
        <ul>
            {0}
        </ul>
    </body>
</html>"""

# The empty one is for files like <iostream>
CEXTS = ("", "c", "cpp", "cxx", "inc", "h", "hh", "hpp", "c++", "ii", "i")

def stem(fname):
    return path.splitext(fname)[0]

def ext(fname):
    return path.splitext(fname)[1]

def write_dir_listing(indirname, outfname):
    lines = []
    outdirname = path.dirname(outfname)
    try:
        os.remove(outfname) # Avoid self-listing.
    except FileNotFoundError:
        pass

    for root, dirs, files in os.walk(indirname):
        def file_filter(f):
            stemName, extName = path.splitext(f)
            if extName != ".html":
                return False
            if ext(stemName)[1:].lower() not in CEXTS:
                return False
            return True

        files = list(filter(file_filter, files))
        if not files:
            continue
        files.sort()
        root = path.relpath(root, outdirname)
        lines.append('<li class="dir"><span class="dirname">{0}</span><ul>'.format(root))
        for file in files:
            lines.append('<li class="file"><a href="{0}">{1}</a></li>'
                    .format(escape(path.join(root, file)), escape(stem(file))))
        lines.append("</ul></li>")
    with open(outfname, "w", encoding="utf-8") as f:
        f.write(OUT_TEMPLATE.format("\n".join(lines)))


if __name__ == "__main__":
    if not (1 < len(sys.argv) < 4):
        sys.exit("Params: <inroot> [outfile]");
    indirname = sys.argv[1]
    if len(sys.argv) > 2:
        outfname = sys.argv[2]
    else:
        dn, bn = path.split(indirname)
        if not bn:
            bn = path.basename(dn)
        if not bn:
            bn = "out"
        outfname = bn + ".html"
    write_dir_listing(indirname, outfname)


