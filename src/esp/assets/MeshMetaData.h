// Copyright (c) Facebook, Inc. and its affiliates.
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#ifndef ESP_ASSETS_MESHMETADATA_H_
#define ESP_ASSETS_MESHMETADATA_H_

/** @file
 * @brief Struct @ref esp::assets::MeshTransformNode, Struct @ref
 * esp::assets::MeshMetaData
 */

#include "esp/core/esp.h"
#include "esp/gfx/magnum.h"

namespace esp {
namespace assets {

/**
 * @brief Stores meta data for objects with a multi-component transformation
 * heirarchy.
 *
 * Some mesh files include a transformation hierarchy. A @ref
 * MeshTransformNode stores this hierarchy and indices for the meshes and
 * materials at each level such that it can be reused to instance meshes later.
 */
struct MeshTransformNode {
  /** @brief Local mesh index within @ref MeshMetaData::meshIndex. */
  int meshIDLocal;

  /** @brief Local material index within @ref MeshMetaData::materialIndex */
  int materialIDLocal;

  /** @brief Object index of asset component in the original file. */
  int componentID;

  /** @brief The component transformation subtrees with this node as the root.
   */
  std::vector<MeshTransformNode> children;

  /** @brief Node local transform to the parent frame */
  Magnum::Matrix4 transformFromLocalToParent;

  /** @brief Default constructor. */
  MeshTransformNode() {
    meshIDLocal = ID_UNDEFINED;
    materialIDLocal = ID_UNDEFINED;
    componentID = ID_UNDEFINED;
  };
};

/**
 * @brief Stores meta data for an asset possibly containing multiple meshes,
 * materials, textures, and a heirarchy of component transform relationships.
 *
 * As each type of data may contain a few items, we save the start index, and
 * the end index (of each type) as a pair. In current implementation: ptex mesh:
 * meshes_ (1 item), textures_ (0 item), materials_ (0 item); instance mesh:
 * meshes_ (1 item), textures_ (0 item), materials_ (0 item); gltf_mesh,
 * glb_mesh: meshes_ (i items), textures (j items), materials_ (k items), i, j,
 * k = 0, 1, 2 ...
 */
struct MeshMetaData {
  /** @brief Start index of a data type in the global asset datastructure. */
  typedef int start;

  /** @brief End index of a data type in the global asset datastructure. */
  typedef int end;

  /** @brief Index range (inclusive) of mesh data for the asset in the global
   * asset datastructure. */
  std::pair<start, end> meshIndex = std::make_pair(ID_UNDEFINED, ID_UNDEFINED);

  /** @brief Index range (inclusive) of texture data for the asset in the global
   * asset datastructure. */
  std::pair<start, end> textureIndex =
      std::make_pair(ID_UNDEFINED, ID_UNDEFINED);

  /** @brief Index range (inclusive) of material data for the asset in the
   * global asset datastructure. */
  std::pair<start, end> materialIndex =
      std::make_pair(ID_UNDEFINED, ID_UNDEFINED);

  /** @brief The root of the mesh component transformation heirarchy tree which
   * stores the relationship between components of the asset.*/
  MeshTransformNode root;

  /** @brief Default constructor. */
  MeshMetaData() = default;
  ;

  /** @brief  Constructor. */
  MeshMetaData(int meshStart,
               int meshEnd,
               int textureStart = ID_UNDEFINED,
               int textureEnd = ID_UNDEFINED,
               int materialStart = ID_UNDEFINED,
               int materialEnd = ID_UNDEFINED) {
    meshIndex = std::make_pair(meshStart, meshEnd);
    textureIndex = std::make_pair(textureStart, textureEnd);
    materialIndex = std::make_pair(materialStart, materialEnd);
  }

  /**
   * @brief Sets the mesh indices for the asset See @ref
   * ResourceManager::meshes_.
   * @param meshStart First index for asset mesh data in the global mesh
   * datastructure.
   * @param meshEnd Final index for asset mesh data in the global mesh
   * datastructure.
   */
  void setMeshIndices(int meshStart, int meshEnd) {
    meshIndex.first = meshStart;
    meshIndex.second = meshEnd;
  }

  /**
   * @brief Sets the texture indices for the asset. See @ref
   * ResourceManager::textures_.
   * @param textureStart First index for asset texture data in the global
   * texture datastructure.
   * @param textureEnd Final index for asset texture data in the global texture
   * datastructure.
   */
  void setTextureIndices(int textureStart, int textureEnd) {
    textureIndex.first = textureStart;
    textureIndex.second = textureEnd;
  }

  /**
   * @brief Sets the material indices for the asset. See @ref
   * ResourceManager::materials_.
   * @param materialStart First index for asset material data in the global
   * material datastructure.
   * @param materialEnd Final index for asset material data in the global
   * material datastructure.
   */
  void setMaterialIndices(int materialStart, int materialEnd) {
    materialIndex.first = materialStart;
    materialIndex.second = materialEnd;
  }
};

}  // namespace assets
}  // namespace esp

#endif  // ESP_ASSETS_MESHMETADATA_H_
