/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          Platform main support module for Windows.
 *
 *
 *
 * Authors: Sarah Walker, <https://pcem-emulator.co.uk/>
 *          Miran Grca, <mgrca8@gmail.com>
 *          Fred N. van Kempen, <decwiz@yahoo.com>
 *
 *          Copyright 2008-2019 Sarah Walker.
 *          Copyright 2016-2019 Miran Grca.
 *          Copyright 2017-2019 Fred N. van Kempen.
 *          Copyright 2021 Laci bá'
 */
#define UNICODE
#define NTDDI_VERSION 0x06010000
#include <windows.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_error.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <direct.h>
#include <wchar.h>
#include <io.h>
#include <stdatomic.h>

#include <86box/86box.h>
#include <86box/config.h>
#include <86box/device.h>
#include <86box/keyboard.h>
#include <86box/mouse.h>
#include <86box/timer.h>
#include <86box/nvr.h>
#include <86box/video.h>
#include <86box/mem.h>
#include <86box/rom.h>
#include <86box/path.h>
#define GLOBAL
#include <86box/plat.h>
#include <86box/thread.h>
#include <86box/ui.h>
#ifdef USE_VNC
#    include <86box/vnc.h>
#endif
#include <86box/gameport.h>
#include <86box/winsr_sdl.h> //psakhis
#include <86box/version.h>
#include <86box/gdbstub.h>
#ifdef MTR_ENABLED
#    include <minitrace/minitrace.h>
#endif

typedef struct {
    WCHAR str[1024];
} rc_str_t;

/* API */
int             rctrl_is_lalt;
int             update_icons;
int             kbd_req_capture;
int             hide_status_bar;
int             hide_tool_bar;
int             fixed_size_x = 640;
int             fixed_size_y = 480;
extern int      title_set;
plat_joystick_t plat_joystick_state[MAX_PLAT_JOYSTICKS];
joystick_t      joystick_state[MAX_JOYSTICKS];
int             joysticks_present;
extern wchar_t  sdl_win_title[512];

SDL_mutex      *blitmtx;
SDL_threadID    eventthread;

/* win_mouse */
SDL_mutex      *mousemutex;
typedef struct mouseinputdata {
    int deltax, deltay, deltaz;
    int mousebuttons;
} mouseinputdata;
static mouseinputdata mousedata;
void
mouse_poll(void)
{
    SDL_LockMutex(mousemutex);
    mouse_x          = mousedata.deltax;
    mouse_y          = mousedata.deltay;
    mouse_z          = mousedata.deltaz;
    mousedata.deltax = mousedata.deltay = mousedata.deltaz = 0;
    mouse_buttons    = mousedata.mousebuttons;
    SDL_UnlockMutex(mousemutex);
}

static int      exit_event         = 0;
uint32_t        lang_id = 0x0409, lang_sys = 0x0409; // Multilangual UI variables, for now all set to LCID of en-US
char            icon_set[256] = "";                  /* name of the iconset to be used */

static const uint16_t sdl_to_xt[0x200] = {
    [SDL_SCANCODE_ESCAPE]       = 0x01,
    [SDL_SCANCODE_1]            = 0x02,
    [SDL_SCANCODE_2]            = 0x03,
    [SDL_SCANCODE_3]            = 0x04,
    [SDL_SCANCODE_4]            = 0x05,
    [SDL_SCANCODE_5]            = 0x06,
    [SDL_SCANCODE_6]            = 0x07,
    [SDL_SCANCODE_7]            = 0x08,
    [SDL_SCANCODE_8]            = 0x09,
    [SDL_SCANCODE_9]            = 0x0A,
    [SDL_SCANCODE_0]            = 0x0B,
    [SDL_SCANCODE_MINUS]        = 0x0C,
    [SDL_SCANCODE_EQUALS]       = 0x0D,
    [SDL_SCANCODE_BACKSPACE]    = 0x0E,
    [SDL_SCANCODE_TAB]          = 0x0F,
    [SDL_SCANCODE_Q]            = 0x10,
    [SDL_SCANCODE_W]            = 0x11,
    [SDL_SCANCODE_E]            = 0x12,
    [SDL_SCANCODE_R]            = 0x13,
    [SDL_SCANCODE_T]            = 0x14,
    [SDL_SCANCODE_Y]            = 0x15,
    [SDL_SCANCODE_U]            = 0x16,
    [SDL_SCANCODE_I]            = 0x17,
    [SDL_SCANCODE_O]            = 0x18,
    [SDL_SCANCODE_P]            = 0x19,
    [SDL_SCANCODE_LEFTBRACKET]  = 0x1A,
    [SDL_SCANCODE_RIGHTBRACKET] = 0x1B,
    [SDL_SCANCODE_RETURN]       = 0x1C,
    [SDL_SCANCODE_LCTRL]        = 0x1D,
    [SDL_SCANCODE_A]            = 0x1E,
    [SDL_SCANCODE_S]            = 0x1F,
    [SDL_SCANCODE_D]            = 0x20,
    [SDL_SCANCODE_F]            = 0x21,
    [SDL_SCANCODE_G]            = 0x22,
    [SDL_SCANCODE_H]            = 0x23,
    [SDL_SCANCODE_J]            = 0x24,
    [SDL_SCANCODE_K]            = 0x25,
    [SDL_SCANCODE_L]            = 0x26,
    [SDL_SCANCODE_SEMICOLON]    = 0x27,
    [SDL_SCANCODE_APOSTROPHE]   = 0x28,
    [SDL_SCANCODE_GRAVE]        = 0x29,
    [SDL_SCANCODE_LSHIFT]       = 0x2A,
    [SDL_SCANCODE_BACKSLASH]    = 0x2B,
    [SDL_SCANCODE_Z]            = 0x2C,
    [SDL_SCANCODE_X]            = 0x2D,
    [SDL_SCANCODE_C]            = 0x2E,
    [SDL_SCANCODE_V]            = 0x2F,
    [SDL_SCANCODE_B]            = 0x30,
    [SDL_SCANCODE_N]            = 0x31,
    [SDL_SCANCODE_M]            = 0x32,
    [SDL_SCANCODE_COMMA]        = 0x33,
    [SDL_SCANCODE_PERIOD]       = 0x34,
    [SDL_SCANCODE_SLASH]        = 0x35,
    [SDL_SCANCODE_RSHIFT]       = 0x36,
    [SDL_SCANCODE_KP_MULTIPLY]  = 0x37,
    [SDL_SCANCODE_LALT]         = 0x38,
    [SDL_SCANCODE_SPACE]        = 0x39,
    [SDL_SCANCODE_CAPSLOCK]     = 0x3A,
    [SDL_SCANCODE_F1]           = 0x3B,
    [SDL_SCANCODE_F2]           = 0x3C,
    [SDL_SCANCODE_F3]           = 0x3D,
    [SDL_SCANCODE_F4]           = 0x3E,
    [SDL_SCANCODE_F5]           = 0x3F,
    [SDL_SCANCODE_F6]           = 0x40,
    [SDL_SCANCODE_F7]           = 0x41,
    [SDL_SCANCODE_F8]           = 0x42,
    [SDL_SCANCODE_F9]           = 0x43,
    [SDL_SCANCODE_F10]          = 0x44,
    [SDL_SCANCODE_NUMLOCKCLEAR] = 0x45,
    [SDL_SCANCODE_SCROLLLOCK]   = 0x46,
    [SDL_SCANCODE_HOME]         = 0x147,
    [SDL_SCANCODE_UP]           = 0x148,
    [SDL_SCANCODE_PAGEUP]       = 0x149,
    [SDL_SCANCODE_KP_MINUS]     = 0x4A,
    [SDL_SCANCODE_LEFT]         = 0x14B,
    [SDL_SCANCODE_KP_5]         = 0x4C,
    [SDL_SCANCODE_RIGHT]        = 0x14D,
    [SDL_SCANCODE_KP_PLUS]      = 0x4E,
    [SDL_SCANCODE_END]          = 0x14F,
    [SDL_SCANCODE_DOWN]         = 0x150,
    [SDL_SCANCODE_PAGEDOWN]     = 0x151,
    [SDL_SCANCODE_INSERT]       = 0x152,
    [SDL_SCANCODE_DELETE]       = 0x153,
    [SDL_SCANCODE_F11]          = 0x57,
    [SDL_SCANCODE_F12]          = 0x58,

    [SDL_SCANCODE_KP_ENTER]  = 0x11c,
    [SDL_SCANCODE_RCTRL]     = 0x11d,
    [SDL_SCANCODE_KP_DIVIDE] = 0x135,
    [SDL_SCANCODE_RALT]      = 0x138,
    [SDL_SCANCODE_KP_9]      = 0x49,
    [SDL_SCANCODE_KP_8]      = 0x48,
    [SDL_SCANCODE_KP_7]      = 0x47,
    [SDL_SCANCODE_KP_6]      = 0x4D,
    [SDL_SCANCODE_KP_4]      = 0x4B,
    [SDL_SCANCODE_KP_3]      = 0x51,
    [SDL_SCANCODE_KP_2]      = 0x50,
    [SDL_SCANCODE_KP_1]      = 0x4F,
    [SDL_SCANCODE_KP_0]      = 0x52,
    [SDL_SCANCODE_KP_PERIOD] = 0x53,

    [SDL_SCANCODE_LGUI]        = 0x15B,
    [SDL_SCANCODE_RGUI]        = 0x15C,
    [SDL_SCANCODE_APPLICATION] = 0x15D,
    [SDL_SCANCODE_PRINTSCREEN] = 0x137
};

uint32_t
timer_onesec(uint32_t interval, void *param)
{
    pc_onesec();
    return interval;
}

typedef struct sdl_blit_params {
    int x, y, w, h;
} sdl_blit_params;

sdl_blit_params params  = { 0, 0, 0, 0 };
int             blitreq = 0;

/* Platform Public data, specific. */
HINSTANCE    hinstance; /* application instance */
int          acp_utf8; /* Windows supports UTF-8 codepage */
volatile int cpu_thread_run = 1;

/* Local data. */
static HANDLE        thMain;
static int           vid_api_inited = 0;
static char         *argbuf;
static int           first_use = 1;
static LARGE_INTEGER StartingTime;
static LARGE_INTEGER Frequency;

static const struct {
    const char *name;
    int         local;
    int (*init)(void *);
    void (*close)(void);
    void (*resize)(int x, int y);
    int (*pause)(void);
    void (*enable)(int enable);
    void (*set_fs)(int fs);
    void (*reload)(void);
} vid_apis[1] = {    
    { "SDL_OpenGL", 1, (int (*)(void *)) sdl_initho, sdl_close, NULL, sdl_pause, sdl_enable, sdl_set_fs, sdl_reload },
  };

extern int title_update;

#ifdef ENABLE_WIN_LOG
int win_do_log = ENABLE_WIN_LOG;

static void
win_log(const char *fmt, ...)
{
    va_list ap;

    if (win_do_log) {
        va_start(ap, fmt);
        pclog_ex(fmt, ap);
        va_end(ap);
    }
}
#else
#    define win_log(fmt, ...)
#endif


size_t
mbstoc16s(uint16_t dst[], const char src[], int len)
{
    if (src == NULL)
        return 0;
    if (len < 0)
        return 0;

    size_t ret = MultiByteToWideChar(CP_UTF8, 0, src, -1, dst, dst == NULL ? 0 : len);

    if (!ret) {
        return -1;
    }

    return ret;
}

size_t
c16stombs(char dst[], const uint16_t src[], int len)
{
    if (src == NULL)
        return 0;
    if (len < 0)
        return 0;

    size_t ret = WideCharToMultiByte(CP_UTF8, 0, src, -1, dst, dst == NULL ? 0 : len, NULL, NULL);

    if (!ret) {
        return -1;
    }

    return ret;
}

int
has_language_changed(uint32_t id)
{
    return (lang_id != id);
}

/* Set (or re-set) the language for the application. */
void
set_language(uint32_t id)
{
   lang_id = id;
}

wchar_t *
plat_get_string(int i)
{
   switch (i) {
        case IDS_2077:
            return L"Click to capture mouse";
        case IDS_2078:
            return L"Press CTRL-END to release mouse";
        case IDS_2079:
            return L"Press CTRL-END or middle button to release mouse";
        case IDS_2080:
            return L"Failed to initialize FluidSynth";
        case IDS_2131:
            return L"Invalid configuration";
        case IDS_4099:
            return L"MFM/RLL or ESDI CD-ROM drives never existed";
        case IDS_2094:
            return L"Failed to set up PCap";
        case IDS_2095:
            return L"No PCap devices found";
        case IDS_2096:
            return L"Invalid PCap device";
        case IDS_2111:
            return L"Unable to initialize FreeType";
        case IDS_2112:
            return L"Unable to initialize SDL, libsdl2 is required";
        case IDS_2132:
            return L"libfreetype is required for ESC/P printer emulation.";
        case IDS_2133:
            return L"libgs is required for automatic conversion of PostScript files to PDF.\n\nAny documents sent to the generic PostScript printer will be saved as PostScript (.ps) files.";
        case IDS_2134:
            return L"libfluidsynth is required for FluidSynth MIDI output.";
        case IDS_2130:
            return L"Make sure libpcap is installed and that you are on a libpcap-compatible network connection.";
        case IDS_2115:
            return L"Unable to initialize Ghostscript";
        case IDS_2063:
            return L"Machine \"%hs\" is not available due to missing ROMs in the roms/machines directory. Switching to an available machine.";
        case IDS_2064:
            return L"Video card \"%hs\" is not available due to missing ROMs in the roms/video directory. Switching to an available video card.";
        case IDS_2129:
            return L"Hardware not available";
        case IDS_2143:
            return L"Monitor in sleep mode";
    }
    return L"";
}

#ifdef MTR_ENABLED
void
init_trace(void)
{
    mtr_init("trace.json");
    mtr_start();
}

void
shutdown_trace(void)
{
    mtr_stop();
    mtr_shutdown();
}
#endif

/* Create a console if we don't already have one. */
static void
CreateConsole(int init)
{
    HANDLE h;
    FILE  *fp;
    fpos_t p;
    int    i;

    if (!init) {
        if (force_debug)
            FreeConsole();
        return;
    }

    /* Are we logging to a file? */
    p = 0;
    (void) fgetpos(stdout, &p);
    if (p != -1)
        return;

    /* Not logging to file, attach to console. */
    if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
        /* Parent has no console, create one. */
        if (!AllocConsole()) {
            /* Cannot create console, just give up. */
            return;
        }
    }
    fp = NULL;
    if ((h = GetStdHandle(STD_OUTPUT_HANDLE)) != NULL) {
        /* We got the handle, now open a file descriptor. */
        if ((i = _open_osfhandle((intptr_t) h, _O_TEXT)) != -1) {
            /* We got a file descriptor, now allocate a new stream. */
            if ((fp = _fdopen(i, "w")) != NULL) {
                /* Got the stream, re-initialize stdout without it. */
                (void) freopen("CONOUT$", "w", stdout);
                setvbuf(stdout, NULL, _IONBF, 0);
                fflush(stdout);
            }
        }
    }

    if (fp != NULL) {
        fclose(fp);
        fp = NULL;
    }
}

static void
CloseConsole(void)
{
    CreateConsole(0);
}

/* Process the commandline, and create standard argc/argv array. */
static int
ProcessCommandLine(char ***argv)
{
    char **args;
    int    argc_max;
    int    i, q, argc;

    if (acp_utf8) {
        i      = strlen(GetCommandLineA()) + 1;
        argbuf = (char *) malloc(i);
        strcpy(argbuf, GetCommandLineA());
    } else {
        i      = c16stombs(NULL, GetCommandLineW(), 0) + 1;
        argbuf = (char *) malloc(i);
        c16stombs(argbuf, GetCommandLineW(), i);
    }

    argc     = 0;
    argc_max = 64;
    args     = (char **) malloc(sizeof(char *) * argc_max);
    if (args == NULL) {
        free(argbuf);
        return (0);
    }

    /* parse commandline into argc/argv format */
    i = 0;
    while (argbuf[i]) {
        while (argbuf[i] == ' ')
            i++;

        if (argbuf[i]) {
            if ((argbuf[i] == '\'') || (argbuf[i] == '"')) {
                q = argbuf[i++];
                if (!argbuf[i])
                    break;
            } else
                q = 0;

            args[argc++] = &argbuf[i];

            if (argc >= argc_max) {
                argc_max += 64;
                args = realloc(args, sizeof(char *) * argc_max);
                if (args == NULL) {
                    free(argbuf);
                    return (0);
                }
            }

            while ((argbuf[i]) && ((q) ? (argbuf[i] != q) : (argbuf[i] != ' ')))
                i++;

            if (argbuf[i]) {
                argbuf[i] = 0;
                i++;
            }
        }
    }

    args[argc] = NULL;
    *argv      = args;

    return (argc);
}

void
main_thread(void *param)
{
    uint32_t old_time, new_time;
    int      drawits, frames;

    framecountx  = 0;
    title_update = 1;
    old_time     = GetTickCount();
    drawits = frames = 0;
    while (!is_quit && cpu_thread_run) {
        /* See if it is time to run a frame of code. */
        new_time = GetTickCount();
#ifdef USE_GDBSTUB
        if (gdbstub_next_asap && (drawits <= 0))
            drawits = 10;
        else
#endif
            drawits += (new_time - old_time);
        old_time = new_time;
        if (drawits > 0 && !dopause) {
            /* Yes, so do one frame now. */
            drawits -= 10;
            if (drawits > 50)
                drawits = 0;

            /* Run a block of code. */
            pc_run();

            /* Every 200 frames we save the machine status. */
            if (++frames >= 200 && nvr_dosave) {
                nvr_save();
                nvr_dosave = 0;
                frames     = 0;
            }
        } else /* Just so we dont overload the host OS. */
            Sleep(1);

        /* If needed, handle a screen resize. */        
        //Psakhis: not need on fullscreen
        /*if (atomic_load(&doresize_monitors[0]) && !video_fullscreen && !is_quit) {        	
            if (vid_resize & 2)
                plat_resize(fixed_size_x, fixed_size_y);
            else
                plat_resize(scrnsz_x, scrnsz_y);
            atomic_store(&doresize_monitors[0], 0);
        }*/
    }

    is_quit = 1;
}

/*
 * We do this here since there is platform-specific stuff
 * going on here, and we do it in a function separate from
 * main() so we can call it from the UI module as well.
 */
void
do_start(void)
{
    LARGE_INTEGER qpc;

    /* We have not stopped yet. */
    is_quit = 0;

    /* Initialize the high-precision timer. */
    timeBeginPeriod(1);
    QueryPerformanceFrequency(&qpc);
    timer_freq = qpc.QuadPart;
    win_log("Main timer precision: %llu\n", timer_freq);

    /* Start the emulator, really. */
    thMain = thread_create(main_thread, NULL);
    SetThreadPriority(thMain, THREAD_PRIORITY_HIGHEST);
}

/* Cleanly stop the emulator. */
void
do_stop(void)
{
    if (SDL_ThreadID() != eventthread) {
        exit_event = 1;
        return;
    }
    if (blitreq) {
        blitreq = 0;
        video_blit_complete();
    }

    while (SDL_TryLockMutex(blitmtx) == SDL_MUTEX_TIMEDOUT) {
        if (blitreq) {
            blitreq = 0;
            video_blit_complete();
        }
    }
    /* Claim the video blitter. */
    startblit();

    vid_apis[vid_api].close();

    pc_close(thMain);

    thMain = NULL;
   
}

int
ui_msgbox(int flags, void *message)
{
    return ui_msgbox_header(flags, NULL, message);
}

int
ui_msgbox_header(int flags, void *header, void *message)
{
    SDL_MessageBoxData       msgdata;
    SDL_MessageBoxButtonData msgbtn;
    if (!header)
        header = (void *) (flags & MBX_ANSI) ? "86Box" : L"86Box";
    if (header <= (void *) 7168)
        header = (void *) plat_get_string((int) header);
    if (message <= (void *) 7168)
        message = (void *) plat_get_string((int) message);
    msgbtn.buttonid = 1;
    msgbtn.text     = "OK";
    msgbtn.flags    = 0;
    memset(&msgdata, 0, sizeof(SDL_MessageBoxData));
    msgdata.numbuttons = 1;
    msgdata.buttons    = &msgbtn;
    int msgflags       = 0;
    if (msgflags & MBX_FATAL)
        msgflags |= SDL_MESSAGEBOX_ERROR;
    else if (msgflags & MBX_ERROR || msgflags & MBX_WARNING)
        msgflags |= SDL_MESSAGEBOX_WARNING;
    else
        msgflags |= SDL_MESSAGEBOX_INFORMATION;
    msgdata.flags = msgflags;
    if (flags & MBX_ANSI) {
        int button      = 0;
        msgdata.title   = header;
        msgdata.message = message;
        SDL_ShowMessageBox(&msgdata, &button);
        return button;
    } else {
        int   button    = 0;
        char *res       = SDL_iconv_string("UTF-8", sizeof(wchar_t) == 2 ? "UTF-16LE" : "UTF-32LE", (char *) message, wcslen(message) * sizeof(wchar_t) + sizeof(wchar_t));
        char *res2      = SDL_iconv_string("UTF-8", sizeof(wchar_t) == 2 ? "UTF-16LE" : "UTF-32LE", (char *) header, wcslen(header) * sizeof(wchar_t) + sizeof(wchar_t));
        msgdata.message = res;
        msgdata.title   = res2;
        SDL_ShowMessageBox(&msgdata, &button);
        free(res);
        free(res2);
        return button;
    }

    return 0;
}

void
plat_get_exe_name(char *s, int size)
{
    wchar_t *temp;

    if (acp_utf8)
        GetModuleFileNameA(hinstance, s, size);
    else {
        temp = malloc(size * sizeof(wchar_t));
        GetModuleFileNameW(hinstance, temp, size);
        c16stombs(s, temp, size);
        free(temp);
    }
}

void
plat_tempfile(char *bufp, char *prefix, char *suffix)
{
    SYSTEMTIME SystemTime;

    if (prefix != NULL)
        sprintf(bufp, "%s-", prefix);
    else
        strcpy(bufp, "");

    GetSystemTime(&SystemTime);
    sprintf(&bufp[strlen(bufp)], "%d%02d%02d-%02d%02d%02d-%03d%s",
            SystemTime.wYear, SystemTime.wMonth, SystemTime.wDay,
            SystemTime.wHour, SystemTime.wMinute, SystemTime.wSecond,
            SystemTime.wMilliseconds,
            suffix);
}

int
plat_getcwd(char *bufp, int max)
{
    wchar_t *temp;

    if (acp_utf8)
        (void) _getcwd(bufp, max);
    else {
        temp = malloc(max * sizeof(wchar_t));
        (void) _wgetcwd(temp, max);
        c16stombs(bufp, temp, max);
        free(temp);
    }

    return (0);
}

int
plat_chdir(char *path)
{
    wchar_t *temp;
    int      len, ret;

    if (acp_utf8)
        return (_chdir(path));
    else {
        len  = mbstoc16s(NULL, path, 0) + 1;
        temp = malloc(len * sizeof(wchar_t));
        mbstoc16s(temp, path, len);

        ret = _wchdir(temp);

        free(temp);
        return ret;
    }
}

FILE *
plat_fopen(const char *path, const char *mode)
{
    wchar_t *pathw, *modew;
    int      len;
    FILE    *fp;

    if (acp_utf8)
        return fopen(path, mode);
    else {
        len   = mbstoc16s(NULL, path, 0) + 1;
        pathw = malloc(sizeof(wchar_t) * len);
        mbstoc16s(pathw, path, len);

        len   = mbstoc16s(NULL, mode, 0) + 1;
        modew = malloc(sizeof(wchar_t) * len);
        mbstoc16s(modew, mode, len);

        fp = _wfopen(pathw, modew);

        free(pathw);
        free(modew);

        return fp;
    }
}

/* Open a file, using Unicode pathname, with 64bit pointers. */
FILE *
plat_fopen64(const char *path, const char *mode)
{
    return plat_fopen(path, mode);
}

void
plat_remove(char *path)
{
    wchar_t *temp;
    int      len;

    if (acp_utf8)
        remove(path);
    else {
        len  = mbstoc16s(NULL, path, 0) + 1;
        temp = malloc(len * sizeof(wchar_t));
        mbstoc16s(temp, path, len);

        _wremove(temp);

        free(temp);
    }
}

void
ui_sb_set_text_w(wchar_t *wstr)
{
}

void
ui_sb_update_icon_state(int tag, int state)
{
}

void
ui_sb_update_icon(int tag, int active)
{
}

void
path_normalize(char *path)
{
    /* No-op */
}

/* Make sure a path ends with a trailing (back)slash. */
void
path_slash(char *path)
{
    if ((path[strlen(path) - 1] != '\\') && (path[strlen(path) - 1] != '/')) {
        strcat(path, "\\");
    }
}

/* Check if the given path is absolute or not. */
int
path_abs(char *path)
{
    if ((path[1] == ':') || (path[0] == '\\') || (path[0] == '/'))
        return (1);

    return (0);
}

/* Return the last element of a pathname. */
char *
plat_get_basename(const char *path)
{
    int c = (int) strlen(path);

    while (c > 0) {
        if (path[c] == '/' || path[c] == '\\')
            return ((char *) &path[c + 1]);
        c--;
    }

    return ((char *) path);
}

void
ui_sb_update_tip(int arg)
{
}

void
ui_sb_update_panes(void)
{
}

void
ui_sb_update_text(void)
{
}

/* Return the 'directory' element of a pathname. */
void
path_get_dirname(char *dest, const char *path)
{
    int   c = (int) strlen(path);
    char *ptr;

    ptr = (char *) path;

    while (c > 0) {
        if (path[c] == '/' || path[c] == '\\') {
            ptr = (char *) &path[c];
            break;
        }
        c--;
    }

    /* Copy to destination. */
    while (path < ptr)
        *dest++ = *path++;
    *dest = '\0';
}

char *
path_get_filename(char *s)
{
    int c = strlen(s) - 1;

    while (c > 0) {
        if (s[c] == '/' || s[c] == '\\')
            return (&s[c + 1]);
        c--;
    }

    return (s);
}

char *
path_get_extension(char *s)
{
    int c = strlen(s) - 1;

    if (c <= 0)
        return (s);

    while (c && s[c] != '.')
        c--;

    if (!c)
        return (&s[strlen(s)]);

    return (&s[c + 1]);
}

void
path_append_filename(char *dest, const char *s1, const char *s2)
{
    strcpy(dest, s1);
    path_slash(dest);
    strcat(dest, s2);
}

void
plat_put_backslash(char *s)
{
    int c = strlen(s) - 1;

    if (s[c] != '/' && s[c] != '\\')
        s[c] = '/';
}

int
plat_dir_check(char *path)
{
    DWORD    dwAttrib;
    int      len;
    wchar_t *temp;

    if (acp_utf8)
        dwAttrib = GetFileAttributesA(path);
    else {
        len  = mbstoc16s(NULL, path, 0) + 1;
        temp = malloc(len * sizeof(wchar_t));
        mbstoc16s(temp, path, len);

        dwAttrib = GetFileAttributesW(temp);

        free(temp);
    }

    return (((dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY))) ? 1 : 0);
}

int
plat_dir_create(char *path)
{
    int      ret, len;
    wchar_t *temp;

    if (acp_utf8)
        return (int) SHCreateDirectoryExA(NULL, path, NULL);
    else {
        len  = mbstoc16s(NULL, path, 0) + 1;
        temp = malloc(len * sizeof(wchar_t));
        mbstoc16s(temp, path, len);

        ret = (int) SHCreateDirectoryExW(NULL, temp, NULL);

        free(temp);

        return ret;
    }
}

void *
plat_mmap(size_t size, uint8_t executable)
{
    return VirtualAlloc(NULL, size, MEM_COMMIT, executable ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE);
}

void
plat_get_global_config_dir(char* strptr)
{
    wchar_t appdata_dir[1024] = { L'\0' };

    if (_wgetenv(L"LOCALAPPDATA") && _wgetenv(L"LOCALAPPDATA")[0] != L'\0') {
        size_t len                 = 0;
        wcsncpy(appdata_dir, _wgetenv(L"LOCALAPPDATA"), 1024);
        len = wcslen(appdata_dir);
        if (appdata_dir[len - 1] != L'\\') {
            appdata_dir[len]     = L'\\';
            appdata_dir[len + 1] = L'\0';
        }
        wcscat(appdata_dir, L"86box");
        CreateDirectoryW(appdata_dir, NULL);
        wcscat(appdata_dir, L"\\");
        c16stombs(strptr, appdata_dir, 1024);
    }
}

void
plat_init_rom_paths(void)
{
    wchar_t appdata_dir[1024] = { L'\0' };

    if (_wgetenv(L"LOCALAPPDATA") && _wgetenv(L"LOCALAPPDATA")[0] != L'\0') {
        char   appdata_dir_a[1024] = { '\0' };
        size_t len                 = 0;
        wcsncpy(appdata_dir, _wgetenv(L"LOCALAPPDATA"), 1024);
        len = wcslen(appdata_dir);
        if (appdata_dir[len - 1] != L'\\') {
            appdata_dir[len]     = L'\\';
            appdata_dir[len + 1] = L'\0';
        }
        wcscat(appdata_dir, L"86box");
        CreateDirectoryW(appdata_dir, NULL);
        wcscat(appdata_dir, L"\\roms");
        CreateDirectoryW(appdata_dir, NULL);
        wcscat(appdata_dir, L"\\");
        c16stombs(appdata_dir_a, appdata_dir, 1024);
        rom_add_path(appdata_dir_a);
    }
}

void
plat_munmap(void *ptr, size_t size)
{
    VirtualFree(ptr, 0, MEM_RELEASE);
}

uint64_t
plat_timer_read(void)
{
    LARGE_INTEGER li;

    QueryPerformanceCounter(&li);

    return (li.QuadPart);
}

static LARGE_INTEGER
plat_get_ticks_common(void)
{
    LARGE_INTEGER EndingTime, ElapsedMicroseconds;

    if (first_use) {
        QueryPerformanceFrequency(&Frequency);
        QueryPerformanceCounter(&StartingTime);
        first_use = 0;
    }

    QueryPerformanceCounter(&EndingTime);
    ElapsedMicroseconds.QuadPart = EndingTime.QuadPart - StartingTime.QuadPart;

    /* We now have the elapsed number of ticks, along with the
       number of ticks-per-second. We use these values
       to convert to the number of elapsed microseconds.
       To guard against loss-of-precision, we convert
       to microseconds *before* dividing by ticks-per-second. */
    ElapsedMicroseconds.QuadPart *= 1000000;
    ElapsedMicroseconds.QuadPart /= Frequency.QuadPart;

    return ElapsedMicroseconds;
}

uint32_t
plat_get_ticks(void)
{
    return (uint32_t) (plat_get_ticks_common().QuadPart / 1000);
}

uint32_t
plat_get_micro_ticks(void)
{
    return (uint32_t) plat_get_ticks_common().QuadPart;
}

void
plat_delay_ms(uint32_t count)
{
    Sleep(count);
}

void
plat_power_off(void)
{
    confirm_exit = 0;
    nvr_save();
    config_save();

    /* Deduct a sufficiently large number of cycles that no instructions will
       run before the main thread is terminated */
    cycles -= 99999999;

    cpu_thread_run = 0;
}

void
plat_pause(int p)
{
    static wchar_t oldtitle[512];
    wchar_t        title[512];

    if ((p == 0) && (time_sync & TIME_SYNC_ENABLED))
        nvr_time_sync();

    dopause = p;
    if (p) {
        wcsncpy(oldtitle, ui_window_title(NULL), sizeof_w(oldtitle) - 1);
        wcscpy(title, oldtitle);
        wcscat(title, L" - PAUSED");
        ui_window_title(title);
    } else {
        ui_window_title(oldtitle);
    }
}

/* unix */
void
ui_sb_bugui(char *str)
{
}

extern void sdl_blit(int x, int y, int w, int h);

int real_sdl_w, real_sdl_h;
void
ui_sb_set_ready(int ready)
{
}
char *xargv[512];

/*end unix*/

/* For the Windows platform, this is the start of the application. */
int WINAPI
WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpszArg, int nCmdShow)
{
    SDL_Event event;	
    char **argv = NULL;
    int    argc;

    /* Initialize the COM library for the main thread. */
    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    /* Check if Windows supports UTF-8 */
    if (GetACP() == CP_UTF8)
        acp_utf8 = 1;
    else
        acp_utf8 = 0;

    /* Set this to the default value */
    video_fullscreen = 1;    

    /* We need this later. */
    hinstance = hInst;

    /* Set the application version ID string. */
    sprintf(emu_version, "%s v%s", EMU_NAME, EMU_VERSION_FULL);

    /* Initialize SDL */
    SDL_Init(0); 
   
    /* Process the command line for options. */
    argc = ProcessCommandLine(&argv);
    
    /* Pre-initialize the system, this loads the config file. */    
    if (!pc_init(argc, argv)) {      	            	
        if (force_debug)
            CreateConsole(0);
      
        free(argbuf);
        free(argv);
        return (1);
    }
    
    if (!pc_init_modules()) {
        ui_msgbox_header(MBX_FATAL, L"No ROMs found.", L"86Box could not find any usable ROM images.\n\nPlease download a ROM set and extract it into the \"roms\" directory.");       
        SDL_Quit();
        return 6;
    }
    
    extern int gfxcard_2;
    gfxcard_2 = 0;
  
    /* Create console window. */
    if (force_debug) {
        CreateConsole(1);
        atexit(CloseConsole);
    }
    
    eventthread = SDL_ThreadID();
    blitmtx     = SDL_CreateMutex();
    if (!blitmtx) {
        fprintf(stderr, "Failed to create blit mutex: %s", SDL_GetError());
        return -1;
    }
    mousemutex = SDL_CreateMutex();
    
    vid_apis[0].init(NULL);            
    
    /* start machine */    
    pc_reset_hard_init();       
    do_start();         
    
    /* Handle our GUI. */
    //i = ui_init(nCmdShow);    
    SDL_AddTimer(1000, timer_onesec, NULL);
    while (!is_quit) {
        static int mouse_inside = 0;        
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    exit_event = 1;
                    break;                                                  
                case SDL_MOUSEWHEEL:
                    {                       		
                        if (mouse_capture || video_fullscreen) {
                            if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
                                event.wheel.x *= -1;
                                event.wheel.y *= -1;
                            }
                            SDL_LockMutex(mousemutex);
                            mousedata.deltaz = event.wheel.y;
                            SDL_UnlockMutex(mousemutex);
                        }
                        break;
                    }
                case SDL_MOUSEMOTION:
                    {
                        if (mouse_capture || video_fullscreen) {                         
                            SDL_LockMutex(mousemutex);
                            mousedata.deltax += event.motion.xrel;
                            mousedata.deltay += event.motion.yrel;
                            SDL_UnlockMutex(mousemutex);
                        }
                        break;
                    }
                case SDL_MOUSEBUTTONDOWN:
                case SDL_MOUSEBUTTONUP:
                    {   
                    	//psakhis   
                    	if (event.button.button == SDL_BUTTON_MIDDLE) {
                    	    exit_event = 1;                                  
                            break; 
                        }    
                        //end psakhis
                                          	
                        if ((event.button.button == SDL_BUTTON_LEFT)
                            && !(mouse_capture || video_fullscreen)
                            && event.button.state == SDL_RELEASED
                            && mouse_inside) {
                            plat_mouse_capture(1);
                            break;
                        }
                        if (mouse_get_buttons() < 3 && event.button.button == SDL_BUTTON_MIDDLE && !video_fullscreen) {
                            plat_mouse_capture(0);
                            break;
                        }
                        if (mouse_capture || video_fullscreen) {
                            int buttonmask = 0;

                            switch (event.button.button) {
                                case SDL_BUTTON_LEFT:
                                    buttonmask = 1;
                                    break;
                                case SDL_BUTTON_RIGHT:
                                    buttonmask = 2;
                                    break;
                                case SDL_BUTTON_MIDDLE:
                                    buttonmask = 4;                                                        		                                      
                                    break;
                                case SDL_BUTTON_X1:
                                    buttonmask = 8;
                                    break;
                                case SDL_BUTTON_X2:
                                    buttonmask = 16;
                                    break;
                            }
                            SDL_LockMutex(mousemutex);
                            if (event.button.state == SDL_PRESSED) {
                                mousedata.mousebuttons |= buttonmask;
                            } else
                                mousedata.mousebuttons &= ~buttonmask;
                            SDL_UnlockMutex(mousemutex);
                        }
                        break;
                    }
                case SDL_RENDER_DEVICE_RESET:
                case SDL_RENDER_TARGETS_RESET:
                    {
                        extern void sdl_reinit_texture(void);
                        sdl_reinit_texture();
                        break;
                    }
                case SDL_KEYDOWN:
                case SDL_KEYUP:
                    {
                        uint16_t xtkey = 0;
                        switch (event.key.keysym.scancode) {
                            default:
                                xtkey = sdl_to_xt[event.key.keysym.scancode];
                                break;
                        }
                        keyboard_input(event.key.state == SDL_PRESSED, xtkey);
                    }
                case SDL_WINDOWEVENT:
                    {
                        switch (event.window.event) {
                            case SDL_WINDOWEVENT_ENTER:
                                mouse_inside = 1;
                                break;
                            case SDL_WINDOWEVENT_LEAVE:
                                mouse_inside = 0;
                                break;
                        }
                    }
            }
        }
        if (mouse_capture && keyboard_ismsexit()) {
            plat_mouse_capture(0);
        }
        if (blitreq) {
            extern void sdl_blit(int x, int y, int w, int h);
            sdl_blit(params.x, params.y, params.w, params.h);
        }
        if (title_set) {
            extern void ui_window_title_real(void);
            ui_window_title_real();
        }
        if (video_fullscreen && keyboard_isfsexit()) {           
            video_fullscreen = 0;
        }
        if (exit_event) {
            do_stop();
            break;
        }
    }
    
    printf("\n");
    SDL_DestroyMutex(blitmtx);
    SDL_DestroyMutex(mousemutex);
    SDL_Quit();
    
    /* Uninitialize COM before exit. */
    CoUninitialize();

    free(argbuf);
    free(argv);
    return (0);
}


/* Return the VIDAPI name for the given number. */
char *
plat_vidapi_name(int api)
{
    char *name = "default";

    switch (api) {
        case 0:
            name = "sdl_opengl";
            break;     
        default:
            fatal("Unknown renderer: %i\n", api);
            break;
    }

    return (name);
}

int
plat_setvid(int api)
{       
    win_log("Initializing VIDAPI: api=%d\n", api);
    startblit();

    /* Close the (old) API. */
    vid_apis[vid_api].close();
    vid_api = api;  

    /* Initialize the (new) API. */
    vid_apis[vid_api].init(NULL);
    endblit();
  

    device_force_redraw();

    vid_api_inited = 1;

    return (1);
}

/* Tell the renderers about a new screen resolution. */
void
plat_vidsize(int x, int y)
{
    if (!vid_api_inited || !vid_apis[vid_api].resize)
        return;

    startblit();
    vid_apis[vid_api].resize(x, y);
    endblit();
}

void
plat_vidapi_enable(int enable)
{
    int i = 1;
   
    if (!vid_api_inited || !vid_apis[vid_api].enable)
        return;

    vid_apis[vid_api].enable(enable != 0);
    
    if (!i)
        return;

    if (enable)
        device_force_redraw();
}

int
get_vidpause(void)
{
    return (vid_apis[vid_api].pause());
}

void
plat_setfullscreen(int on)
{
 
}

void
plat_vid_reload_options(void)
{
    if (!vid_api_inited || !vid_apis[vid_api].reload)
        return;

    vid_apis[vid_api].reload();
}

void
plat_vidapi_reload(void)
{
    vid_apis[vid_api].reload();
}

/* Sets up the program language before initialization. */
uint32_t
plat_language_code(char *langcode)
{
 return 0;
}

/* Converts back the language code to LCID */
void
plat_language_code_r(uint32_t lcid, char *outbuf, int len)
{
 return;
}

void
take_screenshot(void)
{
    startblit();
    monitors[0].mon_screenshots++;
    endblit();
    device_force_redraw();
}

/* LPARAM interface to plat_get_string(). */
LPARAM
win_get_string(int id)
{
    wchar_t *ret;

    ret = plat_get_string(id);
    return ((LPARAM) ret);
}

/* win_joystick.c */
void
joystick_init(void)
{
}
void
joystick_close(void)
{
}
void
joystick_process(void)
{
}

void /* plat_ */
startblit(void)
{
    //WaitForSingleObject(ghMutex, INFINITE);
    SDL_LockMutex(blitmtx);
}

void /* plat_ */
endblit(void)
{
    //ReleaseMutex(ghMutex);
    SDL_UnlockMutex(blitmtx);
}

/* API */
void
ui_sb_mt32lcd(char *str)
{
}
