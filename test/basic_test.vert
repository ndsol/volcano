// Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
#version 450
#extension GL_ARB_separate_shader_objects : enable

// Specify outputs.
out gl_PerVertex {
  vec4 gl_Position;
};
layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

// Specify inputs that are constant ("uniform") for all vertices.
// The uniform buffer is updated by the app once per frame. (Of course, uniforms
// can be updated as often as the app wants to -- assume the app is "simple.")
//
// UniformBufferObject here is automatically extracted for basic_test.cpp to
// use as an #include. See gn/vendor/glslangValidator.gni.
layout(binding = 0) uniform UniformBufferObject {
  mat4 model;
  mat4 view;
  mat4 proj;
} ubo;

// Specify inputs that vary per vertex (read from the vertex buffer).
// See gn/vendor/glslangValidator.gni which automatically generates a header
// with the layout of the following inputs.
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

void main() {
  gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPos, 1.0);
  fragColor = inColor;
  fragTexCoord = inTexCoord;
}
