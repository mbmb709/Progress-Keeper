# Selected Object Coordinates v1.1.0

Geode HUD for the Geometry Dash level editor.

Select one object to see three distinct coordinate systems:
- **GD X/Y**: the object's actual level-space center/anchor in Geometry Dash units. This is *not* a screen coordinate.
- **Grid X/Y**: GD X/Y divided by 30, for blocks of 30 units.
- **Cell X/Y**: 1-based grid cell numbers, from the level's X/Y origin.

Examples:
- An ordinary block centered at (15,15) displays `GD X: 15 Y: 15`, `Grid X: 0.5 Y: 0.5` and `Cell X: 1 Y: 1`.
- Moving the object +30 units to the right updates GD X by +30, Grid X by +1, and Cell X by +1 (for standard block centers).

Values update automatically when the object moves. Deselect to hide the HUD. Multi-selection shows a count.

Fixes in v1.1.0: deduplicates object pointers in the two selection fields; explicitly labels GD, grid and cell coordinates to avoid unit confusion.

Targets GD 2.2081, Geode SDK 5.8.2, Android 64-bit and Windows.
