with import <nixpkgs> {};

mkShell {
  packages = [
    stb
    SDL2
    freetype
    vulkan-headers
    vulkan-loader
    vulkan-validation-layers
    vulkan-extension-layer
    vulkan-tools        # vulkaninfo
    shaderc             # GLSL to SPIRV compiler - glslc
    glslang
    renderdoc           # Graphics debugger
    tracy               # Graphics profiler
    vulkan-tools-lunarg # vkconfig
  ];

  buildInputs = with pkgs; [
    stb
    #freetype
    SDL2
  ];

  nativeBuildInputs = with pkgs; [
    pkg-config
  ];

  VK_LAYER_PATH = "${vulkan-validation-layers}/share/vulkan/explicit_layer.d";
}
