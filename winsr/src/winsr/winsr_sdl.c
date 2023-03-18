#include <SDL.h>
#include <SDL_messagebox.h>

#include <time.h>

#ifndef timersub
#define timersub(a, b, result) \
        do { \
                (result)->tv_sec = (a)->tv_sec - (b)->tv_sec; \
                (result)->tv_usec = (a)->tv_usec - (b)->tv_usec; \
                if ((result)->tv_usec < 0) { \
                        --(result)->tv_sec; \
                        (result)->tv_usec += 1000000; \
                } \
        } while (0)
#endif // timersub

#include <sys/time.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/param.h>

/* This #undef is needed because a SDL include header redefines HAVE_STDARG_H. */
#undef HAVE_STDARG_H
#define HAVE_STDARG_H
#include <86box/86box.h>
#include <86box/device.h>
#include <86box/plat.h>
#include <86box/plat_dynld.h>
#include <86box/video.h>
#include <86box/ui.h>
#include <86box/version.h>
#include <86box/winsr_sdl.h>

#include <86box/switchres_wrapper2.h> //psakhis

#define RENDERER_FULL_SCREEN 1
#define RENDERER_HARDWARE    2
#define RENDERER_OPENGL      4

typedef struct sdl_blit_params {
    int x, y, w, h;
} sdl_blit_params;
extern sdl_blit_params params;
extern int             blitreq;

static SDL_Window    *sdl_win    = NULL;
static SDL_Renderer  *sdl_render = NULL;
static SDL_Texture *sdl_tex    = NULL;
int                 sdl_w = SCREEN_RES_X, sdl_h = SCREEN_RES_Y;
static int          sdl_fs, sdl_flags           = -1;
static int          cur_w, cur_h;
static int          cur_wx = 0, cur_wy = 0, cur_ww = 0, cur_wh = 0;
static volatile int sdl_enabled = 1;
static SDL_mutex   *sdl_mutex   = NULL;

//int                 mouse_capture; /* win_mouse */

int                 title_set         = 0;
int                 resize_pending    = 0;
int                 resize_w          = 0;
int                 resize_h          = 0;
double              mouse_sensitivity = 1.0;                  /* Unused. */
double              mouse_x_error = 0.0, mouse_y_error = 0.0; /* Unused. */
static uint8_t      interpixels[17842176];

//psakhis
static unsigned char  retSR; 
static int          sr_real_width = 0;     
static int          sr_real_height = 0;
static int          sr_last_width = 0;     
static int          sr_last_height = 0;
static double       sr_x_scale = 1.0;
static double       sr_y_scale = 1.0;

extern void RenderImGui(void);
static void
sdl_integer_scale(double *d, double *g)
{
    double ratio;

    if (*d > *g) {
        ratio = floor(*d / *g);
        *d    = *g * ratio;
    } else {
        ratio = ceil(*d / *g);
        *d    = *g / ratio;
    }
}

void sdl_reinit_texture(void);


static int
sdl_display()
{
   int num_displays;
   SDL_Rect dbr;

   if((num_displays = SDL_GetNumVideoDisplays()) < 0)
   {
    pclog("SDL_GetNumVideoDisplays() failed: %s\n", SDL_GetError());
    return 0;
   }	   

   if(SDL_GetDisplayBounds(vid_display, &dbr) < 0)
   {
    pclog("SDL_GetDisplayBounds() failed: %s\n", SDL_GetError());
    return 0;
   }
   
   if (vid_display) {
     SDL_SetWindowPosition(sdl_win, dbr.x, dbr.y);
     SDL_SetWindowSize(sdl_win, dbr.w, dbr.h);        		
   }
   
   return 1;	
}

static void
switchres_flush()
{ 
 pclog("switchres_flush init\n"); 
 sr_mode swres_result;
 int sr_mode_flags = SR_MODE_DONT_FLUSH;    
 retSR = sr_add_mode(304, 240, 59.70, sr_mode_flags, &swres_result); //turrican ii   
 if (swres_result.width == 304) {
    retSR = sr_add_mode(320, 240, 59.70, sr_mode_flags, &swres_result); 
    retSR = sr_add_mode(640, 240, 59.70, sr_mode_flags, &swres_result); //supaplex
 }   
 sr_mode_flags = SR_MODE_INTERLACED | SR_MODE_DONT_FLUSH;       
 retSR = sr_add_mode(720, 480, 59.70, sr_mode_flags, &swres_result);      
  if (swres_result.width == 720) {    
    retSR = sr_add_mode(640, 480, 59.70, sr_mode_flags, &swres_result);     
  }  
 retSR = sr_flush();
 pclog("switchres_flush end\n"); 
}

static void
sdl_stretch(int *w, int *h, int *x, int *y)
{
    double hw, gw, hh, gh, dx, dy, dw, dh, gsr, hsr;
    int    real_sdl_w, real_sdl_h;

    //SDL_GL_GetDrawableSize(sdl_win, &real_sdl_w, &real_sdl_h);        
    
    real_sdl_w = sr_last_width;
    real_sdl_h = sr_last_height;

    hw  = (double) real_sdl_w;
    hh  = (double) real_sdl_h;
    gw  = (double) *w;
    gh  = (double) *h;
    hsr = hw / hh;

    video_fullscreen_scale = FULLSCR_SCALE_FULL; //psakhis

    switch (video_fullscreen_scale) {    
        case FULLSCR_SCALE_FULL:
        default:           
            //*w = real_sdl_w;
            //*h = real_sdl_h;                        
            //*x = 0;            
            //*y = 0;                         
            *w = floor(0.5 + sr_real_width * sr_x_scale);
            *h = floor(0.5 + sr_real_height * sr_y_scale);  
            *x = (real_sdl_w - *w) / 2;
            *y = (real_sdl_h - *h) / 2;
            break;
        case FULLSCR_SCALE_43:
        case FULLSCR_SCALE_KEEPRATIO:
            if (video_fullscreen_scale == FULLSCR_SCALE_43)
                gsr = 4.0 / 3.0;
            else
                gsr = gw / gh;
            if (gsr <= hsr) {
                dw = hh * gsr;
                dh = hh;
            } else {
                dw = hw;
                dh = hw / gsr;
            }
            dx = (hw - dw) / 2.0;
            dy = (hh - dh) / 2.0;
            *w = (int) dw;
            *h = (int) dh;
            *x = (int) dx;
            *y = (int) dy;
            break;
        case FULLSCR_SCALE_INT:
            gsr = gw / gh;
            if (gsr <= hsr) {
                dw = hh * gsr;
                dh = hh;
            } else {
                dw = hw;
                dh = hw / gsr;
            }
            sdl_integer_scale(&dw, &gw);
            sdl_integer_scale(&dh, &gh);
            dx = (hw - dw) / 2.0;
            dy = (hh - dh) / 2.0;
            *w = (int) dw;
            *h = (int) dh;
            *x = (int) dx;
            *y = (int) dy;           
            break;
    }
}

void
sdl_blit_shim(int x, int y, int w, int h, int monitor_index)
{        	       
    params.x = x;
    params.y = y;
    params.w = w;
    params.h = h;        
    if (!(!sdl_enabled || (x < 0) || (y < 0) || (w <= 0) || (h <= 0) || (w > 2048) || (h > 2048) || (buffer32 == NULL) || (sdl_render == NULL) || (sdl_tex == NULL)) || (monitor_index >= 1)) 
        video_copy(interpixels, &(buffer32->line[y][x]), h * 2048 * sizeof(uint32_t));           
    if (screenshots)
        video_screenshot(interpixels, 0, 0, 2048);
    blitreq = 1;
    video_blit_complete_monitor(monitor_index);
}

void ui_window_title_real(void);

void
sdl_real_blit(SDL_Rect *r_src)
{
    SDL_Rect r_dst;
    int      ret; //winx, winy;
    //SDL_GL_GetDrawableSize(sdl_win, &winx, &winy); 
    SDL_RenderClear(sdl_render);

    r_dst   = *r_src;
    r_dst.x = r_dst.y = 0;

    sdl_stretch(&r_dst.w, &r_dst.h, &r_dst.x, &r_dst.y);

    /*if (sdl_fs) {
        sdl_stretch(&r_dst.w, &r_dst.h, &r_dst.x, &r_dst.y);
    } else {
        r_dst.w *= ((float) winx / (float) r_dst.w);
        r_dst.h *= ((float) winy / (float) r_dst.h);
    }*/    
    ret = SDL_RenderCopy(sdl_render, sdl_tex, r_src, &r_dst);
    if (ret)
        fprintf(stderr, "SDL: unable to copy texture to renderer (%s)\n", SDL_GetError());

    SDL_RenderPresent(sdl_render);
}

void
sdl_blit(int x, int y, int w, int h)
{
    SDL_Rect r_src;

    if (!sdl_enabled || (x < 0) || (y < 0) || (w <= 0) || (h <= 0) || (w > 2048) || (h > 2048) || (buffer32 == NULL) || (sdl_render == NULL) || (sdl_tex == NULL)) {
        r_src.x = x;
        r_src.y = y;
        r_src.w = w;
        r_src.h = h;        
        sdl_real_blit(&r_src);
        blitreq = 0;
        return;
    }
   
    SDL_LockMutex(sdl_mutex);       

    //psakhis 
    sr_mode swres_result;      	           
    if (switchres_switch) {   
        pclog("Mode detected %dx%d@%f (%d)\n",switchres_width,switchres_height,switchres_freq,switchres_interlace);                
        struct timeval tval_before, tval_after, tval_result;
        gettimeofday(&tval_before, NULL);
        
        int sr_mode_flags = 0; 
        int sr_height = 240;
    	if (switchres_interlace) {    		
          sr_mode_flags = SR_MODE_INTERLACED;
    	  sr_height = 480;
    	}  
    	retSR = sr_add_mode(switchres_width, sr_height, switchres_freq, sr_mode_flags ,&swres_result);            	
    	retSR = sr_set_mode(swres_result.id);              
        #ifdef _WIN32
        if (sr_last_width != swres_result.width || sr_last_height != swres_result.height) {                                
           SDL_SetWindowSize(sdl_win, swres_result.width, swres_result.height);                      
        }   
        #endif                   
        sr_real_width = switchres_width;  
        sr_real_height = switchres_height;   
        sr_last_width = swres_result.width;  
        sr_last_height = swres_result.height; 
        sr_x_scale = swres_result.x_scale;       
        sr_y_scale = swres_result.y_scale;        
        switchres_switch = 0; 
        
        gettimeofday(&tval_after, NULL);
        timersub(&tval_after, &tval_before, &tval_result);
        pclog("Mode applied, time elapsed: %ld.%06ld\n", (long int)tval_result.tv_sec, (long int)tval_result.tv_usec);                
    }                
    //end psakhis
     
    //not used on fullscreen       
    /*if (resize_pending) {
        if (!video_fullscreen)
            sdl_resize(resize_w, resize_h);
        resize_pending = 0;
    }*/   
            
    r_src.x = x;
    r_src.y = y;
    r_src.w = w;
    r_src.h = h;             
    SDL_UpdateTexture(sdl_tex, &r_src, interpixels, 2048 * 4);
    blitreq = 0;    
              
    sdl_real_blit(&r_src);
    SDL_UnlockMutex(sdl_mutex);
}

static void
sdl_destroy_window(void)
{
   if (sdl_win) {
   	SDL_SetWindowFullscreen(sdl_win, 0);
   	SDL_SetRelativeMouseMode(SDL_FALSE);
        SDL_ShowCursor(SDL_TRUE);
        SDL_SetWindowGrab(sdl_win, SDL_FALSE);
        SDL_DestroyWindow(sdl_win);
        sdl_win = NULL;  
   }
}

static void
sdl_destroy_texture(void)
{
    /* SDL_DestroyRenderer also automatically destroys all associated textures. */
    if (sdl_render != NULL) {
        SDL_DestroyRenderer(sdl_render);
        sdl_render = NULL;
    }
}

void
sdl_close(void)
{    
    
    if (sdl_mutex != NULL)
        SDL_LockMutex(sdl_mutex);     
    
    /* Unregister our renderer! */
    video_setblit(NULL);

    if (sdl_enabled)
        sdl_enabled = 0;

    if (sdl_mutex != NULL) {
        SDL_DestroyMutex(sdl_mutex);
        sdl_mutex = NULL;
    }   
    
    sdl_destroy_texture();             
    sdl_destroy_window();       
    
    /* Quit. */
    SDL_Quit();
    sdl_flags = -1;        
}

void
sdl_enable(int enable)
{
    if (sdl_flags == -1)
        return;

    SDL_LockMutex(sdl_mutex);
    sdl_enabled = !!enable;

    if (enable == 1) {
        SDL_SetWindowSize(sdl_win, cur_ww, cur_wh);
        sdl_reinit_texture();
    }

    SDL_UnlockMutex(sdl_mutex);
}

static void
sdl_select_best_hw_driver(void)
{
    int              i;
    SDL_RendererInfo renderInfo;

    for (i = 0; i < SDL_GetNumRenderDrivers(); ++i) {
        SDL_GetRenderDriverInfo(i, &renderInfo);
        if (renderInfo.flags & SDL_RENDERER_ACCELERATED) {
            SDL_SetHint(SDL_HINT_RENDER_DRIVER, renderInfo.name);
            return;
        }
    }
}

void
sdl_reinit_texture(void)
{
    sdl_destroy_texture();

    if (sdl_flags & RENDERER_HARDWARE) {    	
    	sdl_flags = SDL_RENDERER_ACCELERATED;
    	sdl_flags |= SDL_RENDERER_PRESENTVSYNC;        
        sdl_render = SDL_CreateRenderer(sdl_win, -1, sdl_flags);        
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, video_filter_method ? "1" : "0");
    } else
        sdl_render = SDL_CreateRenderer(sdl_win, -1, SDL_RENDERER_SOFTWARE);

    sdl_tex = SDL_CreateTexture(sdl_render, SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STREAMING, 2048, 2048);
}

void
sdl_set_fs(int fs)
{	 
    SDL_LockMutex(sdl_mutex);          
    SDL_SetWindowFullscreen(sdl_win, SDL_WINDOW_FULLSCREEN);  //psakhis: always full  
    extern void plat_mouse_capture(int fs);
    plat_mouse_capture(fs);
      
    sdl_fs = fs;

    if (fs)
        sdl_flags |= RENDERER_FULL_SCREEN;
    else
        sdl_flags &= ~RENDERER_FULL_SCREEN;

    sdl_reinit_texture();
    SDL_UnlockMutex(sdl_mutex);
}

void
sdl_resize(int x, int y)
{
    int ww = 0, wh = 0, wx = 0, wy = 0;       
    
    if (video_fullscreen & 2)
        return;

    if ((x == cur_w) && (y == cur_h))
        return;

    SDL_LockMutex(sdl_mutex);

    ww = x;
    wh = y;

    cur_w = x;
    cur_h = y;

    cur_wx = wx;
    cur_wy = wy;
    cur_ww = ww;
    cur_wh = wh;    

    SDL_SetWindowSize(sdl_win, cur_ww, cur_wh);
    SDL_GL_GetDrawableSize(sdl_win, &sdl_w, &sdl_h);

    sdl_reinit_texture();

    SDL_UnlockMutex(sdl_mutex);
}
void
sdl_reload(void)
{
    if (sdl_flags & RENDERER_HARDWARE) {
        SDL_LockMutex(sdl_mutex);

        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, video_filter_method ? "1" : "0");
        sdl_reinit_texture();

        SDL_UnlockMutex(sdl_mutex);
    }
}


static int
sdl_init_common(int flags)
{   
    SDL_version ver;

    /* Get and log the version of the DLL we are using. */
    SDL_GetVersion(&ver);
    fprintf(stderr, "SDL: version %d.%d.%d\n", ver.major, ver.minor, ver.patch);

    /* Initialize the SDL system. */
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL: initialization failed (%s)\n", SDL_GetError());
        return (0);
    }

    if (flags & RENDERER_HARDWARE) {
        if (flags & RENDERER_OPENGL) {
            SDL_SetHint(SDL_HINT_RENDER_DRIVER, "OpenGL");
        } else
            sdl_select_best_hw_driver();
    }

    sdl_mutex = SDL_CreateMutex();
    sdl_win   = SDL_CreateWindow("86Box", strncasecmp(SDL_GetCurrentVideoDriver(), "wayland", 7) != 0 && window_remember ? window_x : SDL_WINDOWPOS_CENTERED, strncasecmp(SDL_GetCurrentVideoDriver(), "wayland", 7) != 0 && window_remember ? window_y : SDL_WINDOWPOS_CENTERED, scrnsz_x, scrnsz_y, SDL_WINDOW_OPENGL | (vid_resize & 1 ? SDL_WINDOW_RESIZABLE : 0));    
    
    if (!sdl_display()) {
    	pclog("Failed to index display %d.\n", vid_display);
    	sdl_close();
    	return (0);
    }
        
    sdl_set_fs(video_fullscreen);   

    //psakhis init switchres
    sr_init(); 
    char sr_monitor[256];
    sprintf(sr_monitor, "%d", vid_display);        
    if (vid_display)
     retSR=sr_init_disp(sr_monitor, sdl_win);
    else
     retSR=sr_init_disp("auto", sdl_win);     
    switchres_flush();
    sr_real_width = 640;
    sr_real_height = 480; 
    SDL_GL_GetDrawableSize(sdl_win, &sr_last_width, &sr_last_height);  
    //end psakhis

    /* Make sure we get a clean exit. */
    atexit(sdl_close);
    atexit(sr_deinit);

    /* Register our renderer! */
    video_setblit(sdl_blit_shim);

    sdl_enabled = 1;    

    return (1);
}

int
sdl_inits(void)
{
    return sdl_init_common(0);
}

int
sdl_inith(void)
{
    return sdl_init_common(RENDERER_HARDWARE);
}

int
sdl_initho(void)
{
    return sdl_init_common(RENDERER_HARDWARE | RENDERER_OPENGL);
}

int
sdl_pause(void)
{
    return (0);
}

/*
void
plat_mouse_capture(int on)
{
    SDL_LockMutex(sdl_mutex);
    SDL_SetRelativeMouseMode((SDL_bool) on);
    mouse_capture = on;
    SDL_UnlockMutex(sdl_mutex);
}
*/

void
plat_resize(int w, int h) //not used on fullscreen
{  
    SDL_LockMutex(sdl_mutex);
    resize_w       = w;
    resize_h       = h;
    resize_pending = 1;    
    SDL_UnlockMutex(sdl_mutex);
}

wchar_t    sdl_win_title[512] = { L'8', L'6', L'B', L'o', L'x', 0 };
SDL_mutex *titlemtx           = NULL;

void
ui_window_title_real(void)
{
    char *res;
    if (sizeof(wchar_t) == 1) {
        SDL_SetWindowTitle(sdl_win, (char *) sdl_win_title);
        return;
    }
    res = SDL_iconv_string("UTF-8", sizeof(wchar_t) == 2 ? "UTF-16LE" : "UTF-32LE", (char *) sdl_win_title, wcslen(sdl_win_title) * sizeof(wchar_t) + sizeof(wchar_t));
    if (res) {
        SDL_SetWindowTitle(sdl_win, res);
        SDL_free((void *) res);
    }
    title_set = 0;
}
extern SDL_threadID eventthread;

/* Only activate threading path on macOS, otherwise it will softlock Xorg.
   Wayland doesn't seem to have this issue. */
wchar_t *
ui_window_title(wchar_t *str)
{
    if (!str)
        return sdl_win_title;
#ifdef __APPLE__
    if (eventthread == SDL_ThreadID())
#endif
    {
        memset(sdl_win_title, 0, sizeof(sdl_win_title));
        wcsncpy(sdl_win_title, str, 512);
        ui_window_title_real();
        return str;
    }
#ifdef __APPLE__
    memset(sdl_win_title, 0, sizeof(sdl_win_title));
    wcsncpy(sdl_win_title, str, 512);
    title_set = 1;
#endif
    return str;
}

void
ui_init_monitor(int monitor_index)
{
}
void
ui_deinit_monitor(int monitor_index)
{
}

void
plat_resize_request(int w, int h, int monitor_index)
{
    atomic_store((&doresize_monitors[monitor_index]), 1);
}
