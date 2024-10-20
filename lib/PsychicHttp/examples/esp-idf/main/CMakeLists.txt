# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS ".")

littlefs_create_partition_image(littlefs ${project_dir}/data)