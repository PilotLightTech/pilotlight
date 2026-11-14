# Pilot Light icon

The sandbox uses the amber pilot flame on a charcoal tile as its window icon.

* `pilot-light.png`: original artwork, with transparent corners.
* `pilot-light-256.png`: application/taskbar/Dock image.
* `pilot-light-32.png`: small window/title-bar image.
* `pilot-light.ico`: Windows executable icon, containing 16, 24, 32, 48, 64, 128,
  and 256-pixel images.

Load the PNGs with `plImageI` requesting four channels and pass their RGBA pixels
to `plWindowI::set_icon`. The platform copies the pixels; release the image data
after the call. The sandbox demonstrates this in `sandbox/app.c`.

Windows and X11 support per-window icons. On macOS the icon applies to the whole
application in the Dock. Windows builds also compile `src/pilot_light.rc` and
link its resource into `pilot_light.exe`, so Explorer and shortcuts use the same
artwork. This is included in debug, test, and release builds and does not require
the PNGs or ICO beside the executable. macOS application-bundle packaging is
separate. Windows may temporarily cache an old Explorer/shortcut icon.

The optional Windows title bar uses `plWindowI::set_title_bar`, with a height in
logical pixels and packed `PL_COLOR_32_RGBA` colors. Passing `NULL` restores the
native bar. The flat bar has a centered title and a one-pixel frame; resize hit
areas extend slightly inside the edges for easier grabbing. The client area excludes the title bar, so renderer and UI input
coordinates stay unchanged. Native title-bar dragging, double-click maximize,
the system menu, and keyboard window commands remain available. The caption
buttons are drawn by the platform; the Windows 11 hover Snap Layouts popup is
not implemented. Other platforms retain their native title bars.
