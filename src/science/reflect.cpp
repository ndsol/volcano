/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 * This uses spirv_cross to add reflection into SPIR-V bytecode.
 *
 * This is not to be confused with src/core/reflectionmap.h, which adds
 * reflection into Vulkan structures (with their extensions).
 */
#include "reflect.h"

#include <map>
#include <sstream>
#include <vendor/spirv_cross/spirv_glsl.hpp>

#include "science.h"

using namespace command;
using namespace std;

namespace science {

namespace {  // an anonymous namespace hides its contents outside this file

// TODO: set up a comprehensive test suite to validate reflection.
// TODO: then remove most of the debugging here.

static inline void print_type(std::stringstream& out,
                              spirv_cross::CompilerGLSL& compiler,
                              spirv_cross::SPIRType& t) {
  out << '(' << string_BaseType(t.basetype) << ") sizeof=";
  out << t.width << " x " << t.vecsize << " x " << t.columns;
  if (t.basetype == spirv_cross::SPIRType::Image ||
      t.basetype == spirv_cross::SPIRType::SampledImage) {
    out << " (Image" << (((unsigned)t.image.dim) + 1) << "D)";
  } else if (t.basetype == spirv_cross::SPIRType::Struct) {
    for (size_t i = 0; i < t.member_types.size(); i++) {
      out << "\n    m[" << i << "]:";
      auto mt = compiler.get_type(t.member_types[i]);
      print_type(out, compiler, mt);
    }
  }

  if (t.type_alias) {
    auto pt = compiler.get_type(t.type_alias);
    out << "\n    alias=" << t.type_alias << ':';
    print_type(out, compiler, pt);
  }
}

static inline void print_resource(spirv_cross::CompilerGLSL& compiler,
                                  const spirv_cross::Resource& res) {
  logE("  id=%u base_type_id=%u:", res.id, res.base_type_id);
  std::stringstream out;
  auto bt = compiler.get_type(res.base_type_id);
  print_type(out, compiler, bt);
  logE("%s\n", out.str().c_str());
  auto sc = compiler.get_storage_class(res.id);
  logE("  id=%u storage_class=%u (%s)", res.id, (unsigned)sc,
       string_StorageClass(sc));
  out.clear();
  compiler.get_decoration_bitset(res.id).for_each_bit([&](uint32_t dec) {
    spv::Decoration d = (decltype(d))dec;
    uint32_t v = compiler.get_decoration(res.id, d);
    out << ' ' << string_Decoration(d) << '=' << v;
  });
  logE("%s\n", out.str().c_str());
  logE("  name=\"%s\"\n", res.name.c_str());
}

static inline void print_resources(
    const char* typeName,
    const spirv_cross::SmallVector<spirv_cross::Resource>& resources,
    spirv_cross::CompilerGLSL& compiler) {
  for (size_t i = 0; i < resources.size(); i++) {
    logE("%s[%zu]:\n", typeName, i);
    print_resource(compiler, resources[i]);
  }
}

static void print_stage(spirv_cross::CompilerGLSL& compiler,
                        spirv_cross::ShaderResources& resources,
                        VkShaderStageFlagBits stageBits) {
  std::stringstream out;
  out << "reflecting stage:";
  uint32_t bit = 0;
  for (uint64_t i = 1; i && i <= stageBits; i <<= 1, bit++) {
    if (stageBits & i) {
      VkShaderStageFlagBits f = (decltype(f))i;
      string s = string_VkShaderStageFlagBits(f);
      if (s.substr(0, 16) == "VK_SHADER_STAGE_") {
        s = s.substr(16);
      }
      out << " " << s;
    }
  }
  logE("%s\n", out.str().c_str());

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
    const spirv_cross::SmallVector<spirv_cross::Resource>& resources;
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
        binding.layouts.resize(bindingI);
      }
      if (bindingI == binding.layouts.size()) {
        VkDescriptorSetLayoutBinding layoutBinding;
        memset(&layoutBinding, 0, sizeof(layoutBinding));
        binding.layouts.emplace_back(layoutBinding);
      } else if (!binding.layouts.at(bindingI).descriptorCount) {
        // This layout was not previously initialized, it's just an empty
        // placeholder right now.
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

      auto& layoutBinding = binding.layouts.at(bindingI);
      layoutBinding.binding = bindingI;
      layoutBinding.descriptorCount = 1;
      layoutBinding.descriptorType = rtm.descriptorType;
      layoutBinding.pImmutableSamplers = nullptr;
      // layoutBinding1.stageFlags is set in
      // ShaderLibrary::finalizeDescriptorLibrary to the OR of all stageBits.
      // It is being collected in binding.allStageBits above.
      if (0)
        logI("layout=%zu set=%u binding=%u type=%u\n", layoutIndex, setI,
             bindingI, rtm.descriptorType);
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
        // VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC and
        // VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC are only created if your
        // app wants to enable a dynamic uniform or storage buffer for a
        // particular buffer.
        // VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT is not applicable.
    };
    for (auto& r : resourceTypeMap) {
      if (reflectResource(layoutIndex, stageBits, compiler, r)) {
        logE("reflectResource(%s (%d)) failed\n",
             string_VkDescriptorType(r.descriptorType), r.descriptorType);
        return 1;
      }
    }
    // Check for any unused bindings.
    vector<ShaderLibrary::ShaderBinding>& bindSet = bindings.at(layoutIndex);
    for (size_t setI = 0; setI < bindSet.size(); setI++) {
      for (size_t j = 0; j < bindSet.at(setI).layouts.size(); j++) {
        auto& layoutBinding = bindSet.at(setI).layouts.at(j);
        if (!layoutBinding.descriptorCount) {
          logW("layout=%zu set=%zu binding=%zu is empty and invalid\n",
               layoutIndex, setI, j);
          logW("   consider renumbering layout=%zu set=%zu binding=%zu\n",
               layoutIndex, setI, j + 1);
        }
      }
    }
    // resources.push_constant_buffers are a special case.
    // (Note that right now, only one push_constant block can be defined, but
    // that restriction might be lifted in the future.)
    for (auto& pcb : resources.push_constant_buffers) {
      VkPushConstantRange range;
      memset(&range, 0, sizeof(range));
      range.stageFlags = stageBits;
      range.offset = 0;
      range.size =
          compiler.get_declared_struct_size(compiler.get_type(pcb.type_id));
      pipeInfo.pushConstants.emplace_back(range);
    }

    return 0;
  }

  int finalShaderAddLogic(std::shared_ptr<command::Pipeline> pipe,
                          shared_ptr<Shader> shader, size_t layoutIndex,
                          VkShaderStageFlagBits stageBits,
                          spirv_cross::CompilerGLSL& compiler) {
    if (!pipe) {
      logE("ShaderLibrary: BUG: null pipe after calling info().\n");
      return 1;
    }
    if (!shader) {
      logE("ShaderLibrary: BUG: null shader.\n");
      return 1;
    }
    if (reflectStage(layoutIndex, stageBits, compiler, pipe->info)) {
      logE("ShaderLibrary: reflectStage failed\n");
      return 1;
    }

    // register a pipe that wants to receive layoutIndex when
    // finalizeDescriptorLibrary() is called.
    observers.emplace_back();
    observers.back().pipe = pipe;
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
    // All compute shaders must be added using the compute shader form of add()
    // This is not needed.
    //{spv::ExecutionModelGLCompute, VK_SHADER_STAGE_COMPUTE_BIT},
};

int ShaderLibrary::add(PipeBuilder& pipeBuilder, shared_ptr<Shader> shader,
                       size_t layoutIndex) {
  if (!shader) {
    logE("ShaderLibrary::add: shader is null\n");
    return 1;
  }
  if (!_i) {
    _i = new ShaderLibraryInternal();
  }
  // Decompile the shader
  spirv_cross::CompilerGLSL compiler(shader->bytes);

  spirv_cross::SmallVector<spirv_cross::EntryPoint> eps =
      compiler.get_entry_points_and_stages();
  if (eps.size() < 1) {
    logE("Invalid shader: no entry points.\n");
    return 1;
  }
  auto it = model2stage.find(eps[0].execution_model);
  if (it == model2stage.end()) {
    logE("%sadd(model %d) not allowed\n",
         "ShaderLibrary::", static_cast<int>(eps[0].execution_model));
    return 1;
  }
  pipeBuilder.info();
  if (_i->finalShaderAddLogic(pipeBuilder.pipe, shader, layoutIndex, it->second,
                              compiler)) {
    return 1;
  }
  if (pipeBuilder.info().addShader(pipeBuilder.pass, shader, it->second,
                                   eps[0].name)) {
    logE("ShaderLibrary: addShader failed\n");
    return 1;
  }
  return 0;
}

int ShaderLibrary::add(PipeBuilder& pipeBuilder, shared_ptr<Shader> shader,
                       size_t layoutIndex, VkShaderStageFlagBits stageBits,
                       string entryPointName /*= "main"*/) {
  if (!shader) {
    logE("ShaderLibrary::add: shader is null\n");
    return 1;
  }
  if (!_i) {
    _i = new ShaderLibraryInternal();
  }
  if (!stageBits) {
    logE("BUG: %sadd with stageBits == 0\n", "ShaderLibrary::");
    return 1;
  }
  // Decompile the shader
  spirv_cross::CompilerGLSL compiler(shader->bytes);
  pipeBuilder.info();
  if (_i->finalShaderAddLogic(pipeBuilder.pipe, shader, layoutIndex, stageBits,
                              compiler)) {
    return 1;
  }
  if (pipeBuilder.info().addShader(pipeBuilder.pass, shader, stageBits,
                                   entryPointName)) {
    logE("ShaderLibrary: addShader failed\n");
    return 1;
  }
  return 0;
}

int ShaderLibrary::add(std::shared_ptr<command::Pipeline> compute,
                       size_t layoutIndex /*= 0*/) {
  if (!compute) {
    logE("ShaderLibrary::add: shared_ptr<command::Pipeline> is null.\n");
    return 1;
  }
  if (compute->info.stages.size() != 1) {
    logE("Pipeline has %zu stages. Only 1 allowed for a %s.\n",
         compute->info.stages.size(), "compute pipeline");
    return 1;
  }
  auto& stage = compute->info.stages.at(0);
  if (compute->info.depthsci.sType != 0 ||
      stage.info.stage != VK_SHADER_STAGE_COMPUTE_BIT) {
    logE("ShaderLibrary::add for a %s, is this a %s?\n", "compute pipeline",
         "compute pipeline");
    return 1;
  }
  if (!stage.shader) {
    logE("ShaderLibrary::add for a %s, shader is null\n", "compute pipeline");
    return 1;
  }
  if (!_i) {
    _i = new ShaderLibraryInternal();
  }
  // Decompile the shader
  spirv_cross::CompilerGLSL compiler(stage.shader->bytes);
  return _i->finalShaderAddLogic(compute, stage.shader, layoutIndex,
                                 stage.info.stage, compiler);
}

std::vector<std::vector<ShaderLibrary::ShaderBinding>>&
ShaderLibrary::getBindings() {
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
