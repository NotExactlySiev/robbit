project('robbit', ['c', 'cpp'])

sources = files([
  'src/exact/archive.c', 'src/exact/press.c', 'src/main.c',
  'src/vulkan/buffer.c', 'src/vulkan/device.c', 'src/vulkan/image.c', 'src/vulkan/memory.c',
  'src/vulkan/descriptor.c', 'src/vulkan/vulkan.c', 'src/vulkan/pipeline.c',
  'src/core/image.c', 'src/core/level.c', 'src/core/mesh.c', 'src/core/aloha.c',
  #'src/ui/ImGuiFileDialog.cpp', 'src/ui/ui.cpp', 
])

shaders = [
  'shader/normal.vert',
  'shader/normal.frag',
  'shader/points.vert',
  'shader/points.frag',
]


objcopy = find_program('objcopy')
if host_machine.system() == 'linux' # TODO: solaris, bsd, etc
  if host_machine.cpu_family() == 'x86_64'
    b_arch = 'i386:x86_64'
    b_type = 'elf64-x86-64'
  elif host_machine.cpu_fmaily() == 'x86'
    b_arch = 'i386'
    b_type = 'elf32-i386'
  endif
endif
objcopy_command = [objcopy, '--input', 'binary', '--output', b_type, '@PLAINNAME@.spv', '@OUTPUT@']


glslang = find_program('glslangValidator')
spirv_opt = find_program('spirv-opt')
spirv_remap = find_program('spirv-remap')

shaders_c = []
foreach shader : shaders
    glslang_command = [glslang, '-V'] + ['-o', '@PLAINNAME@.spv', '@INPUT@', '--depfile', '@PLAINNAME@.deps']
    spirv_opt_command = [spirv_opt, '-Os', '@PLAINNAME@.spv', '-o', '@PLAINNAME@.spv']
    #spirv_remap_command = get_option('buildtype').startswith('debug') ? [] : [spirv_remap, '-s', '-o', '.', '-i', '@PLAINNAME@.spv']
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
glfw3_dep = dependency('glfw3')
vulkan_dep = dependency('vulkan', static: true)

executable('robbit', sources, 
  include_directories: inc,
  link_args : '-lm',
  dependencies: [glfw3_dep, vulkan_dep, stb_dep, shaders_dep],
  install: true,
  gui_app: true,
)
