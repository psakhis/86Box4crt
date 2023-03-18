/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          Header file for OpenGL rendering module
 *
 * Authors: Teemu Korhonen
 *
 *          Copyright 2021 Teemu Korhonen
 */

#ifndef UNIX_OPENGL_H
#define UNIX_OPENGL_H

extern int  opengl_init(void);
extern int  opengl_pause(void);
extern void opengl_close(void);
extern void opengl_set_fs(int fs);
extern void opengl_resize(int w, int h);
extern void opengl_reload(void);
extern void opengl_blit(int x, int y, int w, int h);

#endif /*!UNIX_OPENGL_H*/
