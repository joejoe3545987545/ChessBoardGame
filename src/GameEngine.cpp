#include "GameEngine.h"
#include <iostream>
#include <filesystem>
#include <cmath>
#include <vector>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <algorithm>


// ------------------------ 辅助：资源路径查找 ------------------------
static std::string getEngineAssetPath(const std::string& relativePath) {
    if (std::filesystem::exists(relativePath)) {
        return relativePath;
    }
    std::string fallbackPath = "../" + relativePath;
    if (std::filesystem::exists(fallbackPath)) {
        return fallbackPath;
    }
    return relativePath;
}

// ------------------------ 辅助：字体加载 ------------------------
static sf::Font loadSystemFont() {
    sf::Font f;
    std::vector<std::string> fontPaths;

#ifdef _WIN32
    fontPaths = {
        "C:\\Windows\\Fonts\\msyh.ttc",
        "C:\\Windows\\Fonts\\MSYH.TTC",
        "C:\\Windows\\Fonts\\simsun.ttc",
        "C:\\Windows\\Fonts\\simhei.ttf",
        "C:\\Windows\\Fonts\\arial.ttf",
        "C:\\Windows\\Fonts\\arialuni.ttf"
    };
#elif defined(__APPLE__)
    fontPaths = {
        "/System/Library/Fonts/STHeiti Light.ttc",
        "/System/Library/Fonts/PingFang.ttc",
        "/Library/Fonts/Arial.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
        "/System/Library/Fonts/Supplemental/Arial.ttf"
    };
#else
    fontPaths = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/noto/NotoSansSC-Regular.otf",
        "/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc",
        "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc"
    };
#endif

    std::vector<std::string> candidatePaths = {
        getEngineAssetPath("assets/fonts/simhei.ttf"),
        getEngineAssetPath("assets/fonts/msyh.ttc"),
        getEngineAssetPath("assets/fonts/arial.ttf")
    };
    candidatePaths.insert(candidatePaths.end(), fontPaths.begin(), fontPaths.end());

    for (const auto& p : candidatePaths) {
        if (!p.empty() && std::filesystem::exists(p)) {
            if (f.openFromFile(p)) {
                std::cout << "[Info] Font loaded from: " << p << std::endl;
                return f;
            }
        }
    }

    std::cerr << "[Warning] Failed to load any system font. UI text may be invalid." << std::endl;
    return f; // 可能为空
}

// ------------------------ 辅助：保存目录 ------------------------
static std::filesystem::path getSavesDir() {
    std::filesystem::path p = std::filesystem::current_path() / "saves";
    if (!std::filesystem::exists(p)) {
        std::error_code ec;
        std::filesystem::create_directories(p, ec);
        if (ec) {
            std::cerr << "[Error] Failed to create saves dir: " << ec.message() << std::endl;
        }
    }
    return p;
}

// ------------------------ GameEngine 实现 ------------------------
GameEngine::GameEngine()
    : window(sf::VideoMode({WINDOW_WIDTH, WINDOW_HEIGHT}), "chessBoardGame (Professional Edition)"),
      currentState(GameState::MENU),
      currentTurn(1),
      isProfessionalMode(false),
      winReason(L""),
      isGameOver(false),
      isCardAttachedToMouse(false),
      playerColorPref(1),
      aiDifficultySetting(2),
      isAiThinking(false),
      font(loadSystemFont()),
      uiText(font),
      timerText(font),
      dropSound(nullptr),
      aiPlayer(nullptr)
{
    // 初始化音频
    initUI();
    initAudio();

    std::cout << "[Info] GameEngine constructing..." << std::endl;

    try {
        if (font.getInfo().family.empty()) {
            std::cerr << "[Warning] Loaded font appears empty or invalid." << std::endl;
        }
    } catch (...) {
        std::cerr << "[Info] Font info check skipped (getInfo unavailable)." << std::endl;
    }

    uiText.setFillColor(sf::Color(45, 45, 45));
    uiText.setCharacterSize(18);

    timerText.setCharacterSize(22);
    timerText.setStyle(sf::Text::Bold);
    timerText.setFillColor(sf::Color(40, 120, 40));

    // 🌟 顺手把这里重复调用的一句 initUI(); 删掉了，让代码更干净

    // 🌟【安全加载新卡牌图片底保持不变】
    if (newCardTexture.loadFromFile(getEngineAssetPath("assets/Card.png"))) {
        newCardTexture.setSmooth(true); 
        
        // 动态 new 一个对象出来
        newCardSprite = new sf::Sprite(newCardTexture);
        
        sf::Vector2u size = newCardTexture.getSize();
        newCardSprite->setOrigin({size.x / 2.f, size.y / 2.f});
        
        std::cout << "[🎉 Success] 指针版新卡牌加载成功！" << std::endl;
    } else {
        std::cerr << "[❌ Error] 找不到卡牌底图: assets/Card.png" << std::endl;
    }
    // ===== 🌟 紧跟在 newCardSprite 初始化下方的卡槽初始化 =====
    if (cardSlotTexture.loadFromFile("assets/CardSlot.png")) { // 💡 确认好你的素材路径
        cardSlotSprite = new sf::Sprite(cardSlotTexture);
        
        // 1. 🌟 绝对同步：设置与卡牌完全相同的中心原点
        sf::Vector2u slotSize = cardSlotTexture.getSize();
        cardSlotSprite->setOrigin({slotSize.x / 2.f, slotSize.y / 2.f});
        
        // 2. 🌟 绝对同步：设置与卡牌完全相同的 0.18f 缩放
        cardSlotSprite->setScale({0.25f, 0.25f});
        
        // 3. 🌟 绝对同步：让卡槽永久锁死在你指定的卡牌原位中心点
        cardSlotSprite->setPosition({1080.f, 166.f});
    } else {
        std::cout << "[UI Error] 无法加载卡槽图片素材 CardSlot.png" << std::endl;
    }
    std::cout << "[Info] GameEngine constructed." << std::endl;
}

GameEngine::~GameEngine()
{
    menuMusic.stop();
    configMusic.stop();
    battleMusic.stop();
    winMusic.stop(); 
    failMusic.stop(); 
    if (dropSound) delete dropSound;//释放音频

    if (aiPlayer) {
        delete aiPlayer;
        aiPlayer = nullptr;
        std::cout << "[Info] AIPlayer deleted in destructor." << std::endl;
    }
    if (newCardSprite) delete newCardSprite;
    if (cardSlotSprite) delete cardSlotSprite;
    std::cout << "[Info] GameEngine destroyed." << std::endl;
}

void GameEngine::initUI() {
    menuButtons.clear();
    menuButtonBackgrounds.clear();
    gameButtons.clear();
    gameButtonTexts.clear();

    // 🌟【新增】加载卡牌专属的中文字体
    // 假设你把下载好的个性化 `.ttf` 或 `.otf` 字体文件放到了 assets/fonts/ 目录下
    if (!cardFont.openFromFile(getEngineAssetPath("assets/fonts/CardFont.ttf"))) {
    std::cout << "[Font Error] 无法加载卡牌专属字体，自动降级。" << std::endl;
    cardFont = font;
} else {
    std::cout << "[Font] 卡牌专属视觉字体加载成功！" << std::endl;
}

    std::vector<std::wstring> menuTexts = {
        L"本地双人对战", L"人机对战", L"游戏设置", L"规则说明帮助", L"历史记录复盘"
    };

    // 菜单按钮：宽度240，高度50，居中显示
    const float menuButtonWidth = 240.f;
    const float menuButtonHeight = 50.f;
    const float centerX = WINDOW_WIDTH * 0.5f;
    const float menuStartY = 220.f;
    const float menuButtonX = centerX - menuButtonWidth * 0.5f;

    for (size_t i = 0; i < menuTexts.size(); ++i) {
        sf::RectangleShape bg({menuButtonWidth, menuButtonHeight});
        bg.setPosition({menuButtonX, menuStartY + i * 80.f});
        bg.setFillColor(sf::Color(212, 163, 107));
        menuButtonBackgrounds.push_back(bg);

        sf::Text t(font, menuTexts[i], 20);
        t.setFillColor(sf::Color(45, 45, 45));
        sf::FloatRect textBounds = t.getLocalBounds();
        t.setPosition(sf::Vector2f(
            centerX - (textBounds.position.x + textBounds.size.x) * 0.5f,
            menuStartY + i * 80.f + (menuButtonHeight - textBounds.size.y) * 0.5f - textBounds.position.y));
        menuButtons.push_back(t);
    }

    // 游戏HUD按钮：位于底部，3个按钮均匀分布
    std::vector<std::wstring> hudLabels = {L"认输放弃", L"重新开局", L"返回主页"};
    for (size_t i = 0; i < hudLabels.size(); ++i) {
        sf::RectangleShape btn({120.f, 36.f});
        btn.setPosition({static_cast<float>(80 + i * 400), 665.f});
        btn.setFillColor(sf::Color(156, 107, 69));
        gameButtons.push_back(btn);

        sf::Text t(font, hudLabels[i], 15);
        t.setFillColor(sf::Color::White);
        t.setPosition({static_cast<float>(95 + i * 400), 670.f});
        gameButtonTexts.push_back(t);
    }
}


// 全屏切换
void GameEngine::toggleFullscreen() {
    isFullscreen = !isFullscreen;
    window.close();
    
    unsigned int style = isFullscreen ? static_cast<unsigned int>(sf::State::Fullscreen) : static_cast<unsigned int>(sf::State::Windowed);
    window.create(sf::VideoMode({WINDOW_WIDTH, WINDOW_HEIGHT}), 
                 "chessBoardGame (Professional Edition)", style);
    
    // 恢复默认视图，使用窗口真实像素坐标
    window.setView(window.getDefaultView());
    initUI();
    
    std::cout << "[Info] Fullscreen toggled: " << (isFullscreen ? "ON" : "OFF") << std::endl;
}

// ------------------------ 主循环 ------------------------
void GameEngine::run() {
    // 使用默认视图，让屏幕坐标直接与窗口像素对应
    window.setView(window.getDefaultView());
    
    while (window.isOpen()) {
        processEvents();
        update();
        render();
    }
}

// 事件处理
void GameEngine::processEvents() {
    // SFML 3.0 现代事件循环机制
    while (const std::optional event = window.pollEvent()) {
        if (event->is<sf::Event::Closed>()) {
            window.close();
        }

        // 捕捉鼠标按下事件
        if (const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>()) {
            if (mousePressed->button == sf::Mouse::Button::Left) {
                // 精准获取并转换视口/逻辑坐标
                sf::Vector2i pixelPos = sf::Mouse::getPosition(window);
                sf::Vector2f worldPos = window.mapPixelToCoords(pixelPos);
                sf::Vector2i mousePos = sf::Vector2i(static_cast<int>(worldPos.x), static_cast<int>(worldPos.y));

                if (currentState == GameState::MENU) {
                    handleMenuClick(mousePos);
                }
                else if (currentState == GameState::PVE_CONFIG) {
                    handlePVEConfigClick(mousePos);
                }
                else if (currentState == GameState::SETTINGS) {
                    handleSettingsClick(mousePos);
                }
                else if (currentState == GameState::HELP) {
                    // 说明页面：点击任意返回主菜单
                    currentState = GameState::MENU;
                }
                else if (currentState == GameState::HISTORY) {
                    // 历史界面：点击条目加载复盘，否则返回菜单（背景）
                    handleHistoryClick(mousePos);
                }
                else if (currentState == GameState::GAME_OVER) {
                    // 计算出屏幕正中心按钮的精确点击范围
                    if (mousePos.x > 540 && mousePos.x < 740 && mousePos.y > 420 && mousePos.y < 470) {
                        std::cout << "[UI] Game Over Screen: Return to Main Menu clicked." << std::endl;
                        
                        chessboard.reset();
                        playerDeck.resetDeck();
                        
                        currentTurn = 1;
                        isAiThinking = false;
                        isGameOver = false;
                        savedThisGame = false;
                        
                        currentState = GameState::MENU;
                    }
                }
                else if (currentState == GameState::GAME_PVP || currentState == GameState::GAME_PVE) {
                    if (currentState == GameState::GAME_PVE && currentTurn != playerColorPref) {
                        // 🌟【注意】：在标准事件循环中，如果需要跳过非玩家回合的其余点击，用 continue 是对 vacuum 的
                        continue;
                    }

                    // 🌟【核心嵌入点】：调用 handleHUDClick 检测是否点中了卡牌或按钮
                    bool hudIntercepted = handleHUDClick(mousePos);

                    // 如果被 HUD 拦截了（说明点中了吸附卡牌或点击了归位），直接结束本次点击分支判断。
                    // 这样就不会向下执行棋盘落子的 handleMouseClick，完美防止穿透误落子！
                    if (hudIntercepted) {
                        // 点中卡牌后，状态已经在 handleHUDClick 里翻转了，我们直接结束本次对局状态下的点击分支
                        // 这样它就会顺利进入下一个事件轮询，绝不会在棋盘上落子
                        continue; 
                    }

                    // 只有没点中任何 HUD (hudIntercepted == false)，才去处理常规棋盘落子逻辑
                    int lastRow = -1, lastCol = -1;
                    if (chessboard.handleMouseClick(mousePos, currentTurn, lastRow, lastCol)) {
                        // 音频处理
                        dropSound->play();
                        std::cout << "[Audio] Drop sound played." << std::endl;

                        // 点击成功立刻加入手牌数据系统
                        int pattern = chessboard.checkPattern(lastRow, lastCol);
                        if (pattern == 3 || pattern == 4) {
                            playerDeck.drawCard(); 
                        }

                        // 记录历史
                        chessboard.recordMove(lastRow, lastCol, currentTurn);

                        // 检查胜利
                        if (chessboard.checkWin(lastRow, lastCol)) {
                            winReason = (currentTurn == 1) ? L"黑方达成五连，获得了胜利！" : L"白方达成五连，获得了胜利！";
                            isGameOver = true;
                            currentState = GameState::GAME_OVER;
                            std::cout << "[Info] Player " << currentTurn << " wins." << std::endl;
                        } else {
                            currentTurn = (currentTurn == 1) ? 2 : 1;
                            turnClock.restart();

                            if (currentState == GameState::GAME_PVE && currentTurn != playerColorPref) {
                                isAiThinking = true;
                                aiThinkClock.restart();
                            }
                        }
                    }
                }
            }
        }
    }
}

void GameEngine::handleMenuClick(sf::Vector2i mousePos) {
    const float menuButtonWidth = 240.f;
    const float centerX = WINDOW_WIDTH * 0.5f;
    const float menuButtonX = centerX - menuButtonWidth * 0.5f;
    const float menuButtonRightX = centerX + menuButtonWidth * 0.5f;
    const float menuStartY = 220.f;

    if (mousePos.x > menuButtonX && mousePos.x < menuButtonRightX) {
        if (mousePos.y > menuStartY && mousePos.y < menuStartY + 50.f) {
            chessboard.reset();
            
            // 🌟【确切位置】：在这里精准添加！保证主菜单进入新双人对战时隐藏卡牌底图

            currentTurn = 1;
            isAiThinking = false;
            isGameOver = false;
            savedThisGame = false;
            currentState = GameState::GAME_PVP;
            turnClock.restart();
        }
        else if (mousePos.y > menuStartY + 80.f && mousePos.y < menuStartY + 130.f) {
            currentState = GameState::PVE_CONFIG;
        }
        else if (mousePos.y > menuStartY + 160.f && mousePos.y < menuStartY + 210.f) {
            currentState = GameState::SETTINGS;
        }
        else if (mousePos.y > menuStartY + 240.f && mousePos.y < menuStartY + 290.f) {
            currentState = GameState::HELP;
        }
        else if (mousePos.y > menuStartY + 320.f && mousePos.y < menuStartY + 370.f) {
            currentState = GameState::HISTORY;
        }
    }
}

void GameEngine::handlePVEConfigClick(sf::Vector2i mousePos) {
    const float centerX = WINDOW_WIDTH * 0.5f;
    const float btnWidth = 400.f;
    const float btnLeft = centerX - btnWidth * 0.5f;
    const float btnRight = centerX + btnWidth * 0.5f;

    if (mousePos.x > btnLeft && mousePos.x < btnRight) {
        if (mousePos.y > 140 && mousePos.y < 190) {
            aiDifficultySetting = (aiDifficultySetting % 3) + 1;
            std::cout << "[Info] AI difficulty -> " << aiDifficultySetting << std::endl;
        }
        else if (mousePos.y > 210 && mousePos.y < 260) {
            playerColorPref = (playerColorPref == 1) ? 2 : 1;
            std::cout << "[Info] Player color -> " << (playerColorPref == 1 ? "Black" : "White") << std::endl;
        }
        else if (mousePos.y > 350 && mousePos.y < 400) {
            chessboard.reset();
            
            // 🌟【确切位置】：在此处精准添加，保证新PVE对局开始时隐藏卡牌底图

            currentTurn = 1;
            isAiThinking = false;
            isGameOver = false;
            savedThisGame = false;
            currentState = GameState::GAME_PVE;
            turnClock.restart();

            if (aiPlayer) { delete aiPlayer; aiPlayer = nullptr; }
            int aiColor = (playerColorPref == 1) ? 2 : 1;
            aiPlayer = new AIPlayer(aiColor, aiDifficultySetting);

            if (playerColorPref == 2) {
                isAiThinking = true;
                aiThinkClock.restart();
            }
        }
    }

    if (mousePos.x > btnLeft && mousePos.x < btnRight && mousePos.y > 410 && mousePos.y < 460) {
        currentState = GameState::MENU;
    }
}

bool GameEngine::handleHUDClick(sf::Vector2i mousePos) {
    // 只有当玩家手里确实有卡牌时，才需要进行卡牌的碰撞盒检测
    if (!playerDeck.hand.empty() && newCardSprite != nullptr) {
        sf::FloatRect cardClickRect;
        
        // 严格锁定你指定的卡牌物理宽高
        sf::Vector2f cardSize(144.f, 200.f); 
        sf::Vector2f mouseClickPos(static_cast<float>(mousePos.x), static_cast<float>(mousePos.y));

        if (isCardAttachedToMouse) {
            // A. 如果卡牌已被吸附随鼠移动：
            // 此时卡牌的渲染位置是以中心点驱动的：cardPos = worldPos - cardMouseOffset
            // 所以当前帧卡牌的中心点就是 (worldPos - cardMouseOffset)
            // 那么卡牌的左上角物理起点 = 中心点 - 半个高宽 (72.f, 100.f)
            sf::Vector2i pixelPos = sf::Mouse::getPosition(window);
            sf::Vector2f worldPos = window.mapPixelToCoords(pixelPos);
            sf::Vector2f currentCardCenter = worldPos - cardMouseOffset;
            
            cardClickRect = sf::FloatRect(
                sf::Vector2f(currentCardCenter.x - 72.f, currentCardCenter.y - 100.f), 
                cardSize
            );
        } else {
            // B. 如果卡牌在原位静止：
            // 严格控制在你指定的绝对范围：X: [1008.f, 1152.f], Y: [60.f, 260.f]
            // 左上角起点为 (1008.f, 60.f)，高宽为 (144.f, 200.f)
            cardClickRect = sf::FloatRect(sf::Vector2f(1008.f, 60.f), cardSize);
        }

        // 检测当前鼠标点击是否精准落在计算出的卡牌包围盒内部
        if (cardClickRect.contains(mouseClickPos)) {
            
            // 核心锁死：如果是【从未吸附 ➡️ 进入吸附】，计算鼠标相对于卡牌中心点(1080.f, 160.f)的相对偏移量
            if (!isCardAttachedToMouse) {
                sf::Vector2f cardCenter(1080.f, 160.f);
                cardMouseOffset = mouseClickPos - cardCenter;
            }

            // 状态翻转：原本吸附的则归位，原本在原位的则吸附
            isCardAttachedToMouse = !isCardAttachedToMouse; 
            
            std::cout << "[Card UI] 区域吸附成功！当前卡牌绝对物理范围 X:[1008, 1152] Y:[60, 260]" << std::endl;
            
            return true; // 拦截点击，防止穿透落子
        }
    }
    // 1. 处理底部 HUD 按钮区域 (y: 665-701)
    if (mousePos.y > 665 && mousePos.y < 701) {
        if (mousePos.x > 80 && mousePos.x < 200) {
            winReason = (currentTurn == 1) ? L"黑方认输，白方获胜！" : L"白方认输，黑方获胜！";
            isGameOver = true;
            currentState = GameState::GAME_OVER;
            return true; // 告知外部：点击已被处理
        }
        else if (mousePos.x > 480 && mousePos.x < 600) { // 重新开局
            chessboard.reset();
            lastHandEmpty = true; // 标记手牌已空，下次有牌时将重新激活出生特效
            battleMusic.stop();
            battleMusic.play();
            winMusic.stop();
            playerDeck.resetDeck();
            
            // 🌟【确切位置 1】：在此处精准添加，保证局内主动点击“重新开局”时隐藏卡牌底图

            currentTurn = 1;
            isAiThinking = false;
            isGameOver = false;
            savedThisGame = false;
            turnClock.restart();
            if (currentState == GameState::GAME_PVE && playerColorPref == 2) {
                isAiThinking = true;
                aiThinkClock.restart();
            }
            return true; // 告知外部：点击已被处理
        }
        else if (mousePos.x > 880 && mousePos.x < 1000) { // 返回主菜单
            
            // 🌟【确切位置 2】：在此处精准添加，保证中途退出返回主菜单时重置卡牌隐藏状态

            currentState = GameState::MENU;
            return true; // 告知外部：点击已被处理
        }
        // 如果点击在 665-701 范围内但没点中按钮，依然返回 true 防止穿透到底层逻辑
        return true; 
    }

    // 2. 处理手牌区域
    sf::Vector2f mousePoint(static_cast<float>(mousePos.x), static_cast<float>(mousePos.y));
    return false; // 没点中任何手牌
}

void GameEngine::handleSettingsClick(sf::Vector2i mousePos) {
    const float centerX = WINDOW_WIDTH * 0.5f;
    const float btnWidth = 400.f;
    const float btnLeft = centerX - btnWidth * 0.5f;
    const float btnRight = centerX + btnWidth * 0.5f;

    if (mousePos.x > btnLeft && mousePos.x < btnRight) {
        if (mousePos.y > 140 && mousePos.y < 190) {
            isProfessionalMode = !isProfessionalMode;
            std::cout << "[Info] Professional mode: " << (isProfessionalMode ? "ON" : "OFF") << std::endl;
        }
        else if (mousePos.y > 210 && mousePos.y < 260) {
            toggleFullscreen();
        }
    }
    if (mousePos.x > btnLeft && mousePos.x < btnRight && mousePos.y > 410 && mousePos.y < 460) {
        currentState = GameState::MENU;
    }
}

// 点击历史记录界面
void GameEngine::handleHistoryClick(sf::Vector2i mousePos) {
    // mousePos 已经是世界坐标，因此直接判断即可

    // 遍历已渲染的历史行，若命中则加载并开始回放
    for (const auto& it : historyItems) {
        if (it.first.contains(sf::Vector2f(static_cast<float>(mousePos.x), static_cast<float>(mousePos.y)))) {
            // 加载并开始复盘
            const std::filesystem::path& p = it.second;
            std::ifstream ifs(p);
            if (!ifs) {
                std::cerr << "[Error] Failed to open replay file: " << p << std::endl;
                return;
            }

            std::string line;
            std::vector<Move> moves;
            // 读取直到 "Moves:" 行
            while (std::getline(ifs, line)) {
                if (line.rfind("Moves:", 0) == 0) break;
            }
            // 读取 move 行
            while (std::getline(ifs, line)) {
                if (line.empty()) continue;
                std::istringstream iss(line);
                int r,c,pv;
                if (!(iss >> r >> c >> pv)) continue;
                moves.emplace_back(r,c,pv);
            }
            ifs.close();

            if (!moves.empty()) {
                chessboard.startReplay(moves);
                replayClock.restart();
                currentState = GameState::HISTORY; // 保持 History 状态，用 update() 做步进
            }
            return;
        }
    }

    // 点击空白区域则返回主菜单
    currentState = GameState::MENU;
}

void GameEngine::update() {
    // 🌟【全状态音频状态机】：完美互斥控制所有 5 首 BGM 的播放

    if (currentState == GameState::MENU) {
        if (static_cast<int>(menuMusic.getStatus()) != 2) menuMusic.play();
        if (static_cast<int>(configMusic.getStatus()) == 2) configMusic.stop();
        if (static_cast<int>(battleMusic.getStatus()) == 2) battleMusic.stop();
        if (static_cast<int>(winMusic.getStatus()) == 2)    winMusic.stop();
        if (static_cast<int>(failMusic.getStatus()) == 2)   failMusic.stop();
    } 
    else if (currentState == GameState::PVE_CONFIG) {
        if (static_cast<int>(configMusic.getStatus()) != 2) configMusic.play();
        if (static_cast<int>(menuMusic.getStatus()) == 2) menuMusic.stop();
        if (static_cast<int>(battleMusic.getStatus()) == 2) battleMusic.stop();
        if (static_cast<int>(winMusic.getStatus()) == 2)    winMusic.stop();
        if (static_cast<int>(failMusic.getStatus()) == 2)   failMusic.stop();
    } 
    else if (currentState == GameState::GAME_PVP || currentState == GameState::GAME_PVE) {
        if (static_cast<int>(battleMusic.getStatus()) != 2) battleMusic.play();
        if (static_cast<int>(menuMusic.getStatus()) == 2) menuMusic.stop();
        if (static_cast<int>(configMusic.getStatus()) == 2) configMusic.stop();
        if (static_cast<int>(winMusic.getStatus()) == 2)    winMusic.stop();
        if (static_cast<int>(failMusic.getStatus()) == 2)   failMusic.stop();
    } 
    else if (currentState == GameState::GAME_OVER) {
        // 🌟【结算界面音频逻辑】
        if (static_cast<int>(menuMusic.getStatus()) == 2)   menuMusic.stop();
        if (static_cast<int>(configMusic.getStatus()) == 2) configMusic.stop();
        if (static_cast<int>(battleMusic.getStatus()) == 2) {
            battleMusic.stop();
            std::cout << "[Audio] 对局结束，切断对战音乐" << std::endl;
        }

        // 🌟【核心修复：精准输赢判断】
        bool isPlayerWon = true; 

        // 判定条件1：如果是思考超时引起的游戏结束
        if (winReason.find(L"思考超时") != std::wstring::npos) {
            // 如果当前轮到谁，就说明是谁超时了。如果超时的人等同于玩家颜色，说明玩家输了
            if (currentTurn == playerColorPref) {
                isPlayerWon = false;
            } else {
                isPlayerWon = true;
            }
        }
        // 判定条件2：如果文本里明确写了白方获胜，且玩家是执黑(1)的，说明玩家输了（被AI打败）
        else if (winReason.find(L"白方获胜") != std::wstring::npos && playerColorPref == 1) {
            isPlayerWon = false;
        }
        // 判定条件3：如果文本里明确写了黑方获胜，且玩家是执白(2)的，说明玩家也输了
        else if (winReason.find(L"黑方获胜") != std::wstring::npos && playerColorPref == 2) {
            isPlayerWon = false;
        }
        // 判定条件4：直接包含 AI 获胜相关的特定字符串
        else if (winReason.find(L"AI 计算完美") != std::wstring::npos) {
            isPlayerWon = false; // 只要是 AI 算赢的，玩家铁定输了
        }
        else {
            // 其余情况（如 PVP 人人对战、或者玩家获胜），一律播放胜利音乐
            isPlayerWon = true;
        }

        // 3. 播放对应的结算音乐
        if (isPlayerWon) {
            if (static_cast<int>(winMusic.getStatus()) != 2) {
                winMusic.play();
                std::cout << "[Audio] 恭喜胜出！播放 BGM-Win" << std::endl;
            }
            if (static_cast<int>(failMusic.getStatus()) == 2) failMusic.stop();
        } else {
            if (static_cast<int>(failMusic.getStatus()) != 2) {
                failMusic.play();
                std::cout << "[Audio] 遗憾落败！播放 BGM-Fail" << std::endl;
            }
            if (static_cast<int>(winMusic.getStatus()) == 2) winMusic.stop();
        }
    }
    else {
        if (static_cast<int>(menuMusic.getStatus()) == 2)   menuMusic.stop();
        if (static_cast<int>(configMusic.getStatus()) == 2) configMusic.stop();
        if (static_cast<int>(battleMusic.getStatus()) == 2) { battleMusic.stop(); }
        if (static_cast<int>(winMusic.getStatus()) == 2)    winMusic.stop();
        if (static_cast<int>(failMusic.getStatus()) == 2)   failMusic.stop();
    }

    // 回放步进优先
    if (chessboard.isReplaying()) {
        if (replayClock.getElapsedTime().asSeconds() >= replayStepSeconds) {
            replayClock.restart();
            bool more = chessboard.stepReplay();
            if (!more) {
                // 回放结束后保持在 History 页面
            }
        }
        return;
    }

    if (currentState != GameState::GAME_PVP && currentState != GameState::GAME_PVE) return;

    float elapsedSeconds = turnClock.getElapsedTime().asSeconds();
    if (elapsedSeconds >= 45.f) {
        winReason = (currentTurn == 1) ? L"黑方思考超时，白方获胜！" : L"白方思考超时，黑方获胜！";
        isGameOver = true;
        currentState = GameState::GAME_OVER; // 🌟 触发游戏结束状态
        return;
    }

    if (currentState == GameState::GAME_PVE && currentTurn != playerColorPref) {
        if (!aiPlayer) {
            std::cerr << "[Warning] aiPlayer was null; recreating..." << std::endl;
            int aiColor = (playerColorPref == 1) ? 2 : 1;
            aiPlayer = new AIPlayer(aiColor, aiDifficultySetting);
        }

        if (!isAiThinking) {
            isAiThinking = true;
            aiThinkClock.restart();
        }

        if (aiThinkClock.getElapsedTime().asSeconds() >= 0.8f) {
            auto move = aiPlayer->calculateBestMove(chessboard);
            int aiRow = move.first;
            int aiCol = move.second;

            if (aiRow >= 0 && aiRow < 15 && aiCol >= 0 && aiCol < 15) {
                chessboard.placePieceByAI(aiRow, aiCol, currentTurn, aiRow, aiCol);

                if (dropSound) {
                    dropSound->play();
                    std::cout << "[Audio] AI Drop sound played." << std::endl;
                }

                chessboard.recordMove(aiRow, aiCol, currentTurn);
                isAiThinking = false;

                if (chessboard.checkWin(aiRow, aiCol)) {
                    winReason = L"AI 计算完美，白方获胜！"; // 🌟 这里包含了“获胜！”而不是简单的“胜”
                    isGameOver = true;
                    currentState = GameState::GAME_OVER; // 🌟 触发游戏结束状态
                    std::cout << "[Info] AI wins at (" << aiRow << "," << aiCol << ")" << std::endl;
                } else {
                    currentTurn = playerColorPref;
                    turnClock.restart();
                }
            } else {
                std::cerr << "[Error] AI returned invalid move (" << aiRow << "," << aiCol << ")." << std::endl;
                isAiThinking = false;
                currentTurn = playerColorPref;
                turnClock.restart();
            }
        }
    }

    // 如果局面刚结束并未保存，则保存一次（自动保存原封不动保持在最底部）
    if (isGameOver && !savedThisGame) {
        try {
            auto dir = getSavesDir();
            auto now = std::chrono::system_clock::now();
            std::time_t tt = std::chrono::system_clock::to_time_t(now);
            std::tm tm;
    #ifdef _WIN32
            localtime_s(&tm, &tt);
    #else
            localtime_r(&tt, &tm);
    #endif
            std::ostringstream ss;
            ss << std::put_time(&tm, "%Y%m%d_%H%M%S");
            std::string timestr = ss.str();

            // 🌟 修复：保存时精准捕获正确的游戏模式字符串
            std::string modeStr = (currentState == GameState::GAME_OVER) ? ((playerColorPref != 0 && aiPlayer != nullptr) ? "PVE" : "PVP") : "OTHER";
            if (modeStr == "OTHER" && (winReason.find(L"AI") != std::wstring::npos)) modeStr = "PVE";

            std::string resultStr = isGameOver ? "Finished" : "Unfinished";
            std::string filename = timestr + "_" + modeStr + "_" + resultStr + ".txt";
            auto filepath = dir / filename;

            std::ofstream ofs(filepath);
            if (!ofs) {
                std::cerr << "[Error] Cannot open file to save: " << filepath << std::endl;
            } else {
                auto moves = chessboard.getMoveHistory();
                ofs << "Mode: " << modeStr << "\n";
                ofs << "Date: " << timestr << "\n";
                ofs << "Result: " << resultStr << "\n";
                ofs << "Moves:\n";
                for (const auto &m : moves) {
                    ofs << m.row << " " << m.col << " " << m.player << "\n";
                }
                ofs.close();
                std::cout << "[Info] Saved game to " << filepath << std::endl;
                savedThisGame = true;
            }
        } catch (const std::exception& ex) {
            std::cerr << "[Error] Exception while saving game: " << ex.what() << std::endl;
        }
    }
}

// ------------------------ 渲染 ------------------------
void GameEngine::render() {
    window.clear(sf::Color(250, 215, 175));

    switch (currentState) {
        case GameState::MENU:
            renderMenu();
            break;
        case GameState::PVE_CONFIG:
            renderPVEConfig();
            break;
        case GameState::GAME_PVP:
        case GameState::GAME_PVE:
            renderGameplay();
            break;
        case GameState::GAME_OVER:
            renderGameOver();
            break;
        case GameState::SETTINGS:
            renderSettings();
            break;
        case GameState::HELP:
        case GameState::HISTORY:
            renderInfo();
            break;
        default:
            break;
    }

    window.display();
}

void GameEngine::renderMenu() {
    uiText.setCharacterSize(72);
    uiText.setFillColor(sf::Color(100, 60, 30));
    uiText.setString(sf::String(L"五子棋"));
    sf::FloatRect titleBounds = uiText.getLocalBounds();
    uiText.setPosition(sf::Vector2f(WINDOW_WIDTH * 0.5f - titleBounds.size.x * 0.5f, 90.f));
    window.draw(uiText);

    for (auto& bg : menuButtonBackgrounds) window.draw(bg);
    for (auto& btn : menuButtons) window.draw(btn);
}

void GameEngine::renderPVEConfig() {
    const float centerX = WINDOW_WIDTH * 0.5f;
    const float buttonWidth = 400.f;
    const float buttonX = centerX - buttonWidth * 0.5f;

    uiText.setCharacterSize(48);
    uiText.setFillColor(sf::Color(100, 60, 30));
    uiText.setString(sf::String(L"对战配置"));
    sf::FloatRect titleBounds = uiText.getLocalBounds();
    uiText.setPosition(sf::Vector2f(centerX - (titleBounds.position.x + titleBounds.size.x) * 0.5f, 50.f));
    window.draw(uiText);

    sf::RectangleShape btn1({buttonWidth, 50.f});
    btn1.setPosition({buttonX, 140.f});
    btn1.setFillColor(sf::Color(218, 171, 118));
    window.draw(btn1);

    uiText.setCharacterSize(18);
    uiText.setFillColor(sf::Color(60, 40, 20));

    std::wstring diffText;
    switch (aiDifficultySetting) {
        case 1: diffText = L"当前AI难度 ： 初级"; break;
        case 2: diffText = L"当前AI难度 ： 明智"; break;
        case 3: diffText = L"当前AI难度 ： 大神"; break;
        default: diffText = L"当前AI难度 ： 未知"; break;
    }
    uiText.setString(sf::String(diffText));
    sf::FloatRect diffBounds = uiText.getLocalBounds();
    uiText.setPosition(sf::Vector2f(centerX - (diffBounds.position.x + diffBounds.size.x) * 0.5f, 150.f));
    window.draw(uiText);

    sf::RectangleShape btn2({buttonWidth, 50.f});
    btn2.setPosition({buttonX, 210.f});
    btn2.setFillColor(sf::Color(218, 171, 118));
    window.draw(btn2);

    std::wstring colorText = (playerColorPref == 1) ? L"执子选择 ： 玩家执黑" : L"执子选择 ： 玩家执白";
    uiText.setString(sf::String(colorText));
    sf::FloatRect colorBounds = uiText.getLocalBounds();
    uiText.setPosition(sf::Vector2f(centerX - (colorBounds.position.x + colorBounds.size.x) * 0.5f, 220.f));
    window.draw(uiText);

    sf::RectangleShape btnStart({buttonWidth, 50.f});
    btnStart.setPosition({buttonX, 350.f});
    btnStart.setFillColor(sf::Color(214, 86, 43));
    window.draw(btnStart);

    uiText.setFillColor(sf::Color::White);
    uiText.setString(sf::String(L"进入游戏"));
    sf::FloatRect startBounds = uiText.getLocalBounds();
    uiText.setPosition(sf::Vector2f(centerX - (startBounds.position.x + startBounds.size.x) * 0.5f, 360.f));
    window.draw(uiText);
}

void GameEngine::renderGameplay() {
    // 1. 渲染棋盘
    chessboard.draw(window);

    // 🌟【独立 UI 层：卡槽背景常驻】
    // 只要处于对局状态（PVP 或 PVE），卡槽就从一开始无条件永久渲染在右侧
    if (currentState == GameState::GAME_PVP || currentState == GameState::GAME_PVE) {
        if (cardSlotSprite != nullptr) {
            window.draw(*cardSlotSprite);
        }
    }

    // 🌟【动态交互层：卡牌实体与文字】
    if (currentState == GameState::GAME_PVP || currentState == GameState::GAME_PVE) {
        if (newCardSprite != nullptr) {
            
            bool showCard = false; // 默认不显示

            // 只要玩家手里通过 checkPattern 成功抽到了卡牌，就激活卡牌显示
            if (!playerDeck.hand.empty()) {
                showCard = true;
            } 
            else {
                // 如果手里没有卡牌，开启全盘扫描作为保底机制
                int targetPiece = (currentState == GameState::GAME_PVE) ? playerColorPref : currentTurn;

                for (int r = 0; r < 15; ++r) {
                    for (int c = 0; c < 15; ++c) {
                        if (chessboard.getPiece(r, c) != targetPiece) continue;

                        int dr[] = {0, 1, 1, 1};
                        int dc[] = {1, 0, 1, -1};

                        for (int d = 0; d < 4; ++d) {
                            int r2 = r + dr[d],     c2 = c + dc[d];
                            int r3 = r + dr[d] * 2, c3 = c + dc[d] * 2;

                            if (r3 >= 0 && r3 < 15 && c3 >= 0 && c3 < 15) {
                                if (chessboard.getPiece(r2, c2) == targetPiece && 
                                    chessboard.getPiece(r3, c3) == targetPiece) {
                                    showCard = true;
                                    break;
                                }
                            }
                        }
                        if (showCard) break;
                    }
                    if (showCard) break;
                }
            }

            // 🌟 只有满足 showCard 条件时，才把卡牌和文字盖在卡槽上面渲染
            if (showCard) {
                // 【时序捕获】：当发现手牌刚刚诞生时，一瞬间重置并启动动画时钟
                if (lastHandEmpty && !playerDeck.hand.empty()) {
                    cardAnimClock.restart();
                    isAnimatingCard = true;
                    lastHandEmpty = false;
                }

                // 获取动画当前消耗的时间
                float animTime = cardAnimClock.getElapsedTime().asSeconds();

                // 动态位置计算
                sf::Vector2f cardPos;
                if (isCardAttachedToMouse) {
                    sf::Vector2i pixelPos = sf::Mouse::getPosition(window);
                    sf::Vector2f worldPos = window.mapPixelToCoords(pixelPos);
                    cardPos = worldPos - cardMouseOffset;
                } else {
                    cardPos = sf::Vector2f(1080.f, 160.f);
                }

                newCardSprite->setScale({0.18f, 0.18f});
                newCardSprite->setPosition(cardPos); 

                // 获取纹理的原始完整物理尺寸
                sf::Vector2u texSize = newCardTexture.getSize();

                // 🌟 动效一：【0.02秒阶梯式下落裁剪 —— 50阶极客精细度重构版】
                // 每 0.02 秒为一段，1.0 秒刚好完美细分为 50 段
                int segmentCount = static_cast<int>(animTime / 0.02f) + 1; 
                if (segmentCount > 50) segmentCount = 50; // 封顶 50 段，全貌展现
                if (animTime > 1.0f) segmentCount = 50;   // 超过 1 秒必定全开

                // 计算当前展现的比例 (1/50, 2/50, ... 到 50/50)
                float currentHeightPercent = segmentCount / 50.f;
                int currentRectHeight = static_cast<int>(texSize.y * currentHeightPercent);

                // 应用 50 阶微级阶梯裁剪矩形
                newCardSprite->setTextureRect(sf::IntRect({0, 0}, {static_cast<int>(texSize.x), currentRectHeight}));

                // 🌟 动效二：【1.5秒纯白褪去变原色】（完美适配 SFML 3.0 的全局常量写法）
                if (animTime <= 1.5f) {
                    // 计算纯白褪色的反向进度：一开始是 1.0（全白发光），1.5秒时衰减到 0.0（恢复原色）
                    float whiteProgress = 1.0f - (animTime / 1.5f);
                    if (whiteProgress < 0.f) whiteProgress = 0.f;

                    // 1. 先画底色支撑
                    newCardSprite->setColor(sf::Color(255, 255, 255, 255));
                    window.draw(*newCardSprite, sf::BlendAlpha);

                    // 2. 核心加算发光叠层，造成极强的纯白熔炉诞生感
                    uint8_t whiteEmit = static_cast<uint8_t>(255 * whiteProgress);
                    newCardSprite->setColor(sf::Color(whiteEmit, whiteEmit, whiteEmit, 255));
                    window.draw(*newCardSprite, sf::BlendAdd);
                } 
                else {
                    // 超过 1.5 秒，动画安全收尾
                    newCardSprite->setColor(sf::Color(255, 255, 255, 255));
                    window.draw(*newCardSprite, sf::BlendAlpha);
                    isAnimatingCard = false;
                }

                // 🌟【文字时序控制：高精快调提速版】
                if (!playerDeck.hand.empty()) {
                    const auto& currentCard = playerDeck.hand.back();
                    uiText.setFont(cardFont);

                    // 🌟【核心提速改动】：将文字的淡入周期缩短为 0.3 秒完成
                    // 只要动画一启动，文字就在 0.3 秒内迅速冲向 255 纯色状态，不再拖泥带水
                    float textFadeFactor = animTime / 0.3f; 
                    if (textFadeFactor > 1.f) textFadeFactor = 1.f;
                    uint8_t textAlpha = static_cast<uint8_t>(255 * textFadeFactor);

                    // 1. 名字在第 1 截（前 0.02 秒）露头时就渲染
                    if (segmentCount >= 1) {
                        uiText.setCharacterSize(14); 
                        uiText.setFillColor(sf::Color(255, 255, 255, textAlpha)); 
                        uiText.setString(currentCard.name);
                        // 💡 应用你微调后的最新名字精准坐标
                        uiText.setPosition({cardPos.x - 26.f, cardPos.y - 82.f}); 
                        window.draw(uiText);
                    }

                    // 2. 描述文字（处于 cardPos.y - 45.f 中上段）
                    // 在第 20 截（约 0.4 秒，卡牌一扫过该高度）提早展现，并直接应用 0.3 秒的快调淡入
                    if (segmentCount >= 20) { 
                        uiText.setCharacterSize(11);
                        uiText.setFillColor(sf::Color(230, 230, 230, textAlpha)); 
                        uiText.setString(currentCard.description);
                        // 💡 保持你之前满意的描述精准坐标
                        uiText.setPosition({cardPos.x - 44.f, cardPos.y - 45.f}); 
                        window.draw(uiText);
                    }

                    uiText.setFont(font); // 恢复主系统字体
                }
            } else {
                // 如果当前没有触发显示，重置状态标记，让下一次抽卡能重新触发动画
                lastHandEmpty = true;
            }
        }
    }

    // 🌟【修改：预落子圆环浮现逻辑】
    // 只有在对局进行中、且 AI 没有在思考、且【鼠标当前没有吸附卡牌】时，才渲染预落子圆环
    if (currentState == GameState::GAME_PVP || currentState == GameState::GAME_PVE) {
        if (!(currentState == GameState::GAME_PVE && isAiThinking)) {
            
            // 🌟 核心判断：只有在鼠标没有抓取卡牌时，才允许棋盘抓取鼠标并绘制提示环
            if (!isCardAttachedToMouse) {
                chessboard.drawHoverRing(window, currentTurn); 
            }
        }
    }

    // 2. 渲染 UI 按钮
    for (auto& rect : gameButtons) window.draw(rect);
    for (auto& text : gameButtonTexts) window.draw(text);

    // 3. 渲染计时器
    int remainingTime = static_cast<int>(std::ceil(45.f - turnClock.getElapsedTime().asSeconds()));
    if (remainingTime < 0) remainingTime = 0;
    timerText.setFillColor(remainingTime <= 10 ? sf::Color(220, 40, 40) : sf::Color(40, 120, 40));
    timerText.setString(sf::String(L"剩余时间: " + std::to_wstring(remainingTime) + L" 秒"));
    timerText.setPosition({900.f, 15.f});
    window.draw(timerText);
}

void GameEngine::renderGameOver() {
    const float centerX = WINDOW_WIDTH * 0.5f;
    chessboard.draw(window);

    sf::RectangleShape cover({static_cast<float>(WINDOW_WIDTH), static_cast<float>(WINDOW_HEIGHT)});
    cover.setFillColor(sf::Color(0, 0, 0, 180));
    window.draw(cover);

    uiText.setCharacterSize(48);
    uiText.setFillColor(sf::Color::White);
    uiText.setString(sf::String(L"对局结束"));
    sf::FloatRect titleBounds = uiText.getLocalBounds();
    uiText.setPosition(sf::Vector2f(centerX - (titleBounds.position.x + titleBounds.size.x) * 0.5f, 200.f));
    window.draw(uiText);

    sf::Text resultText(font, winReason, 32);
    resultText.setFillColor(sf::Color(240, 190, 40));
    sf::FloatRect resultBounds = resultText.getLocalBounds();
    resultText.setPosition(sf::Vector2f(centerX - (resultBounds.position.x + resultBounds.size.x) * 0.5f, 300.f));
    window.draw(resultText);

    sf::RectangleShape btnReset({200.f, 50.f});
    btnReset.setPosition({centerX - 100.f, 420.f});
    btnReset.setFillColor(sf::Color(200, 50, 50));
    window.draw(btnReset);

    uiText.setCharacterSize(18);
    uiText.setFillColor(sf::Color::White);
    uiText.setString(sf::String(L"回到主菜单"));
    sf::FloatRect resetBounds = uiText.getLocalBounds();
    uiText.setPosition(sf::Vector2f(centerX - (resetBounds.position.x + resetBounds.size.x) * 0.5f, 430.f));
    window.draw(uiText);
}

void GameEngine::renderSettings() {
    const float centerX = WINDOW_WIDTH * 0.5f;
    const float buttonWidth = 400.f;
    const float buttonX = centerX - buttonWidth * 0.5f;

    uiText.setCharacterSize(48);
    uiText.setFillColor(sf::Color(100, 60, 30));
    uiText.setString(sf::String(L"游戏设置"));
    sf::FloatRect titleBounds = uiText.getLocalBounds();
    uiText.setPosition(sf::Vector2f(centerX - (titleBounds.position.x + titleBounds.size.x) * 0.5f, 50.f));
    window.draw(uiText);

    // 专业模式按钮
    sf::RectangleShape btn1({buttonWidth, 50.f});
    btn1.setPosition({buttonX, 140.f});
    btn1.setFillColor(sf::Color(218, 171, 118));
    window.draw(btn1);

    uiText.setCharacterSize(18);
    uiText.setFillColor(sf::Color(60, 40, 20));
    std::wstring modeText = isProfessionalMode ? L"专业模式：开启" : L"专业模式：关闭";
    uiText.setString(sf::String(modeText));
    sf::FloatRect modeBounds = uiText.getLocalBounds();
    uiText.setPosition(sf::Vector2f(centerX - (modeBounds.position.x + modeBounds.size.x) * 0.5f, 150.f));
    window.draw(uiText);

    // 全屏模式按钮
    sf::RectangleShape btn2({buttonWidth, 50.f});
    btn2.setPosition({buttonX, 210.f});
    btn2.setFillColor(sf::Color(218, 171, 118));
    window.draw(btn2);

    std::wstring fullscreenText = isFullscreen ? L"全屏模式：开启" : L"全屏模式：关闭";
    uiText.setString(sf::String(fullscreenText));
    sf::FloatRect fullBounds = uiText.getLocalBounds();
    uiText.setPosition(sf::Vector2f(centerX - (fullBounds.position.x + fullBounds.size.x) * 0.5f, 220.f));
    window.draw(uiText);
}

void GameEngine::renderInfo() {
    uiText.setCharacterSize(32);
    uiText.setFillColor(sf::Color(100, 60, 30));

    std::wstring infoTitle = (currentState == GameState::HELP) ? L"游戏规则" : L"历史记录";
    uiText.setString(sf::String(infoTitle));
    uiText.setPosition({220.f, 50.f});
    window.draw(uiText);

    uiText.setCharacterSize(16);
    uiText.setFillColor(sf::Color(60, 40, 20));

    if (currentState == GameState::HELP) {
        std::wstring infoContent = L"五子棋规则：\n在 15×15 的棋盘上，\n黑白双方轮流落子，\n先达成五连者获胜。\n\n点击屏幕任意位置返回。";
        uiText.setString(sf::String(infoContent));
        uiText.setPosition({100.f, 150.f});
        window.draw(uiText);
        return;
    }

    // HISTORY 界面：列出 saves/ 下的文件（按文件名排序）
    auto dir = getSavesDir();
    std::vector<std::filesystem::path> files;
    if (std::filesystem::exists(dir)) {
        for (auto &entry : std::filesystem::directory_iterator(dir)) {
            if (entry.is_regular_file()) files.push_back(entry.path());
        }
    }
    // 以文件名倒序（最新在前）
    std::sort(files.begin(), files.end(), std::greater<>());

    historyItems.clear();

    float startX = 80.f;
    float startY = 120.f;
    float lineH = 45.f;
    int idx = 0;
    if (files.empty()) {
        std::wstring infoContent = L"暂无历史记录。\n\n\n点击屏幕任意位置返回。";
        uiText.setString(sf::String(infoContent));
        uiText.setPosition({100.f, 150.f});
        window.draw(uiText);
        return;
    }

    for (const auto& p : files) {
        if (idx >= 12) break; // 显示最多 12 条，避免超出
        sf::RectangleShape rowBg({900.f, 40.f});
        rowBg.setPosition({startX, startY + idx * lineH});
        rowBg.setFillColor(sf::Color(245,235,220));
        window.draw(rowBg);

        std::wstring nameW;
        std::string fn = p.filename().string();
        // 将 std::string 转为 wstring 简单方式（仅用于 ASCII 文件名）
        nameW.assign(fn.begin(), fn.end());

        sf::Text fnText(font, nameW, 16);
        fnText.setFillColor(sf::Color(60,40,20));
        fnText.setPosition({startX + 10.f, startY + idx * lineH + 8.f});
        window.draw(fnText);

        // 保存该行的 rect 与路径，供点击检测使用（注意：getGlobalBounds 返回 FloatRect）
        historyItems.emplace_back(rowBg.getGlobalBounds(), p);
        idx++;
    }

}

//音频
void GameEngine::initAudio() {
    // 1. 落子音效
    if (dropBuffer.loadFromFile("assets/drop.wav")) {
        dropSound = new sf::Sound(dropBuffer);
    } else {
        std::cerr << "[Error] Cannot load assets/drop.wav" << std::endl;
    }

    // 2. 背景音乐
    if (!menuMusic.openFromFile(getEngineAssetPath("assets/BGM-MainMenu.ogg"))) {
        std::cout << "[Audio Error] 无法加载主菜单音乐 assets/BGM-MainMenu.ogg" << std::endl;
    } else {
        // 🌟【修复】：SFML 3.0 中 setLoop 变成了 setLooping
        menuMusic.setLooping(true); 
        menuMusic.setVolume(50.f); 
        std::cout << "[Audio] 主菜单音乐加载成功！" << std::endl;
    }
    if (!configMusic.openFromFile(getEngineAssetPath("assets/BGM-Gap.ogg"))) {
        std::cout << "[Audio Error] 无法加载配置界面音乐 assets/BGM-Gap.ogg" << std::endl;
    } else {
        configMusic.setLooping(true); // 歌曲放完了自动循环播放
        configMusic.setVolume(50.f);  // 设置音量
        std::cout << "[Audio] 配置界面音乐 BGM-Gap 加载成功！" << std::endl;
    }
    if (!battleMusic.openFromFile(getEngineAssetPath("assets/BGM-InBattle_01.ogg"))) {
        std::cout << "[Audio Error] 无法加载对战音乐 assets/BGM-InBattle_01.ogg" << std::endl;
    } else {
        battleMusic.setLooping(true); // 歌曲放完了自动循环播放
        battleMusic.setVolume(40.f);  // 🌟 对局音乐建议音量稍微小一点（比如 40.f），防止盖过落子音效
        std::cout << "[Audio] 对战音乐 BGM-InBattle_01 加载成功！" << std::endl;
    }
    if (!winMusic.openFromFile(getEngineAssetPath("assets/BGM-Win.ogg"))) {
        std::cout << "[Audio Error] 无法加载胜利音乐 assets/BGM-Win.ogg" << std::endl;
    } else {
        winMusic.setLooping(true); // 结算界面一般也设置循环，直到玩家点击离开
        winMusic.setVolume(50.f);
        std::cout << "[Audio] 胜利音乐 BGM-Win 加载成功！" << std::endl;
    }

    // 6. 🌟【新增】加载失败结算背景音乐
    if (!failMusic.openFromFile(getEngineAssetPath("assets/BGM-Fail.ogg"))) {
        std::cout << "[Audio Error] 无法加载失败音乐 assets/BGM-Fail.ogg" << std::endl;
    } else {
        failMusic.setLooping(true);
        failMusic.setVolume(50.f);
        std::cout << "[Audio] 失败音乐 BGM-Fail 加载成功！" << std::endl;
    }
}
