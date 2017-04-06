/* Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
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
  }
  return "string_BaseType(unknown)";
}

static inline void print_type(spirv_cross::CompilerGLSL& compiler,
                              spirv_cross::SPIRType& t) {
  const char* baseTypeStr = string_BaseType(t.basetype);
  fprintf(stderr, "(%s) sizeof=%u x %u x %u", baseTypeStr, t.width, t.vecsize,
          t.columns);
  if (t.basetype == spirv_cross::SPIRType::Image ||
      t.basetype == spirv_cross::SPIRType::SampledImage) {
    fprintf(stderr, " (Image%uD)", ((unsigned)t.image.dim) + 1);
  } else if (t.basetype == spirv_cross::SPIRType::Struct) {
    for (size_t i = 0; i < t.member_types.size(); i++) {
      fprintf(stderr, "\n  m[%zu]:", i);
      auto mt = compiler.get_type(t.member_types.at(i));
      print_type(compiler, mt);
    }
  }

  if (t.type_alias) {
    auto pt = compiler.get_type(t.type_alias);
    fprintf(stderr, "\n    alias=%u:", t.type_alias);
    print_type(compiler, pt);
  }
}

static inline void print_resource(spirv_cross::CompilerGLSL& compiler,
                                  const spirv_cross::Resource& res) {
  fprintf(stderr, "  id=%u base_type_id=%u:", res.id, res.base_type_id);
  auto bt = compiler.get_type(res.base_type_id);
  print_type(compiler, bt);
  auto sc = compiler.get_storage_class(res.id);
  fprintf(stderr, "\n  id=%u storage_class=%u (%s)", res.id, (unsigned)sc,
          string_StorageClass(sc));
  auto mask = compiler.get_decoration_mask(res.id);
  uint32_t dec = 0;
  for (uint64_t i = 1; i && i <= mask; i <<= 1, dec++) {
    if (mask & i) {
      spv::Decoration d = (decltype(d))dec;
      uint32_t v = compiler.get_decoration(res.id, d);
      fprintf(stderr, " %s=%u", string_Decoration(d), v);
    }
  }
  fprintf(stderr, "\n  name=\"%s\"\n", res.name.c_str());
}

static inline void print_resources(
    const char* typeName, const std::vector<spirv_cross::Resource>& resources,
    spirv_cross::CompilerGLSL& compiler) {
  for (size_t i = 0; i < resources.size(); i++) {
    fprintf(stderr, "%s[%zu]:\n", typeName, i);
    print_resource(compiler, resources.at(i));
  }
}

}  // namespace science
