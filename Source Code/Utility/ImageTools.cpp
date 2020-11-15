#include "stdafx.h"

#include "Headers/ImageTools.h"

#include "Core/Headers/StringHelper.h"
#include "Utility/Headers/Localization.h"
#include "Platform/File/Headers/FileManagement.h"
#include "Platform/Video/Headers/RenderAPIEnums.h"

#define STB_IMAGE_IMPLEMENTATION
#pragma warning(push)
#pragma warning(disable:4505) //unreferenced local function has been removed
#pragma warning(disable:4189) //local variable is initialized but not referenced
#pragma warning(disable:4244) //conversion from X to Y possible loss of data
#include "stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"

#include "nv_dds.h"

#define IL_STATIC_LIB
#undef _UNICODE
#include <IL/il.h>
#include <IL/ilu.h>
#pragma warning(pop)

namespace Divide::ImageTools {

Mutex ImageDataInterface::_loadingMutex;

ImageData::ImageData() noexcept 
    : _compressed(false),
      _flip(false),
      _16Bit(false),
      _isHDR(false),
      _alpha(false),
      _bpp(0)

{
    _format = GFXImageFormat::COUNT;
    _dataType = GFXDataFormat::COUNT;
    _compressedTextureType = TextureType::COUNT;
}

bool ImageData::addLayer(Byte* data, const size_t size, const U16 width, const U16 height, const U16 depth) {
    ImageLayer& layer = _layers.emplace_back();
    return layer.allocateMip(data, size, width, height, depth);
}

bool ImageData::addLayer(const bool srgb, const U16 refWidth, const U16 refHeight, const stringImpl& fileName) {
    _name = fileName;

    if (Util::CompareIgnoreCase(_name.substr(_name.find_last_of('.') + 1), "DDS")) {
        return loadDDS_IL(srgb, refWidth, refHeight, fileName);
    }

    ImageLayer& layer = _layers.emplace_back();
    stbi_set_flip_vertically_on_load_thread(_flip ? TRUE : FALSE);

    I32 width = 0, height = 0, comp = 0;
    bufferPtr data;
    _isHDR = stbi_is_hdr(_name.c_str()) == TRUE;
    if (_isHDR) {
        set16Bit(false);
        data = stbi_loadf(_name.c_str(), &width, &height, &comp, 0);
    } else {
        if (is16Bit()) {
            data = stbi_load_16(_name.c_str(), &width, &height, &comp, 0);
        } else {
            data = stbi_load(_name.c_str(), &width, &height, &comp, 0);
        }
    }

    if (data == nullptr) {
        Console::errorfn(Locale::get(_ID("ERROR_IMAGETOOLS_INVALID_IMAGE_FILE")), _name.c_str());
        return false;
    }

    switch (comp) {
        case 1 : {
            _format = GFXImageFormat::RED;
            _bpp = 8;
        } break;
        case 2: {
            _format = GFXImageFormat::RG;
            _bpp = 16;
        } break;
        case 3: {
            _format = GFXImageFormat::RGB;
            _bpp = 24;
        } break;
        case 4: {
            _format = GFXImageFormat::RGBA;
            _bpp = 32;
        } break;
        default:
            DIVIDE_UNEXPECTED_CALL("Invalid file format!");
            break;
    }

    _dataType = is16Bit() ? GFXDataFormat::UNSIGNED_SHORT : isHDR() ? GFXDataFormat::FLOAT_32 : GFXDataFormat::UNSIGNED_BYTE;

    if (_isHDR) {
        _bpp *= 4;
    } else if (is16Bit()) {
        _bpp *= 2;
    }

    vectorEASTL<U32> resizedData = {};
    if (refWidth != 0 && refHeight != 0 && (refWidth != width || refHeight != height)) {
        resizedData.resize(refWidth * refHeight, 0u);
        I32 ret;
        if (is16Bit()) {
            ret = stbir_resize_uint16_generic(static_cast<U16*>(data), width, height, 0,
                                              reinterpret_cast<U16*>(resizedData.data()), refWidth, refHeight, 0,
                                              comp, -1, 0,
                                              STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT, STBIR_COLORSPACE_LINEAR,
                                              nullptr);
        } else if (isHDR()) {
            ret = stbir_resize_float(static_cast<F32*>(data), width, height, 0, reinterpret_cast<F32*>(resizedData.data()), refWidth, refHeight, 0, comp);
        } else {
            if (srgb) {
                ret = stbir_resize_uint8_srgb(static_cast<U8*>(data), width, height, 0, reinterpret_cast<U8*>(resizedData.data()), refWidth, refHeight, 0, comp, -1, 0);
            } else {
                ret = stbir_resize_uint8(static_cast<U8*>(data), width, height, 0, reinterpret_cast<U8*>(resizedData.data()), refWidth, refHeight, 0, comp);
            }
        }

        if (ret == 1) {
            width = refWidth;
            height = refHeight;
            stbi_image_free(static_cast<U16*>(data));
            data = static_cast<bufferPtr>(resizedData.data());
        }
    }

    const size_t baseSize = width * height * _bpp / 8;

    // most formats do not have an alpha channel
    _alpha = comp % 2 == 0;
    _compressed = false;
    bool ret = false;
    if (data != nullptr) {
        if (_isHDR) {
            ret = layer.allocateMip(static_cast<F32*>(data), baseSize / 4, to_U16(width), to_U16(height), 1u);
        } else if (is16Bit()) {
            ret = layer.allocateMip(static_cast<U16*>(data), baseSize / 2, to_U16(width), to_U16(height), 1u);
        } else {
            ret = layer.allocateMip(static_cast<U8*>(data), baseSize / 1, to_U16(width), to_U16(height), 1u);
        }
        stbi_image_free(data);
    }

    return ret;
}

bool ImageData::loadDDS_IL(const bool srgb, const U16 refWidth, const U16 refHeight, const stringImpl& filename) {
    ACKNOWLEDGE_UNUSED(refWidth);
    ACKNOWLEDGE_UNUSED(refHeight);

    const auto CheckError = []() {
        ILenum error = ilGetError();
        while (error != IL_NO_ERROR) {
            DebugBreak();
            error = ilGetError();
        }
    };

    ACKNOWLEDGE_UNUSED(srgb);
    std::lock_guard<Mutex> lock(ImageDataInterface::_loadingMutex);

    U32 ilTexture = 0;
    ilInit(); 
    CheckError();

    ilGenImages(1, &ilTexture);
    CheckError();

    ilBindImage(ilTexture);
    ilEnable(IL_TYPE_SET);
    //ilEnable(IL_ORIGIN_SET);
    //ilOriginFunc(_flip ? IL_ORIGIN_LOWER_LEFT : IL_ORIGIN_UPPER_LEFT);
    ilSetInteger(IL_KEEP_DXTC_DATA, IL_TRUE);
    ilTypeFunc(IL_UNSIGNED_BYTE);
    if (ilLoadImage(filename.c_str()) == IL_FALSE) {
        CheckError();
        Console::errorfn(Locale::get(_ID("ERROR_IMAGETOOLS_INVALID_IMAGE_FILE")), _name.c_str());
        ilDeleteImage(ilTexture);
        return false;
    }

    ILinfo imageInfo;
    iluGetImageInfo(&imageInfo);
    CheckError();

    if (imageInfo.Origin == IL_ORIGIN_UPPER_LEFT) {
        iluFlipImage();
    }

    const I32 numLayers = ilGetInteger(IL_NUM_LAYERS) + 1;
    assert(ilGetInteger(IL_IMAGE_TYPE) == IL_UNSIGNED_BYTE);
    const I32 dxtc = ilGetInteger(IL_DXTC_DATA_FORMAT);
    _bpp = to_U8(ilGetInteger(IL_IMAGE_BPP));
    if (ilGetInteger(IL_IMAGE_CUBEFLAGS) > 0) {
        _compressedTextureType = TextureType::TEXTURE_CUBE_MAP;
    } else {
        _compressedTextureType = ilGetInteger(IL_IMAGE_DEPTH) > 1
                                     ? TextureType::TEXTURE_3D
                                     : TextureType::TEXTURE_2D;
    }
    _compressed = dxtc == IL_DXT1 ||
        dxtc == IL_DXT2 ||
        dxtc == IL_DXT3 ||
        dxtc == IL_DXT4 ||
        dxtc == IL_DXT5;

    const I32 channelCount = ilGetInteger(IL_IMAGE_CHANNELS);
    CheckError();

    _alpha = false;
    if (_compressed) {
        switch (dxtc) {
            case IL_DXT1: {
                _format = channelCount == 3 ? GFXImageFormat::COMPRESSED_RGB_DXT1 : GFXImageFormat::COMPRESSED_RGBA_DXT1;
                _alpha = channelCount == 4;
            }  break;
            case IL_DXT3: {
                _format = GFXImageFormat::COMPRESSED_RGBA_DXT3;
                _alpha = true;
            } break;
            case IL_DXT5: {
                _format = GFXImageFormat::COMPRESSED_RGBA_DXT5;
                _alpha = true;
            } break;
            default: {
                DIVIDE_UNEXPECTED_CALL("Unsupported compressed format!");
                ilDeleteImage(ilTexture);
                CheckError();
                return false;
            }
        }
    } else {
        const I32 format = ilGetInteger(IL_IMAGE_FORMAT);
        switch (format) {
            case IL_BGR:
                _format = GFXImageFormat::BGR;
                break;
            case IL_RGB:
                _format = GFXImageFormat::RGB;
                break;
            case IL_BGRA:
                _format = GFXImageFormat::BGRA;
                _alpha = true;
                break;
            case IL_RGBA: {
                _format = GFXImageFormat::RGBA;
                _alpha = true;
            } break;
            case IL_LUMINANCE: {
                assert(false && "LUMINANCE image format is no longer supported!");
            } break;
            default: {
                DIVIDE_UNEXPECTED_CALL("Unsupported image format!");
                ilDeleteImage(ilTexture);
                CheckError();
                return false;
            }
        }
    }

    const I32 numMips = ilGetInteger(IL_NUM_MIPMAPS) + 1;
    if (_compressed && _alpha) {
        ilBindImage(ilTexture);
        ilActiveLayer(0);
        ilActiveImage(0);
        ilActiveMipmap(0);

        const ILint width = ilGetInteger(IL_IMAGE_WIDTH);
        const ILint height = ilGetInteger(IL_IMAGE_HEIGHT);
        const ILint depth = ilGetInteger(IL_IMAGE_DEPTH);
        _decompressedData.resize(to_size(width) * height * depth * 4);

        ilCopyPixels(0, 0, 0, width, height, depth, IL_RGBA, IL_UNSIGNED_BYTE, _decompressedData.data());
        CheckError();
    }

    const I32 numImagePasses = _compressedTextureType == TextureType::TEXTURE_CUBE_MAP ? 6 : 1;
    _dataType = GFXDataFormat::UNSIGNED_BYTE;

    _layers.reserve(_layers.size() + numLayers * numImagePasses);
    for (I32 l = 0; l < numLayers; ++l) {
        for (I32 p = 0; p < numImagePasses; ++p) {
            ImageLayer& layer = _layers.emplace_back();

            for (U8 m = 0; m < numMips; ++m) {
                ilBindImage(ilTexture);
                ilActiveLayer(l);
                ilActiveImage(p);
                ilActiveMipmap(m);
                const ILint width = ilGetInteger(IL_IMAGE_WIDTH);
                const ILint height = ilGetInteger(IL_IMAGE_HEIGHT);
                const ILint depth = ilGetInteger(IL_IMAGE_DEPTH);
                CheckError();

                const I32 size = _compressed ? ilGetDXTCData(nullptr, 0, dxtc) : width * height * depth * _bpp;
                CheckError();

                U8* data = layer.allocateMip<U8>(to_size(size), to_U16(width), to_U16(height), to_U16(depth));
                if (_compressed) {
                    ilGetDXTCData(data, size, dxtc);
                    CheckError();
                } else {
                    memcpy(data, ilGetData(), size);
                }
            }
        }
    }

    ilDeleteImage(ilTexture);
    CheckError();
    return true;
}

bool ImageData::loadDDS_NV(const bool srgb, const U16 refWidth, const U16 refHeight, const stringImpl& filename) {
    ACKNOWLEDGE_UNUSED(srgb);
    ACKNOWLEDGE_UNUSED(refWidth);
    ACKNOWLEDGE_UNUSED(refHeight);

    nv_dds::CDDSImage image;
    image.load(filename, _flip);
    _alpha = image.get_components() == 4;
    switch(image.get_type()) {
        case nv_dds::TextureFlat:
            _compressedTextureType = image.get_height() == 0 ? TextureType::TEXTURE_1D
                                                             : TextureType::TEXTURE_2D;
            break;
        case nv_dds::Texture3D:
            _compressedTextureType = TextureType::TEXTURE_3D;
            break;
        case nv_dds::TextureCubemap:
            _compressedTextureType = TextureType::TEXTURE_CUBE_MAP;
            break;
        case nv_dds::TextureNone:
            DIVIDE_UNEXPECTED_CALL("Unsupported texture type!");
            break;
    }

    _compressed = image.is_compressed();
    _bpp = image.get_format() == nv_dds::Format::DXT1 ? 8 : 16;
    switch(image.get_format()) {
      case nv_dds::Format::DXT1:
          assert(_compressed);
          _format = GFXImageFormat::COMPRESSED_RGB_DXT1;
          break;
      case nv_dds::Format::DXT3:
          assert(_compressed);
          _format = GFXImageFormat::COMPRESSED_RGBA_DXT3;
          break;
      case nv_dds::Format::DXT5:
          assert(_compressed);
          _format = GFXImageFormat::COMPRESSED_RGBA_DXT5;
          break;
      case nv_dds::Format::BGR:
      case nv_dds::Format::RGB: {
          assert(!_compressed);
          _bpp = 24;
          _format = GFXImageFormat::RGB;
      } break;
      case nv_dds::Format::BGRA:
      case nv_dds::Format::RGBA: {
          assert(!_compressed);
          _format = GFXImageFormat::RGBA;
          _bpp = 32;
      } break;
      case nv_dds::Format::LUMINANCE: {
          assert(!_compressed);
          assert(false && "LUMINANCE image format is no longer supported!");
      } break;
      case nv_dds::Format::COUNT:
          DIVIDE_UNEXPECTED_CALL("Unsupported image format!");
          break;
    }

    _dataType = GFXDataFormat::UNSIGNED_BYTE;

    const U32 numMips = image.get_num_mipmaps();
    ImageLayer& layer = _layers.emplace_back();
    if (layer.allocateMip<U8>(static_cast<U8*>(image), image.get_size(), to_U16(image.get_width()), to_U16(image.get_height()), to_U16(image.get_depth()))) {

        for (U8 i = 0; i < numMips; ++i) {
            const nv_dds::CSurface& mipMap = image.get_mipmap(i);
            if (!layer.allocateMip<U8>(static_cast<U8*>(mipMap), mipMap.get_size(), to_U16(mipMap.get_width()), to_U16(mipMap.get_height()), to_U16(mipMap.get_depth()))) {
                DIVIDE_UNEXPECTED_CALL();
            }
        }

        image.clear();
        return true;
    }

    return false;
}

UColour4 ImageData::getColour(const I32 x, const I32 y, U32 layer, const U8 mipLevel) const {
    ACKNOWLEDGE_UNUSED(layer);

    UColour4 returnColour;
    getColour(x, y, returnColour.r, returnColour.g, returnColour.b, returnColour.a, mipLevel);
    return returnColour;
}

void ImageData::getRed(const I32 x, const I32 y, U8& r, const U32 layer, const U8 mipLevel) const {
    assert(!_compressed || mipLevel == 0);
    assert(_layers.size() > layer);

    const I32 idx = (y * _layers[layer].getMip(mipLevel)->_dimensions.width + x) * (_bpp / 8);
    r = _compressed ? _decompressedData[idx + 0] : static_cast<U8*>(_layers[layer].getMip(mipLevel)->data())[idx + 0];
}

void ImageData::getGreen(const I32 x, const I32 y, U8& g, const U32 layer, const U8 mipLevel) const {
    assert(!_compressed || mipLevel == 0);
    assert(_layers.size() > layer);

    const size_t idx = to_size(y * _layers[layer].getMip(mipLevel)->_dimensions.width + x) * (_bpp / 8);
    g = _compressed ? _decompressedData[idx + 1] : static_cast<U8*>(_layers[layer].getMip(mipLevel)->data())[idx + 1];
}

void ImageData::getBlue(const I32 x, const I32 y, U8& b, const U32 layer, const U8 mipLevel) const {
    assert(!_compressed || mipLevel == 0);
    assert(_layers.size() > layer);

    const size_t idx = to_size(y * _layers[layer].getMip(mipLevel)->_dimensions.width + x) * (_bpp / 8);
    b = _compressed ? _decompressedData[idx + 2] : static_cast<U8*>(_layers[layer].getMip(mipLevel)->data())[idx + 2];
}

void ImageData::getAlpha(const I32 x, const I32 y, U8& a, const U32 layer, const U8 mipLevel) const {
    assert(!_compressed || mipLevel == 0);
    assert(_layers.size() > layer);

    a = 255;
    if (_alpha) {
        const size_t idx = to_size(y * _layers[layer].getMip(mipLevel)->_dimensions.width + x) * (_bpp / 8);
        a = _compressed ? _decompressedData[idx + 3] : static_cast<U8*>(_layers[layer].getMip(mipLevel)->data())[idx + 3];
    }
}

void ImageData::getColour(const I32 x, const I32 y, U8& r, U8& g, U8& b, U8& a, const U32 layer, const U8 mipLevel) const {
    assert(!_compressed || mipLevel == 0);
    assert(_layers.size() > layer);

    const I32 idx = (y * _layers[layer].getMip(mipLevel)->_dimensions.width + x) * (_bpp / 8);
    const U8* src = _compressed ? _decompressedData.data() : static_cast<U8*>(_layers[layer].getMip(mipLevel)->data());

    r = src[idx + 0]; g = src[idx + 1]; b = src[idx + 2]; a = _alpha ? src[idx + 3] : 255;
}

bool ImageDataInterface::CreateImageData(const ResourcePath& filename, const U16 refWidth, const U16 refHeight, const bool srgb, ImageData& imgOut) {
    if (fileExists(filename)) {
        return imgOut.addLayer(srgb, refWidth, refHeight, filename.str());
    }

    return false;
}

I8 SaveToTGA(const stringImpl& filename, const vec2<U16>& dimensions, U8 pixelDepth, U8* imageData) noexcept {
    const U8 cGarbage = 0;
    const I16 iGarbage = 0;
    const U16 width = dimensions.width;
    const U16 height = dimensions.height;

    // open file and check for errors
    FILE* file = fopen(filename.c_str(), "wb");
    if (file == nullptr) {
        return -1;
    }

    // compute image type: 2 for RGB(A), 3 for greyscale
    const U8 mode = pixelDepth / 8;
    const U8 type = pixelDepth == 24 || pixelDepth == 32 ? 2 : 3;

    // write the header
    fwrite(&cGarbage, sizeof(U8), 1, file);
    fwrite(&cGarbage, sizeof(U8), 1, file);
    fwrite(&type, sizeof(U8), 1, file);

    fwrite(&iGarbage, sizeof(I16), 1, file);
    fwrite(&iGarbage, sizeof(I16), 1, file);
    fwrite(&cGarbage, sizeof(U8), 1, file);
    fwrite(&iGarbage, sizeof(I16), 1, file);
    fwrite(&iGarbage, sizeof(I16), 1, file);

    fwrite(&width, sizeof(U16), 1, file);
    fwrite(&height, sizeof(U16), 1, file);
    fwrite(&pixelDepth, sizeof(U8), 1, file);
    fwrite(&cGarbage, sizeof(U8), 1, file);

    // convert the image data from RGB(a) to BGR(A)
    if (mode >= 3)
        for (I32 i = 0; i < width * height * mode; i += mode) {
            const U8 aux = imageData[i];
            imageData[i] = imageData[i + 2];
            imageData[i + 2] = aux;
        }

    // save the image data
    fwrite(imageData, sizeof(U8), to_size(width) * height * mode, file);
    fclose(file);
    return 0;
}

/// saves a series of files with names "filenameX.tga"
I8 SaveSeries(const stringImpl& filename, const vec2<U16>& dimensions, const U8 pixelDepth, U8* imageData) {
    static I32 savedImages = 0;
    // compute the new filename by adding the
    // series number and the extension
    const stringImpl newFilename(Util::StringFormat("%s_%d.tga", filename.c_str(), savedImages));

    // save the image
    const I8 status = SaveToTGA(newFilename, dimensions, pixelDepth, imageData);

    // increase the counter
    if (status == 0) {
        savedImages++;
    }

    return status;
}
}  // namespace Divide::ImageTools

