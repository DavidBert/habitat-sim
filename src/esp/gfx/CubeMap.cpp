// Copyright (c) Facebook, Inc. and its affiliates.
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree
#include "CubeMap.h"
#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/StridedArrayView.h>
#include <Corrade/PluginManager/Manager.h>
#include <Corrade/Utility/Assert.h>
#include <Corrade/Utility/FormatStl.h>
#include <Magnum/DebugTools/TextureImage.h>
#include <Magnum/GL/Framebuffer.h>
#include <Magnum/GL/RenderbufferFormat.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/GL/TextureFormat.h>
#include <Magnum/Image.h>
#include <Magnum/ImageView.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Shaders/Generic.h>
#include <Magnum/Trade/AbstractImageConverter.h>
#include <Magnum/Trade/ImageData.h>

namespace Mn = Magnum;
namespace Cr = Corrade;

namespace esp {
namespace gfx {

const Mn::GL::Framebuffer::ColorAttachment colorAttachment[6] = {
    Mn::GL::Framebuffer::ColorAttachment{0},
    Mn::GL::Framebuffer::ColorAttachment{1},
    Mn::GL::Framebuffer::ColorAttachment{2},
    Mn::GL::Framebuffer::ColorAttachment{3},
    Mn::GL::Framebuffer::ColorAttachment{4},
    Mn::GL::Framebuffer::ColorAttachment{5},
};

// TODO:
// const Mn::GL::Framebuffer::ColorAttachment objectIdAttachment =
//    Mn::GL::Framebuffer::ColorAttachment{1};

/**
 * @brief check if the class instance is created with corresponding texture
 * enabled
 */
void textureTypeSanityCheck(CubeMap::Flags& flag,
                            CubeMap::TextureType type,
                            const std::string& functionNameStr) {
  switch (type) {
    case CubeMap::TextureType::Color:
      CORRADE_ASSERT(flag & CubeMap::Flag::ColorTexture,
                     functionNameStr.c_str()
                         << "instance was not created with color "
                            "texture output enabled.", );
      return;
      break;
    case CubeMap::TextureType::Depth:
      CORRADE_ASSERT(flag & CubeMap::Flag::DepthTexture,
                     functionNameStr.c_str()
                         << "instance was not created with depth "
                            "texture output enabled.", );
      return;
      break;
  }
  CORRADE_INTERNAL_ASSERT_UNREACHABLE();
}

/**
 * @brief convert cube face index to Magnum::GL::CubeMapCoordinate
 */
Magnum::GL::CubeMapCoordinate convertFaceIndexToCubeMapCoordinate(
    unsigned int faceIndex) {
  CORRADE_ASSERT(
      faceIndex < 6,
      "In CubeMap: ConvertFaceIndexToCubeMapCoordinate(): the index of "
      "the cube side"
          << faceIndex << "is illegal.",
      Mn::GL::CubeMapCoordinate::PositiveX);
  return Mn::GL::CubeMapCoordinate(int(Mn::GL::CubeMapCoordinate::PositiveX) +
                                   faceIndex);
}

/**
 * @brief get texture type string for texture filename
 */
const char* getTextureTypeFilenameString(CubeMap::TextureType type) {
  switch (type) {
    case CubeMap::TextureType::Color:
      return "rgba";
      break;
    case CubeMap::TextureType::Depth:
      return "depth";
      break;
  }
  CORRADE_INTERNAL_ASSERT_UNREACHABLE();
}

/**
 * @brief get the pixel format based on texture type (color, depth objectId
 * etc.)
 */
Mn::PixelFormat getPixelFormat(CubeMap::TextureType type) {
  switch (type) {
    case CubeMap::TextureType::Color:
      return Mn::PixelFormat::RGBA8Unorm;
      break;
    case CubeMap::TextureType::Depth:
      return Mn::PixelFormat::R32F;
      break;
      /*
      case CubeMap::TextureType::ObjectId:
      return Mn::PixelFormat::R32UI;
      */
  }
  CORRADE_INTERNAL_ASSERT_UNREACHABLE();
}

CubeMap::CubeMap(int imageSize, Flags flags) : flags_(flags) {
#ifndef MAGNUM_TARGET_WEBGL
  Mn::GL::Renderer::enable(Mn::GL::Renderer::Feature::SeamlessCubeMapTexture);
#endif
  reset(imageSize);
}

bool CubeMap::reset(int imageSize) {
  if (imageSize_ == imageSize) {
    return false;
  }

  imageSize_ = imageSize;
  CORRADE_ASSERT(imageSize_ > 0,
                 "CubeMap::reset(): image size" << imageSize << "is illegal.",
                 false);
  // create an empty cubemap texture
  recreateTexture();

  // prepare frame buffer and render buffer
  recreateFramebuffer();

  // attach render buffer to frame buffer
  attachFramebufferRenderbuffer();

  return true;
}

void CubeMap::attachFramebufferRenderbuffer() {
  if (flags_ & Flag::ColorTexture) {
    for (unsigned int index = 0; index < 6; ++index) {
      Magnum::GL::CubeMapCoordinate cubeMapCoord =
          convertFaceIndexToCubeMapCoordinate(index);
      frameBuffer_.attachCubeMapTexture(colorAttachment[index],
                                        *textures_[TextureType::Color],
                                        cubeMapCoord, 0);
    }
  }
  if (!(flags_ & Flag::DepthTexture)) {
    frameBuffer_.attachRenderbuffer(
        Mn::GL::Framebuffer::BufferAttachment::Depth, optionalDepthBuffer_);
  }
}

void CubeMap::recreateTexture() {
  Mn::Vector2i size{imageSize_, imageSize_};

  // color texture
  if (flags_ & Flag::ColorTexture) {
    auto& colorTexture = textures_[TextureType::Color];
    colorTexture = std::make_unique<Mn::GL::CubeMapTexture>();
    (*colorTexture)
        .setWrapping(Mn::GL::SamplerWrapping::ClampToEdge)
        .setMinificationFilter(Mn::GL::SamplerFilter::Linear,
                               Mn::GL::SamplerMipmap::Linear)
        .setMagnificationFilter(Mn::GL::SamplerFilter::Linear);

    if (flags_ & Flag::BuildMipmap) {
      // RGBA8 is for the LDR. Use RGBA16F for the HDR (TODO)
      (*colorTexture)
          .setStorage(Mn::Math::log2(imageSize_) + 1,
                      Mn::GL::TextureFormat::RGBA8, size);
    } else {
      (*colorTexture).setStorage(1, Mn::GL::TextureFormat::RGBA8, size);
    }
  }

  // depth texture
  if (flags_ & Flag::DepthTexture) {
    auto& depthTexture = textures_[TextureType::Depth];
    depthTexture = std::make_unique<Mn::GL::CubeMapTexture>();
    (*depthTexture)
        .setWrapping(Mn::GL::SamplerWrapping::ClampToEdge)
        .setMinificationFilter(Mn::GL::SamplerFilter::Nearest)
        .setMagnificationFilter(Mn::GL::SamplerFilter::Nearest)
        .setStorage(1, Mn::GL::TextureFormat::DepthComponent32F, size);
  }
}

void CubeMap::recreateFramebuffer() {
  Mn::Vector2i viewportSize{imageSize_, imageSize_};
  frameBuffer_ = Mn::GL::Framebuffer{{{}, viewportSize}};
  // optional depth buffer is 24-bit integer pixel, which is different from the
  // depth texture (32-bit float)
  optionalDepthBuffer_.setStorage(Mn::GL::RenderbufferFormat::DepthComponent24,
                                  viewportSize);
}

void CubeMap::prepareToDraw(unsigned int cubeSideIndex) {
  mapForDraw(cubeSideIndex);

  // sorry, unlike color buffers, for depth buffer you have to reattach it every
  // time
  // however, if NOT using depth texture, we do not need to attach the depth
  // buffer again and again
  if (flags_ & Flag::DepthTexture) {
    Magnum::GL::CubeMapCoordinate cubeMapCoord =
        convertFaceIndexToCubeMapCoordinate(cubeSideIndex);
    frameBuffer_.attachCubeMapTexture(
        Mn::GL::Framebuffer::BufferAttachment::Depth,
        *textures_[TextureType::Depth], cubeMapCoord, 0);
  }

  frameBuffer_.clearDepth(1.0f).clearColor(0,                // color attachment
                                           Mn::Vector4ui{0}  // clear color
  );

  CORRADE_INTERNAL_ASSERT(
      frameBuffer_.checkStatus(Mn::GL::FramebufferTarget::Draw) ==
      Mn::GL::Framebuffer::Status::Complete);
}

void CubeMap::mapForDraw(unsigned int colorAttachmentIndex) {
  frameBuffer_.mapForDraw({
      {Mn::Shaders::Generic3D::ColorOutput,
       colorAttachment[colorAttachmentIndex]},
      // TODO:
      //{Mn::Shaders::Generic3D::ObjectIdOutput, objectIdAttachment}
  });
}

Mn::GL::CubeMapTexture& CubeMap::getTexture(TextureType type) {
  textureTypeSanityCheck(flags_, type, "CubeMap::getTexture():");
  return *textures_[type];
}

#ifndef MAGNUM_TARGET_WEBGL
// because Mn::Image2D image = textures_[type]->image(...)
// requires desktop OpenGL
bool CubeMap::saveTexture(TextureType type,
                          const std::string& imageFilePrefix) {
  textureTypeSanityCheck(flags_, type, "CubeMap::saveTexture():");

  Cr::PluginManager::Manager<Mn::Trade::AbstractImageConverter> manager;
  Cr::Containers::Pointer<Mn::Trade::AbstractImageConverter> converter;
  if (!(converter = manager.loadAndInstantiate("AnyImageConverter"))) {
    return false;
  }

  const char* coordStrings[6] = {"+X", "-X", "+Y", "-Y", "+Z", "-Z"};
  for (int iFace = 0; iFace < 6; ++iFace) {
    Mn::Image2D image = textures_[type]->image(
        convertFaceIndexToCubeMapCoordinate(iFace), 0, {getPixelFormat(type)});

    std::string filename = "";
    switch (type) {
      case TextureType::Color: {
        filename = Cr::Utility::formatString("{}.{}.{}.png", imageFilePrefix,
                                             getTextureTypeFilenameString(type),
                                             coordStrings[iFace]);
      } break;

      case TextureType::Depth: {
        filename = Cr::Utility::formatString("{}.{}.{}.hdr", imageFilePrefix,
                                             getTextureTypeFilenameString(type),
                                             coordStrings[iFace]);
      } break;
    }
    CORRADE_ASSERT(!filename.empty(),
                   "CubeMap::saveTexture(): Unknown texture type.", false);

    if (!converter->exportToFile(image, filename)) {
      return false;
    } else {
      LOG(INFO) << "Saved image " << iFace << " to " << filename;
    }
  }

  return true;
}
#endif

void CubeMap::loadTexture(TextureType type,
                          const std::string& imageFilePrefix,
                          const std::string& imageFileExtension) {
  textureTypeSanityCheck(flags_, type, "CubeMap::loadTexture():");

  // plugin manager used to instantiate importers which in turn are used
  // to load image data
  Cr::PluginManager::Manager<Mn::Trade::AbstractImporter> manager;
  Cr::Containers::Pointer<Mn::Trade::AbstractImporter> importer =
      manager.loadAndInstantiate("AnyImageImporter");
  CORRADE_INTERNAL_ASSERT(importer);

  const char* coordStrings[6] = {"+X", "-X", "+Y", "-Y", "+Z", "-Z"};
  int imageSize = 0;

  // set images
  Mn::GL::CubeMapTexture* texture = nullptr;
  switch (type) {
    case TextureType::Color:
      texture = textures_[TextureType::Color].get();
      break;

    case TextureType::Depth:
      texture = textures_[TextureType::Depth].get();
      break;
  }
  CORRADE_ASSERT(texture, "CubeMap::loadTexture(): Unknown texture type.", );

  for (int iFace = 0; iFace < 6; ++iFace) {
    // open image file

    std::string filename = Cr::Utility::formatString(
        "{}.{}.{}.{}", imageFilePrefix, getTextureTypeFilenameString(type),
        coordStrings[iFace], imageFileExtension);

    importer->openFile(filename);
    Cr::Containers::Optional<Mn::Trade::ImageData2D> imageData =
        importer->image2D(0);

    // sanity checks
    CORRADE_INTERNAL_ASSERT(imageData);
    Mn::Vector2i size = imageData->size();
    CORRADE_ASSERT(
        size.x() == size.y(),
        " CubeMap::loadTexture(): each texture image must be a square.", );
    if (iFace == 0) {
      imageSize = size.x();
      reset(imageSize);
    } else {
      CORRADE_ASSERT(size.x() == imageSize,
                     " CubeMap::loadTexture(): texture images must have the "
                     "same size.", );
    }

    switch (type) {
      case TextureType::Color:
        texture->setSubImage(convertFaceIndexToCubeMapCoordinate(iFace), 0, {},
                             *imageData);
        break;

      case TextureType::Depth: {
        // The pixel format for depth texture is R32F. When it is saved as hdr,
        // the single channel is expanded to three channels by repeating the R
        // channel 3 times (and it becomes RGB32F).
        // That means when it is loaded, we need to dedup by taking just the
        // first component (R component) out of each pixel.
        Cr::Containers::StridedArrayView2D<const Mn::Color3>
            imageStridedArrayView = imageData->pixels<Mn::Color3>();

        Cr::Containers::Array<float> depthImage{
            static_cast<size_t>(size.x() * size.y())};
        unsigned int idx = 0;
        for (auto row : imageStridedArrayView) {
          for (const Mn::Color3& pixel : row) {
            depthImage[idx++] = pixel.r();
          }
        }

        Mn::ImageView2D imageView(Mn::PixelFormat::R32F, imageData->size(),
                                  depthImage);
        texture->setSubImage(convertFaceIndexToCubeMapCoordinate(iFace), 0, {},
                             imageView);
      } break;
    }
  }
  // Color texture ONLY, NOT for depth
  if ((flags_ & Flag::BuildMipmap) && (flags_ & Flag::ColorTexture)) {
    texture->generateMipmap();
  }
}  // namespace gfx

void CubeMap::renderToTexture(CubeMapCamera& camera,
                              scene::SceneGraph& sceneGraph,
                              RenderCamera::Flags flags) {
  CORRADE_ASSERT(camera.isInSceneGraph(sceneGraph),
                 "CubeMap::renderToTexture(): camera is NOT attached to the "
                 "current scene graph.", );
  // ==== projection matrix ====
  // CAREFUL! In this function the projection matrix of the camera is assumed to
  // be set already outside of this function by the user.
  // we simply do sanity check here.
  {
    Mn::Vector2i vp = camera.viewport();
    CORRADE_ASSERT(vp == Mn::Vector2i{imageSize_},
                   "CubeMap::renderToTexture(): the image size with in the "
                   "CubeMapCamera, which is"
                       << vp << "compared to" << imageSize_
                       << "is not correct.", );
  }

  // ==== camera matrix ====
  // In case user set the relative transformation of
  // the camera node before calling this function, original viewing matrix of
  // the camera MUST be updated as well.
  camera.updateOriginalViewingMatrix();
  frameBuffer_.bind();
  for (int iFace = 0; iFace < 6; ++iFace) {
    camera.switchToFace(iFace);
    prepareToDraw(iFace);

    // TODO:
    // camera should have flags so that it can do "low quality" rendering,
    // e.g., no normal maps, no specular lighting, low-poly meshes,
    // low-quality textures.

    for (auto& it : sceneGraph.getDrawableGroups()) {
      // TODO: remove || true
      if (it.second.prepareForDraw(camera) || true) {
        camera.draw(it.second, flags);
      }
    }
  }  // iFace

  // CAREFUL!!!
  // switchToFace() will change the local transformation of this camera node!
  // If you do not do anything, in next rendering cycle, since
  // camera.updateOriginalViewingMatrix() is called, the original viewing matrix
  // will be updated by mistake!!! To prevent such mistakes, local
  // transformation of this camera node must be reset.
  camera.restoreTransformation();

  // Color texture ONLY, NOT for depth
  if ((flags_ & Flag::BuildMipmap) && (flags_ & Flag::ColorTexture)) {
    textures_[TextureType::Color]->generateMipmap();
  }
}

}  // namespace gfx
}  // namespace esp
