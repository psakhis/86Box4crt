# 86Box4crt
86Box fork for 15khz
 
https://github.com/86Box/86Box

This fork is a very accurate version of 86Box with additional features:
 - Exclusive fullscreen (no windowed or borderless modes) so Low input lag
 - Added vid_display for render on another monitor
 - KMS version on Linux / Console version on Windows (no gui)
 - Switchres integrated to switch resolutions on the fly (see https://github.com/antonioginer/switchres)
 - Can work with native resolutions or super resolutions (configure it on switchres.ini)
 - 320x200 mode 13h patched on VGA card to work with 60hz (no tearing)

Supported vid_renderers
 - sdl_opengl
 - opengl_core (use it for shaders or Linux X)

To exit, press mouse middle button
 
You need to configure your machines with 86Box regular version. 

On Linux, GroovyArcade is recommended on KMS. (see https://github.com/substring/os)
