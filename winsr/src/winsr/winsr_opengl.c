/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          Rendering module for OpenGL
 *
 * TODO:    More shader features
 *          - scaling
 *          - multipass
 *          - previous frames
 *          (UI) options
 *          More error handling
 *
 *
 *
 * Authors: Teemu Korhonen
 *
 *          Copyright 2021 Teemu Korhonen
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#define UNICODE
#include <Windows.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <glad/glad.h>

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

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#if !defined(_MSC_VER) || defined(__clang__)
#    include <stdatomic.h>
#else
typedef LONG atomic_flag;
#    define atomic_flag_clear(OBJ)        InterlockedExchange(OBJ, 0)
#    define atomic_flag_test_and_set(OBJ) InterlockedExchange(OBJ, 1)
#endif

#include <86box/86box.h>
#include <86box/plat.h>
#include <86box/video.h>
#include <86box/winsr_opengl.h>
#include <86box/winsr_opengl_glslp.h>

#include <86box/switchres_wrapper2.h> //psakhis

static const int INIT_WIDTH   = 640;
static const int INIT_HEIGHT  = 400;
static const int BUFFERPIXELS = 4194304;  /* Same size as render_buffer, pow(2048 + 64, 2). */
static const int BUFFERBYTES  = 16777216; /* Pixel is 4 bytes. */
static const int BUFFERCOUNT  = 3;        /* How many buffers to use for pixel transfer (2-3 is commonly recommended). */
static const int ROW_LENGTH   = 2048;     /* Source buffer row lenght (including padding) */

typedef struct sdl_blit_params {
    int x, y, w, h;
} sdl_blit_params;
extern sdl_blit_params params;
extern int             blitreq;

//psakhis
static unsigned char       retSR; 
static int          sr_real_width = 0;     
static int          sr_real_height = 0;
static int          sr_last_width = 0;     
static int          sr_last_height = 0;
static double       sr_x_scale = 1.0;
static double       sr_y_scale = 1.0;

/**
 * @brief A dedicated OpenGL thread.
 * OpenGL context's don't handle multiple threads well.
 */
//static thread_t *thread = NULL;

/**
 * @brief A window usable with an OpenGL context
 */
static volatile int opengl_enabled = 0;
static SDL_GLContext context = NULL;
static SDL_Window  *sdl_win     = NULL;
static SDL_mutex   *sdl_mutex   = NULL;

/**
 * @brief Blit event parameters.
 */
typedef struct
{
    int                  w, h;
    void                *buffer; /* Buffer for pixel transfer, allocated by gpu driver. */
    volatile atomic_flag in_use; /* Is buffer currently in use. */
    GLsync               sync;   /* Fence sync object used by opengl thread to track pixel transfer completion. */
} blit_info_t;

/**
 * @brief Array of blit_infos, one for each buffer.
 */
static blit_info_t *blit_info = NULL;
static int video_width = 0;
static int video_height = 0;

/**
 * @brief Buffer index of next write operation.
 */
static int write_pos = 0;
static int read_pos = 0;

/**
 * @brief Resize event parameters.
 */
/* 
static struct
{
    int      width, height, fullscreen, scaling_mode;
    mutex_t *mutex;
} resize_info = { 0 };
*/

/**
 * @brief Renderer options
 */
static struct
{
    int      vsync;              /* Vertical sync; 0 = off, 1 = on */
    int      frametime;          /* Frametime in microseconds, or -1 to sync with blitter */
    char     shaderfile[512];    /* Shader file path. Match the length of openfilestring in win_dialog.c */
    int      shaderfile_changed; /* Has shader file path changed. To prevent unnecessary shader recompilation. */
    int      filter;             /* 0 = Nearest, 1 = Linear */
    int      filter_changed;     /* Has filter changed. */
    //mutex_t *mutex;
} options = { 0 };

/**
 * @brief Identifiers to OpenGL objects and uniforms.
 */
typedef struct
{
    GLuint vertexArrayID;
    GLuint vertexBufferID;
    GLuint textureID;
    GLuint unpackBufferID;
    GLuint shader_progID;

    /* Uniforms */

    GLint input_size;
    GLint output_size;
    GLint texture_size;
    GLint frame_count;
} gl_identifiers;

gl_identifiers gl = { 0 };

/**
 * @brief (Re-)apply shaders to OpenGL context.
 * @param gl Identifiers from initialize
 */
static void
apply_shaders(gl_identifiers *gl)
{
    GLuint old_shader_ID = 0;

    if (gl->shader_progID != 0)
        old_shader_ID = gl->shader_progID;

    if (strlen(options.shaderfile) > 0)
        gl->shader_progID = load_custom_shaders(options.shaderfile);
    else
        gl->shader_progID = 0;

    if (gl->shader_progID == 0)
        gl->shader_progID = load_default_shaders();

    glUseProgram(gl->shader_progID);

    /* Delete old shader if one exists (changing shader) */
    if (old_shader_ID != 0)
        glDeleteProgram(old_shader_ID);

    GLint vertex_coord = glGetAttribLocation(gl->shader_progID, "VertexCoord");
    if (vertex_coord != -1) {
        glEnableVertexAttribArray(vertex_coord);
        glVertexAttribPointer(vertex_coord, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), 0);
    }

    GLint tex_coord = glGetAttribLocation(gl->shader_progID, "TexCoord");
    if (tex_coord != -1) {
        glEnableVertexAttribArray(tex_coord);
        glVertexAttribPointer(tex_coord, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (void *) (2 * sizeof(GLfloat)));
    }

    GLint color = glGetAttribLocation(gl->shader_progID, "Color");
    if (color != -1) {
        glEnableVertexAttribArray(color);
        glVertexAttribPointer(color, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (void *) (4 * sizeof(GLfloat)));
    }

    GLint mvp_matrix = glGetUniformLocation(gl->shader_progID, "MVPMatrix");
    if (mvp_matrix != -1) {
        static const GLfloat mvp[] = {
            1.f, 0.f, 0.f, 0.f,
            0.f, 1.f, 0.f, 0.f,
            0.f, 0.f, 1.f, 0.f,
            0.f, 0.f, 0.f, 1.f
        };
        glUniformMatrix4fv(mvp_matrix, 1, GL_FALSE, mvp);
    }

    GLint frame_direction = glGetUniformLocation(gl->shader_progID, "FrameDirection");
    if (frame_direction != -1)
        glUniform1i(frame_direction, 1); /* always forward */

    gl->input_size   = glGetUniformLocation(gl->shader_progID, "InputSize");
    gl->output_size  = glGetUniformLocation(gl->shader_progID, "OutputSize");
    gl->texture_size = glGetUniformLocation(gl->shader_progID, "TextureSize");
    gl->frame_count  = glGetUniformLocation(gl->shader_progID, "FrameCount");
}

/**
 * @brief Initialize OpenGL context
 * @return Identifiers
 */
static int
initialize_glcontext(gl_identifiers *gl)
{
    /* Vertex, texture 2d coordinates and color (white) making a quad as triangle strip */
    static const GLfloat surface[] = {
        -1.f, 1.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f,
        1.f, 1.f, 1.f, 0.f, 1.f, 1.f, 1.f, 1.f,
        -1.f, -1.f, 0.f, 1.f, 1.f, 1.f, 1.f, 1.f,
        1.f, -1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f
    };

    glGenVertexArrays(1, &gl->vertexArrayID);

    glBindVertexArray(gl->vertexArrayID);

    glGenBuffers(1, &gl->vertexBufferID);
    glBindBuffer(GL_ARRAY_BUFFER, gl->vertexBufferID);
    glBufferData(GL_ARRAY_BUFFER, sizeof(surface), surface, GL_STATIC_DRAW);

    glGenTextures(1, &gl->textureID);
    glBindTexture(GL_TEXTURE_2D, gl->textureID);

    static const GLfloat border_color[] = { 0.f, 0.f, 0.f, 1.f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border_color);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, options.filter ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, options.filter ? GL_LINEAR : GL_NEAREST);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, INIT_WIDTH, INIT_HEIGHT, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);

    glGenBuffers(1, &gl->unpackBufferID);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, gl->unpackBufferID);

    void *buf_ptr = NULL;

    if (GLAD_GL_ARB_buffer_storage) {
        /* Create persistent buffer for pixel transfer. */
        glBufferStorage(GL_PIXEL_UNPACK_BUFFER, BUFFERBYTES * BUFFERCOUNT, NULL, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);

        buf_ptr = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, BUFFERBYTES * BUFFERCOUNT, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
    } else {
        /* Fallback; create our own buffer. */
        buf_ptr = malloc(BUFFERBYTES * BUFFERCOUNT);

        glBufferData(GL_PIXEL_UNPACK_BUFFER, BUFFERBYTES * BUFFERCOUNT, NULL, GL_STREAM_DRAW);
    }

    if (buf_ptr == NULL)
        return 0; /* Most likely out of memory. */

    /* Split the buffer area for each blit_info and set them available for use. */
    for (int i = 0; i < BUFFERCOUNT; i++) {
        blit_info[i].buffer = (byte *) buf_ptr + BUFFERBYTES * i;
        atomic_flag_clear(&blit_info[i].in_use);
    }

    glClearColor(0.f, 0.f, 0.f, 1.f);

    apply_shaders(gl);

    return 1;
}

/**
 * @brief Clean up OpenGL context
 * @param gl Identifiers from initialize
 */
static void
finalize_glcontext(gl_identifiers *gl)
{
    if (GLAD_GL_ARB_buffer_storage)
        glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
    else
        free(blit_info[0].buffer);

    glDeleteProgram(gl->shader_progID);
    glDeleteBuffers(1, &gl->unpackBufferID);
    glDeleteTextures(1, &gl->textureID);
    glDeleteBuffers(1, &gl->vertexBufferID);
    glDeleteVertexArrays(1, &gl->vertexArrayID);
}

static int
opengl_display()
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


/**
 * @brief Renders a frame and swaps the buffer
 * @param gl Identifiers from initialize
 */
static void
render_and_swap(gl_identifiers *gl)
{
    static int frame_counter = 0;

    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    SDL_GL_SwapWindow(sdl_win);

    if (gl->frame_count != -1)
        glUniform1i(gl->frame_count, frame_counter = (frame_counter + 1) & 1023);
}

/**
 * @brief Handle failure in OpenGL thread.
 * Keeps the thread sleeping until closing.
 */
static void
opengl_fail(void)
{
    if (sdl_win != NULL) {
        SDL_DestroyWindow(sdl_win);
        sdl_win = NULL;
    }

    wchar_t *message = plat_get_string(IDS_2153);
    wchar_t *header  = plat_get_string(IDS_2154);    
    
    pclog("OpenGL fail: (%s) %s\n", header, message); 
   
}
/*
static void __stdcall opengl_debugmsg_callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam)
{
    pclog("OpenGL: %s\n", message);
}
*/

void
opengl_real_blit(int x, int y, int w, int h)
{
      blit_info_t *info = &blit_info[read_pos];                 
      /* Resize the texture */
      if (video_width != info->w || video_height != info->h) {
      	 video_width = info->w;
      	 video_height = info->h;
         glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
         glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, video_width, video_height, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
         glBindBuffer(GL_PIXEL_UNPACK_BUFFER, gl.unpackBufferID);
      }
           
      glViewport(x, y, w, h);
      
      if (gl.output_size != -1)
       glUniform2f(gl.output_size, w, h);
      
      if (!GLAD_GL_ARB_buffer_storage) {
        /* Fallback method, copy data to pixel buffer. */
        glBufferSubData(GL_PIXEL_UNPACK_BUFFER, BUFFERBYTES * read_pos, info->h * ROW_LENGTH * sizeof(uint32_t), info->buffer);
      }
      
      /* Update texture from pixel buffer. */
      glPixelStorei(GL_UNPACK_SKIP_PIXELS, BUFFERPIXELS * read_pos);
      glPixelStorei(GL_UNPACK_ROW_LENGTH, ROW_LENGTH);
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, info->w, info->h, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
                            	
      glFinish();
      
     /* if (GLAD_GL_ARB_sync) {
      	if (info->sync != NULL && glClientWaitSync(info->sync, GL_SYNC_FLUSH_COMMANDS_BIT, 0) != GL_TIMEOUT_EXPIRED) {
           glDeleteSync(info->sync);
           info->sync = NULL;                    
        }
      }*/    
        	         
      atomic_flag_clear(&info->in_use);
                    
      read_pos = (read_pos + 1) % BUFFERCOUNT;	
	
}


void
opengl_blit(int x, int y, int w, int h)
{        
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
    //FULLSCR_SCALE_FULL 
    int ww = floor(0.5 + sr_real_width * sr_x_scale);
    int hh = floor(0.5 + sr_real_height * sr_y_scale);   
    int xx = (sr_last_width - ww) / 2;
    int yy = (sr_last_height - hh) / 2;
    
    opengl_real_blit(xx, yy, ww, hh); 
    render_and_swap(&gl);    
    blitreq = 0;    
    SDL_UnlockMutex(sdl_mutex);       
}

void
opengl_blit_shim(int x, int y, int w, int h, int monitor_index)
{   
    params.x = x;
    params.y = y;
    params.w = w;
    params.h = h;                   
    
    int row;    
            
    if ((x < 0) || (y < 0) || (w <= 0) || (h <= 0) || (w > 2048) || (h > 2048) || (buffer32 == NULL) || (!opengl_enabled) || monitor_index >= 1) {      
    	video_blit_complete_monitor(monitor_index);
        return;                
    } 
    
    int full_buffered = atomic_flag_test_and_set(&blit_info[write_pos].in_use);
    if (full_buffered) {
       blitreq = 1; 
       video_blit_complete_monitor(monitor_index);  
       return;     
    } 
    
    for (row = 0; row < h; ++row)
        video_copy(&(((uint8_t *) blit_info[write_pos].buffer)[row * ROW_LENGTH * sizeof(uint32_t)]), &(buffer32->line[y + row][x]), w * sizeof(uint32_t));
    
    blit_info[write_pos].w = w;
    blit_info[write_pos].h = h;        
    
    if (monitors[0].mon_screenshots)
        video_screenshot(blit_info[write_pos].buffer, 0, 0, ROW_LENGTH);
            
    /* Add fence to track when above gl commands are complete. */
    /*if (GLAD_GL_ARB_sync)    
       blit_info[write_pos].sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    */    
    write_pos = (write_pos + 1) % BUFFERCOUNT;            
    blitreq = 1;              
    video_blit_complete_monitor(monitor_index);
}

/*
static int
framerate_to_frametime(int framerate)
{
    if (framerate < 0)
        return -1;

    return (int) ceilf(1.e6f / (float) framerate);
}
*/

int
opengl_init(void)
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
    
    options.vsync     = 1;
    options.frametime = -1;
    //strcpy_s(options.shaderfile, sizeof(options.shaderfile), video_shader);
    snprintf(options.shaderfile, sizeof(options.shaderfile), "%s", video_shader);

    options.shaderfile_changed = 0;
    options.filter             = video_filter_method;
    options.filter_changed     = 0;
    
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, options.vsync);

    if (GLAD_GL_ARB_debug_output && log_path[0] != '\0')
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG | SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    else
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    
    sdl_mutex = SDL_CreateMutex();
    sdl_win   = SDL_CreateWindow("86Box", strncasecmp(SDL_GetCurrentVideoDriver(), "wayland", 7) != 0 && window_remember ? window_x : SDL_WINDOWPOS_CENTERED, strncasecmp(SDL_GetCurrentVideoDriver(), "wayland", 7) != 0 && window_remember ? window_y : SDL_WINDOWPOS_CENTERED, scrnsz_x, scrnsz_y, SDL_WINDOW_OPENGL | (vid_resize & 1 ? SDL_WINDOW_RESIZABLE : 0));    
       
    if (!opengl_display()) {
    	pclog("Failed to index display %d.\n", vid_display);
    	opengl_fail();
    	return 0;
    }
    
    opengl_set_fs(video_fullscreen);     

    context = SDL_GL_CreateContext(sdl_win);

    if (context == NULL) {
        pclog("OpenGL: failed to create OpenGL context.\n");
        opengl_fail();
        return 0;
    }

    SDL_GL_SetSwapInterval(options.vsync);

    if (!gladLoadGLLoader(SDL_GL_GetProcAddress)) {
        pclog("OpenGL: failed to set OpenGL loader.\n");
        SDL_GL_DeleteContext(context);
        opengl_fail();
        return 0;
    }

    if (GLAD_GL_ARB_debug_output && log_path[0] != '\0') {
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
        glDebugMessageControlARB(GL_DONT_CARE, GL_DEBUG_TYPE_PERFORMANCE_ARB, GL_DONT_CARE, 0, 0, GL_FALSE);
        //glDebugMessageCallbackARB(opengl_debugmsg_callback, NULL);
    }

    pclog("OpenGL vendor: %s\n", glGetString(GL_VENDOR));
    pclog("OpenGL renderer: %s\n", glGetString(GL_RENDERER));
    pclog("OpenGL version: %s\n", glGetString(GL_VERSION));
    pclog("OpenGL shader language version: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

    /* Check that the driver actually reports version 3.0 or later */
    GLint major = -1;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    if (major < 3) {
        pclog("OpenGL: Minimum OpenGL version 3.0 is required.\n");
        SDL_GL_DeleteContext(context);
        opengl_fail();
        return 0;
    }

    /* Check if errors have been generated at this point */
    GLenum gl_error = glGetError();
    if (gl_error != GL_NO_ERROR) {
        /* Log up to 10 errors */
        int i = 0;
        do {
            pclog("OpenGL: Error %u\n", gl_error);
            i++;
        } while ((gl_error = glGetError()) != GL_NO_ERROR && i < 10);

        SDL_GL_DeleteContext(context);
        opengl_fail();
        return 0;
    }
           
    blit_info = (blit_info_t *) malloc(BUFFERCOUNT * sizeof(blit_info_t));
    memset(blit_info, 0, BUFFERCOUNT * sizeof(blit_info_t));

    /* Buffers are not yet allocated, set them as in use. */
    for (int i = 0; i < BUFFERCOUNT; i++)
        atomic_flag_test_and_set(&blit_info[i].in_use);

    write_pos = 0;    
    read_pos = 0;
    
    if (!initialize_glcontext(&gl)) {
        pclog("OpenGL: failed to initialize.\n");
        finalize_glcontext(&gl);
        SDL_GL_DeleteContext(context);
        opengl_fail();
        return 0;
    }
      
    if (gl.frame_count != -1)
        glUniform1i(gl.frame_count, 0);
    if (gl.output_size != -1)
        glUniform2f(gl.output_size, INIT_WIDTH, INIT_HEIGHT);
    
    video_width = INIT_WIDTH;
    video_height = INIT_HEIGHT;
        
    render_and_swap(&gl);
    
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
                         
    opengl_enabled = 1;
                                  
    atexit(opengl_close);     
    atexit(sr_deinit);   

    video_setblit(opengl_blit_shim);
            
    return 1;
}

int
opengl_pause(void)
{
    return 0;
}

void
opengl_close(void)
{
   if (sdl_mutex != NULL)
     SDL_LockMutex(sdl_mutex);   
   
   video_setblit(NULL);
       
   if (opengl_enabled) {  
      opengl_enabled = 0;      
      SDL_GL_DeleteContext(context);              
      free(blit_info);   
   }
   
   if (sdl_win) {
   	SDL_SetWindowFullscreen(sdl_win, 0);
   	SDL_SetRelativeMouseMode(SDL_FALSE);
        SDL_ShowCursor(SDL_TRUE);
        SDL_SetWindowGrab(sdl_win, SDL_FALSE);
        SDL_DestroyWindow(sdl_win);
        sdl_win = NULL;  
   }
      
   
   if (sdl_mutex != NULL) {
        SDL_DestroyMutex(sdl_mutex);
        sdl_mutex = NULL;
   }
    
   SDL_Quit();       

}

void
opengl_set_fs(int fs)
{
    SDL_LockMutex(sdl_mutex);          
    SDL_SetWindowFullscreen(sdl_win, SDL_WINDOW_FULLSCREEN);  //psakhis: always full  
    extern void plat_mouse_capture(int fs);
    plat_mouse_capture(fs);
    SDL_UnlockMutex(sdl_mutex);
}

void
opengl_resize(int w, int h)
{

}

void
opengl_reload(void)
{
   
}
