cmake_minimum_required(VERSION 2.6)
project(plugins)

add_subdirectory(cg_fltk)
add_subdirectory(crg_grid)
add_subdirectory(crg_stereo_view)
add_subdirectory(cmi_io)
#add_subdirectory(examples)

add_custom_target(plugins)
add_dependencies(plugins cg_fltk crg_grid crg_stereo_view cmi_io)

set_target_properties(plugins PROPERTIES FOLDER "${FOLDER_NAME_PLUGINS}")


