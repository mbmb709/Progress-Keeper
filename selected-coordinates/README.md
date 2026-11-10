# Selected Object Coordinates v1.2.0

## Object position
- **GD X/Y**: coordinates of the object's center/anchor in level units (30 units per block).
- **Grid X/Y**: 30-unit grid coordinates.
- **Cell X/Y**: 1-based indices of the 30-unit cell.

## Camera OFFSET trigger calculator
The **Camera Offset** trigger uses 10 per block, but level coordinates use 30 per block. Its values describe the camera's **relative displacement from the default camera center at trigger activation**, not the absolute world-space location.

1. Place or choose a marker object at the position the camera **normally centers on** at the point the trigger activates.
2. Select that reference object and click **Set Ref**.
3. Select the object at the camera center you WANT.
4. Read **Camera OFFSET X/Y**. The mod calculates `(destination - reference) / 3` for each axis.
5. Enter those two numbers into the **Camera Offset** trigger. If your current camera center is not accurately represented by the reference marker (due to player tracking, zoom, static cameras, or other camera triggers), the result will not correspond to the desired absolute screen center.

Select **Clear Ref** to reset. The reference is scoped to the editor session and is saved by coordinates rather than an object pointer. The HUD does not change the trigger; it just calculates values.

**Important:** This is NOT a direct absolute X/Y to Camera Offset conversion. A camera offset cannot guarantee the camera lands at a fixed world X/Y if the default camera location changes.

Target: Geometry Dash 2.2081, Geode 5.8.2, Android64/Windows builds.
