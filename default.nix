with import <nixpkgs> {};

mkShell {
  packages = [
    stb
    glfw
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
    glfw
    #freetype
  ];

  nativeBuildInputs = with pkgs; [
    pkg-config
  ];

  #LD_LIBRARY_PATH="${glfw}/lib:${freetype}/lib:${vulkan-loader}/lib:${vulkan-validation-layers}/lib";
  #VULKAN_SDK = "${vulkan-headers}";
  #STB_PATH = "${stb}/include/stb";
  VK_LAYER_PATH = "${vulkan-validation-layers}/share/vulkan/explicit_layer.d";
}
