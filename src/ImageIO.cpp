// ---------------------------------------------------------------------------
// ImageIO.cpp  -  the single translation unit that compiles stb_image
// ---------------------------------------------------------------------------
// stb_image is a public-domain single-header decoder (PNG, JPG, BMP, GIF, TGA,
// PSD, ...). It must be instantiated in exactly one .cpp via the
// STB_IMAGE_IMPLEMENTATION macro; keeping it isolated here means the heavy
// decoder code is compiled once and the rest of the engine only sees the small
// loadImageRGB() declaration in <raytracer/Texture.h>.
// ---------------------------------------------------------------------------

#include <raytracer/Texture.h>

#include <string>
#include <vector>

// We only need 8-bit LDR decoding; trim a little build time / binary size.
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#include "stb_image.h"

namespace rt_imageio {

bool loadImageRGB(const std::string& path, int& width, int& height,
                  std::vector<uint8_t>& rgbOut) {
    int channels = 0;
    // Force 3 channels (RGB) so the pixel layout matches ImageTexture::sample.
    unsigned char* data = stbi_load(path.c_str(), &width, &height,
                                    &channels, 3);
    if (!data) return false;

    const size_t bytes = size_t(width) * size_t(height) * 3;
    rgbOut.assign(data, data + bytes);
    stbi_image_free(data);
    return true;
}

} // namespace rt_imageio
