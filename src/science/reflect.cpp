/* Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
 * This file is only built if "use_spirv_cross_reflection" is enabled.
 */
#include "reflect.h"
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
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
  ShaderLibraryInternal(ShaderLibrary* self) : self{*self} {}

  struct ShaderState {
    ShaderState(const uint32_t* buf, uint32_t len)
        : data{buf, buf + len / sizeof(*buf)} {}
    vector<uint32_t> data;
    bool isStaged{false};
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
      fprintf(stderr, "ShaderLibrary::load: shader already exists\n");
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
        fprintf(stderr,
                "WARNING: shader at stage %s:\n"
                "WARNING: layout(binding=?) not found for id %u, assuming "
                "binding=0\n",
                string_VkShaderStageFlagBits(stageBits), res.id);
      }
      VkDescriptorSetLayoutBinding VkInit(layoutBinding);
      layoutBinding.binding = bindingI;
      layoutBinding.descriptorCount = 1;
      layoutBinding.descriptorType = rtm.descriptorType;
      layoutBinding.pImmutableSamplers = nullptr;
      // layoutBinding1.stageFlags is set in
      // ShaderLibrary::makeDescriptorLibrary to the OR of all stageBits.
      binding.layouts.emplace_back(layoutBinding);
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
        fprintf(stderr, "reflectResources(%s (%d)) failed\n",
                string_VkDescriptorType(r.descriptorType), r.descriptorType);
        return 1;
      }
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
  // All this mmap() machinery is duplicated in command/shader.cpp.
  // Other than creating a file-layer-abstraction library to factor this out,
  // the only way to grab the mmaped bytes is to duplicate the mmap code.
  int infile = open(filename, O_RDONLY);
  if (infile < 0) {
    fprintf(stderr, "ShaderLibrary::load: open(%s) failed: %d %s\n", filename,
            errno, strerror(errno));
    // Failed. Return a null shared_ptr.
    return shared_ptr<Shader>();
  }
  struct stat s;
  if (fstat(infile, &s) == -1) {
    fprintf(stderr, "ShaderLibrary::load: fstat(%s) failed: %d %s\n", filename,
            errno, strerror(errno));
    // Failed. Return a null shared_ptr.
    return shared_ptr<Shader>();
  }
  uint32_t* map = reinterpret_cast<uint32_t*>(
      mmap(0, s.st_size, PROT_READ, MAP_SHARED, infile, 0 /*offset*/));
  if (!map) {
    fprintf(stderr, "ShaderLibrary::load: mmap(%s) failed: %d %s\n", filename,
            errno, strerror(errno));
    close(infile);
    // Failed. Return a null shared_ptr.
    return shared_ptr<Shader>();
  }

  auto shader = shared_ptr<Shader>(new Shader(dev));
  int r = shader->loadSPV(map, s.st_size);
  if (!r && _i->addShaderState(shader, map, s.st_size)) {
    r = 1;
  }
  if (munmap(map, s.st_size) < 0) {
    fprintf(stderr, "ShaderLibrary::load: munmap(%s) failed: %d %s\n", filename,
            errno, strerror(errno));
    close(infile);
    // Failed. Return a null shared_ptr.
    return shared_ptr<Shader>();
  }
  if (close(infile) < 0) {
    fprintf(stderr, "ShaderLibrary::load: close(%s) failed: %d %s\n", filename,
            errno, strerror(errno));
    // Failed. Return a null shared_ptr.
    return shared_ptr<Shader>();
  }
  if (r) {
    // shader->loadSPV() or _i->addShaderState() failed. Return a null
    // shared_ptr.
    return shared_ptr<Shader>();
  }
  return shader;
}

int ShaderLibrary::stage(RenderPass& renderPass, PipeBuilder& pipe,
                         VkShaderStageFlagBits stageBits,
                         shared_ptr<Shader> shader,
                         string entryPointName /*= "main"*/) {
  if (!_i) {
    fprintf(stderr, "BUG: ShaderLibrary::stage before ShaderLibrary::load\n");
    return 1;
  }
  if (!stageBits) {
    fprintf(stderr, "BUG: ShaderLibrary::stage with stageBits == 0\n");
    return 1;
  }
  auto state = _i->states.find(shader);
  if (state == _i->states.end()) {
    fprintf(stderr,
            "BUG: ShaderLibrary::stage before ShaderLibrary::load.\n"
            "BUG: where did this shared_ptr<Shader> come from?\n");
    return 1;
  }

  return _i->addStage(state->second, stageBits) ||
         pipe.pipeline.info.addShader(shader, renderPass, stageBits,
                                      entryPointName);
}

int ShaderLibrary::makeDescriptorLibrary(DescriptorLibrary& descriptorLibrary) {
  if (!_i) {
    fprintf(stderr,
            "BUG: ShaderLibrary::makeDescriptorLibrary before "
            "ShaderLibrary::load\n");
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
    fprintf(stderr,
            "WARNING: ShaderLibrary::makeDescriptorLibrary found "
            "%u unused Shader%s.\n"
            "WARNING: Every Shader must be used at least once in "
            "a call to ShaderLibrary::stage.\n",
            unusedCount, unusedCount == 1 ? "" : "s");
  }

  // Estimate max count of DescriptorSet objects the DescriptorLibrary will use
  uint32_t maxSets = 0;
  // Collect all types for DescriptorLibrary::ctorError
  vector<VkDescriptorType> types;

  descriptorLibrary.layouts.clear();
  descriptorLibrary.layouts.reserve(_i->bindings.size());
  for (size_t bindingI = 0; bindingI < _i->bindings.size(); bindingI++) {
    auto binding = _i->bindings.at(bindingI);
    maxSets++;

    vector<VkDescriptorSetLayoutBinding> libBindings(binding.layouts.size());
    for (size_t layoutI = 0; layoutI < binding.layouts.size(); layoutI++) {
      auto& layout = binding.layouts.at(layoutI);
      types.emplace_back(layout.descriptorType);

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

  if (descriptorLibrary.pool.ctorError(maxSets, types)) {
    fprintf(stderr, "DescriptorPool::ctorError failed\n");
    return 1;
  }
  return 0;
}

unique_ptr<memory::DescriptorSet> DescriptorLibrary::makeSet(
    PipeBuilder& pipe, size_t layoutI /*= 0*/) {
  if (!pool.vk) {
    fprintf(stderr,
            "BUG: DescriptorLibrary::makeSet(%zu) "
            "before ShaderLibrary::makeDescriptorLibrary\n",
            layoutI);
    return unique_ptr<memory::DescriptorSet>();
  }
  if (layoutI > layouts.size()) {
    fprintf(stderr, "BUG: DescriptorLibrary::makeSet(%zu) with %zu layouts\n",
            layoutI, layouts.size());
    return unique_ptr<memory::DescriptorSet>();
  }
  auto& layout = layouts.at(layoutI);

  auto* set = new memory::DescriptorSet(pool);
  if (set->ctorError(layout)) {
    // TODO: set may fail if DescriptorPool is full.
    // Allocate another DescriptorPool and retry.
    fprintf(stderr, "DescriptorSet::ctorError failed\n");
    delete set;
    return unique_ptr<memory::DescriptorSet>();
  }

  pipe.pipeline.info.setLayouts.emplace_back(layout.vk);
  return unique_ptr<memory::DescriptorSet>(set);
}

ShaderLibrary::~ShaderLibrary() {
  if (_i) {
    delete _i;
  }
}

}  // namespace science
