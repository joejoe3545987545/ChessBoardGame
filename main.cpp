#include <SFML/Graphics.hpp>
#include <filesystem>
#include <iostream>
#include <cstdlib>
#include <ctime>
#include "GameEngine.h"

int main(int argc, char* argv[]) {
    std::srand(static_cast<unsigned>(std::time(nullptr)));
    // 避免在 Windows 上因为硬编码 macOS 路径导致崩溃。
    try {
        if (!std::filesystem::exists("assets")) {
            auto exePath = std::filesystem::weakly_canonical(std::filesystem::path(argv[0])).parent_path();
            if (std::filesystem::exists(exePath / "assets")) {
                std::filesystem::current_path(exePath);
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Warning: cannot change current path: " << e.what() << std::endl;
    }

    std::cout << "Current working directory: "
              << std::filesystem::current_path() << std::endl;

    // 2. 🌟 恢复你原本的游戏引擎启动代码！
    // 看看你原本的 main 函数里写的是什么，把它放回来。通常长这样：
    GameEngine engine;
    engine.run();       // 让游戏引擎跑起来，这样窗口就不会退出了

    return 0;
}