#ifndef _WINSR_SDL_H
#define _WINSR_SDL_H

extern void sdl_close(void);
extern int  sdl_inits(void);
extern int  sdl_inith(void);
extern int  sdl_initho(void);
extern int  sdl_pause(void);
extern void sdl_resize(int x, int y);
extern void sdl_enable(int enable);
extern void sdl_set_fs(int fs);
extern void sdl_reload(void);
extern void sdl_blit(int x, int y, int w, int h);

#endif /*_WINSR_SDL_H*/
