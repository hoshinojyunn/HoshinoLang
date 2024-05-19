cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
project(hoshino)
set(EXECUTABLE_OUTPUT_PATH .build/../bin)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_COMPILER /usr/bin/clang++)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wno-unused-parameter -Wno-attributes") # TODO: remove

# 需要加上-rdynamic(或者-Wl,-export-dynamic) 该选项的作用是将所有的符号添加到动态符号表中
# 如果没有这个选项 则动态符号表一般只有链接时的链接符号 
# 那么在使用dlopen打开某个动态对象时就无法找到相应的符号 例如jit的lookup("putchard")
# putchard是自定义的一个函数 当使用lookup查找函数时 所有模块以及c标准库函数中都没有该符号时
# 就会去查找外部的动态符号表 此时若有-rdynamic 动态符号表中就要putchard的符号 就能找到定义
# （如果不用-rdynamic，就想办法让程序在运行时能够获取链接符号的定义，比如用动态链接库）
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -glldb -fstandalone-debug -fsanitize=address -fno-omit-frame-pointer -fno-optimize-sibling-calls")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to `Debug` as none was specified.")
  set(CMAKE_BUILD_TYPE "Debug")
endif()
# include your headers
include_directories(includes)
set(SOURCES 
    src/lexer/token.cpp
    src/context.cpp
    src/code_gen/ir_code_gen.cpp
    src/main.cpp
)

find_package(LLVM REQUIRED CONFIG)
if(NOT LLVM_FOUND OR LLVM_VERSION VERSION_LESS 7.0)
  message(SEND_ERROR "LLVM >= 7.0 is required to build hoshino")
endif()
include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})
llvm_map_components_to_libnames(llvm_libs core orcjit native)


# add_subdirectory(builtin_lib)
# 使用动态链接库 这样在jit查找函数时能够在运行时查找到函数定义
# (jit的lookup内部使用dlopen查找函数（我猜的）)
add_library(builtin_lib SHARED builtin_lib/lib.cpp)
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lib)
set(LibSTR builtin_lib)


add_executable(${CMAKE_PROJECT_NAME} ${SOURCES})

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${llvm_libs})
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${LibSTR})

set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin) 