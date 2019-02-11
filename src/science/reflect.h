/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 */
#include <vendor/spirv_cross/spirv_glsl.hpp>

namespace science {

static inline const char* string_StorageClass(spv::StorageClass sc) {
  switch (sc) {
    case spv::StorageClassUniformConstant:
      return "UniformConstant";
    case spv::StorageClassInput:
      return "Input";
    case spv::StorageClassUniform:
      return "Uniform";
    case spv::StorageClassOutput:
      return "Output";
    case spv::StorageClassWorkgroup:
      return "Workgroup";
    case spv::StorageClassCrossWorkgroup:
      return "CrossWorkgroup";
    case spv::StorageClassPrivate:
      return "Private";
    case spv::StorageClassFunction:
      return "Function";
    case spv::StorageClassGeneric:
      return "Generic";
    case spv::StorageClassPushConstant:
      return "PushConstant";
    case spv::StorageClassAtomicCounter:
      return "AtomicCounter";
    case spv::StorageClassImage:
      return "Image";
    case spv::StorageClassStorageBuffer:
      return "StorageBuffer";
    case spv::StorageClassMax:
      break;
  }
  return "string_StorageClass(unknown)";
}

static inline const char* string_Decoration(spv::Decoration d) {
  switch (d) {
    case spv::DecorationRelaxedPrecision:
    case spv::DecorationSpecId:
      return "SpecId";
    case spv::DecorationBlock:
      return "Block";
    case spv::DecorationBufferBlock:
      return "BufferBlock";
    case spv::DecorationRowMajor:
      return "RowMajor";
    case spv::DecorationColMajor:
      return "ColMajor";
    case spv::DecorationArrayStride:
      return "ArrayStride";
    case spv::DecorationMatrixStride:
      return "MatrixStride";
    case spv::DecorationGLSLShared:
      return "GLSLShared";
    case spv::DecorationGLSLPacked:
      return "GLSLPacked";
    case spv::DecorationCPacked:
      return "CPacked";
    case spv::DecorationBuiltIn:
      return "BuiltIn";
    case spv::DecorationNoPerspective:
      return "NoPerspective";
    case spv::DecorationFlat:
      return "Flat";
    case spv::DecorationPatch:
      return "Patch";
    case spv::DecorationCentroid:
      return "Centroid";
    case spv::DecorationSample:
      return "Sample";
    case spv::DecorationInvariant:
      return "Invariant";
    case spv::DecorationRestrict:
      return "Restrict";
    case spv::DecorationAliased:
      return "Aliased";
    case spv::DecorationVolatile:
      return "Volatile";
    case spv::DecorationConstant:
      return "Constant";
    case spv::DecorationCoherent:
      return "Coherent";
    case spv::DecorationNonWritable:
      return "NonWritable";
    case spv::DecorationNonReadable:
      return "NonReadable";
    case spv::DecorationUniform:
      return "Uniform";
    case spv::DecorationSaturatedConversion:
      return "SaturatedConversion";
    case spv::DecorationStream:
      return "Stream";
    case spv::DecorationLocation:
      return "Location";
    case spv::DecorationComponent:
      return "Component";
    case spv::DecorationIndex:
      return "Index";
    case spv::DecorationBinding:
      return "Binding";
    case spv::DecorationDescriptorSet:
      return "DescriptorSet";
    case spv::DecorationOffset:
      return "Offset";
    case spv::DecorationXfbBuffer:
      return "XfbBuffer";
    case spv::DecorationXfbStride:
      return "XfbStride";
    case spv::DecorationFuncParamAttr:
      return "FuncParamAttr";
    case spv::DecorationFPRoundingMode:
      return "FPRoundingMode";
    case spv::DecorationFPFastMathMode:
      return "FPFastMathMode";
    case spv::DecorationLinkageAttributes:
      return "LinkageAttributes";
    case spv::DecorationNoContraction:
      return "NoContraction";
    case spv::DecorationInputAttachmentIndex:
      return "InputAttachmentIndex";
    case spv::DecorationAlignment:
      return "Alignment";
    case spv::DecorationMaxByteOffset:
      return "MaxByteOffset";
    case spv::DecorationAlignmentId:
      return "AlignmentId";
    case spv::DecorationMaxByteOffsetId:
      return "MaxByteOffsetId";
    case spv::DecorationExplicitInterpAMD:
      return "ExplicitInterpAMD";
    case spv::DecorationOverrideCoverageNV:
      return "OverrideCoverageNV";
    case spv::DecorationPassthroughNV:
      return "PassthroughNV";
    case spv::DecorationViewportRelativeNV:
      return "ViewportRelativeNV";
    case spv::DecorationSecondaryViewportRelativeNV:
      return "SecondaryViewportRelativeNV";
    case spv::DecorationHlslCounterBufferGOOGLE:
      return "HlslCounterBufferGOOGLE";
    case spv::DecorationHlslSemanticGOOGLE:
      return "HlslSemanticGOOGLE";
    case spv::DecorationMax:
      break;
  }
  return "string_Decoration(unknown)";
}

static inline const char* string_BaseType(spirv_cross::SPIRType::BaseType t) {
  switch (t) {
    case spirv_cross::SPIRType::Unknown:
      return "Unknown";
    case spirv_cross::SPIRType::Void:
      return "Void";
    case spirv_cross::SPIRType::Boolean:
      return "Boolean";
    case spirv_cross::SPIRType::Char:
      return "Char";
    case spirv_cross::SPIRType::SByte:
      return "SByte";
    case spirv_cross::SPIRType::UByte:
      return "UByte";
    case spirv_cross::SPIRType::Short:
      return "Short";
    case spirv_cross::SPIRType::UShort:
      return "UShort";
    case spirv_cross::SPIRType::Int:
      return "Int";
    case spirv_cross::SPIRType::UInt:
      return "UInt";
    case spirv_cross::SPIRType::Int64:
      return "Int64";
    case spirv_cross::SPIRType::UInt64:
      return "UInt64";
    case spirv_cross::SPIRType::AtomicCounter:
      return "AtomicCounter";
    case spirv_cross::SPIRType::Half:
      return "Half";
    case spirv_cross::SPIRType::Float:
      return "Float";
    case spirv_cross::SPIRType::Double:
      return "Double";
    case spirv_cross::SPIRType::Struct:
      return "Struct";
    case spirv_cross::SPIRType::Image:
      return "Image";
    case spirv_cross::SPIRType::SampledImage:
      return "SampledImage";
    case spirv_cross::SPIRType::Sampler:
      return "Sampler";
    case spirv_cross::SPIRType::ControlPointArray:
      return "ControlPointArray";
  }
  return "string_BaseType(unknown)";
}

}  // namespace science
