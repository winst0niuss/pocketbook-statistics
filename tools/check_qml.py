#!/usr/bin/env python3
"""Catches QML mistakes that qmllint accepts and the engine then rejects.

A duplicate function or id makes the component fail to instantiate, which on a
reader looks like the app no longer opening: main.cpp exits when the scene has
no root object, and there is no console to say why.

So does a property named after one the base type already has. QQuickItem marks
x, y, width, height and their neighbours FINAL, and a `property var y` on an
Item is a compile error for the whole document — which takes the file that
imports it down with it. qmllint reports that as a warning, and only its exit
code gates the build, so it shipped in 1.7.0-rc6 as an app that would not open.
"""
import collections
import glob
import os
import re
import sys

QML_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                       "qt", "qml")

def duplicates(names):
    return [n for n, count in collections.Counter(names).items() if count > 1]

# Properties QQuickItem (and the types built on it) already define. Declaring
# one of these in QML is "Cannot override FINAL property" at compile time.
RESERVED = {
    "x", "y", "z", "width", "height", "opacity", "visible", "enabled", "state",
    "parent", "data", "children", "resources", "anchors", "rotation", "scale",
    "transform", "transformOrigin", "clip", "focus", "activeFocus", "smooth",
    "antialiasing", "implicitWidth", "implicitHeight", "childrenRect",
    "baselineOffset", "layer", "containmentMask", "states", "transitions",
}

problems = []
for path in sorted(glob.glob(os.path.join(QML_DIR, "*.qml"))):
    src = open(path, encoding="utf-8").read()
    name = os.path.basename(path)
    for fn in duplicates(re.findall(r'^\s*function\s+(\w+)\s*\(', src, re.M)):
        problems.append("%s: function %s declared twice" % (name, fn))
    for ident in duplicates(re.findall(r'^\s*id:\s*(\w+)\s*$', src, re.M)):
        problems.append("%s: id %s used twice" % (name, ident))
    # The value is optional: `property var y` on its own line is the same
    # compile error as one with a binding behind it.
    for prop in re.findall(r'^\s*(?:readonly\s+|default\s+|required\s+)*property\s+\w+(?:<[^>]+>)?\s+(\w+)\s*(?::|$)',
                           src, re.M):
        if prop in RESERVED:
            problems.append("%s: property %s overrides a built-in one" % (name, prop))
    if src.count("{") != src.count("}"):
        problems.append("%s: braces don't balance" % name)

for p in problems:
    print("qmlcheck: " + p, file=sys.stderr)
print("qmlcheck: %d files, %d problems" % (len(glob.glob(os.path.join(QML_DIR, "*.qml"))),
                                           len(problems)))
sys.exit(1 if problems else 0)
