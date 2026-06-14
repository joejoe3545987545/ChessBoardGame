cmake_minimum_required(VERSION 3.24)
project(chessBoardGame LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 支持 Windows 和 macOS 的 SFML 查找
if(WIN32)
    # 请确保这里指向完整 SFML SDK 根目录（包含 include/ 和 lib/）
    if(NOT DEFINED SFML_ROOT)
        set(SFML_ROOT "D:/SFML-3.0.2" CACHE PATH "Path to SFML root on Windows")
    endif()
    list(APPEND CMAKE_PREFIX_PATH "${SFML_ROOT}")
elseif(APPLE)
    list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew")
endif()

# 寻找 SFML 3.0.x 组件
find_package(SFML 3.0 COMPONENTS Graphics Window System Audio REQUIRED HINTS "${SFML_ROOT}")

# 自动搜集项目下的所有源文件
file(GLOB_RECURSE SOURCES "main.cpp" "src/*.cpp")

# 生成可执行程序
add_executable(chessBoardGame ${SOURCES}
        include/Card.h
        include/DeckManager.h
        src/DeckManager.cpp)

# 显式地将 include 目录绑定给 chessBoardGame 目标
target_include_directories(chessBoardGame PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")

# 为 chessBoardGame 目标链接对应的 SFML 3.0 模块
target_link_libraries(chessBoardGame PRIVATE
        SFML::Graphics
        SFML::Window
        SFML::System
        SFML::Audio
)

# 🌟【核心修正】双保险资源拷贝脚本
# 1. 拷贝到 cmake-build-debug 根目录下 (普通路径访问)
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

# 2. 拷贝到 Mac App 包内部的 MacOS 目录下 (防止 Mac 独有的 Bundle 导致路径失效)
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/chessBoardGame.app/Contents/MacOS")