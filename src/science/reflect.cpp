/* Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
 * This file is only built if "use_spirv_cross_reflection" is enabled.
 */
#include "reflect.h"
#include <map>
#include <vendor/spirv_cross/spirv_glsl.hpp>
#include "science.h"

#ifndef USE_SPIRV_CROSS_REFLECTION
#error USE_SPIRV_CROSS_REFLECTION should be defined if this file is being built.
#endif

using namespace command;
using namespace std;

namespace science {

namespace {  // an anonymous namespace hides its contents outside this file

// TODO: remove fprintf(stderr) from this file.
// TODO: set up a comprehensive test suite to validate reflection.
// TODO: then remove most of the debugging here.

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

static void print_stage(spirv_cross::CompilerGLSL& compiler,
                        spirv_cross::ShaderResources& resources,
                        VkShaderStageFlagBits stageBits) {
  fprintf(stderr, "reflecting stage:");
  uint32_t bit = 0;
  for (uint64_t i = 1; i && i <= stageBits; i <<= 1, bit++) {
    if (stageBits & i) {
      VkShaderStageFlagBits f = (decltype(f))i;
      string s = string_VkShaderStageFlagBits(f);
      if (s.substr(0, 16) == "VK_SHADER_STAGE_") {
        s = s.substr(16);
      }
      fprintf(stderr, " %s", s.c_str());
    }
  }
  fprintf(stderr, "\n");

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
  ShaderLibraryInternal(ShaderLibrary* self) : self(*self) {}

  struct ShaderState {
    ShaderState(const uint32_t* buf, uint32_t len)
        : data{buf, buf + len / sizeof(*buf)} {}
    vector<uint32_t> data;
    bool isStaged{false};
    vector<VkPushConstantRange> pushConsts;
  };

  struct ShaderBinding {
    vector<VkDescriptorSetLayoutBinding> layouts;
    uint32_t allStageBits{0};
  };

  struct ResourceTypeMap {
    VkDescriptorType descriptorType;
    const vector<spirv_cross::Resource>& resources;
  };

  int addShaderState(shared_ptr<Shader> shader, const uint32_t* buf,
                     uint32_t len) {
    auto result = states.emplace(make_pair(shader, ShaderState{buf, len}));
    if (!result.second) {
      logE("%sload: shader already exists\n", "ShaderLibrary::");
      return 1;
    }
    return 0;
  }

  int reflectResource(VkShaderStageFlagBits stageBits,
                      spirv_cross::CompilerGLSL& compiler,
                      ResourceTypeMap& rtm) {
    for (auto& res : rtm.resources) {
      uint32_t setI = 0;
      auto mask = compiler.get_decoration_mask(res.id);
      if (mask & (((uint64_t)1) << spv::DecorationDescriptorSet)) {
        setI = compiler.get_decoration(res.id, spv::DecorationDescriptorSet);
      }
      if (bindings.size() < setI + 1) {
        bindings.resize(setI + 1);
      }

      ShaderBinding& binding = bindings.at(setI);
      binding.allStageBits |= stageBits;

      uint32_t bindingI = 0;
      if (mask & (((uint64_t)1) << spv::DecorationBinding)) {
        bindingI = compiler.get_decoration(res.id, spv::DecorationBinding);
      } else {
        logW("WARNING: shader at stage %s:\n",
             string_VkShaderStageFlagBits(stageBits));
        logW("layout(binding=?) not found for id %u, using binding=0\n",
             res.id);
      }
      if (bindingI == binding.layouts.size()) {
        VkDescriptorSetLayoutBinding VkInit(layoutBinding);
        layoutBinding.binding = bindingI;
        layoutBinding.descriptorCount = 1;
        layoutBinding.descriptorType = rtm.descriptorType;
        layoutBinding.pImmutableSamplers = nullptr;
        // layoutBinding1.stageFlags is set in
        // ShaderLibrary::makeDescriptorLibrary to the OR of all stageBits. It
        // is being collected in binding.allStageBits above.
        binding.layouts.emplace_back(layoutBinding);
      } else if (bindingI > binding.layouts.size()) {
        logE("ERROR: shader at stage %s: binding=%u skips binding=%zu\n",
             string_VkShaderStageFlagBits(stageBits), bindingI,
             binding.layouts.size());
        return 1;
      } else if (binding.layouts.at(bindingI).descriptorType !=
                 rtm.descriptorType) {
        logE("ERROR: shader stage %s: binding=%u of type=%u conflicts with\n",
             string_VkShaderStageFlagBits(stageBits), bindingI,
             rtm.descriptorType);
        logE("ERROR: shader stage %s: binding=%u of type=%u already defined\n",
             string_VkShaderStageFlagBits(stageBits), bindingI,
             binding.layouts.at(bindingI).descriptorType);
        return 1;
      }
    }
    return 0;
  }

  int addStage(ShaderState& state, VkShaderStageFlagBits stageBits) {
    state.isStaged = true;

    // Decompile the shader and reflect the shader layouts.
    spirv_cross::CompilerGLSL compiler(state.data);
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
      if (reflectResource(stageBits, compiler, r)) {
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
      state.pushConsts.emplace_back(range);
    }

    return 0;
  }

  map<shared_ptr<Shader>, ShaderState> states;
  vector<ShaderBinding> bindings;
  ShaderLibrary& self;
};

shared_ptr<Shader> ShaderLibrary::load(const uint32_t* spvBegin, size_t len) {
  if (!_i) {
    _i = new ShaderLibraryInternal(this);
  }

  auto shader = shared_ptr<Shader>(new Shader(dev));
  if (shader->loadSPV(spvBegin, len) ||
      _i->addShaderState(shader, spvBegin, len)) {
    // Failed. Return a null shared_ptr.
    return shared_ptr<Shader>();
  }
  return shader;
}

shared_ptr<Shader> ShaderLibrary::load(const char* filename) {
  if (!_i) {
    _i = new ShaderLibraryInternal(this);
  }
  MMapFile infile;
  if (infile.mmapRead(filename)) {
    logE("%sload: mmapRead(%s) failed\n", "ShaderLibrary::", filename);
    return shared_ptr<Shader>();
  }
  uint32_t* map = reinterpret_cast<uint32_t*>(infile.map);
  auto shader = shared_ptr<Shader>(new Shader(dev));
  if (shader->loadSPV(map, infile.len) ||
      _i->addShaderState(shader, map, infile.len)) {
    return shared_ptr<Shader>();
  }
  return shader;
}

int ShaderLibrary::stage(RenderPass& renderPass, PipeBuilder& pipeb,
                         VkShaderStageFlagBits stageBits,
                         shared_ptr<Shader> shader,
                         string entryPointName /*= "main"*/) {
  if (!_i) {
    logE("BUG: %sstage before %sload\n", "ShaderLibrary::", "ShaderLibrary::");
    return 1;
  }
  if (!stageBits) {
    logE("BUG: %sstage with stageBits == 0\n", "ShaderLibrary::");
    return 1;
  }
  auto state = _i->states.find(shader);
  if (state == _i->states.end()) {
    logE("BUG: %sstage before %sload.\n", "ShaderLibrary::", "ShaderLibrary::");
    logE("Where did the shared_ptr<Shader> come from?\n");
    return 1;
  }

  auto& info = pipeb.info();
  auto& s = state->second;
  if (_i->addStage(s, stageBits) ||
      info.addShader(shader, renderPass, stageBits, entryPointName)) {
    logE("ShaderLibrary: addStage or addShader failed\n");
    return 1;
  }
  info.pushConstants.insert(info.pushConstants.end(), s.pushConsts.begin(),
                            s.pushConsts.end());
  return 0;
}

int ShaderLibrary::makeDescriptorLibrary(
    DescriptorLibrary& descriptorLibrary,
    const multiset<VkDescriptorType>& wantTypes) {
  if (!_i) {
    logE("BUG: %smakeDescriptorLibrary before %sload\n",
         "ShaderLibrary::", "ShaderLibrary::");
    return 1;
  }
  // Find any unused shaders.
  unsigned unusedCount = 0;
  for (auto state : _i->states) {
    if (!state.second.isStaged) {
      unusedCount++;
    }
  }
  if (unusedCount) {
    logW("WARNING: %smakeDescriptorLibrary found %u unused Shader%s.\n",
         "ShaderLibrary::", unusedCount, unusedCount == 1 ? "" : "s");
    logW("Every %sload call must have at least one call to %sstage.\n",
         "ShaderLibrary::", "ShaderLibrary::");
  }

  // Estimate max count of DescriptorSet objects the DescriptorLibrary will use
  uint32_t maxSets = 0;
  // Collect all types for DescriptorLibrary::ctorError
  multiset<VkDescriptorType> types;

  descriptorLibrary.layouts.clear();
  descriptorLibrary.layouts.reserve(_i->bindings.size());
  for (size_t bindingI = 0; bindingI < _i->bindings.size(); bindingI++) {
    auto binding = _i->bindings.at(bindingI);
    maxSets++;

    vector<VkDescriptorSetLayoutBinding> libBindings(binding.layouts.size());
    for (size_t layoutI = 0; layoutI < binding.layouts.size(); layoutI++) {
      auto& layout = binding.layouts.at(layoutI);
      types.emplace(layout.descriptorType);

      // This could be more efficient if stage bits could be broken down
      // to per-stage granularity.
      layout.stageFlags =
          static_cast<VkShaderStageFlagBits>(binding.allStageBits);
      libBindings.at(layoutI) = layout;
    }

    descriptorLibrary.layouts.emplace_back(dev);
    auto& libLayout = descriptorLibrary.layouts.at(bindingI);
    if (libLayout.ctorError(dev, libBindings)) {
      fprintf(stderr, "descriptorLibrary.layouts[%zu].ctorError failed\n",
              bindingI);
      return 1;
    }
  }

  if (wantTypes.size()) {
    if (wantTypes.size() > types.size()) {
      types = wantTypes;
    } else {
      logW("makeDescriptorLibrary found %zu types.\n", types.size());
      logW("wantTypes.size() was only %zu\n", wantTypes.size());
      logW("Note that this is before descriptorSetMaxCopies = %zu is applied\n",
           descriptorSetMaxCopies);
    }
  }

  multiset<VkDescriptorType> typesMultiple;
  for (size_t i = 0; i < descriptorSetMaxCopies; i++) {
    typesMultiple.insert(types.begin(), types.end());
  }
  if (descriptorLibrary.pool.ctorError(maxSets * descriptorSetMaxCopies,
                                       typesMultiple)) {
    fprintf(stderr, "DescriptorPool::ctorError failed\n");
    return 1;
  }
  return 0;
}

unique_ptr<memory::DescriptorSet> DescriptorLibrary::makeSet(size_t layoutI) {
  if (!pool.vk) {
    logE("BUG: %smakeSet(%zu) before %smakeDescriptorLibrary\n",
         "DescriptorLibrary::", layoutI, "ShaderLibrary::");
    return unique_ptr<memory::DescriptorSet>();
  }
  if (layoutI > layouts.size()) {
    logE("BUG: %smakeSet(%zu) with %zu layouts\n",
         "DescriptorLibrary::", layoutI, layouts.size());
    return unique_ptr<memory::DescriptorSet>();
  }

  auto* set = new memory::DescriptorSet(pool);
  if (set->ctorError(layouts.at(layoutI))) {
    // TODO: set may fail if DescriptorPool is full.
    // Allocate another DescriptorPool and retry.
    logE("DescriptorSet::ctorError failed\n");
    delete set;
    return unique_ptr<memory::DescriptorSet>();
  }
  return unique_ptr<memory::DescriptorSet>(set);
}

ShaderLibrary::~ShaderLibrary() {
  if (_i) {
    delete _i;
  }
}

}  // namespace science
