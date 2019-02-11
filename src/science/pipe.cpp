/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 */

#include "science.h"

namespace science {

int PipeBuilder::deriveFrom(PipeBuilder& other) {
  pipe = std::make_shared<command::Pipeline>(pass);
  pipe->info = other.info();
  return 0;
}

int PipeBuilder::swap(PipeBuilder& other) {
  if (!pipe) {
    logE("PipeBuilder::swap: this pipe is NULL\n");
    return 1;
  }
  if (&pass != &other.pass) {
    logE("PipeBuilder::swap: this.pass=%p other.pass=%p - cannot differ.\n",
         &pass, &other.pass);
    return 1;
  }

  // Take it for granted that *this and other are compatible. Validating that
  // is a big job best left to the Vulkan validation layers.
  //
  // If *this and other are not compatible, swap() produces undefined behavior
  // up to and including a hard lockup.
  for (size_t i = 0; i < pass.pipelines.size(); i++) {
    if (pass.pipelines.at(i) == other.pipe) {
      pass.pipelines.at(i) = pipe;
      return 0;
    }
  }

  logE("PipeBuilder::swap: PipeBuilder %p with pipe %p not found in pass %p\n",
       &other, &*other.pipe, pass.vk ? pass.vk.printf() : NULL);
  return 1;
}

}  // namespace science
