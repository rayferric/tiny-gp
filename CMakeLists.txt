cmake_minimum_required(VERSION 3.10)
project(tiny_gp)

add_library(tiny_gp SHARED tiny_gp.c)
target_link_libraries(tiny_gp m)

add_executable(tiny_gp_cli tiny_gp_cli.c)
target_link_libraries(tiny_gp_cli tiny_gp)

# Dodatkowy target z wyłączonymi funkcjami sin i cos
add_library(tiny_gp_no_sin_cos SHARED tiny_gp.c)
target_compile_definitions(tiny_gp_no_sin_cos PRIVATE NO_SIN_COS)
