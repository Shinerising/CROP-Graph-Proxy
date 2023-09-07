# CMakeList.txt: CROP Graph Proxy 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#
cmake_minimum_required (VERSION 3.8)

project ("CROP Graph Proxy")

file(GLOB GraphProxy_SRC
     "src/*.h"
     "src/*.cpp"
)

add_executable (GraphProxy ${GraphProxy_SRC})
target_link_libraries(GraphProxy "-static -lwinhttp")

# TODO: 如有需要，请添加测试并安装目标。
