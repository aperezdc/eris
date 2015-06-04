/*
 * nanovg-utils.c
 * Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#define GLFW_INCLUDE_ES2
#include <GLFW/glfw3.h>
#include "nanovg/src/nanovg.h"

#define NANOVG_GLES2_IMPLEMENTATION
#include "nanovg/src/nanovg_gl.h"
#include "nanovg/src/nanovg_gl_utils.h"

#include <stdbool.h>


static void
printGlfwError (int error, const char *desc)
{
    fprintf (stderr, "GLFW error %d: %s\n", error, desc);
    fflush (stderr);
}


static void
keyCallback (GLFWwindow *window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose (window, GL_TRUE);
}


GLFWwindow*
nvgWindow (const char *window_name,
           unsigned    window_width,
           unsigned    window_height)
{
    static bool initialized = false;
    if (!initialized) {
        if (!glfwInit())
            return NULL;
        glfwSetErrorCallback (printGlfwError);
        glfwWindowHint (GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
        glfwWindowHint (GLFW_CONTEXT_VERSION_MAJOR, 2);
        glfwWindowHint (GLFW_CONTEXT_VERSION_MINOR, 0);
        initialized = true;
    }

    GLFWwindow *w = glfwCreateWindow (window_width,
                                      window_height,
                                      window_name,
                                      NULL,
                                      NULL);
    if (w) {
        glfwSetKeyCallback (w, keyCallback);
    }

    return w;
}


void
nvgMakeCurrent (GLFWwindow *window)
{
    glfwMakeContextCurrent (window);
    glfwSwapInterval (0);
}


float
nvgTime (void)
{
    return glfwGetTime ();
}


void
nvgFrameStart (GLFWwindow *window, NVGcontext *vg)
{
    int winWidth, winHeight, fbWidth, fbHeight;
    glfwGetWindowSize (window, &winWidth, &winHeight);
    glfwGetFramebufferSize (window, &fbWidth, &fbHeight);
    glViewport (0, 0, fbWidth, fbHeight);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    nvgBeginFrame (vg, winWidth, winHeight, (float) fbWidth / (float) winWidth);
}


NVGcontext*
nvgCreate (bool debug)
{
    return nvgCreateGLES2 (NVG_ANTIALIAS | NVG_STENCIL_STROKES |
                           (debug ? NVG_DEBUG : 0));
}


void
nvgDelete (NVGcontext *vg)
{
    nvgDeleteGLES2 (vg);
}


void
nvgFrameEnd (GLFWwindow *window, NVGcontext *vg)
{
    nvgEndFrame (vg);
    glfwSwapBuffers (window);
    glfwPollEvents ();
}


bool
nvgDone (GLFWwindow *window)
{
    return !!glfwWindowShouldClose (window);
}


void
nvgExit (void)
{
    glfwTerminate ();
}
