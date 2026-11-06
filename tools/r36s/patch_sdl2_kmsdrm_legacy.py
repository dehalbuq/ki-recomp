#!/usr/bin/env python3
"""Patch *only* SDL2 2.30.12's KMSDRM GBM/EGL surface negotiation.

The stock SDL2 KMSDRM backend hard-codes ARGB8888, which can fail
with an older Mali RK3326 EGL/GBM stack that prefers opaque XRGB8888
or RGB565. We negotiate multiple visual IDs before returning fatal.
The commercial game's code, image and emulated clock are untouched.
"""
import argparse
from pathlib import Path

p=argparse.ArgumentParser()
p.add_argument("source",type=Path,help="SDL2-2.30.12/src/video/kmsdrm/SDL_kmsdrmvideo.c")
a=p.parse_args()
source=a.source.read_text()
start=source.index("int KMSDRM_CreateSurfaces(_THIS, SDL_Window *window)")
end=source.index("\n#ifdef SDL_INPUT_LINUXEV",start)
old=source[start:end]
required=[
    "uint32_t surface_fmt = GBM_FORMAT_ARGB8888;",
    "KMSDRM_gbm_surface_create(viddata->gbm_dev,",
    "SDL_EGL_CreateSurface(_this, (NativeWindowType)windata->gs);",
    "return ret;",
]
if not all(x in old for x in required) or old.count("return ret;") != 1:
    raise SystemExit("SDL KMSDRM source changed; refusing unverified patch")
# Replacement is kept explicit to allow review in public Git.
patched=r"""/* R36S legacy Mali GBM/EGL: try opaque scanout formats too.
   Do not touch graphics drivers or GLIBC. */
int KMSDRM_CreateSurfaces(_THIS, SDL_Window *window)
{
    SDL_VideoData *viddata = ((SDL_VideoData *)_this->driverdata);
    SDL_WindowData *windata = (SDL_WindowData *)window->driverdata;
    SDL_VideoDisplay *display = SDL_GetDisplayForWindow(window);
    SDL_DisplayData *dispdata = (SDL_DisplayData *)display->driverdata;
    const uint32_t formats[] = {
        GBM_FORMAT_XRGB8888, GBM_FORMAT_ARGB8888, GBM_FORMAT_RGB565
    };
    const char *names[] = { "XRGB8888", "ARGB8888", "RGB565" };
    const uint32_t flags = GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING;

    if (windata->gs) {
        KMSDRM_DestroySurfaces(_this, window);
    }
    KMSDRM_GetModeToSet(window, &dispdata->mode);
    display->current_mode.w = dispdata->mode.hdisplay;
    display->current_mode.h = dispdata->mode.vdisplay;
    display->current_mode.refresh_rate = dispdata->mode.vrefresh;

    for (size_t i = 0; i < SDL_arraysize(formats); ++i) {
        const uint32_t fmt = formats[i];
        if (!KMSDRM_gbm_device_is_format_supported(viddata->gbm_dev,
                                                   fmt, flags)) {
            SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO,
                        "KI KMSDRM: GBM %s unsupported flag combination; trying anyway",
                        names[i]);
        }
        windata->gs = KMSDRM_gbm_surface_create(viddata->gbm_dev,
            dispdata->mode.hdisplay, dispdata->mode.vdisplay, fmt, flags);
        if (!windata->gs) {
            SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO,
                        "KI KMSDRM: GBM %s create failed (driver/format)",
                        names[i]);
            continue;
        }

        SDL_EGL_SetRequiredVisualId(_this, fmt);
        windata->egl_surface = SDL_EGL_CreateSurface(_this,
            (NativeWindowType)windata->gs);
        if (windata->egl_surface == EGL_NO_SURFACE) {
            SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO,
                        "KI KMSDRM: EGL window surface %s failed: %s",
                        names[i], SDL_GetError());
            KMSDRM_gbm_surface_destroy(windata->gs);
            windata->gs = NULL;
            continue;
        }

        /* SDL2 creates the window before the GLES2 renderer/context. */
        if (SDL_EGL_MakeCurrent(_this, windata->egl_surface,
                               (EGLContext)SDL_GL_GetCurrentContext()) != 0) {
            SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO,
                        "KI KMSDRM: EGL make-current %s failed: %s",
                        names[i], SDL_GetError());
            SDL_EGL_DestroySurface(_this, windata->egl_surface);
            windata->egl_surface = EGL_NO_SURFACE;
            KMSDRM_gbm_surface_destroy(windata->gs);
            windata->gs = NULL;
            continue;
        }

        display->current_mode.format = fmt == GBM_FORMAT_RGB565
            ? SDL_PIXELFORMAT_RGB565 : SDL_PIXELFORMAT_XRGB8888;
        windata->egl_surface_dirty = SDL_FALSE;
        SDL_LogInfo(SDL_LOG_CATEGORY_VIDEO,
                    "KI KMSDRM: selected GBM/EGL surface %s", names[i]);
        SDL_SendWindowEvent(window, SDL_WINDOWEVENT_RESIZED,
                            dispdata->mode.hdisplay, dispdata->mode.vdisplay);
        return 0;
    }
    return SDL_SetError("KI KMSDRM: no compatible GBM/EGL surface (tried XRGB8888, ARGB8888, RGB565)");
}
"""
result=source[:start]+patched+source[end:]
assert result.count("KI KMSDRM: selected GBM/EGL surface")==1
assert len(result)>len(source)-1000
a.source.write_text(result)
print("Patched SDL2 KMSDRM GL/GBM format negotiation (3 formats)")
