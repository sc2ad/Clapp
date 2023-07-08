project('Clapp', 'cpp',
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++20'])

# The installed headers
headers = install_headers(
  'include/clapp/clapp.hpp',
  subdir: 'clapp',
)

gtest_proj = subproject('gtest')
gtest_dep = gtest_proj.get_variable('gtest_dep')
gtest_main_dep = gtest_proj.get_variable('gtest_main_dep')

clapp_include = include_directories('include')

clapp_dep = declare_dependency(include_directories: clapp_include)

subdir('tests')
