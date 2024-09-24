project('robbit', ['c', 'cpp'])

sources = files([
  'src/exact/archive.c', 'src/exact/press.c', 'src/main.c',
  'src/vulkan/buffer.c', 'src/vulkan/device.c', 'src/vulkan/image.c',
  'src/vulkan/memory.c', 'src/vulkan/descriptor.c', 'src/vulkan/vulkan.c',
  'src/vulkan/pipeline.c', 'src/vulkan/swapchain.c', 'src/vulkan/present.c',
  'src/core/level.c', 'src/core/mesh.c', 'src/core/aloha.c',
])

shaders = [
  'shader/normal.vert',
  'shader/normal.frag',
  'shader/points.vert',
  'shader/points.frag',
]

objcopy = find_program('objcopy')
glslang = find_program('glslangValidator')
spirv_opt = find_program('spirv-opt')
spirv_remap = find_program('spirv-remap')

objcopy_command = [objcopy, '--input', 'binary', '--output', 'elf64-x86-64', '@PLAINNAME@.spv', '@OUTPUT@']
glslang_command = [glslang, '-V'] + ['-o', '@PLAINNAME@.spv', '@INPUT@', '--depfile', '@PLAINNAME@.deps']
spirv_opt_command = [spirv_opt, '-Os', '@PLAINNAME@.spv', '-o', '@PLAINNAME@.spv']
#spirv_remap_command = get_option('buildtype').startswith('debug') ? [] : [spirv_remap, '-s', '-o', '.', '-i', '@PLAINNAME@.spv']

shaders_c = []
foreach shader : shaders
    shaders_c += custom_target(
        input: shader,
        output: '@PLAINNAME@.spv',
        depfile: '@PLAINNAME@.deps',
        command: glslang_command
                 #+ spirv_opt_command
                 #+ spirv_remap_command
    )
endforeach

shaders_dep = declare_dependency(sources: shaders_c)

inc = include_directories(['src/include'])

stb_dep = dependency('stb')
sdl2_dep = dependency('SDL2')
vulkan_dep = dependency('vulkan', static: true)
tracy_dep = dependency('tracy', static: true)

if get_option('tracy_enable') and get_option('buildtype') != 'debugoptimized'
  warning('buildtype not set')
endif

executable('robbit', sources, 
  include_directories: inc,
  link_args : '-lm',
  dependencies: [sdl2_dep, vulkan_dep, stb_dep, shaders_dep, tracy_dep],
  install: true,
  gui_app: true,
)
