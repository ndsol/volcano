// Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
#version 450
#extension GL_ARB_separate_shader_objects : enable

// Specify outputs. location = 0 references the index in
// VkSubpassDescription.pcolorAttachments[].
layout(location = 0) out vec4 outColor;

// Specify inputs.
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

// Specify inputs that are constant ("uniform") for all vertices.
// The uniform buffer is updated by the app once per frame.
// (Note that uniforms can be rebound to another descriptor set, just like
// other inputs to the shader, and that this can happen anywhere in the frame.)
layout(binding = 1) uniform sampler2D texSampler;

void main() {
  outColor = texture(texSampler, fragTexCoord);
}
