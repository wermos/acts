// This file is part of the ACTS project.
//
// Copyright (C) 2016 ACTS project team
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

///////////////////////////////////////////////////////////////////
// DetExtension.h, ACTS project, DD4hepDetector plugin
///////////////////////////////////////////////////////////////////

#ifndef ACTS_DD4HEPDETECTORELEMENT_DETEXTENSION_H
#define ACTS_DD4HEPDETECTORELEMENT_DETEXTENSION_H 1

#include "ACTS/Plugins/DD4hepPlugins/IDetExtension.hpp"
// DD4hep
#include <vector>
#include "DD4hep/Detector.h"

namespace Acts {

/// @class DetExtension
///
/// Implementation of the IDetExtension class, which uses the extension
/// mechanism
/// of DD4hep, needed for the translation from the DD4hep geometry into the
/// tracking geometry of the ACTS package.
/// In this way, the segmentation of the sensitive detector elements can be
/// directly accessed from DD4hep to ensure consistency between the full and the
/// tracking geometry.
/// Since in DD4hep volumes used as a cylinder (detector layers are binned in r
/// and
/// z, e.g. central barrel volume) and discs (detector layers are binned in r
/// and
/// phi, e.g. end caps) are both described as a ROOT TGeoConeSeg one needs to
/// distinguish between these volume types by setting the shape.

class DetExtension : public IDetExtension
{
public:
  /// Constructor
  DetExtension();
  /// Constructor for volume with shape to distinguish between disc and cylinder
  /// volume
  /// @param shape The type of the shape defined in IDetExtension can be either
  /// disc or cylinder
  DetExtension(ShapeType shape);
  /// Constructor for layer with modules
  /// @param axes is the axis orientation with respect to the tracking frame
  DetExtension(const std::string& axes);
  /// Constructor for layer with support structure and modules
  /// @note the numer of bins determines the granularity of the material
  /// map of the layer
  /// @param bins1 The number of bins in first direction of the layer
  /// which is phi for both, cylinder and disc layers.
  /// @param bins2 The number of bins in second direction of the layer
  /// which is r in case of a disc layer and z in case of a cylinder layer
  /// @param layerMatPos states if the material should be mapped on the inner,
  /// the center or the outer surface of the layer
  /// @param axes is the axis orientation with respect to the tracking frame
  DetExtension(size_t             bins1,
               size_t             bins2,
               LayerMaterialPos   layerMatPos,
               const std::string& axes = "XYZ");
  /// Copy constructor
  DetExtension(const DetExtension&, const DD4hep::Geometry::DetElement&);
  /// destructor
  ~DetExtension() = default;
  /// Possibility to set shape of a volume to distinguish between disc and
  /// cylinder volume
  /// @param shape The type of the shape defined in IDetExtension can be either
  /// disc or cylinder
  void
  setShape(ShapeType shape) override;
  /// Access shape type of a volume to distinguish between disc and cylinder
  /// volume
  /// @return shape The type of the shape defined in IDetExtension can be either
  /// disc or cylinder
  ShapeType
  shape() const override;
  /// possibility to mark layer to have support material
  /// @note the numer of bins determines the granularity of the material
  /// map of the layer
  /// @param bins1 The number of bins in first direction of the layer
  /// which is phi for both, cylinder and disc layers.
  /// @param bins2 The number of bins in second direction of the layer
  /// which is r in case of a disc layer and z in case of a cylinder layer
  /// @param layerMatPos states if the material should be mapped on the inner,
  /// the center or the outer surface of the layer
  void
  supportMaterial(size_t           bins1,
                  size_t           bins2,
                  LayerMaterialPos layerMatPos) override;
  /// Bool returning true if the layers should carry material
  bool
  hasSupportMaterial() const override;
  /// Access to the two bin numbers determining the granularity of the two
  /// dimensional grid
  /// on which the material of the layer should be mapped on
  std::pair<size_t, size_t>
  materialBins() const override;
  /// returns states if the material should be mapped on the inner,
  /// the center or the outer surface of the layer
  Acts::LayerMaterialPos
  layerMaterialPos() const override;
  /// Possibility to set contained detector modules of a layer
  /// @param axes is the axis orientation with respect to the tracking frame
  ///        it is a string of the three characters x, y and z (standing for the
  ///        three axes)
  ///        there is a distinction between capital and lower case characters :
  ///        - capital      -> positive orientation of the axis
  ///        - lower case   -> negative oriantation of the axis
  ///        example options are "XYZ" -> identical frame definition (default
  ///        value)
  ///                            "YZX" -> node y axis is tracking x axis, etc.
  ///                            "XzY" -> negative node z axis is tracking y
  ///                            axis, etc.
  void
  setAxes(const std::string& axes = "XYZ") override;
  /// Access the orientation of the module in respect to the tracking frame
  /// @return string describing the orientation of the axes
  const std::string
  axes() const override;

private:
  /// Shape type of a volume defined in IDetExtension can be either disc or
  /// cylinder
  ShapeType m_shape;
  /// Stating if the layer will carry material
  bool m_supportMaterial;
  /// The number of bins in first direction of the layer
  /// which is phi for both, cylinder and disc layers.
  size_t m_bins1;
  /// The number of bins in second direction of the layer
  /// which is r in case of a disc layer and z in case of a cylinder layer
  size_t m_bins2;
  /// States if the material should be mapped on the inner,
  /// the center or the outer surface of the layer
  LayerMaterialPos m_layerMatPos;
  /// orientation of a module in respect to the tracking frame
  std::string m_axes;
};
}

inline void
Acts::DetExtension::setShape(Acts::ShapeType type)
{
  m_shape = std::move(type);
}

inline Acts::ShapeType
Acts::DetExtension::shape() const
{
  return m_shape;
}

inline void
Acts::DetExtension::supportMaterial(size_t           bins1,
                                    size_t           bins2,
                                    LayerMaterialPos layerMatPos)
{
  m_supportMaterial = true;
  m_bins1           = bins1;
  m_bins2           = bins2;
  m_layerMatPos     = layerMatPos;
}

inline bool
Acts::DetExtension::hasSupportMaterial() const
{
  return m_supportMaterial;
}

inline Acts::LayerMaterialPos
Acts::DetExtension::layerMaterialPos() const
{
  return m_layerMatPos;
}

inline std::pair<size_t, size_t>
Acts::DetExtension::materialBins() const
{
  std::pair<size_t, size_t> bins(m_bins1, m_bins2);
  return (bins);
}

inline void
Acts::DetExtension::setAxes(const std::string& axes)
{
  m_axes = axes;
}

inline const std::string
Acts::DetExtension::axes() const
{
  return m_axes;
}

#endif  // ACTS_DD4HEPDETECTORELEMENT_DETEXTENSION_H
