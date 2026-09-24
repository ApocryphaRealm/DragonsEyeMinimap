# Optional files - not shipped in the main package

`SKSE/Plugins/DragonsEyeMinimap-TimeWidget.ini` is the Time Widget add-on's switch: Dragon's Eye Minimap shows the
time widget only while this file sits next to `DragonsEyeMinimap.ini`. It lived in `dist\` until 1.7.3, but no
release ever shipped it (the widget is opt-in), and the package gate's "everything in dist ships" rule would have
started installing it for every player. It is kept here so an optional download or a FOMOD option can offer it.
