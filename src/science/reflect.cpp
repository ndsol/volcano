/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 * This uses spirv_cross to add reflection into SPIR-V bytecode.
 *
 * This is not to be confused with src/core/reflectionmap.h, which adds
 * reflection into Vulkan structures (with their extensions).
 */
#include "reflect.h"

#include <map>
#include <vendor/spirv_cross/spirv_glsl.hpp>

#include "science.h"

using namespace command;
using namespace std;

namespace science {

namespace {  // an anonymous namespace hides its contents outside this file

// TODO: set up a comprehensive test suite to validate reflection.
// TODO: then remove most of the debugging here.

static inline void print_type(spirv_cross::CompilerGLSL& compiler,
                              spirv_cross::SPIRType& t) {
  const char* baseTypeStr = string_BaseType(t.basetype);
  logE("(%s) sizeof=%u x %u x %u", baseTypeStr, t.width, t.vecsize, t.columns);
  if (t.basetype == spirv_cross::SPIRType::Image ||
      t.basetype == spirv_cross::SPIRType::SampledImage) {
    logE(" (Image%uD)", ((unsigned)t.image.dim) + 1);
  } else if (t.basetype == spirv_cross::SPIRType::Struct) {
    for (size_t i = 0; i < t.member_types.size(); i++) {
      logE("\n  m[%zu]:", i);
      auto mt = compiler.get_type(t.member_types.at(i));
      print_type(compiler, mt);
    }
  }

  if (t.type_alias) {
    auto pt = compiler.get_type(t.type_alias);
    logE("\n    alias=%u:", t.type_alias);
    print_type(compiler, pt);
  }
}

static inline void print_resource(spirv_cross::CompilerGLSL& compiler,
                                  const spirv_cross::Resource& res) {
  logE("  id=%u base_type_id=%u:", res.id, res.base_type_id);
  auto bt = compiler.get_type(res.base_type_id);
  print_type(compiler, bt);
  auto sc = compiler.get_storage_class(res.id);
  logE("\n  id=%u storage_class=%u (%s)", res.id, (unsigned)sc,
       string_StorageClass(sc));
  compiler.get_decoration_bitset(res.id).for_each_bit([&](uint32_t dec) {
    spv::Decoration d = (decltype(d))dec;
    uint32_t v = compiler.get_decoration(res.id, d);
    logE(" %s=%u", string_Decoration(d), v);
  });
  logE("\n  name=\"%s\"\n", res.name.c_str());
}

static inline void print_resources(
    const char* typeName, const vector<spirv_cross::Resource>& resources,
    spirv_cross::CompilerGLSL& compiler) {
  for (size_t i = 0; i < resources.size(); i++) {
    logE("%s[%zu]:\n", typeName, i);
    print_resource(compiler, resources.at(i));
  }
}

static void print_stage(spirv_cross::CompilerGLSL& compiler,
                        spirv_cross::ShaderResources& resources,
                        VkShaderStageFlagBits stageBits) {
  logE("reflecting stage:");
  uint32_t bit = 0;
  for (uint64_t i = 1; i && i <= stageBits; i <<= 1, bit++) {
    if (stageBits & i) {
      VkShaderStageFlagBits f = (decltype(f))i;
      string s = string_VkShaderStageFlagBits(f);
      if (s.substr(0, 16) == "VK_SHADER_STAGE_") {
        s = s.substr(16);
      }
      logE(" %s", s.c_str());
    }
  }
  logE("\n");

  print_resources("uniform_buffers", resources.uniform_buffers, compiler);
  print_resources("storage_buffers", resources.storage_buffers, compiler);
  print_resources("storage_images", resources.storage_images, compiler);
  print_resources("sampled_images", resources.sampled_images, compiler);
  print_resources("push_constant_buffers", resources.push_constant_buffers,
                  compiler);
  print_resources("atomic_counters", resources.atomic_counters, compiler);
  print_resources("separate_images", resources.separate_images, compiler);
  print_resources("separate_samplers", resources.separate_samplers, compiler);
}

}  // anonymous namespace

struct ShaderLibraryInternal {
  ShaderLibraryInternal() {}

  struct ResourceTypeMap {
    VkDescriptorType descriptorType;
    const vector<spirv_cross::Resource>& resources;
  };

  int reflectResource(size_t layoutIndex, VkShaderStageFlagBits stageBits,
                      spirv_cross::CompilerGLSL& compiler,
                      ResourceTypeMap& rtm) {
    if (bindings.size() < layoutIndex + 1) {
      bindings.resize(layoutIndex + 1);
    }
    vector<ShaderLibrary::ShaderBinding>& bindSet = bindings.at(layoutIndex);
    for (auto& res : rtm.resources) {
      uint32_t setI = 0;
      auto bitset = compiler.get_decoration_bitset(res.id);
      if (bitset.get(spv::DecorationDescriptorSet)) {
        setI = compiler.get_decoration(res.id, spv::DecorationDescriptorSet);
      }
      if (bindSet.size() < setI + 1) {
        bindSet.resize(setI + 1);
      }

      ShaderLibrary::ShaderBinding& binding = bindSet.at(setI);
      binding.allStageBits |= stageBits;

      uint32_t bindingI = 0;
      if (bitset.get(spv::DecorationBinding)) {
        bindingI = compiler.get_decoration(res.id, spv::DecorationBinding);
      } else {
        logW("WARNING: shader at stage %s:\n",
             string_VkShaderStageFlagBits(stageBits));
        logW("layout(binding=?) not found for id %u, using binding=0\n",
             res.id);
      }
      if (bindingI > binding.layouts.size()) {
        binding.layouts.resize(bindingI + 1);
      }
      if (bindingI == binding.layouts.size()) {
        VkDescriptorSetLayoutBinding VkInit(layoutBinding);
        layoutBinding.binding = bindingI;
        layoutBinding.descriptorCount = 1;
        layoutBinding.descriptorType = rtm.descriptorType;
        layoutBinding.pImmutableSamplers = nullptr;
        // layoutBinding1.stageFlags is set in
        // ShaderLibrary::finalizeDescriptorLibrary to the OR of all stageBits.
        // It is being collected in binding.allStageBits above.
        binding.layouts.emplace_back(layoutBinding);
      } else if (binding.layouts.at(bindingI).descriptorType !=
                 rtm.descriptorType) {
        logE("%s%s: binding=%u of type=%u conflicts with\n",
             "ERROR: shader stage ", string_VkShaderStageFlagBits(stageBits),
             bindingI, rtm.descriptorType);
        logE("%s%s: binding=%u of type=%u already in set=%u layout=%zu\n",
             "ERROR: shader stage ", string_VkShaderStageFlagBits(stageBits),
             bindingI, binding.layouts.at(bindingI).descriptorType, setI,
             layoutIndex);
        return 1;
      }
    }
    return 0;
  }

  int reflectStage(size_t layoutIndex, VkShaderStageFlagBits stageBits,
                   spirv_cross::CompilerGLSL& compiler,
                   PipelineCreateInfo& pipeInfo) {
    // Use reflection to read the shader layouts.
    auto resources = compiler.get_shader_resources();
    if (0) print_stage(compiler, resources, stageBits);

    vector<ResourceTypeMap> resourceTypeMap{
        {VK_DESCRIPTOR_TYPE_SAMPLER, resources.separate_samplers},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, resources.sampled_images},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, resources.separate_images},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, resources.storage_images},
        //{VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER has no matching vector},
        //{VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER has no matching vector},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, resources.uniform_buffers},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, resources.storage_buffers},
        //{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC has no matching vector},
        //{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC has no matching vector},
        // VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT is not applicable.
    };
    for (auto& r : resourceTypeMap) {
      if (reflectResource(layoutIndex, stageBits, compiler, r)) {
        logE("reflectResource(%s (%d)) failed\n",
             string_VkDescriptorType(r.descriptorType), r.descriptorType);
        return 1;
      }
    }
    // resources.push_constant_buffers are a special case.
    // (Note that right now, only one push_constant block can be defined, but
    // that restriction might be lifted in the future.)
    for (auto& pcb : resources.push_constant_buffers) {
      VkPushConstantRange VkInit(range);
      range.stageFlags = stageBits;
      range.offset = 0;
      range.size =
          compiler.get_declared_struct_size(compiler.get_type(pcb.type_id));
      pipeInfo.pushConstants.emplace_back(range);
    }

    return 0;
  }

  int finalShaderAddLogic(PipeBuilder& pipeBuilder, shared_ptr<Shader> shader,
                          size_t layoutIndex, VkShaderStageFlagBits stageBits,
                          spirv_cross::CompilerGLSL& compiler,
                          string entryPointName) {
    if (reflectStage(layoutIndex, stageBits, compiler, pipeBuilder.info())) {
      logE("ShaderLibrary: reflectStage failed\n");
      return 1;
    }
    if (pipeBuilder.info().addShader(pipeBuilder.pass, shader, stageBits,
                                     entryPointName)) {
      logE("ShaderLibrary: addShader failed\n");
      return 1;
    }

    // register a pipe that wants to receive layoutIndex when
    // finalizeDescriptorLibrary() is called.
    if (!pipeBuilder.pipe) {
      logE("ShaderLibrary: BUG: null pipeBuilder after calling info().\n");
      return 1;
    }
    observers.emplace_back();
    observers.back().pipe = pipeBuilder.pipe;
    observers.back().layoutIndex = layoutIndex;
    return 0;
  }

  vector<vector<ShaderLibrary::ShaderBinding>> bindings;

  std::vector<ShaderLibrary::FinalizeObserver> observers;
};

static const map<spv::ExecutionModel, VkShaderStageFlagBits> model2stage = {
    {spv::ExecutionModelVertex, VK_SHADER_STAGE_VERTEX_BIT},
    {spv::ExecutionModelTessellationControl,
     VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT},
    {spv::ExecutionModelTessellationEvaluation,
     VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT},
    {spv::ExecutionModelGeometry, VK_SHADER_STAGE_GEOMETRY_BIT},
    {spv::ExecutionModelFragment, VK_SHADER_STAGE_FRAGMENT_BIT},
    {spv::ExecutionModelGLCompute, VK_SHADER_STAGE_COMPUTE_BIT},
};

int ShaderLibrary::add(PipeBuilder& pipeBuilder, shared_ptr<Shader> shader,
                       size_t layoutIndex) {
  if (!_i) {
    _i = new ShaderLibraryInternal();
  }
  // Decompile the shader
  spirv_cross::CompilerGLSL compiler(shader->bytes);

  vector<spirv_cross::EntryPoint> eps = compiler.get_entry_points_and_stages();
  if (eps.size() < 1) {
    logE("Invalid shader: no entry points.\n");
    return 1;
  }
  auto it = model2stage.find(eps.at(0).execution_model);
  if (it == model2stage.end()) {
    logE("BUG: %sadd(model %d) not implemented\n",
         "ShaderLibrary::", static_cast<int>(eps.at(0).execution_model));
    return 1;
  }
  return _i->finalShaderAddLogic(pipeBuilder, shader, layoutIndex, it->second,
                                 compiler, eps.at(0).name);
}

int ShaderLibrary::add(PipeBuilder& pipeBuilder, shared_ptr<Shader> shader,
                       size_t layoutIndex, VkShaderStageFlagBits stageBits,
                       string entryPointName /*= "main"*/) {
  if (!_i) {
    _i = new ShaderLibraryInternal();
  }
  if (!stageBits) {
    logE("BUG: %sadd with stageBits == 0\n", "ShaderLibrary::");
    return 1;
  }
  // Decompile the shader
  spirv_cross::CompilerGLSL compiler(shader->bytes);
  return _i->finalShaderAddLogic(pipeBuilder, shader, layoutIndex, stageBits,
                                 compiler, entryPointName);
}

const std::vector<std::vector<ShaderLibrary::ShaderBinding>>&
ShaderLibrary::getBindings() const {
  if (!_i) {
    // Not a typo; getBindings is called from finalizeDescriptorLibrary:
    logF("BUG: %sfinalizeDescriptorLibrary() before %sadd\n",
         "ShaderLibrary::", "ShaderLibrary::");
  }
  return _i->bindings;
}

const std::vector<ShaderLibrary::FinalizeObserver>&
ShaderLibrary::getObservers() const {
  if (!_i) {
    // Not a typo; getObservers is called from finalizeDescriptorLibrary:
    logF("BUG: %sfinalizeDescriptorLibrary() before %sadd\n",
         "ShaderLibrary::", "ShaderLibrary::");
  }
  return _i->observers;
}

ShaderLibrary::~ShaderLibrary() {
  if (_i) {
    delete _i;
  }
}

}  // namespace science
