# HammerPatch

The program can be downloaded [here](https://github.com/crashfort/HammerPatch/releases). Visit [here](https://twitch.streamlabs.com/crashfort/) if you wish to support the development.

## Installing
Both `HammerPatch.dll` and `HammerPatchLauncher.exe` should go in the same directory as `hammer.exe`. This will be in the `bin` directory of each Source game. Examples:

* steamapps\common\Counter-Strike Source\bin\
* steamapps\common\Half-Life 2\bin\

Instead of using `hammer.exe` to start, you should now use `HammerPatchLauncher.exe`.

**This will not magically fix existing corrupted brushes, it will only have a change on brushes that were saved with this.**

## Vertices moving on load
In default Hammer, unless your geometry is of perfectly straight angles, the vertices will move every time you open the map. This is because the vertices' positions are recalculated every time from plane points. This is a lossy process and will only get worse every time the map is loaded.

In HammerPatch, all vertices are saved separately and are restored on load to overwrite Hammer's estimations. Here are some images that illustrate this problem.

### Figure 1
Say you have a simple default primitive. This is a cylinder with 32 sides.

![Shape 1](https://raw.githubusercontent.com/crashfort/HammerPatch/master/Images/Shape1.png)

### Figure 2
In default Hammer, this is what you'd see if you zoom in on such primitive after loading a map of one iteration.

![Shape 1 zoom default](https://raw.githubusercontent.com/crashfort/HammerPatch/master/Images/Shape1_Zoom_Default.png)

### Figure 3
Here's the same shape but when saved and loaded with HammerPatch. It is the exact same vertices as it was created with.

![Shape 1 zoom HammerPatch](https://raw.githubusercontent.com/crashfort/HammerPatch/master/Images/Shape1_Zoom_HammerPatch.png)

### Figure 4
This problem gets more problematic on advanced shapes.

![Shape 2](https://raw.githubusercontent.com/crashfort/HammerPatch/master/Images/Shape2.png)

### Figure 5
Again, this is what it looks like in default Hammer after one save iteration.

![Shape 2 zoom default](https://raw.githubusercontent.com/crashfort/HammerPatch/master/Images/Shape2_Zoom_Default.png)

### Figure 6
The same map as saved and loaded by HammerPatch.

![Shape 2 zoom HammerPatch](https://raw.githubusercontent.com/crashfort/HammerPatch/master/Images/Shape2_Zoom_HammerPatch.png)
