#ifndef SDL_VIEWPORT_H
#define SDL_VIEWPORT_H

#include <SDL2/SDL.h>
#include <cstdint>

// Thin optional adapter that bridges the SDL-free Renderer to an SDL window.
// A game embedding the ray tracer can either use this helper or present the
// renderer's pixel buffer through its own SDL/OpenGL/Metal pipeline.
//
// The window can be larger than the rendered image: pass a smaller
// renderWidth/renderHeight to ray trace at a reduced internal resolution and
// let the GPU upscale it to the window. This is the usual trick for keeping a
// real-time ray traced game interactive.
//
// Typical per-frame usage:
//     viewport.beginFrame(pixels);   // upload + blit the rendered image
//     // ...draw overlays via viewport.renderer()...
//     viewport.endFrame();           // present to screen
class SDLViewport {
public:
    // If renderWidth/renderHeight are <= 0 the render resolution matches the
    // window size (backward-compatible behavior).
    SDLViewport(const char* title, int windowWidth, int windowHeight,
                int renderWidth = 0, int renderHeight = 0)
        : width_(renderWidth  > 0 ? renderWidth  : windowWidth),
          height_(renderHeight > 0 ? renderHeight : windowHeight),
          windowWidth_(windowWidth), windowHeight_(windowHeight) {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) return;
        window_ = SDL_CreateWindow(title,
                                   SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                   windowWidth, windowHeight, SDL_WINDOW_SHOWN);
        if (!window_) return;
        renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED);
        if (!renderer_) {
            // Fall back to a software renderer (headless / no-GPU systems).
            renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);
        }
        if (!renderer_) return;
        texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888,
                                     SDL_TEXTUREACCESS_STATIC, width_, height_);
    }

    ~SDLViewport() {
        if (texture_)  SDL_DestroyTexture(texture_);
        if (renderer_) SDL_DestroyRenderer(renderer_);
        if (window_)   SDL_DestroyWindow(window_);
        SDL_Quit();
    }

    SDLViewport(const SDLViewport&) = delete;
    SDLViewport& operator=(const SDLViewport&) = delete;

    bool valid() const { return window_ && renderer_ && texture_; }

    int width()  const { return width_; }
    int height() const { return height_; }

    // Size of the on-screen window (may differ from the render resolution).
    int windowWidth()  const { return windowWidth_; }
    int windowHeight() const { return windowHeight_; }

    SDL_Renderer* renderer() const { return renderer_; }
    SDL_Window*   window()   const { return window_; }

    void setTitle(const char* title) { SDL_SetWindowTitle(window_, title); }

    // Upload the rendered image and blit it to the back buffer.
    void beginFrame(const uint32_t* pixels) {
        SDL_UpdateTexture(texture_, nullptr, pixels,
                          width_ * static_cast<int>(sizeof(uint32_t)));
        SDL_RenderClear(renderer_);
        SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);
    }

    // Present the composed frame (call after drawing any overlays).
    void endFrame() { SDL_RenderPresent(renderer_); }

private:
    int width_;
    int height_;
    int windowWidth_;
    int windowHeight_;
    SDL_Window*   window_   = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture*  texture_  = nullptr;
};

#endif // SDL_VIEWPORT_H
