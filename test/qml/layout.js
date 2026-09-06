.pragma library

/* Nothing may stick out past the edge of the screen.
 *
 * The screens do not scroll — this is e-ink — so anything wider than the tab is
 * simply lost: a spacer once went in beside a Row that already applied its own
 * spacing, the figures row came out 21 px too wide, and the last letter of a
 * label went off the side. That is arithmetic, and arithmetic is checkable.
 *
 * Walks the whole tree in the tab's own coordinates. A subtree under an item
 * that clips is skipped: overflowing there is what clipping is for.
 */
function offenders(root) {
    var bad = [];

    function walk(item, clipped) {
        for (var i = 0; i < item.children.length; i++) {
            var child = item.children[i];
            /* Timers, Connections and the like are children too. */
            if (child.width === undefined || child.visible === undefined)
                continue;
            if (!child.visible || child.width <= 0) {
                continue;
            }
            if (!clipped) {
                var at = child.mapToItem(root, 0, 0);
                if (at.x < -0.5 || at.x + child.width > root.width + 0.5) {
                    bad.push(child + " spans " + Math.round(at.x) + ".."
                             + Math.round(at.x + child.width) + " of " + root.width);
                }
            }
            walk(child, clipped || child.clip === true);
        }
    }

    walk(root, root.clip === true);
    return bad;
}
