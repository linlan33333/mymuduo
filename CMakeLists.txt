cmake_minimum_required(VERSION 2.5)
project(mymuduo)


# mymuduo最终编译成动态库，这里设置动态库的路径，放在根目录的lib文件夹下面
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)
# 设置调试信息，以及启动C++语言标准
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++14 -fPIC")

# 定义参与编译的源代码文件, .表示当前文件夹下的所有文件都参与编译，SRC_LIST表示编译后的文件名字组合起来放到名为SRC_LIST的变量里面，即SRC_LIST代表所有编译后的文件
aux_source_directory(. SRC_LIST)
# 编译生成动态库mymuduo
add_library(mymuduo SHARED ${SRC_LIST})

###
### 每次新写的代码源文件经常没有参与编译，可以右键CMakeList.txt文件，选择“清理所有项目的重新配置”，
### 此时会重新编译makefile，就把新文件都包括进来了，这时再右键选择“清理所有项目的重新生成”即可
###