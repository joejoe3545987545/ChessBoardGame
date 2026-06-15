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
    if (cardSlotSprite != nullptr) {
        // 设置圆形的半径
        detectionZone.setRadius(zoneRadius);
        
        // 💡 修复 1：SFML 3.0 的 setOrigin 必须传入 sf::Vector2f，加上大括号 {} 即可
        detectionZone.setOrigin({zoneRadius, zoneRadius});

        // 核心坐标计算：动态获取卡槽边界，精准贴在它的正下方
        sf::FloatRect readerBounds = cardSlotSprite->getGlobalBounds();
        
        // 💡 修复 2：SFML 3.0 的 sf::Rect 成员全面改为 position 和 size
        // left -> position.x ； width -> size.x
        float centerX = 640;      // 水平居中
        // top -> position.y ； height -> size.y
        float centerY = 100; 

        // 💡 修复 3：SFML 3.0 的 setPosition 同样必须用大括号 {} 括起来
        detectionZone.setPosition({centerX, centerY});

        // 开发调试用颜色：半透明绿，方便等会儿测试看位置
        detectionZone.setFillColor(sf::Color(0, 0, 0, 0)); 
        detectionZone.setOutlineThickness(0.f);
        detectionZone.setOutlineColor(sf::Color::Green);
    } else {
        std::cerr << "[UI Error] 卡槽精灵未初始化，无法设置检测区域。" << std::endl;
    }
    // 🌟 第二卡槽（卡槽 1 正下方 220px，当前空置备用）
    if (cardSlotTexture2.loadFromFile(getEngineAssetPath("assets/CardSlot.png"))) {
        cardSlotTexture2.setSmooth(true);
        cardSlotSprite2 = new sf::Sprite(cardSlotTexture2);
        sf::Vector2u slot2Size = cardSlotTexture2.getSize();
        cardSlotSprite2->setOrigin({slot2Size.x / 2.f, slot2Size.y / 2.f});
        cardSlotSprite2->setScale({0.25f, 0.25f});
        cardSlotSprite2->setPosition({1080.f, 486.f});  // 卡槽 1 下方 320px
    }
    // ============================================================================
    // 🌟【修正版】：完美适配 SFML 3.0 规范的读卡器夹层素材初始化
    // ============================================================================
    cardReaderBottomTexture = std::make_unique<sf::Texture>();
    // 💡 1. 必须先通过 loadFromFile 把纹理加载好
    if (cardReaderBottomTexture->loadFromFile(getEngineAssetPath("assets/CardReader_Bottom.png"))) {
        cardReaderBottomTexture->setSmooth(true);
        
        // 💡 2. 核心修正：创建 Sprite 的时候，直接把已经加载好的纹理 (*cardReaderBottomTexture) 塞进去！
        cardReaderBottomSprite = std::make_unique<sf::Sprite>(*cardReaderBottomTexture);
        
        // 设置中心原点、缩放与位置
        sf::Vector2u size = cardReaderBottomTexture->getSize();
        cardReaderBottomSprite->setOrigin({size.x / 2.f, size.y / 2.f});
        cardReaderBottomSprite->setScale({0.4f, 0.4f});
        cardReaderBottomSprite->setPosition({640.f, 0.f}); 
        
        std::cout << "[🎉 Success] 读卡器底座 CardReader_Bottom 初始化成功！" << std::endl;
    } else {
        std::cerr << "[❌ UI Error] 无法加载素材 assets/CardReader_Bottom.png" << std::endl;
    }

    cardReaderTopTexture = std::make_unique<sf::Texture>();
    // 💡 3. 同理，顶层也必须先 loadFromFile
    if (cardReaderTopTexture->loadFromFile(getEngineAssetPath("assets/CardReader_Top.png"))) {
        cardReaderTopTexture->setSmooth(true);
        
        // 💡 4. 核心修正：创建 Sprite 的时候，直接传入顶层纹理 (*cardReaderTopTexture)
        cardReaderTopSprite = std::make_unique<sf::Sprite>(*cardReaderTopTexture);
        
        // 保持严丝合缝的对齐
        sf::Vector2u size = cardReaderTopTexture->getSize();
        cardReaderTopSprite->setOrigin({size.x / 2.f, size.y / 2.f});
        cardReaderTopSprite->setScale({0.4f, 0.4f});
        cardReaderTopSprite->setPosition({640.f, 0.f}); 
        
        std::cout << "[🎉 Success] 读卡器滑盖 CardReader_Top 初始化成功！" << std::endl;
    } else {
        std::cerr << "[❌ UI Error] 无法加载素材 assets/CardReader_Top.png" << std::endl;
    }
    std::cout << "[Info] GameEngine constructed." << std::endl;
}

GameEngine::~GameEngine()
{
    menuMusic.stop();
    configMusic.stop();
    battleMusic.stop();
    battleMusic02.stop();
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
    if (cardSlotSprite2) delete cardSlotSprite2;
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

                    // 🔒 卡牌动画期间禁止棋盘落子
                    if (isBusyAnimating) continue;

                    // 只有没点中任何 HUD (hudIntercepted == false)，才去处理常规棋盘落子逻辑
                    int lastRow = -1, lastCol = -1;

                    // 🌟【行动点前置拦截】：在真正让棋盘落子前，先检查当前玩家手里还有没有允许落子的行动点
                    if (!hasValidActionPoint(true)) {
                        std::cout << "[ActionPoint] 拦截落子：当前玩家没有包含【允许下棋】判定的行动点！" << std::endl;
                        // 既然不能下棋，这次点击事件直接被作废拦截
                        continue; 
                    }

                    // 行动点检查通过，允许调用棋盘的原生落子逻辑
                    if (chessboard.handleMouseClick(mousePos, currentTurn, lastRow, lastCol)) {
                        
                        // 🌟【行动点核销】：落子物理成功！核销扣除 1 个支持下棋的行动点
                        consumeActionPoint(true);

                        // 音频处理
                        dropSound->play();
                        std::cout << "[Audio] Drop sound played." << std::endl;

                        // 点击成功立刻加入手牌数据系统（三子或四子连线抽卡）
                        int pattern = chessboard.checkPattern(lastRow, lastCol);
                        if (pattern == 3 || pattern == 4) {
                            playerDeck.drawCard();
                            handSlotAssign.push_back(1);  // 新卡默认放入槽 1
                            newCardJustDrawn = true;      // 触发出生动画
                        }
                        // 🎵 四子连星 → 危机触发，延长 battle_02 或作为切歌入口
                        if (pattern == 4) {
                            if (battleMusicState == BattleMusicState::BATTLE_02) {
                                addCrisisTime(30.f);
                            } else if (battleMusicState == BattleMusicState::NORMAL) {
                                triggerBattleMusic02();
                            }
                        }

                        // 记录历史
                        chessboard.recordMove(lastRow, lastCol, currentTurn);

                        // 检查胜利
                        if (chessboard.checkWin(lastRow, lastCol)) {
                            winReason = (currentTurn == 1) ? L"黑方达成五连，获得了胜利！" : L"白方达成五连，获得了胜利！";
                            isGameOver = true;
                            currentState = GameState::GAME_OVER;
                            std::cout << "[Info] Player " << currentTurn << " wins." << std::endl;
                        } 
                        else {
                            // 🌟【行动点核心替换点】：删掉你原先落子后强行换人和重置时钟的代码！
                            // 移交给我们的总体结算中心。结算中心会判断：点数清零了才换人刷新；点数未清零则锁死倒计时让玩家继续行动。
                            settleActionPoints();

                            // 🌟【AI适配钩子】：如果结算中心发现行动点真的空了，且已经自动切换到机器回合，则激活AI思考
                            if (currentState == GameState::GAME_PVE && currentTurn != playerColorPref && currentTurnActionPoints.size() == 1) {
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
            initActionPointsForTurn();
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
            initActionPointsForTurn();
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
    // 🔒 卡牌动画期间禁止抓取卡牌
    if (!playerDeck.hand.empty() && newCardSprite != nullptr && !isBusyAnimating) {
        sf::FloatRect cardClickRect;
        
        // 严格锁定你指定的卡牌物理宽高
        sf::Vector2f cardSize(144.f, 200.f);
        sf::Vector2f mouseClickPos(static_cast<float>(mousePos.x), static_cast<float>(mousePos.y));
        // 各槽独立顶卡索引
        int slot1Top = -1, slot2Top = -1;
        for (size_t i = 0; i < handSlotAssign.size(); ++i) {
            if (handSlotAssign[i] == 1) slot1Top = static_cast<int>(i);
            else                        slot2Top = static_cast<int>(i);
        }

        if (isCardAttachedToMouse) {
            // A. 吸附态 — 卡牌跟随鼠标
            sf::Vector2i pixelPos = sf::Mouse::getPosition(window);
            sf::Vector2f worldPos = window.mapPixelToCoords(pixelPos);
            sf::Vector2f currentCardCenter = worldPos - cardMouseOffset;
            cardClickRect = sf::FloatRect(
                {currentCardCenter.x - 72.f, currentCardCenter.y - 100.f}, cardSize);
        }

        // 🔍 非吸附态拾取：先槽2后槽1，显式计算无lambda
        if (!isCardAttachedToMouse) {
            // ── 槽2顶卡 ──
            if (slot2Top >= 0) {
                int stack2 = 0;
                for (int j = 0; j < slot2Top; ++j)
                    if (handSlotAssign[j] == 2) stack2++;
                float x2 = 1008.f + stack2 * 5.f;
                float y2 = 380.f  + stack2 * 5.f;
                sf::FloatRect r2({x2, y2}, cardSize);
                if (r2.contains(mouseClickPos)) {
                    attachedCardIndex = slot2Top;
                    float cx2 = 1080.f + stack2 * 5.f;
                    float cy2 = 480.f  + stack2 * 5.f;
                    cardMouseOffset = mouseClickPos - sf::Vector2f(cx2, cy2);
                    isCardAttachedToMouse = true;
                    isReturningToSlot = false;
                    std::cout << "[Card UI] 吸附槽2顶卡" << std::endl;
                    return true;
                }
            }
            // ── 槽1顶卡 ──
            if (slot1Top >= 0) {
                int stack1 = 0;
                for (int j = 0; j < slot1Top; ++j)
                    if (handSlotAssign[j] == 1) stack1++;
                float x1 = 1008.f + stack1 * 5.f;
                float y1 = 60.f   + stack1 * 5.f;
                sf::FloatRect r1({x1, y1}, cardSize);
                if (r1.contains(mouseClickPos)) {
                    attachedCardIndex = slot1Top;
                    float cx1 = 1080.f + stack1 * 5.f;
                    float cy1 = 160.f  + stack1 * 5.f;
                    cardMouseOffset = mouseClickPos - sf::Vector2f(cx1, cy1);
                    isCardAttachedToMouse = true;
                    isReturningToSlot = false;
                    std::cout << "[Card UI] 吸附槽1顶卡" << std::endl;
                    return true;
                }
            }
        }

        // 吸附态点击：松开卡牌
        if (isCardAttachedToMouse && cardClickRect.contains(mouseClickPos)) {
            {
                // 2. 🌟【核心改动】从【鼠标吸附】 ➡️ 【松开卡牌】
                
                // 📐 计算当前点击位置到圆形区域中心 (640, 100) 的距离
                float dx = mouseClickPos.x - 640.f;
                float dy = mouseClickPos.y - 100.f;
                float distance = std::sqrt(dx * dx + dy * dy);

                if (distance <= zoneRadius) {
                    // 🌟【行动点前置拦截】：在触发湮灭（即出牌）前，检查是否拥有"允许出牌"的行动点
                    if (!hasValidActionPoint(false)) {
                        // 没有出牌权限 → 拒绝湮灭，改为弹回卡槽
                        std::cout << "[ActionPoint] 拦截出牌：当前玩家没有包含【允许出牌】判定的行动点！" << std::endl;
                        isCardAttachedToMouse = false;
                        isReturningToSlot = true;
                        returnStartPos = newCardSprite->getPosition();
                        returnDelayClock.restart();
                        return true; // 拦截点击
                    }

                    // 🔮 分支甲：点击在圆内 ➡️ 触发湮灭仪式
                    isCardAttachedToMouse = false;

                    annihilateState = CardAnnihilateState::PAUSE_BEFORE; // 激活阶段1：原地停顿0.2s
                    annihilateClock.restart();                           // 启动湮灭专用的独立时钟

                    // ⏸️ 暂停回合计时器（卡牌动画期间不计时）
                    if (!isTurnPaused) {
                        isTurnPaused = true;
                        pauseClock.restart();
                    }

                    // 🎵 触发战斗音乐切换到 battle_02
                    triggerBattleMusic02();

                    // 🔒 锁定玩家操作和 AI（动画期间禁止一切行动）
                    isBusyAnimating = true;

                    std::cout << "[Card UI] 点击在圆内！释放卡牌并触发湮灭仪式。" << std::endl;
                }
                else {
                    // 🔮 分支乙：点击在圆外 ➡️ 检查是否落在槽位存放区
                    sf::FloatRect slot1Zone({1008.f, 60.f},  cardSize);
                    sf::FloatRect slot2Zone({1008.f, 380.f}, cardSize);

                    isCardAttachedToMouse = false;
                    isReturningToSlot = true;
                    returnStartPos = newCardSprite->getPosition();
                    returnDelayClock.restart();

                    if (slot2Zone.contains(mouseClickPos) && attachedCardIndex >= 0) {
                        Card moved = playerDeck.hand[attachedCardIndex];
                        playerDeck.hand.erase(playerDeck.hand.begin() + attachedCardIndex);
                        handSlotAssign.erase(handSlotAssign.begin() + attachedCardIndex);
                        playerDeck.hand.push_back(moved);
                        handSlotAssign.push_back(2);
                        attachedCardIndex = static_cast<int>(playerDeck.hand.size()) - 1;
                        std::cout << "[Card UI] 存入槽 2" << std::endl;
                    } else if (slot1Zone.contains(mouseClickPos) && attachedCardIndex >= 0) {
                        Card moved = playerDeck.hand[attachedCardIndex];
                        playerDeck.hand.erase(playerDeck.hand.begin() + attachedCardIndex);
                        handSlotAssign.erase(handSlotAssign.begin() + attachedCardIndex);
                        playerDeck.hand.push_back(moved);
                        handSlotAssign.push_back(1);
                        attachedCardIndex = static_cast<int>(playerDeck.hand.size()) - 1;
                        std::cout << "[Card UI] 存入槽 1" << std::endl;
                    }
                }
            }

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
            newCardJustDrawn = false;
            handSlotAssign.clear();
            attachedCardIndex = -1;
            isCardAttachedToMouse = false;
            if (newCardSprite) newCardSprite->setPosition({1080.f, 160.f}); // 复位卡牌精灵到槽 1
            battleMusic.stop();
            battleMusic.play();
            battleMusic02.stop();                                // 切断 battle_02
            battleMusicState = BattleMusicState::NORMAL;         // 重置音乐状态机
            battleMusic.setVolume(BATTLE_VOLUME);                // 确保音量正常
            isBusyAnimating = false;                             // 解锁动画锁
            winMusic.stop();
            playerDeck.resetDeck();
            
            // 🌟【确切位置 1】：在此处精准添加，保证局内主动点击“重新开局”时隐藏卡牌底图

            currentTurn = 1;
            initActionPointsForTurn();          // 🌟 重新开局时初始化行动点池
            showcaseState = CardShowcaseState::NONE; // 🌟 重置卡牌展示状态
            isAiThinking = false;
            isGameOver = false;
            savedThisGame = false;
            turnClock.restart();
            turnTimePaused = 0.f;  // 重置暂停累计
            isTurnPaused   = false;
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
        // 先停掉非战斗音乐
        if (static_cast<int>(menuMusic.getStatus()) == 2) menuMusic.stop();
        if (static_cast<int>(configMusic.getStatus()) == 2) configMusic.stop();
        if (static_cast<int>(winMusic.getStatus()) == 2)    winMusic.stop();
        if (static_cast<int>(failMusic.getStatus()) == 2)   failMusic.stop();

        // ── 战斗音乐状态机 ──
        switch (battleMusicState) {
            case BattleMusicState::NORMAL:
                if (static_cast<int>(battleMusic.getStatus()) != 2) battleMusic.play();
                battleMusic.setVolume(BATTLE_VOLUME);
                break;

            case BattleMusicState::SWITCHING_TO_02: {
                float progress = crossfadeClock.getElapsedTime().asSeconds() / CROSSFADE_DURATION;
                if (progress >= 1.f) {
                    battleMusic.setVolume(0.f);
                    battleMusic02.setVolume(BATTLE_VOLUME);
                    battleMusicState = BattleMusicState::BATTLE_02;
                    battle02Remaining = BATTLE_02_BASE_TIME;
                    battleTimer.restart();
                } else {
                    battleMusic.setVolume(BATTLE_VOLUME * (1.f - progress));
                    battleMusic02.setVolume(BATTLE_VOLUME * progress);
                }
                if (static_cast<int>(battleMusic.getStatus()) != 2) battleMusic.play();
                if (static_cast<int>(battleMusic02.getStatus()) != 2) battleMusic02.play();
                break;
            }

            case BattleMusicState::BATTLE_02: {
                if (battleTimer.getElapsedTime().asSeconds() >= battle02Remaining) {
                    battleMusicState = BattleMusicState::SWITCHING_TO_01;
                    crossfadeClock.restart();
                }
                if (static_cast<int>(battleMusic02.getStatus()) != 2) battleMusic02.play();
                break;
            }

            case BattleMusicState::SWITCHING_TO_01: {
                float progress = crossfadeClock.getElapsedTime().asSeconds() / CROSSFADE_DURATION;
                if (progress >= 1.f) {
                    battleMusic02.setVolume(0.f);
                    battleMusic.setVolume(BATTLE_VOLUME);
                    battleMusicState = BattleMusicState::NORMAL;
                } else {
                    battleMusic02.setVolume(BATTLE_VOLUME * (1.f - progress));
                    battleMusic.setVolume(BATTLE_VOLUME * progress);
                }
                if (static_cast<int>(battleMusic.getStatus()) != 2) battleMusic.play();
                if (static_cast<int>(battleMusic02.getStatus()) != 2) battleMusic02.play();
                break;
            }
        }
    }
    else if (currentState == GameState::GAME_OVER) {
        // 🌟【结算界面音频逻辑】
        if (static_cast<int>(menuMusic.getStatus()) == 2)   menuMusic.stop();
        if (static_cast<int>(configMusic.getStatus()) == 2) configMusic.stop();
        if (static_cast<int>(battleMusic.getStatus()) == 2) {
            battleMusic.stop();
            std::cout << "[Audio] 对局结束，切断对战音乐" << std::endl;
        }
        if (static_cast<int>(battleMusic02.getStatus()) == 2) battleMusic02.stop();
        battleMusicState = BattleMusicState::NORMAL;   // 重置音乐状态
        isBusyAnimating = false;                       // 解锁动画锁

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

    float elapsedSeconds = getEffectiveTurnTime();
    if (elapsedSeconds >= 45.f) {
        winReason = (currentTurn == 1) ? L"黑方思考超时，白方获胜！" : L"白方思考超时，黑方获胜！";
        isGameOver = true;
        currentState = GameState::GAME_OVER; // 🌟 触发游戏结束状态
        return;
    }

    if (currentState == GameState::GAME_PVE && currentTurn != playerColorPref) {
        // 🔒 卡牌动画期间暂停 AI（不开始新思考，也不落子）
        if (isBusyAnimating) return;

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
        if (cardSlotSprite2 != nullptr) {
            window.draw(*cardSlotSprite2);
        }
    }

    // ============================================================================
    // 🌟【三明治夹层渲染 - 第 1 层：Bottom 底座】
    // 在卡牌渲染之前，先渲染读卡器的底层，这样卡牌就会叠在读卡器底座的上面
    // ============================================================================
    if (currentState == GameState::GAME_PVP || currentState == GameState::GAME_PVE) {
        if (cardReaderBottomSprite != nullptr) {
            window.draw(*cardReaderBottomSprite);
        }
    }

    // 🌟【动态交互层：多卡堆叠渲染】
    if (currentState == GameState::GAME_PVP || currentState == GameState::GAME_PVE) {
        if (newCardSprite != nullptr && !playerDeck.hand.empty()) {

            // 【时序捕获】：抽卡标记触发出生动画（比 handSize 变化更可靠）
            if (newCardJustDrawn) {
                cardAnimClock.restart();
                isAnimatingCard = true;
                newCardJustDrawn = false;
            }

            // 获取出生动画当前消耗的时间（仅顶卡使用）
            float animTime = cardAnimClock.getElapsedTime().asSeconds();

            // ── 双槽分组渲染 ──
            // 活跃卡索引：当前被状态机操控的卡（吸附中 / 湮灭中 / 默认顶卡）
            int activeIdx;
            if (isCardAttachedToMouse)          activeIdx = attachedCardIndex;
            else if (annihilateState != CardAnnihilateState::NONE) activeIdx = attachedCardIndex;
            else if (isReturningToSlot)         activeIdx = attachedCardIndex;
            else                                activeIdx = static_cast<int>(playerDeck.hand.size() - 1);
            sf::Vector2f targetPos;
            sf::Vector2f preLoopPos = newCardSprite->getPosition();

            for (int slot = 1; slot <= 2; ++slot) {
                float baseX = 1080.f;
                float baseY = (slot == 1) ? 160.f : 480.f;
                int stackIdx = 0;
                for (size_t i = 0; i < playerDeck.hand.size(); ++i) {
                    if (handSlotAssign[i] != slot) continue;
                    sf::Vector2f offset(stackIdx * 5.f, stackIdx * 5.f);
                    sf::Vector2f cardPos(baseX + offset.x, baseY + offset.y);

                    if (static_cast<int>(i) == activeIdx) {
                        targetPos = cardPos; // 活跃卡回归点
                    } else {
                        // 静态卡牌：完整渲染 + 槽顶卡文字
                        newCardSprite->setScale({0.18f, 0.18f});
                        newCardSprite->setPosition(cardPos);
                        sf::Vector2u texSize = newCardTexture.getSize();
                        newCardSprite->setTextureRect(sf::IntRect({0, 0}, {static_cast<int>(texSize.x), static_cast<int>(texSize.y)}));
                        newCardSprite->setColor(sf::Color(255, 255, 255, 255));
                        window.draw(*newCardSprite, sf::BlendAlpha);
                        // 每张卡都渲染文字，渲染顺序天然保证上层遮下层
                        {
                            const auto& sc = playerDeck.hand[i];
                            uiText.setFont(cardFont);
                            uiText.setCharacterSize(14);
                            uiText.setFillColor(sf::Color(255, 255, 255, 255));
                            uiText.setString(sc.name);
                            uiText.setPosition({cardPos.x - 26.f, cardPos.y - 82.f});
                            window.draw(uiText);
                            uiText.setCharacterSize(11);
                            uiText.setFillColor(sf::Color(230, 230, 230, 255));
                            uiText.setString(sc.description);
                            uiText.setPosition({cardPos.x - 44.f, cardPos.y - 45.f});
                            window.draw(uiText);
                            uiText.setFont(font);
                        }
                    }
                    stackIdx++;
                }
            }

            // 恢复精灵到活跃卡的正确位置
            if (isCardAttachedToMouse) {
                sf::Vector2i pix = sf::Mouse::getPosition(window);
                sf::Vector2f wpos = window.mapPixelToCoords(pix);
                newCardSprite->setPosition(wpos - cardMouseOffset);
            } else {
                newCardSprite->setPosition(preLoopPos);
            }

            // ── 顶卡：完整状态机 + 动画 + 文字 ──

            // ============================================================
            // 🌟【新复合精调版】：支持鼠标吸附、弹回以及全新的湮灭仪式计算
            // ============================================================
            {
            sf::Vector2f cardPos;

                if (isCardAttachedToMouse) {
                    // 1. 正常吸附鼠标状态
                    sf::Vector2i pixelPos = sf::Mouse::getPosition(window);
                    sf::Vector2f worldPos = window.mapPixelToCoords(pixelPos);
                    cardPos = worldPos - cardMouseOffset;
                } 
                else if (annihilateState != CardAnnihilateState::NONE) {
                    // 🌟【核心插入：卡牌多阶段湮灭仪式状态机驱动】
                    float elapsed = annihilateClock.getElapsedTime().asSeconds();
                    cardPos = newCardSprite->getPosition(); // 默认保持当前帧的物理位置

                    switch (annihilateState) {
                        case CardAnnihilateState::PAUSE_BEFORE:
                            // 【阶段 1】：点中后，在放开的原地完美死寂停顿 0.2 秒
                            if (elapsed >= 0.2f) {
                                annihilateState = CardAnnihilateState::MOVE_TO_200;
                                annihilateClock.restart(); // 为下一阶段重新计时
                                std::cout << "[Annihilate] 阶段 1 结束，落向 (640, 200)" << std::endl;
                            }
                            break;

                        case CardAnnihilateState::MOVE_TO_200: {
                            // 🎯 设定第二阶段的目标过渡坐标
                            sf::Vector2f targetAnnihilatePos(640.f, 200.f);

                            // 🚀 核心缓动算法：先快后慢的指数级缓出曲线 (与飞回卡槽的因子 0.07f 完美同步)
                            cardPos.x = cardPos.x + (targetAnnihilatePos.x - cardPos.x) * 0.07f;
                            cardPos.y = cardPos.y + (targetAnnihilatePos.y - cardPos.y) * 0.07f;

                            // 📐 实时计算当前卡牌与目标点 (640, 200) 的绝对像素距离
                            float dist = std::sqrt(std::pow(targetAnnihilatePos.x - cardPos.x, 2) + 
                                                   std::pow(targetAnnihilatePos.y - cardPos.y, 2));

                            // 当绝对距离极其接近（小于 0.5 像素）时，宣告彻底就位
                            if (dist < 0.5f) {
                                cardPos = targetAnnihilatePos; // 强行精准落位消除微小误差
                                
                                // 瞬间切入下一阶段：原地停顿 0.5 秒
                                annihilateState = CardAnnihilateState::PAUSE_AFTER;
                                annihilateClock.restart(); // ⏱️ 为接下来的 0.5 秒停顿期重新开始计时
                                
                                std::cout << "[Annihilate] 丝滑缓动成功！已抵达 (640, 200)，开始静止 0.5 秒" << std::endl;
                            }
                            break;
                        }

                        case CardAnnihilateState::PAUSE_AFTER:
                            // 【阶段 3】：在 (640, 200) 稳稳卡住停顿 0.5 秒
                            cardPos = {640.f, 200.f};
                            if (elapsed >= 0.5f) {
                                annihilateState = CardAnnihilateState::MOVE_TO_ZERO;
                                annihilateClock.restart();
                                std::cout << "[Annihilate] 阶段 3 结束，启动虚无上升序列" << std::endl;
                            }
                            break;

                        case CardAnnihilateState::MOVE_TO_ZERO: {
                            // 【阶段 4】：缓慢向上移动到虚无边界 (640, 70)
                            float moveSpeed = 45.f; // 保持你满意的慢速
                            float nextY = cardPos.y - (moveSpeed * 0.016f); // 采用 60 帧近似步长
                            cardPos = {640.f, nextY};

                            // 边界判定：一旦触及或超越顶端 Y = 70.f
                            if (nextY <= 70.f) {
                                cardPos = {640.f, 70.f}; // 🌟 精准锁死在 70.f 的位置，不再往上飘
                                
                                // 🌟 转向全新阶段 5：在这里开始最终的定格
                                annihilateState = CardAnnihilateState::PAUSE_FINAL; 
                                annihilateClock.restart(); // ⏱️ 重新开始为 0.5 秒定格时间计时
                                std::cout << "[Annihilate] 阶段 4 结束，到达 Y=70，开始最终定格 0.5 秒..." << std::endl;
                            }
                            break;
                        }

                        case CardAnnihilateState::PAUSE_FINAL: {
                            cardPos = {640.f, 70.f}; 
                            
                            // ============================================================================
                            // 🔮 湮灭特效渲染 — 连续插值，无帧率依赖
                            //    出生：0→50段出现 + 极亮→正常(1.5s)
                            //    湮灭：连续消失 + 正常→极亮(1.0s)
                            // ============================================================================
                            float disappearTime = elapsed;
                            sf::Vector2u texSize = newCardTexture.getSize();

                            // ⏱️ 1. 湮灭裁剪：连续插值替代固定50段量化，消除帧率拍频闪烁
                            //    纹理高 1111px，60Hz 每帧 ~18.5px，144Hz 每帧 ~7.7px，始终平滑
                            float heightPercent = 1.0f - (disappearTime / 1.0f);
                            if (heightPercent < 0.f) heightPercent = 0.f;
                            if (disappearTime > 0.98f) heightPercent = 0.f; // 最后 20ms 提前隐藏，消除残影闪烁
                            int currentRectHeight = static_cast<int>(texSize.y * heightPercent);

                            newCardSprite->setScale({0.18f, 0.18f});
                            newCardSprite->setPosition(cardPos);
                            newCardSprite->setTextureRect(sf::IntRect({0, 0}, {static_cast<int>(texSize.x), currentRectHeight}));

                            // ⏱️ 2. 纯白焚化辉光：原色 → 极亮，1秒内线性递增（镜像出生动画的 1.5s 衰减）
                            float whiteProgress = disappearTime / 1.0f;   // 0.0 → 1.0
                            if (whiteProgress > 1.f) whiteProgress = 1.f;

                            // ⏱️ 尾部软淡出：最后 0.4s 基底 alpha 线性衰减，最后 20ms 强制归零
                            float baseAlpha = 1.0f;
                            if (disappearTime > 0.98f) baseAlpha = 0.f;
                            else if (disappearTime > 0.6f) {
                                baseAlpha = 1.0f - (disappearTime - 0.6f) / 0.4f;
                                if (baseAlpha < 0.f) baseAlpha = 0.f;
                            }
                            uint8_t finalAlpha = static_cast<uint8_t>(255 * baseAlpha);

                            // 1️⃣ 基底：尾部渐弱融入辉光
                            newCardSprite->setColor(sf::Color(255, 255, 255, finalAlpha));
                            window.draw(*newCardSprite, sf::BlendAlpha);

                            // 2️⃣ 白光叠层：同步尾部收缩，避免残留白影
                            uint8_t whiteEmit = static_cast<uint8_t>(255 * whiteProgress * baseAlpha);
                            newCardSprite->setColor(sf::Color(whiteEmit, whiteEmit, whiteEmit, 255));
                            window.draw(*newCardSprite, sf::BlendAdd);

                            // ⏱️ 3. 文字：随裁剪进度独立淡出（0.8s 内完全消失，略快于画面）
                            if (!playerDeck.hand.empty() && activeIdx >= 0) {
                                const auto& currentCard = playerDeck.hand[activeIdx];
                                uiText.setFont(cardFont);

                                float textFade = 1.0f - (disappearTime / 0.8f);
                                if (textFade < 0.f) textFade = 0.f;
                                uint8_t textAlpha = static_cast<uint8_t>(255 * textFade);

                                // 名字：剩余高度 >2% 时可见（连续判定，无帧率依赖）
                                if (heightPercent > 0.02f) {
                                    uiText.setCharacterSize(14);
                                    uiText.setFillColor(sf::Color(255, 255, 255, textAlpha));
                                    uiText.setString(currentCard.name);
                                    uiText.setPosition({cardPos.x - 26.f, cardPos.y - 82.f});
                                    window.draw(uiText);
                                }

                                // 描述：剩余高度 >40% 时可见
                                if (heightPercent > 0.40f) {
                                    uiText.setCharacterSize(11);
                                    uiText.setFillColor(sf::Color(230, 230, 230, textAlpha));
                                    uiText.setString(currentCard.description);
                                    uiText.setPosition({cardPos.x - 44.f, cardPos.y - 45.f});
                                    window.draw(uiText);
                                }
                                uiText.setFont(font);
                            }
                            // ============================================================================

                            // ⏱️ 动画总长为 1.0 秒，湮灭完成 → 启动卡牌效果展示
                            if (elapsed >= 1.0f) {
                                if (!playerDeck.hand.empty() && attachedCardIndex >= 0) {
                                    // 🌟 捕获卡牌数据并按索引移出手牌
                                    showcasedCard = playerDeck.hand[attachedCardIndex];
                                    playerDeck.hand.erase(playerDeck.hand.begin() + attachedCardIndex);
                                    handSlotAssign.erase(handSlotAssign.begin() + attachedCardIndex);
                                    attachedCardIndex = -1;  // 重置吸附索引
                                    std::cout << "[Annihilate] 燃尽并完全淡出，卡牌正式完全湮灭！" << std::endl;

                                    // 🌟 启动效果展示状态机（PAUSE → APPEAR → DISPLAY → FADE_OUT → 触发效果）
                                    showcaseState = CardShowcaseState::PAUSE;
                                    showcaseClock.restart();
                                    newCardSprite->setPosition({640.f, 360.f}); // 消除读卡器残影
                                }
                                annihilateState = CardAnnihilateState::NONE; // 关闭湮灭状态机
                            }
                            break;
                        }

                        default:
                            break;
                    }
                }
                else if (isReturningToSlot) {
                    // 2. 激活弹回动画状态机中
                    float returnElapsed = returnDelayClock.getElapsedTime().asSeconds();
                    
                    if (returnElapsed < 0.15f) {
                        cardPos = returnStartPos;
                    } 
                    else {
                        sf::Vector2f currentPos = newCardSprite->getPosition();
                        cardPos.x = currentPos.x + (targetPos.x - currentPos.x) * 0.07f;
                        cardPos.y = currentPos.y + (targetPos.y - currentPos.y) * 0.07f;

                        float dist = std::sqrt(std::pow(targetPos.x - cardPos.x, 2) + std::pow(targetPos.y - cardPos.y, 2));
                        if (dist < 0.5f) {
                            cardPos = targetPos;
                            isReturningToSlot = false; 
                        }
                    }
                } 
                else {
                    // 3. 常态静止稳定地常驻在卡槽中央
                    cardPos = targetPos;
                }
                
                if (annihilateState != CardAnnihilateState::PAUSE_FINAL) {
                // 更新卡牌位置并锁死 0.18 的微缩缩放比例
                newCardSprite->setScale({0.18f, 0.18f});
                newCardSprite->setPosition(cardPos); 

                // 获取纹理的原始完整物理尺寸
                sf::Vector2u texSize = newCardTexture.getSize();

                // 🌟 动效一：连续插值替代固定50段量化，消除帧率拍频闪烁
                float currentHeightPercent = animTime / 1.0f;  // 0.0 → 1.0 连续增长
                if (currentHeightPercent > 1.f) currentHeightPercent = 1.f;
                if (animTime > 1.0f) currentHeightPercent = 1.f;

                // ⏳ 弹回阶段过了停顿期后，强制全显
                if (isReturningToSlot && returnDelayClock.getElapsedTime().asSeconds() >= 0.3f) {
                    currentHeightPercent = 1.f;
                }
                int currentRectHeight = static_cast<int>(texSize.y * currentHeightPercent);

                // 应用 50 阶微级阶梯裁剪矩形
                newCardSprite->setTextureRect(sf::IntRect({0, 0}, {static_cast<int>(texSize.x), currentRectHeight}));

                // 🌟 动效二：【1.5秒纯白褪去变原色】
                if (animTime <= 1.5f) {
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
                    newCardSprite->setColor(sf::Color(255, 255, 255, 255));
                    window.draw(*newCardSprite, sf::BlendAlpha);
                    isAnimatingCard = false;
                }

                // 🌟【文字时序控制】显示活跃卡文字（吸附/湮灭/默认）
                if (!playerDeck.hand.empty() && activeIdx >= 0) {
                    const auto& currentCard = playerDeck.hand[activeIdx];
                    uiText.setFont(cardFont);

                    float textFadeFactor = animTime / 0.3f; 
                    if (textFadeFactor > 1.f) textFadeFactor = 1.f;
                    uint8_t textAlpha = static_cast<uint8_t>(255 * textFadeFactor);

                    // 1. 名字：剩余高度 >2% 时渲染（连续判定，无帧率依赖）
                    if (currentHeightPercent > 0.02f) {
                        uiText.setCharacterSize(14);
                        uiText.setFillColor(sf::Color(255, 255, 255, textAlpha));
                        uiText.setString(currentCard.name);
                        uiText.setPosition({cardPos.x - 26.f, cardPos.y - 82.f});
                        window.draw(uiText);
                    }

                    // 2. 描述文字：剩余高度 >40% 时渲染
                    if (currentHeightPercent > 0.40f) {
                        uiText.setCharacterSize(11);
                        uiText.setFillColor(sf::Color(230, 230, 230, textAlpha));
                        uiText.setString(currentCard.description);
                        uiText.setPosition({cardPos.x - 44.f, cardPos.y - 45.f});
                        window.draw(uiText);
                    }

                    uiText.setFont(font); // 恢复主系统字体
                }
                }
            } // 顶卡状态机作用域结束
        }
    }

    // ============================================================================
    // 🌟 卡牌效果展示动画（湮灭完成后，画面中央展示）
    //    流程: PAUSE(0.5s) → APPEAR(1.2s) → DISPLAY(2s) → FADE_OUT(1s) → 触发效果
    // ============================================================================
    if (showcaseState != CardShowcaseState::NONE && newCardSprite != nullptr) {
        float t = showcaseClock.getElapsedTime().asSeconds();
        sf::Vector2f centerPos(640.f, 360.f);
        sf::Vector2u texSize = newCardTexture.getSize();
        const float TARGET_SCALE = 0.38f;
        const float START_SCALE  = 0.05f;
        const float NORMAL_SCALE = 0.18f;              // 卡槽正常缩放
        const float SCALE_RATIO  = TARGET_SCALE / NORMAL_SCALE;  // ≈ 2.111
        const int   NAME_FONT    = static_cast<int>(14 * SCALE_RATIO);  // ≈ 30
        const int   DESC_FONT    = static_cast<int>(11 * SCALE_RATIO);  // ≈ 23
        const float NAME_OFF_X   = 26.f * SCALE_RATIO;  // ≈ 55
        const float NAME_OFF_Y   = 82.f * SCALE_RATIO;  // ≈ 173
        const float DESC_OFF_X   = 44.f * SCALE_RATIO;  // ≈ 93
        const float DESC_OFF_Y   = 45.f * SCALE_RATIO;  // ≈ 95

        switch (showcaseState) {

            case CardShowcaseState::PAUSE:
                // 湮灭后 0.3s 空白停顿
                if (t >= 0.3f) {
                    showcaseState = CardShowcaseState::APPEAR;
                    showcaseClock.restart();
                    std::cout << "[Showcase] 停顿结束，开始中央展示出现动画。" << std::endl;
                }
                break;

            case CardShowcaseState::APPEAR: {
                // ── 缩放：指数缓出 0.05 → 0.38（先快后慢）──
                float scaleFactor = 1.0f - std::exp(-t * 8.0f);
                float currentScale = START_SCALE + (TARGET_SCALE - START_SCALE) * scaleFactor;

                // ── 50 段阶梯裁剪：0 → 50（镜像出生动画）──
                float heightPercent = t / 1.0f;  // 连续插值，0.0 → 1.0，无帧率依赖
                if (heightPercent > 1.f) heightPercent = 1.f;
                int rectHeight = static_cast<int>(texSize.y * heightPercent);

                // ── 白光辉光：极亮 → 正常（1.2s，镜像出生动画）──
                float whiteProgress = 1.0f - (t / 1.2f);
                if (whiteProgress < 0.f) whiteProgress = 0.f;

                newCardSprite->setScale({currentScale, currentScale});
                newCardSprite->setPosition(centerPos);
                newCardSprite->setTextureRect(sf::IntRect({0, 0}, {static_cast<int>(texSize.x), rectHeight}));

                // 双通道渲染（与出生动画完全一致）
                newCardSprite->setColor(sf::Color(255, 255, 255, 255));
                window.draw(*newCardSprite, sf::BlendAlpha);

                uint8_t whiteEmit = static_cast<uint8_t>(255 * whiteProgress);
                newCardSprite->setColor(sf::Color(whiteEmit, whiteEmit, whiteEmit, 255));
                window.draw(*newCardSprite, sf::BlendAdd);

                // 文字淡入
                float textFade = t / 0.5f;
                if (textFade > 1.f) textFade = 1.f;
                uint8_t ta = static_cast<uint8_t>(255 * textFade);

                uiText.setFont(cardFont);
                if (heightPercent > 0.02f) {
                    uiText.setCharacterSize(NAME_FONT);
                    uiText.setFillColor(sf::Color(255, 255, 255, ta));
                    uiText.setString(showcasedCard.name);
                    uiText.setPosition({centerPos.x - NAME_OFF_X, centerPos.y - NAME_OFF_Y});
                    window.draw(uiText);
                }
                if (heightPercent > 0.40f) {
                    uiText.setCharacterSize(DESC_FONT);
                    uiText.setFillColor(sf::Color(230, 230, 230, ta));
                    uiText.setString(showcasedCard.description);
                    uiText.setPosition({centerPos.x - DESC_OFF_X, centerPos.y - DESC_OFF_Y});
                    window.draw(uiText);
                }
                uiText.setFont(font);

                if (t >= 1.2f) {
                    showcaseState = CardShowcaseState::DISPLAY;
                    showcaseClock.restart();
                    std::cout << "[Showcase] 出现完成，进入完整展示阶段。" << std::endl;
                }
                break;
            }

            case CardShowcaseState::DISPLAY: {
                // ── 完整展示 2 秒 ──
                newCardSprite->setScale({TARGET_SCALE, TARGET_SCALE});
                newCardSprite->setPosition(centerPos);
                newCardSprite->setTextureRect(sf::IntRect({0, 0}, {static_cast<int>(texSize.x), static_cast<int>(texSize.y)}));
                newCardSprite->setColor(sf::Color(255, 255, 255, 255));
                window.draw(*newCardSprite, sf::BlendAlpha);

                uiText.setFont(cardFont);
                uiText.setCharacterSize(NAME_FONT);
                uiText.setFillColor(sf::Color(255, 255, 255, 255));
                uiText.setString(showcasedCard.name);
                uiText.setPosition({centerPos.x - NAME_OFF_X, centerPos.y - NAME_OFF_Y});
                window.draw(uiText);

                uiText.setCharacterSize(DESC_FONT);
                uiText.setFillColor(sf::Color(230, 230, 230, 255));
                uiText.setString(showcasedCard.description);
                uiText.setPosition({centerPos.x - DESC_OFF_X, centerPos.y - DESC_OFF_Y});
                window.draw(uiText);
                uiText.setFont(font);

                if (t >= 1.2f) {
                    showcaseState = CardShowcaseState::FADE_OUT;
                    showcaseClock.restart();
                    std::cout << "[Showcase] 展示结束，开始消退动画。" << std::endl;
                }
                break;
            }

            case CardShowcaseState::FADE_OUT: {
                // ── 原色→极亮 + 透明度 255→0（0.4s）──
                float whiteProgress = t / 0.4f;
                if (whiteProgress > 1.f) whiteProgress = 1.f;
                float alphaProgress = 1.0f - whiteProgress;
                uint8_t currentAlpha = static_cast<uint8_t>(255 * alphaProgress);

                newCardSprite->setScale({TARGET_SCALE, TARGET_SCALE});
                newCardSprite->setPosition(centerPos);
                newCardSprite->setTextureRect(sf::IntRect({0, 0}, {static_cast<int>(texSize.x), static_cast<int>(texSize.y)}));

                // BlendAlpha 基底（含透明度衰减）
                newCardSprite->setColor(sf::Color(255, 255, 255, currentAlpha));
                window.draw(*newCardSprite, sf::BlendAlpha);

                // BlendAdd 白光叠层（含透明度衰减，同步熄灭）
                uint8_t whiteEmit = static_cast<uint8_t>(255 * whiteProgress * alphaProgress);
                newCardSprite->setColor(sf::Color(whiteEmit, whiteEmit, whiteEmit, 255));
                window.draw(*newCardSprite, sf::BlendAdd);

                // 文字同步淡出
                uiText.setFont(cardFont);
                uiText.setCharacterSize(NAME_FONT);
                uiText.setFillColor(sf::Color(255, 255, 255, currentAlpha));
                uiText.setString(showcasedCard.name);
                uiText.setPosition({centerPos.x - NAME_OFF_X, centerPos.y - NAME_OFF_Y});
                window.draw(uiText);

                uiText.setCharacterSize(DESC_FONT);
                uiText.setFillColor(sf::Color(230, 230, 230, currentAlpha));
                uiText.setString(showcasedCard.description);
                uiText.setPosition({centerPos.x - DESC_OFF_X, centerPos.y - DESC_OFF_Y});
                window.draw(uiText);
                uiText.setFont(font);

                if (t >= 0.4f) {
                    // ▶️ 恢复回合计时 + 解锁玩家操作
                    if (isTurnPaused) {
                        turnTimePaused += pauseClock.getElapsedTime().asSeconds();
                        isTurnPaused = false;
                    }
                    isBusyAnimating = false;

                    std::cout << "[Showcase] 消退完成，正式触发卡牌效果 —— "
                              << showcasedCard.name.c_str() << std::endl;
                    consumeActionPoint(false);
                    applyCardEffect(showcasedCard);
                    settleActionPoints();
                    showcaseState = CardShowcaseState::NONE;
                }
                break;
            }
        }
    }
    // ============================================================================

    if (currentState == GameState::GAME_PVP || currentState == GameState::GAME_PVE) {
        window.draw(detectionZone);
    }

    // ============================================================================
    // 🌟【三明治夹层渲染 - 第 3 层：Top 滑盖】
    // 最后把读卡器的上盖面板盖在最上面，遮挡住通过它下方的卡牌
    // ============================================================================
    if (currentState == GameState::GAME_PVP || currentState == GameState::GAME_PVE) {
        if (cardReaderTopSprite != nullptr) {
            window.draw(*cardReaderTopSprite);
        }
    }

    // 🌟【预落子圆环浮现逻辑】
    if (currentState == GameState::GAME_PVP || currentState == GameState::GAME_PVE) {
        if (!(currentState == GameState::GAME_PVE && isAiThinking)) {
            // 只有在鼠标没有抓取卡牌 且 不在动画期间 时，才绘制预落子提示环
            if (!isCardAttachedToMouse && !isBusyAnimating) {
                chessboard.drawHoverRing(window, currentTurn);
            }
        }
    }

    // 2. 渲染 UI 按钮
    for (auto& rect : gameButtons) window.draw(rect);
    for (auto& text : gameButtonTexts) window.draw(text);

    // 3. 渲染计时器
    int remainingTime = static_cast<int>(std::ceil(45.f - getEffectiveTurnTime()));
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
    if (!battleMusic02.openFromFile(getEngineAssetPath("assets/BGM-InBattle_02.ogg"))) {
        std::cout << "[Audio Error] 无法加载对战音乐 assets/BGM-InBattle_02.ogg" << std::endl;
    } else {
        battleMusic02.setLooping(true);
        battleMusic02.setVolume(0.f);  // 初始静音，crossfade 时渐显
        std::cout << "[Audio] 对战音乐 BGM-InBattle_02 加载成功！" << std::endl;
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

// ============================================================================
// 🌟【核心实现：行动点复合管理系统】
// ============================================================================

// 1. 回合初始分发：玩家获得一个既能下棋又能出牌的【万能行动点】
void GameEngine::initActionPointsForTurn() {
    currentTurnActionPoints.clear();
    currentTurnActionPoints.push_back({ true, true }); // 双能均支持
    std::cout << "[ActionPoint] 新回合开始，自动分发 1 个【下棋+出牌】双能行动点。" << std::endl;
}

// 2. 行动合法性检查
bool GameEngine::hasValidActionPoint(bool isPieceDrop) const {
    for (const auto& ap : currentTurnActionPoints) {
        if (isPieceDrop && ap.canPieceDrop) return true;
        if (!isPieceDrop && ap.canPlayCard) return true;
    }
    return false;
}

// 3. 智能行动点扣除算法
bool GameEngine::consumeActionPoint(bool isPieceDrop) {
    if (isPieceDrop) {
        // 优先寻找“只能下棋，不能出牌”的【专能行动点】，从而将珍贵的“双能万能点”留到最后
        for (auto it = currentTurnActionPoints.begin(); it != currentTurnActionPoints.end(); ++it) {
            if (it->canPieceDrop && !it->canPlayCard) {
                currentTurnActionPoints.erase(it);
                std::cout << "[ActionPoint] 成功消耗 1 个【仅当下棋】的专属行动点。" << std::endl;
                return true;
            }
        }
        // 如果没有专属点，再被迫消耗“双能行动点”
        for (auto it = currentTurnActionPoints.begin(); it != currentTurnActionPoints.end(); ++it) {
            if (it->canPieceDrop && it->canPlayCard) {
                currentTurnActionPoints.erase(it);
                std::cout << "[ActionPoint] 成功消耗 1 个【双能】行动点。" << std::endl;
                return true;
            }
        }
    } 
    else {
        // 优先寻找“只能出牌，不能下棋”的【专能行动点】
        for (auto it = currentTurnActionPoints.begin(); it != currentTurnActionPoints.end(); ++it) {
            if (!it->canPieceDrop && it->canPlayCard) {
                currentTurnActionPoints.erase(it);
                std::cout << "[ActionPoint] 成功消耗 1 个【仅当出牌】的专属行动点。" << std::endl;
                return true;
            }
        }
        // 否则消耗双能点
        for (auto it = currentTurnActionPoints.begin(); it != currentTurnActionPoints.end(); ++it) {
            if (it->canPieceDrop && it->canPlayCard) {
                currentTurnActionPoints.erase(it);
                std::cout << "[ActionPoint] 成功消耗 1 个【双能】行动点。" << std::endl;
                return true;
            }
        }
    }
    return false;
}

// 4. 卡牌赠送接口：未来你的各种卡牌效果完成时，直接调用此函数即可赠送任意组合的行动点
void GameEngine::addActionPoint(bool canPiece, bool canCard) {
    currentTurnActionPoints.push_back({ canPiece, canCard });
    std::cout << "[ActionPoint] 卡牌效果加持！赠送行动点成功：[" 
              << (canPiece ? " 允许下棋 " : "") << (canCard ? " 允许出牌 " : "") << "]" << std::endl;
}

// 5. 总体结算中心：在每个离散动作（落子、卡牌效果跑完）后统一调用
void GameEngine::settleActionPoints() {
    std::cout << "[ActionPoint] 触发总体结算。当前剩余行动点总数: " << currentTurnActionPoints.size() << std::endl;

    // 严格满足要求：只有在行动点蓄水池彻底空了（为零）的时候，才重置倒计时并切换回合
    if (currentTurnActionPoints.empty()) {
        std::cout << "[ActionPoint] 行动点数归零。当前回合强行中止结算，移交回合控制权。" << std::endl;
        
        // 换人逻辑（对接你原本的局内逻辑）
        currentTurn = (currentTurn == 1) ? 2 : 1; 
        
        // ⏳ 只有在这里，回合正式切换时，倒计时才允许重置
        turnClock.restart();
        turnTimePaused = 0.f;  // 新回合，清零暂停累计
        isTurnPaused   = false;

        // 自动为下一个接管回合的玩家刷新基础行动点
        initActionPointsForTurn();
    } 
    else {
        std::cout << "[ActionPoint] 结算完成：仍有可用点数，请继续行动。⏳ 回合倒计时安全锁死，不予重置。" << std::endl;
    }
}

// 6. 卡牌效果分发器：湮灭完成后由 GameEngine 统一调度
void GameEngine::applyCardEffect(const Card& card) {
    std::cout << "[CardEffect] 触发卡牌效果 —— ID:" << card.id
              << " 名称:" << card.name.c_str()
              << " 效果类型:" << static_cast<int>(card.effect) << std::endl;

    switch (card.effect) {
        case CardEffect::FORCE_DROP:
            // "连击"：给予两次仅落子行动点（只能下棋，不能再次出牌）
            addActionPoint(true, false);
            addActionPoint(true, false);
            std::cout << "[CardEffect] 连击发动！获得 2 次额外落子机会。" << std::endl;
            break;
        case CardEffect::CHANGE_WIN_RULE: {
            // "隐忍"：将敌方（对手）的胜利条件改为 card.value（六子连星）
            int opponent = (currentTurn == 1) ? 2 : 1;
            chessboard.setWinCondition(opponent, card.value);
            std::cout << "[CardEffect] 隐忍发动！敌方（玩家" << opponent
                      << "）胜利条件改为 " << card.value << " 子连星。" << std::endl;
            break;
        }
        case CardEffect::REMOVE_OPPONENT:
            // TODO: 后续实现 —— 移除对方棋子
            std::cout << "[CardEffect] REMOVE_OPPONENT 尚未实现，预留框架。" << std::endl;
            break;
        case CardEffect::EXTRA_TURN:
            // TODO: 后续实现 —— 额外完整回合
            std::cout << "[CardEffect] EXTRA_TURN 尚未实现，预留框架。" << std::endl;
            break;
        default:
            std::cout << "[CardEffect] 未知或无效果卡牌。" << std::endl;
            break;
    }
}

// 7. 有效回合时间（实际时间 - 卡牌动画期间的暂停时间）
float GameEngine::getEffectiveTurnTime() const {
    float elapsed = turnClock.getElapsedTime().asSeconds();
    float paused  = turnTimePaused;
    if (isTurnPaused) {
        paused += pauseClock.getElapsedTime().asSeconds();
    }
    return elapsed - paused;
}

// 8. 战斗音乐：CardReader 触发切换到 battle_02
void GameEngine::triggerBattleMusic02() {
    if (battleMusicState == BattleMusicState::NORMAL) {
        battleMusicState = BattleMusicState::SWITCHING_TO_02;
        crossfadeClock.restart();
        battleMusic02.setVolume(0.f);
        if (static_cast<int>(battleMusic02.getStatus()) != 2) battleMusic02.play();
        std::cout << "[Audio] CardReader 检测 —— 切换到 BATTLE_02" << std::endl;
    }
}

// 9. 战斗音乐：危机状态延长 battle_02 时长（可叠加，无上限）
void GameEngine::addCrisisTime(float seconds) {
    if (battleMusicState == BattleMusicState::BATTLE_02) {
        battle02Remaining += seconds;
        std::cout << "[Audio] 危机触发 —— battle_02 延长 " << seconds
                  << "s，剩余 " << battle02Remaining << "s" << std::endl;
    }
}
