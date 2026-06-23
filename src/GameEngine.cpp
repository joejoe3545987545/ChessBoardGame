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
#include <cstring>


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
    : window(sf::VideoMode::getDesktopMode(), "chessBoardGame (Professional Edition)",
             sf::Style::None, sf::State::Windowed),
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

    gameView = sf::View(sf::FloatRect({0.f, 0.f}, {2560.f, 1440.f}));
    applyViewport();
    window.setView(gameView);

    std::cout << "[Info] GameEngine constructing..." << std::endl;

    try {
        if (font.getInfo().family.empty()) {
            std::cerr << "[Warning] Loaded font appears empty or invalid." << std::endl;
        }
    } catch (...) {
        std::cerr << "[Info] Font info check skipped (getInfo unavailable)." << std::endl;
    }

    uiText.setFillColor(sf::Color(45, 45, 45));
    uiText.setCharacterSize(36);

    timerText.setCharacterSize(44);
    timerText.setFillColor(sf::Color(255, 255, 255));

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
    // 紫卡底纹
    if (purpleCardTexture.loadFromFile(getEngineAssetPath("assets/Card_01.png"))) {
        purpleCardTexture.setSmooth(true);
        purpleCardSprite = new sf::Sprite(purpleCardTexture);
        sf::Vector2u ps = purpleCardTexture.getSize();
        purpleCardSprite->setOrigin({ps.x / 2.f, ps.y / 2.f});
        std::cout << "[UI] 紫卡底纹 Card_01.png 加载成功" << std::endl;
    }
    initCardFragmentCache();
    // ===== 🌟 紧跟在 newCardSprite 初始化下方的卡槽初始化 =====
    if (cardSlotTexture.loadFromFile("assets/CardSlot.png")) { // 💡 确认好你的素材路径
        cardSlotSprite = new sf::Sprite(cardSlotTexture);
        
        // 1. 🌟 绝对同步：设置与卡牌完全相同的中心原点
        sf::Vector2u slotSize = cardSlotTexture.getSize();
        cardSlotSprite->setOrigin({slotSize.x / 2.f, slotSize.y / 2.f});
        
        // 2. 🌟 绝对同步：设置与卡牌完全相同的 0.18f 缩放
        cardSlotSprite->setScale({0.50f, 0.50f});
        
        // 3. 🌟 绝对同步：让卡槽永久锁死在你指定的卡牌原位中心点
        cardSlotSprite->setPosition({2260.f, 331.f});
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
        float centerX = 300;     // 与 CardReader X 对齐
        // top -> position.y ； height -> size.y
        float centerY = 380;  // 检测区域位置

        // 💡 修复 3：SFML 3.0 的 setPosition 同样必须用大括号 {} 括起来
        detectionZone.setPosition({centerX, centerY});

        // 开发调试用颜色：半透明绿，方便等会儿测试看位置
        detectionZone.setFillColor(sf::Color(0, 0, 0, 0));
        detectionZone.setOutlineThickness(0.f);
        detectionZone.setOutlineColor(sf::Color::Green);
    } else {
        std::cerr << "[UI Error] 卡槽精灵未初始化，无法设置检测区域。" << std::endl;
    }
    // CardPortal 检测区（与 Reader 对称，Y=1060）
    portalDetectionZone.setRadius(zoneRadius);
    portalDetectionZone.setOrigin({zoneRadius, zoneRadius});
    portalDetectionZone.setPosition({300.f, 1060.f});
    portalDetectionZone.setFillColor(sf::Color(0, 0, 0, 0));
    portalDetectionZone.setOutlineThickness(0.f);
    portalDetectionZone.setOutlineColor(sf::Color(128, 0, 128)); // 紫色调试框
    // 🌟 第二卡槽（卡槽 1 正下方 220px，当前空置备用）
    if (cardSlotTexture2.loadFromFile(getEngineAssetPath("assets/CardSlot.png"))) {
        cardSlotTexture2.setSmooth(true);
        cardSlotSprite2 = new sf::Sprite(cardSlotTexture2);
        sf::Vector2u slot2Size = cardSlotTexture2.getSize();
        cardSlotSprite2->setOrigin({slot2Size.x / 2.f, slot2Size.y / 2.f});
        cardSlotSprite2->setScale({0.50f, 0.50f});
        cardSlotSprite2->setPosition({2260.f, 971.f});
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
        cardReaderBottomSprite->setScale({0.8f, 0.8f});
        cardReaderBottomSprite->setPosition({300.f, 210.f});
        
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
        cardReaderTopSprite->setScale({0.8f, 0.8f});
        cardReaderTopSprite->setPosition({300.f, 210.f});
        
        std::cout << "[🎉 Success] 读卡器滑盖 CardReader_Top 初始化成功！" << std::endl;
    } else {
        std::cerr << "[❌ UI Error] 无法加载素材 assets/CardReader_Top.png" << std::endl;
    }

    // 🌟 CardPortal（下方，与 CardReader 对称，y=1440）
    cardPortalBottomTexture = std::make_unique<sf::Texture>();
    if (cardPortalBottomTexture->loadFromFile(getEngineAssetPath("assets/CardPortal_Bottom.png"))) {
        cardPortalBottomTexture->setSmooth(true);
        cardPortalBottomSprite = std::make_unique<sf::Sprite>(*cardPortalBottomTexture);
        sf::Vector2u pSize = cardPortalBottomTexture->getSize();
        cardPortalBottomSprite->setOrigin({pSize.x / 2.f, pSize.y / 2.f});
        cardPortalBottomSprite->setScale({0.8f, 0.8f});
        cardPortalBottomSprite->setPosition({300.f, 1230.f});
    }
    cardPortalTopTexture = std::make_unique<sf::Texture>();
    if (cardPortalTopTexture->loadFromFile(getEngineAssetPath("assets/CardPortal_Top.png"))) {
        cardPortalTopTexture->setSmooth(true);
        cardPortalTopSprite = std::make_unique<sf::Sprite>(*cardPortalTopTexture);
        sf::Vector2u pSize = cardPortalTopTexture->getSize();
        cardPortalTopSprite->setOrigin({pSize.x / 2.f, pSize.y / 2.f});
        cardPortalTopSprite->setScale({0.8f, 0.8f});
        cardPortalTopSprite->setPosition({300.f, 1230.f});
    }

    // 🌟 主菜单背景图层加载
    if (menuWhiteTex.loadFromFile(getEngineAssetPath("assets/white_HR.png"))) {
        menuWhiteSpr.setTexture(menuWhiteTex, true);
        menuWhiteSpr.setOrigin({menuWhiteTex.getSize().x / 2.f, menuWhiteTex.getSize().y / 2.f});
    }
    if (menuBlackTex.loadFromFile(getEngineAssetPath("assets/black_HR.png"))) {
        menuBlackSpr.setTexture(menuBlackTex, true);
        menuBlackSpr.setOrigin({menuBlackTex.getSize().x / 2.f, menuBlackTex.getSize().y / 2.f});
    }
    initFramePieceFragCache(); // HR纹理已加载，此时生成碎片缓存

    // 🌟 主菜单 UI 素材
    if (uiFrameTex.loadFromFile(getEngineAssetPath("assets/UI_Frame.png"))) {
        uiFrameSpr.setTexture(uiFrameTex, true);
        uiFrameSpr.setOrigin({uiFrameTex.getSize().x / 2.f, uiFrameTex.getSize().y / 2.f});
    }
    if (mainTitleTex.loadFromFile(getEngineAssetPath("assets/MainTittle.png"))) {
        mainTitleSpr.setTexture(mainTitleTex, true);
        mainTitleSpr.setOrigin({mainTitleTex.getSize().x / 2.f, mainTitleTex.getSize().y / 2.f});
    }
    settingsMenuLoaded = settingsMenuTex.loadFromFile(getEngineAssetPath("assets/SettingsMenu.png"));
    if (settingsMenuLoaded) {
        settingsMenuSpr.setTexture(settingsMenuTex, true);
        settingsMenuSpr.setOrigin({settingsMenuTex.getSize().x / 2.f, settingsMenuTex.getSize().y / 2.f});
    }
    if (virusTex.loadFromFile(getEngineAssetPath("assets/virus.png"))) {
        virusSpr.setTexture(virusTex, true);
        virusSpr.setOrigin({virusTex.getSize().x / 2.f, virusTex.getSize().y / 2.f});
    }
    if (skullTex.loadFromFile(getEngineAssetPath("assets/skull.png"))) {
        skullSpr.setTexture(skullTex, true);
        skullSpr.setOrigin({skullTex.getSize().x / 2.f, skullTex.getSize().y / 2.f});
    }
    if (ringTex.loadFromFile(getEngineAssetPath("assets/ring.png"))) {
        ringSpr.setTexture(ringTex, true);
        ringSpr.setOrigin({ringTex.getSize().x / 2.f, ringTex.getSize().y / 2.f});
    }
    if (watchTex.loadFromFile(getEngineAssetPath("assets/watch.png"))) {
        watchSpr.setTexture(watchTex, true);
        watchSpr.setOrigin({watchTex.getSize().x / 2.f, watchTex.getSize().y / 2.f});
    }
    if (blackHRTex.loadFromFile(getEngineAssetPath("assets/black_HR.png"))) {
        blackHRSpr = new sf::Sprite(blackHRTex);
        blackHRSpr->setOrigin({blackHRTex.getSize().x/2.f, blackHRTex.getSize().y/2.f});
    }
    if (chessFrameTex.loadFromFile(getEngineAssetPath("assets/ChessFrame.png"))) {
        chessFrameSpr.setTexture(chessFrameTex, true);
        chessFrameSpr.setOrigin({chessFrameTex.getSize().x / 2.f, chessFrameTex.getSize().y / 2.f});
    }
    // 数字图片计时器
    for (int d = 0; d < 10; ++d) {
        char buf[64]; snprintf(buf, sizeof(buf), "assets/Numbers/%d.png", d);
        if (digitTex[d].loadFromFile(getEngineAssetPath(buf))) {
            digitSpr[d] = new sf::Sprite(digitTex[d]);
            digitSpr[d]->setOrigin({digitTex[d].getSize().x / 2.f, digitTex[d].getSize().y / 2.f});
        }
    }
    // 🌟 画中画初始化
    pipRT.resize({1280u, 720u});
    pipSpr = new sf::Sprite(pipRT.getTexture());
    pipAnimStart(1);  // 启动动画 1

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
        L"本地双人对战", L"人机对战", L"游戏设置", L"规则说明帮助", L"历史记录复盘", L"退出游戏"
    };

    // 菜单按钮视觉尺寸 vs 红框检测尺寸
    const float menuButtonWidth = 320.f;
    const float menuButtonHeight = 67.f;
    const float hitWidth  = menuButtonWidth * 1.1f;    // 检测横 ×1.1
    const float hitHeight = menuButtonHeight * 1.8f;   // 检测纵 ×1.8
    const float centerX = WINDOW_WIDTH * 0.5f;
    const float menuStartY = 543.f;
    const float menuButtonX = centerX - menuButtonWidth * 0.5f;
    const float hitX = centerX - hitWidth * 0.5f;

    for (size_t i = 0; i < menuTexts.size(); ++i) {
        float extraGap = static_cast<float>(i) * 30.f;   // 往下逐级多隔 30px
        float btnY = menuStartY + i * 107.f + extraGap;
        // 红色校准框=检测范围
        sf::RectangleShape bg({hitWidth, hitHeight});
        bg.setPosition({hitX, btnY - (hitHeight - menuButtonHeight) * 0.5f});
        bg.setFillColor(sf::Color(0, 0, 0, 0));  // 透明（仅用于定位）
        menuButtonBackgrounds.push_back(bg);

        sf::Text t(cardFont, menuTexts[i], 48);
        t.setFillColor(sf::Color(50, 15, 70));
        sf::FloatRect textBounds = t.getLocalBounds();
        t.setPosition(sf::Vector2f(
            centerX - (textBounds.position.x + textBounds.size.x) * 0.5f,
            btnY + (menuButtonHeight - textBounds.size.y) * 0.5f - textBounds.position.y));
        menuButtons.push_back(t);
    }

    // 游戏 HUD 按钮已移除，改为 ESC 暂停菜单
}


// ── PIP 动画循环系统 ──
static constexpr int PIP_ANIM_COUNT = 3;  // 当前动画总数

void GameEngine::pipAnimNext() {
    pipAnimIndex++;
    if (pipAnimIndex > PIP_ANIM_COUNT) pipAnimIndex = 1;
    pipAnimStart(pipAnimIndex);
}

void GameEngine::pipAnimStart(int animIdx) {
    pipAnimIndex = animIdx;
    pipAnimState = 0;
    pipAnimClock.restart();
    pipFadeAlpha = 0.f;
    pipFadeClock.restart();
    pipFragsInit = false; pipShowFragsInit = false; pipDecayFrags.clear();
    pipTotalDur = (animIdx == 1) ? 7.f : 10.f;
}

// 全屏切换
void GameEngine::toggleFullscreen() {
    isFullscreen = !isFullscreen;
    window.close();

    if (isFullscreen) {
        sf::VideoMode desktop = sf::VideoMode::getDesktopMode();
        window.create(desktop, "chessBoardGame (Professional Edition)",
                      sf::Style::None, sf::State::Windowed);
    } else {
        window.create(sf::VideoMode({1920, 1080}), "chessBoardGame (Professional Edition)",
                      sf::Style::Default, sf::State::Windowed);
    }
    gameView = sf::View(sf::FloatRect({0.f, 0.f}, {2560.f, 1440.f}));
    applyViewport();
    window.setView(gameView);
    initUI();

    std::cout << "[Info] Fullscreen toggled: " << (isFullscreen ? "ON" : "OFF") << std::endl;
}

void GameEngine::applyViewport() {
    sf::Vector2u winSize = window.getSize();
    float windowRatio = static_cast<float>(winSize.x) / static_cast<float>(winSize.y);
    float gameRatio   = 1920.f / 1080.f;
    if (windowRatio > gameRatio) {
        float vpW = gameRatio / windowRatio;
        gameView.setViewport(sf::FloatRect({(1.f - vpW) / 2.f, 0.f}, {vpW, 1.f}));
    } else if (windowRatio < gameRatio) {
        float vpH = windowRatio / gameRatio;
        gameView.setViewport(sf::FloatRect({0.f, (1.f - vpH) / 2.f}, {1.f, vpH}));
    }
    // 等比时 viewport 保持全屏 (0,0,1,1)
}

// ------------------------ 主循环 ------------------------
void GameEngine::run() {
    window.setView(gameView);
    GameState lastState = currentState;
    while (window.isOpen()) {
        processEvents();
        if (currentState != lastState && !transitioning) {
            nextState = currentState; currentState = lastState;
            transitioning = true; transitionClock.restart();
        }
        if (transitioning) {
            float t = transitionClock.getElapsedTime().asSeconds();
            if (t >= 0.25f && currentState != nextState) { currentState = nextState; lastState = nextState; }
            if (t >= 0.5f) transitioning = false;
        } else { lastState = currentState; }
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

        // F11 切换全屏 / 窗口
        if (const auto* keyEv = event->getIf<sf::Event::KeyPressed>()) {
            if (keyEv->code == sf::Keyboard::Key::F11) {
                toggleFullscreen();
            } else if (keyEv->code == sf::Keyboard::Key::F10) {
                showAIDebug = !showAIDebug;
            } else if (keyEv->code == sf::Keyboard::Key::Escape) {
                if (currentState == GameState::GAME_PVP || currentState == GameState::GAME_PVE) {
                    isPaused = !isPaused;
                    if (isPaused) {
                        pauseFadeClock.restart();  // 进入 → 淡入
                        if (!isTurnPaused) { isTurnPaused = true; pauseClock.restart(); }
                    } else {
                        pauseFadeClock.restart();  // 退出 → 淡出
                        if (isTurnPaused) {
                            turnTimePaused += pauseClock.getElapsedTime().asSeconds();
                            isTurnPaused = false;
                        }
                    }
                }
            }
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
                    // 检测区：宽 352，高 120.6，以 (centerX, 680) 为中心
                    float btnCX = WINDOW_WIDTH * 0.5f;
                    float btnHitW = 320.f * 1.1f;
                    float btnHitH = 67.f * 1.8f;
                    float btnTop = 680.f - btnHitH * 0.5f;
                    if (mousePos.x > btnCX - btnHitW * 0.5f && mousePos.x < btnCX + btnHitW * 0.5f &&
                        mousePos.y > btnTop && mousePos.y < btnTop + btnHitH) {
                        std::cout << "[UI] Game Over Screen: Return to Main Menu clicked." << std::endl;
                        
                        chessboard.reset();
                        playerDeck.resetDeck();
            memset(infected, 0, sizeof(infected)); memset(infectionActive, 0, sizeof(infectionActive)); quarantineTimer = -1; plagueOwner = 0;
            aiCardPlayState = AICardPlayState::IDLE; aiPlayingCardIndex = -1; aiCardIsPurpleTransfer = false;

                        currentTurn = 1;
                        isAiThinking = false;
                        isGameOver = false;
                        playerFramePieces = 0; enemyFramePieces = 0; fpAnimState = FramePieceAnimState::IDLE; fpAnimFrags.clear(); pendingFramePieces.clear();
                        savedThisGame = false;
                        isPaused = false;
                        playerInvincible = false; playerInvinciblePlus = false; aiOnlyDrop = false; aiOnlyCard = false;
                        showAIDebug = false;
                        isSelectingPiece = false;
                        selectPieceStep = 0;
                        pendingDestroys.clear();
                        isBulkDestroying = false;
                        isReturningHandToDeck = false; returnDeckInit = false; returnDeckFrags.clear(); returnDeckPos.clear(); returnDeckTex.clear(); curseRemoving = false; curseRemovingIdx = -1; curseRemoveFrags.clear(); curseRemoveFragsInit = false; cardDecayFrags.clear(); cardDecayTex = 0; cardBirthActive = false; cardBirthFrags.clear(); cardBirthInit = false; showcaseFragsInit = false; showcaseFrags.clear(); yiDiShiQinActive = false; yiDiShiQinResponded = false; yiDiShiQinPenalizing = false; yiDiShiQinPenalty = 0;
                        isCardAttachedToMouse = false;
                        attachedCardIndex = -1;
                        handSlotAssign.clear();
                        isReturningToSlot = false;
                        newCardJustDrawn = false;
                        showcaseState = CardShowcaseState::NONE;
                        annihilateState = CardAnnihilateState::NONE;
                        isTurnPaused = false;
                        turnTimePaused = 0.f;
                        isBusyAnimating = false;
                        currentTurnActionPoints.clear();
                        // 停止所有战斗/结算音乐
                        battleMusic.stop();
                        battleMusic02.stop();
                        battleMusicState = BattleMusicState::NORMAL;
                        winMusic.stop();
                        failMusic.stop();
                        // 重置乱码卡片
                        glitchCardPos = {2260.f, 1200.f};
                        glitchAngle = 0.f;
                        glitchAngularVel = 0.f;
                        glitchDragging = false;
                        glitchVelocity = {0.f, 0.f};
                        glitchPrevMouse = {0.f, 0.f};
                        lastMouseDir = {0.f, 0.f};
                        glitchRenderPos = {2260.f, 1200.f};
                        // 重置卡牌精灵
                        if (newCardSprite) newCardSprite->setPosition({2260.f, 160.f});

                        currentState = GameState::MENU;
                    }
                }
                else if (currentState == GameState::GAME_PVP || currentState == GameState::GAME_PVE) {
                    // 暂停菜单拦截所有点击
                    if (isPaused) {
                        handlePauseMenuClick(mousePos);
                        continue;
                    }

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

                    // 🎯 笼络选子模式：手动坐标转换检测有子格子
                    if (isSelectingPiece && !hudIntercepted) {
                        const float BW = 15.f * 72.f; // BOARD_WIDTH (2K)
                        float bx = (2560.f - BW) * 0.5f + 36.f;
                        float by = (1440.f - BW) * 0.5f + 36.f;
                        int col = static_cast<int>(std::round((mousePos.x - bx) / 72.f));
                        int row = static_cast<int>(std::round((mousePos.y - by) / 72.f));
                        if (row >= 0 && row < 15 && col >= 0 && col < 15) {
                            int piece = chessboard.getPiece(row, col);
                            if (selectPieceStep == 10 && piece != 0 && piece != selectPiecePlayer) {
                                // 疫病选子：感染敌方棋子
                                infected[row][col] = true; infectionActive[row][col] = false;
                                plagueOwner = selectPiecePlayer;
                                isSelectingPiece = false;
                                selectPieceStep = 0;
                                std::cout << "[Plague] 疫病感染棋子 (" << row << "," << col << ")" << std::endl;
                                // 完成卡牌效果（同即时效果恢复流程）
                                if (isTurnPaused) {
                                    turnTimePaused += pauseClock.getElapsedTime().asSeconds();
                                    isTurnPaused = false;
                                }
                                consumeActionPoint(false);
                                settleActionPoints();
                                isBusyAnimating = false;
                            } else if (selectPieceStep == 1 && piece == selectPiecePlayer) {
                                chessboard.startDestroyAnim(row, col);
                                isBusyAnimating = true;
                                selectPieceStep = 2;
                                std::cout << "[Convert] 销毁动画启动。" << std::endl;
                            } else if (selectPieceStep == 2 && !isBusyAnimating) {
                                int enemy = (selectPiecePlayer == 1) ? 2 : 1;
                                if (piece == enemy) {
                                    chessboard.startConvertAnim(row, col, enemy, selectPiecePlayer);
                                    isBusyAnimating = true;
                                    isSelectingPiece = false;
                                    selectPieceStep = 3;
                                    std::cout << "[Convert] 转化动画启动。" << std::endl;
                                }
                            }
                        }
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

                        // 落子抛物线动画
                        chessboard.startDropAnim(lastRow, lastCol, currentTurn);

                        // 🌟【行动点核销】：落子物理成功！核销扣除 1 个支持下棋的行动点
                        consumeActionPoint(true);

                        // 音频处理
                        dropSound->play();
                        std::cout << "[Audio] Drop sound played." << std::endl;

                        // 点击成功立刻加入手牌数据系统（三子或四子连线抽卡）
                        int dir = -1;
                        int pattern = chessboard.checkPattern(lastRow, lastCol, dir);
                        // 盲目延后：三子连星延长移除回合
                        if (pattern >= 3) {
                            for (auto& c : playerDeck.hand)
                                if (c.cardColor == 1 && c.transferred && c.effect == CardEffect::BLIND)
                                    { c.value++; std::cout << "[Blind] 玩家三连，盲目延后至 " << c.value << " 回合" << std::endl; }
                        }
                        if (pattern == 3 || pattern == 4) {
                            size_t before = playerDeck.hand.size();
                            playerDeck.drawCard();
                            if (playerDeck.hand.size() > before) {
                                handSlotAssign.push_back(1);
                                newCardJustDrawn = true;
                            }
                            chessboard.markPatternUsed(lastRow, lastCol, dir);
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

                        // 🌟【胜利制重构】多方向5连检测，每方向放一个帧棋子
                        for (int pass = 0; pass < 4; ++pass) {
                            if (!chessboard.checkWin(lastRow, lastCol)) break;
                            int sr, sc, er, ec;
                            chessboard.getWinLine(sr, sc, er, ec);
                            chessboard.markWinCellsAsScored(sr, sc, er, ec);
                            awardFramePiece(currentTurn);
                            chessboard.clearWinLine();
                        }
                        if (!isGameOver && !isBusyAnimating) {
                            // 🌟【行动点核心替换点】：动画期间跳过AP结算，由completeFramePieceAnim处理
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
    // ── 乱码卡片点击优先：卡片在最上层，遮挡按钮时卡片优先 ──
    {
        float gcx = glitchCardPos.x, gcy = glitchCardPos.y;
        float dx = mousePos.x - gcx, dy = mousePos.y - gcy;
        const float cardHalfDiag = 300.f;  // 卡片半对角线 ≈ sqrt(173²+240²) ≈ 296
        if (dx * dx + dy * dy < cardHalfDiag * cardHalfDiag) {
            return;  // 点击在卡片上，不触发按钮
        }
    }

    const float visualWidth = 320.f;
    const float centerX = WINDOW_WIDTH * 0.5f;
    const float hitWidth = visualWidth * 1.1f;   // 横向 ×1.1
    const float hitHeight = 67.f * 1.8f;          // 纵向 ×1.8
    const float hitX = centerX - hitWidth * 0.5f;
    const float hitRX = centerX + hitWidth * 0.5f;
    const float menuStartY = 543.f;

    if (mousePos.x > hitX && mousePos.x < hitRX) {
        if (mousePos.y > menuStartY && mousePos.y < menuStartY + hitHeight) {
            chessboard.reset();
            playerDeck.resetDeck();
            memset(infected, 0, sizeof(infected)); memset(infectionActive, 0, sizeof(infectionActive)); quarantineTimer = -1; plagueOwner = 0;
            aiCardPlayState = AICardPlayState::IDLE; aiPlayingCardIndex = -1; aiCardIsPurpleTransfer = false;
            handSlotAssign.clear();
            attachedCardIndex = -1;
            isCardAttachedToMouse = false;
            isBusyAnimating = false;
            isSelectingPiece = false;
            selectPieceStep  = 0;
            pendingDestroys.clear();
            isBulkDestroying = false;
            isReturningHandToDeck = false; returnDeckInit = false; returnDeckFrags.clear(); returnDeckPos.clear(); returnDeckTex.clear(); curseRemoving = false; curseRemovingIdx = -1; curseRemoveFrags.clear(); curseRemoveFragsInit = false; cardDecayFrags.clear(); cardDecayTex = 0; cardBirthActive = false; cardBirthFrags.clear(); cardBirthInit = false; showcaseFragsInit = false; showcaseFrags.clear(); yiDiShiQinActive = false; yiDiShiQinResponded = false; yiDiShiQinPenalizing = false; yiDiShiQinPenalty = 0;
            newCardJustDrawn = false;
            showcaseState = CardShowcaseState::NONE;
            annihilateState = CardAnnihilateState::NONE;
            isReturningToSlot = false;
            if (newCardSprite) newCardSprite->setPosition({2260.f, 160.f});
            battleMusic02.stop();
            battleMusicState = BattleMusicState::NORMAL;
            battleMusic.setVolume(BATTLE_VOLUME);
            currentTurn = 1;
            isAiThinking = false;
            isGameOver = false;
            playerFramePieces = 0; enemyFramePieces = 0; fpAnimState = FramePieceAnimState::IDLE; fpAnimFrags.clear(); pendingFramePieces.clear();
            savedThisGame = false;
            currentState = GameState::GAME_PVP;
            initActionPointsForTurn();
            turnClock.restart();
        }
        else if (mousePos.y > menuStartY + 137.f && mousePos.y < menuStartY + 137.f + hitHeight) {
            currentState = GameState::PVE_CONFIG;
        }
        else if (mousePos.y > menuStartY + 274.f && mousePos.y < menuStartY + 274.f + hitHeight) {
            currentState = GameState::SETTINGS;
        }
        else if (mousePos.y > menuStartY + 411.f && mousePos.y < menuStartY + 411.f + hitHeight) {
            currentState = GameState::HELP;
        }
        else if (mousePos.y > menuStartY + 548.f && mousePos.y < menuStartY + 548.f + hitHeight) {
            currentState = GameState::HISTORY;
        }
        else if (mousePos.y > menuStartY + 685.f && mousePos.y < menuStartY + 685.f + hitHeight) {
            window.close();
        }
    }
}

void GameEngine::handlePVEConfigClick(sf::Vector2i mousePos) {
    // 与主菜单一致的检测区：宽 320×1.1=352, 高 67×1.8=120.6
    const float centerX = WINDOW_WIDTH * 0.5f;
    const float hitWidth  = 320.f * 1.1f;
    const float hitHeight = 67.f * 1.8f;
    const float hitX = centerX - hitWidth * 0.5f;
    const float hitRX = centerX + hitWidth * 0.5f;
    const float menuStartY = 543.f;
    const float spacing = 137.f;
    const float hyOff = (hitHeight - 67.f) * 0.5f;  // 检测区居中偏移

    if (mousePos.x > hitX && mousePos.x < hitRX) {
        if (mousePos.y > menuStartY - hyOff && mousePos.y < menuStartY - hyOff + hitHeight) {
            aiDifficultySetting = (aiDifficultySetting % 3) + 1;
            std::cout << "[Info] AI difficulty -> " << aiDifficultySetting << std::endl;
        }
        else if (mousePos.y > menuStartY + spacing - hyOff && mousePos.y < menuStartY + spacing - hyOff + hitHeight) {
            playerColorPref = (playerColorPref == 1) ? 2 : 1;
            std::cout << "[Info] Player color -> " << (playerColorPref == 1 ? "Black" : "White") << std::endl;
        }
        else if (mousePos.y > menuStartY + spacing * 2 - hyOff && mousePos.y < menuStartY + spacing * 2 - hyOff + hitHeight) {
            chessboard.reset();
            playerDeck.resetDeck();
            memset(infected, 0, sizeof(infected)); memset(infectionActive, 0, sizeof(infectionActive)); quarantineTimer = -1; plagueOwner = 0;
            aiCardPlayState = AICardPlayState::IDLE; aiPlayingCardIndex = -1; aiCardIsPurpleTransfer = false;
            handSlotAssign.clear();
            attachedCardIndex = -1;
            isCardAttachedToMouse = false;
            isBusyAnimating = false;
            isSelectingPiece = false;
            selectPieceStep  = 0;
            pendingDestroys.clear();
            isBulkDestroying = false;
            isReturningHandToDeck = false; returnDeckInit = false; returnDeckFrags.clear(); returnDeckPos.clear(); returnDeckTex.clear(); curseRemoving = false; curseRemovingIdx = -1; curseRemoveFrags.clear(); curseRemoveFragsInit = false; cardDecayFrags.clear(); cardDecayTex = 0; cardBirthActive = false; cardBirthFrags.clear(); cardBirthInit = false; showcaseFragsInit = false; showcaseFrags.clear(); yiDiShiQinActive = false; yiDiShiQinResponded = false; yiDiShiQinPenalizing = false; yiDiShiQinPenalty = 0;
            newCardJustDrawn = false;
            showcaseState = CardShowcaseState::NONE;
            annihilateState = CardAnnihilateState::NONE;
            isReturningToSlot = false;
            if (newCardSprite) newCardSprite->setPosition({2260.f, 160.f});
            battleMusic02.stop();
            battleMusicState = BattleMusicState::NORMAL;
            battleMusic.setVolume(BATTLE_VOLUME);
            currentTurn = 1;
            isAiThinking = false;
            isGameOver = false;
            playerFramePieces = 0; enemyFramePieces = 0; fpAnimState = FramePieceAnimState::IDLE; fpAnimFrags.clear(); pendingFramePieces.clear();
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
        else if (mousePos.y > menuStartY + spacing * 3 - hyOff && mousePos.y < menuStartY + spacing * 3 - hyOff + hitHeight) {
            currentState = GameState::MENU;
        }
    }
}

bool GameEngine::handleHUDClick(sf::Vector2i mousePos) {
    // 只有当玩家手里确实有卡牌时，才需要进行卡牌的碰撞盒检测
    // 🔒 卡牌动画期间禁止抓取卡牌
    if (!playerDeck.hand.empty() && newCardSprite != nullptr && !isBusyAnimating) {
        sf::FloatRect cardClickRect;
        
        // 严格锁定你指定的卡牌物理宽高
        sf::Vector2f cardSize(288.f, 400.f);
        sf::Vector2f mouseClickPos(static_cast<float>(mousePos.x), static_cast<float>(mousePos.y));
        // 各槽独立顶卡索引
        int slot1Top = -1, slot2Top = -1;
        for (size_t i = 0; i < handSlotAssign.size(); ++i) {
            if (handSlotAssign[i] == 1) slot1Top = static_cast<int>(i);
            else if (handSlotAssign[i] == 2) slot2Top = static_cast<int>(i);
        }

        if (isCardAttachedToMouse) {
            // A. 吸附态 — 卡牌跟随鼠标
            sf::Vector2i pixelPos = sf::Mouse::getPosition(window);
            sf::Vector2f worldPos = window.mapPixelToCoords(pixelPos);
            sf::Vector2f currentCardCenter = worldPos - cardMouseOffset;
            cardClickRect = sf::FloatRect(
                {currentCardCenter.x - 144.f, currentCardCenter.y - 200.f}, cardSize);
        }

        // 🔍 非吸附态拾取：先槽2后槽1，显式计算无lambda
        if (!isCardAttachedToMouse && (!cardBirthActive || cardBirthClock.getElapsedTime().asSeconds() >= 1.0f)) {
            // ── 槽2顶卡 ──
            if (slot2Top >= 0) {
                int stack2 = 0;
                for (int j = 0; j < slot2Top; ++j)
                    if (handSlotAssign[j] == 2) stack2++;
                float x2 = 2116.f + stack2 * 6.f;
                float y2 = 760.f  + stack2 * 6.f;
                sf::FloatRect r2({x2, y2}, cardSize);
                if (r2.contains(mouseClickPos)) {
                    attachedCardIndex = slot2Top;
                    float cx2 = 2260.f + stack2 * 6.f;
                    float cy2 = 960.f  + stack2 * 6.f;
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
                float x1 = 2116.f + stack1 * 6.f;
                float y1 = 120.f  + stack1 * 6.f;
                sf::FloatRect r1({x1, y1}, cardSize);
                if (r1.contains(mouseClickPos)) {
                    attachedCardIndex = slot1Top;
                    float cx1 = 2260.f + stack1 * 6.f;
                    float cy1 = 320.f  + stack1 * 6.f;
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
                float dx = mouseClickPos.x - 300.f;
                float dy = mouseClickPos.y - 380.f;
                float distance = std::sqrt(dx * dx + dy * dy);

                // ── 紫卡传送：检测 Portal 区域 ──
                float pdx = mouseClickPos.x - 300.f;
                float pdy = mouseClickPos.y - 1060.f;
                float pDist = std::sqrt(pdx * pdx + pdy * pdy);
                bool isPurpleCard = (attachedCardIndex >= 0 && attachedCardIndex < static_cast<int>(playerDeck.hand.size()) &&
                                    playerDeck.hand[attachedCardIndex].cardColor == 1 &&
                                    !playerDeck.hand[attachedCardIndex].transferred); // 已传送的不可再传

                // 🌟 以地事秦回应：拖橙卡到 Portal 或 Reader 都算送牌
                bool isOrangeCard = (attachedCardIndex >= 0 && attachedCardIndex < (int)playerDeck.hand.size() &&
                                     playerDeck.hand[attachedCardIndex].cardColor == 0);
                if (yiDiShiQinActive && !yiDiShiQinResponded && isOrangeCard && pDist <= zoneRadius) {
                    isCardAttachedToMouse = false;
                    int sendIdx2 = attachedCardIndex;
                    attachedCardIndex = -1;
                    yiDiShiQinResponded = true;
                    // 走紫卡传送动画
                    startPurpleCardSend(sendIdx2);
                    std::cout << "[以地事秦] 已传送橙卡回应！" << std::endl;
                    return true;
                }

                if (isPurpleCard && pDist <= zoneRadius) {
                    if (!hasValidActionPoint(false)) {
                        isCardAttachedToMouse = false;
                        isReturningToSlot = true;
                        returnStartPos = newCardSprite->getPosition();
                        returnDelayClock.restart();
                        return true;
                    }
                    // 紫卡传送：snap → 下滑 → 进入敌方手牌
                    isCardAttachedToMouse = false;
                    int sendIdx = attachedCardIndex;
                    attachedCardIndex = -1;
                    startPurpleCardSend(sendIdx);
                    return true;
                }

                if (distance <= zoneRadius && !isPurpleCard) {
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
                    sf::FloatRect slot1Zone({2116.f, 120.f}, cardSize);
                    sf::FloatRect slot2Zone({2116.f, 760.f}, cardSize);

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
    // 处理手牌区域
    sf::Vector2f mousePoint(static_cast<float>(mousePos.x), static_cast<float>(mousePos.y));
    return false; // 没点中任何手牌
}

// ── 暂停菜单点击 ──
void GameEngine::handlePauseMenuClick(sf::Vector2i mousePos) {
    const float hitW = 320.f * 1.1f;
    const float hitH = 67.f * 1.8f;
    const float centerX = WINDOW_WIDTH * 0.5f;
    const float hitX = centerX - hitW * 0.5f;
    const float btnH = 67.f;
    const float hyOff = (hitH - btnH) * 0.5f;  // 检测区居中偏移

    const float btnY[4] = { 623.f, 760.f, 947.f, 1134.f };
    const float btnOffX[4] = { -100.f, 0.f, 100.f, 240.f };
    const float btnOffY[4] = { -50.f, 0.f, 0.f, 0.f };

    // 遍历四个按钮
    for (int i = 0; i < 4; ++i) {
        float bx = hitX + btnOffX[i];
        float by = btnY[i] - hyOff + btnOffY[i];
        if (mousePos.x > bx && mousePos.x < bx + hitW &&
            mousePos.y > by && mousePos.y < by + hitH) {
            switch (i) {
            case 0: // 认输放弃
                winReason = (currentTurn == 1) ? L"黑方认输，白方获胜！" : L"白方认输，黑方获胜！";
                isGameOver = true;
                isPaused = false; pauseFadeClock.restart();
                currentState = GameState::GAME_OVER;
                break;
            case 1: // 重开一局
                chessboard.reset();
                newCardJustDrawn = false;
                handSlotAssign.clear();
                attachedCardIndex = -1;
                isCardAttachedToMouse = false;
                isBusyAnimating = false;
                isSelectingPiece = false;
                selectPieceStep = 0;
                pendingDestroys.clear();
                isBulkDestroying = false;
                isReturningHandToDeck = false; returnDeckInit = false; returnDeckFrags.clear(); returnDeckPos.clear(); returnDeckTex.clear(); curseRemoving = false; curseRemovingIdx = -1; curseRemoveFrags.clear(); curseRemoveFragsInit = false; cardDecayFrags.clear(); cardDecayTex = 0; cardBirthActive = false; cardBirthFrags.clear(); cardBirthInit = false; showcaseFragsInit = false; showcaseFrags.clear(); yiDiShiQinActive = false; yiDiShiQinResponded = false; yiDiShiQinPenalizing = false; yiDiShiQinPenalty = 0;
                if (newCardSprite) newCardSprite->setPosition({2260.f, 160.f});
                battleMusic.stop();
                battleMusic.play();
                battleMusic02.stop();
                battleMusicState = BattleMusicState::NORMAL;
                battleMusic.setVolume(BATTLE_VOLUME);
                winMusic.stop();
                playerDeck.resetDeck();
            memset(infected, 0, sizeof(infected)); memset(infectionActive, 0, sizeof(infectionActive)); quarantineTimer = -1; plagueOwner = 0;
            aiCardPlayState = AICardPlayState::IDLE; aiPlayingCardIndex = -1; aiCardIsPurpleTransfer = false;
                currentTurn = 1;
                initActionPointsForTurn();
                showcaseState = CardShowcaseState::NONE;
                isAiThinking = false;
                isGameOver = false;
                playerFramePieces = 0; enemyFramePieces = 0; fpAnimState = FramePieceAnimState::IDLE; fpAnimFrags.clear(); pendingFramePieces.clear();
                savedThisGame = false;
                isPaused = false; pauseFadeClock.restart();
                turnClock.restart();
                turnTimePaused = 0.f;
                isTurnPaused = false;
                if (currentState == GameState::GAME_PVE && playerColorPref == 2) {
                    isAiThinking = true;
                    aiThinkClock.restart();
                }
                playerInvincible = false;
                playerInvinciblePlus = false;
                showAIDebug = false;
                break;
            case 2: // 返回主页
                battleMusic.stop();
                battleMusic02.stop();
                battleMusicState = BattleMusicState::NORMAL;
                winMusic.stop();
                failMusic.stop();
                isPaused = false; pauseFadeClock.restart();
                isBusyAnimating = false;
                isSelectingPiece = false;
                selectPieceStep = 0;
                showcaseState = CardShowcaseState::NONE;
                annihilateState = CardAnnihilateState::NONE;
                isCardAttachedToMouse = false;
                attachedCardIndex = -1;
                handSlotAssign.clear();
                currentTurnActionPoints.clear();
                playerInvincible = false;
                playerInvinciblePlus = false;
                showAIDebug = false;
                if (newCardSprite) newCardSprite->setPosition({2260.f, 160.f});
                currentState = GameState::MENU;
                break;
            case 3: // 退出游戏
                window.close();
                break;
            }
            return;
        }
    }
}

void GameEngine::handleSettingsClick(sf::Vector2i mousePos) {
    // 与主菜单一致的检测区：宽 320×1.1=352, 高 67×1.8=120.6
    const float centerX = WINDOW_WIDTH * 0.5f;
    const float hitWidth  = 320.f * 1.1f;
    const float hitHeight = 67.f * 1.8f;
    const float hitX = centerX - hitWidth * 0.5f;
    const float hitRX = centerX + hitWidth * 0.5f;
    const float menuStartY = 543.f;
    const float spacing = 137.f;

    if (mousePos.x > hitX && mousePos.x < hitRX) {
        if (mousePos.y > menuStartY && mousePos.y < menuStartY + hitHeight) {
            isProfessionalMode = !isProfessionalMode;
            std::cout << "[Info] Professional mode: " << (isProfessionalMode ? "ON" : "OFF") << std::endl;
        }
        else if (mousePos.y > menuStartY + spacing && mousePos.y < menuStartY + spacing + hitHeight) {
            toggleFullscreen();
        }
        else if (mousePos.y > menuStartY + spacing * 2 && mousePos.y < menuStartY + spacing * 2 + hitHeight) {
            currentState = GameState::MENU;
        }
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
    // ── 疫病系统：每回合开始时处理 ──
    {
        static int lastProcessedTurn = -1;
        if ((currentState == GameState::GAME_PVP || currentState == GameState::GAME_PVE) &&
            currentTurn != lastProcessedTurn && !isGameOver) {
            lastProcessedTurn = currentTurn;
            processInfection();
            // processPurpleCurses 移至 settleActionPoints 确保每次换边都触发
        }
    }

    // 🌟 画中画淡入淡出（首1s淡入，末0.5s淡出）
    {
        float ft = pipFadeClock.getElapsedTime().asSeconds();
        if (ft < 1.f)          pipFadeAlpha = ft;               // 淡入
        else if (pipTotalDur - ft < 1.f) pipFadeAlpha = (pipTotalDur - ft) / 1.f;  // 淡出
        else                   pipFadeAlpha = 1.f;               // 全显
    }

    // 🌟 PIP 边界反弹淡出/淡入
    {
        float bt = pipBounceClock.getElapsedTime().asSeconds();
        switch (pipBounce) {
        case BOUNCE_OUT:
            pipBounceAlpha = 1.f - bt / 0.5f; if (pipBounceAlpha < 0.f) pipBounceAlpha = 0.f;
            if (bt >= 0.5f) { pipBounce = BOUNCE_HIDE; pipBounceClock.restart(); }
            break;
        case BOUNCE_HIDE:
            pipBounceAlpha = 0.f;
            if (bt >= 2.f) { pipBounce = BOUNCE_IN; pipBounceClock.restart(); }
            break;
        case BOUNCE_IN:
            pipBounceAlpha = bt / 0.5f; if (pipBounceAlpha > 1.f) pipBounceAlpha = 1.f;
            if (bt >= 0.5f) { pipBounce = BOUNCE_NONE; pipBounceAlpha = 1.f; }
            break;
        default: break;
        }
    }

    // 🌟 画中画匀速往返 (320↔2240, 50px/s)
    {
        const float SPEED = 50.f;
        const float LEFT = 320.f, RIGHT = 2240.f;
        float range = RIGHT - LEFT;
        float period = 2.f * range / SPEED;
        float t = fmodf(pipClock.getElapsedTime().asSeconds(), period);
        float prevX = pipPos.x;
        pipPos.x = (t < period / 2.f)
            ? LEFT  + SPEED * t
            : RIGHT - SPEED * (t - period / 2.f);
        pipPos.y = 540.f;
        // 检测到达边界
        if (pipBounce == BOUNCE_NONE) {
            float margin = 3.f;
            if ((prevX > LEFT + margin && pipPos.x <= LEFT + margin) ||
                (prevX < LEFT + margin && pipPos.x >= LEFT + margin && prevX > 0)) {
                // 到达左边界
            }
            if ((prevX < RIGHT - margin && pipPos.x >= RIGHT - margin) ||
                (prevX > RIGHT - margin && pipPos.x <= RIGHT - margin && prevX > LEFT)) {
                // 到达右边界
            }
            // 简化：距离边界 < 2px 时触发
            if (pipPos.x <= LEFT + 2.f || pipPos.x >= RIGHT - 2.f) {
                pipBounce = BOUNCE_OUT;
                pipBounceClock.restart();
            }
        }
    }

    // 🌟【全状态音频状态机】：完美互斥控制所有 5 首 BGM 的播放

    if (currentState == GameState::MENU ||
        currentState == GameState::SETTINGS ||
        currentState == GameState::HELP ||
        currentState == GameState::HISTORY) {
        if (static_cast<int>(menuMusic.getStatus()) != 2) menuMusic.play();
        if (static_cast<int>(configMusic.getStatus()) == 2) configMusic.stop();
        if (static_cast<int>(battleMusic.getStatus()) == 2) battleMusic.stop();
        if (static_cast<int>(battleMusic02.getStatus()) == 2) battleMusic02.stop();
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

        // ── 战斗音乐状态机（串行过渡：先淡出→停→再淡入，两音轨永不重叠）──
        switch (battleMusicState) {
            case BattleMusicState::NORMAL:
                if (static_cast<int>(battleMusic.getStatus()) != 2) battleMusic.play();
                battleMusic.setVolume(BATTLE_VOLUME);
                break;

            case BattleMusicState::SWITCHING_TO_02: {
                // Phase 1 (0~1s): 淡出 battleMusic
                // Phase 2 (1~2s): 停 battleMusic，淡入 battleMusic02
                float t = crossfadeClock.getElapsedTime().asSeconds();
                if (t < 1.0f) {
                    battleMusic.setVolume(BATTLE_VOLUME * (1.f - t));
                    if (static_cast<int>(battleMusic.getStatus()) != 2) battleMusic.play();
                } else if (t < 2.0f) {
                    if (static_cast<int>(battleMusic.getStatus()) == 2) battleMusic.stop();
                    float t2 = t - 1.0f;
                    battleMusic02.setVolume(BATTLE_VOLUME * t2);
                    if (static_cast<int>(battleMusic02.getStatus()) != 2) battleMusic02.play();
                } else {
                    battleMusic02.setVolume(BATTLE_VOLUME);
                    battleMusicState = BattleMusicState::BATTLE_02;
                    battle02Remaining = BATTLE_02_BASE_TIME;
                    battleTimer.restart();
                }
                break;
            }

            case BattleMusicState::BATTLE_02:
                if (static_cast<int>(battleMusic02.getStatus()) != 2) battleMusic02.play();
                battleMusic02.setVolume(BATTLE_VOLUME);
                if (battleTimer.getElapsedTime().asSeconds() >= battle02Remaining) {
                    battleMusicState = BattleMusicState::SWITCHING_TO_01;
                    crossfadeClock.restart();
                }
                break;

            case BattleMusicState::SWITCHING_TO_01: {
                // Phase 1 (0~1s): 淡出 battleMusic02
                // Phase 2 (1~2s): 停 battleMusic02，淡入 battleMusic
                float t = crossfadeClock.getElapsedTime().asSeconds();
                if (t < 1.0f) {
                    battleMusic02.setVolume(BATTLE_VOLUME * (1.f - t));
                    if (static_cast<int>(battleMusic02.getStatus()) != 2) battleMusic02.play();
                } else if (t < 2.0f) {
                    if (static_cast<int>(battleMusic02.getStatus()) == 2) battleMusic02.stop();
                    float t2 = t - 1.0f;
                    battleMusic.setVolume(BATTLE_VOLUME * t2);
                    if (static_cast<int>(battleMusic.getStatus()) != 2) battleMusic.play();
                } else {
                    battleMusic.setVolume(BATTLE_VOLUME);
                    battleMusicState = BattleMusicState::NORMAL;
                }
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
        // 判定条件1b：🌟【胜利制重构】PVE 帧棋子获胜
        else if (winReason == L"我方获胜") {
            isPlayerWon = true;
        }
        else if (winReason == L"敌方获胜") {
            isPlayerWon = false;
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

    // 🌟 破釜沉舟退牌动画完成处理
    if (isReturningHandToDeck && returnHandToDeckClock.getElapsedTime().asSeconds() >= 2.0f) {
        // 碎片移到衰减缓冲区（继续播放直到自然淡出）
        for (auto& frags : returnDeckFrags)
            for (auto& f : frags)
                if (f.alpha > 0.f) cardDecayFrags.push_back(f);
        if (!returnDeckTex.empty()) cardDecayTex = returnDeckTex[0];
        // 退牌动画结束，正式将手牌移回牌库
        for (size_t i = 0; i < playerDeck.hand.size(); ++i)
            playerDeck.deck.push_back(playerDeck.hand[i]);
        playerDeck.hand.clear();
        handSlotAssign.clear();
        attachedCardIndex = -1;
        isReturningHandToDeck = false; returnDeckInit = false; returnDeckFrags.clear(); returnDeckPos.clear(); returnDeckTex.clear();
        std::cout << "[Sacrifice] 退牌动画完成，手牌已返还牌库。" << std::endl;
        // 进入销毁阶段
        executeSacrificeDestroy(pendingSacrificeDestroys);
    }

    // 🌟 紫卡诅咒到期移除动画完成
    if (curseRemoving && curseRemoveClock.getElapsedTime().asSeconds() >= 2.0f) {
        // 碎片移到衰减缓冲区
        for (auto& f : curseRemoveFrags)
            if (f.alpha > 0.f) cardDecayFrags.push_back(f);
        cardDecayTex = curseRemoveTex;
        // 正式移除卡牌
        if (curseRemovingIdx >= 0 && curseRemovingIdx < (int)playerDeck.hand.size()) {
            playerDeck.hand.erase(playerDeck.hand.begin() + curseRemovingIdx);
            handSlotAssign.erase(handSlotAssign.begin() + curseRemovingIdx);
            if (curseRemovingIdx == attachedCardIndex) attachedCardIndex = -1;
        }
        curseRemoving = false;
        curseRemovingIdx = -1;
        curseRemoveFrags.clear();
        curseRemoveFragsInit = false;
        std::cout << "[Blind] 玩家盲目移除动画完成" << std::endl;
    }

    // 🎬 棋子动画完成处理（笼络 + 批量销毁）
    if (chessboard.updatePieceAnim()) {
        int ar = chessboard.animRow, ac = chessboard.animCol;
        bool handled = false;
        // ── 破釜沉舟批量销毁链 ──
        if (isBulkDestroying) {
            chessboard.placePieceByAI(ar, ac, 0, ar, ac);
            chessboard.recordMove(ar, ac, 0);
            chessboard.finishPieceAnim();
            pendingDestroys.pop_back();
            handled = true;
            if (!pendingDestroys.empty()) {
                auto& p = pendingDestroys.back();
                chessboard.startDestroyAnim(p.first, p.second);
            } else {
                isBulkDestroying = false;
            isReturningHandToDeck = false; returnDeckInit = false; returnDeckFrags.clear(); returnDeckPos.clear(); returnDeckTex.clear(); curseRemoving = false; curseRemovingIdx = -1; curseRemoveFrags.clear(); curseRemoveFragsInit = false; cardDecayFrags.clear(); cardDecayTex = 0; cardBirthActive = false; cardBirthFrags.clear(); cardBirthInit = false; showcaseFragsInit = false; showcaseFrags.clear(); yiDiShiQinActive = false; yiDiShiQinResponded = false; yiDiShiQinPenalizing = false; yiDiShiQinPenalty = 0;
                isBusyAnimating = false;
                if (isTurnPaused) {
                    turnTimePaused += pauseClock.getElapsedTime().asSeconds();
                    isTurnPaused = false;
                }
                consumeActionPoint(false);
                settleActionPoints();
                std::cout << "[Sacrifice] 批量销毁完成。" << std::endl;
            }
        }
        // ── 笼络 ──
        else if (selectPieceStep == 2) {
            chessboard.placePieceByAI(ar, ac, 0, ar, ac);
            chessboard.recordMove(ar, ac, 0);
            chessboard.finishPieceAnim();
            std::cout << "[Convert] 销毁完成。" << std::endl;
            // AI 自动进入转化阶段（判断发起方是否为 AI）
            bool aiStarted = (currentState == GameState::GAME_PVE && selectPiecePlayer != playerColorPref);
            if (aiStarted && aiPlayer) {
                auto enemy = aiPlayer->chooseEnemyPieceToConvert(chessboard);
                if (enemy.first >= 0) {
                    chessboard.startConvertAnim(enemy.first, enemy.second,
                        (selectPiecePlayer == 1) ? 2 : 1, selectPiecePlayer);
                    isBusyAnimating = true;
                    selectPieceStep = 3;
                    std::cout << "[AI] 笼络自动选敌方: (" << enemy.first << "," << enemy.second << ")" << std::endl;
                } else {
                    isBusyAnimating = false;
                    isSelectingPiece = false;
                }
            } else {
                isBusyAnimating = false;
            }
            handled = true;
        } else if (selectPieceStep == 3) {
            chessboard.placePieceByAI(ar, ac, selectPiecePlayer, ar, ac);
            chessboard.recordMove(ar, ac, selectPiecePlayer);
            chessboard.finishPieceAnim();
            selectPieceStep = 0;
            isSelectingPiece = false;
            isBusyAnimating = false;
            // 若 AI 笼络在 SHOWCASING 期间完成，清理残留状态
            if (aiCardPlayState == AICardPlayState::SHOWCASING) {
                aiCardPlayState = AICardPlayState::IDLE;
            }
            isAiThinking = false;
            if (isTurnPaused) {
                turnTimePaused += pauseClock.getElapsedTime().asSeconds();
                isTurnPaused = false;
            }
            consumeActionPoint(false);
            handled = true;
            // 🌟【胜利制重构】多方向检测，每方向放一个帧棋子
            for (int pass = 0; pass < 4; ++pass) {
                if (!chessboard.checkWin(ar, ac)) break;
                int sr, sc, er, ec;
                chessboard.getWinLine(sr, sc, er, ec);
                chessboard.markWinCellsAsScored(sr, sc, er, ec);
                awardFramePiece(selectPiecePlayer);
                chessboard.clearWinLine();
            }
            if (!isGameOver && !isBusyAnimating) {
                settleActionPoints();
            }
            std::cout << "[Convert] 转化完成！" << std::endl;
        }
        if (!handled) {
            chessboard.finishPieceAnim(); // 兜底：任何遗漏情况都清理动画
        }
    }

    // 暂停时冻结对局进程
    if (isPaused && (currentState == GameState::GAME_PVP || currentState == GameState::GAME_PVE)) {
        return;
    }

    float elapsedSeconds = getEffectiveTurnTime();
    if (elapsedSeconds >= 45.f && !playerInvincible) {
        winReason = (currentTurn == 1) ? L"黑方思考超时，白方获胜！" : L"白方思考超时，黑方获胜！";
        isGameOver = true;
        currentState = GameState::GAME_OVER; // 🌟 触发游戏结束状态
        return;
    }

    // 紫卡传送动画驱动
    if (purpleSendState != PurpleSendState::IDLE) {
        updatePurpleCardSend();
    }

    // AI 出牌动画驱动（无论谁回合都要跑）
    if (aiCardPlayState != AICardPlayState::IDLE) {
        updateAICardAnimation();
    }

    // ── AI 回合状态机 ──
    if (currentState == GameState::GAME_PVE && currentTurn != playerColorPref) {
        // ── AI 疫病自动选子 ──
        if (isSelectingPiece && selectPieceStep == 10 && aiPlayer) {
            auto enemy = aiPlayer->chooseEnemyPieceToConvert(chessboard);
            if (enemy.first >= 0) {
                infected[enemy.first][enemy.second] = true; infectionActive[enemy.first][enemy.second] = false;
                plagueOwner = currentTurn;
                isSelectingPiece = false;
                selectPieceStep = 0;
                std::cout << "[AI] 疫病自动感染: (" << enemy.first << "," << enemy.second << ")" << std::endl;
            }
            return;
        }

        // ── AI 笼络自动选子（优先于卡片动画）──
        if (isSelectingPiece && selectPieceStep == 1 && aiPlayer) {
            auto own = aiPlayer->chooseOwnPieceToSacrifice(chessboard);
            if (own.first >= 0) {
                chessboard.startDestroyAnim(own.first, own.second);
                isBusyAnimating = true;
                selectPieceStep = 2;
                std::cout << "[AI] 笼络自动选己方: (" << own.first << "," << own.second << ")" << std::endl;
            }
            return;
        }

        // AI 出牌动画驱动（即使 isBusyAnimating 也需运行）
        if (aiCardPlayState != AICardPlayState::IDLE) {
            updateAICardAnimation();
            return;
        }

        // 动画进行中则暂停 AI 决策
        if (isBusyAnimating) return;

        if (!aiPlayer) {
            std::cerr << "[Warning] aiPlayer was null; recreating..." << std::endl;
            int aiColor = (playerColorPref == 1) ? 2 : 1;
            aiPlayer = new AIPlayer(aiColor, aiDifficultySetting);
        }

        // 🌟 AI 以地事秦回应：仅回合首次进入时判断（isAiThinking仍为false）
        if (!isAiThinking && yiDiShiQinActive && !yiDiShiQinResponded && !yiDiShiQinPenalizing) {
            std::vector<int> orangeIdxs;
            for (size_t oi = 0; oi < playerDeck.aiHand.size(); ++oi)
                if (playerDeck.aiHand[oi].cardColor == 0) orangeIdxs.push_back((int)oi);
            if (!orangeIdxs.empty() && (rand() % 100) < 70) {
                int pick = orangeIdxs[rand() % orangeIdxs.size()];
                yiDiShiQinResponded = true;
                Card sent = playerDeck.aiHand[pick];
                playerDeck.aiHand.erase(playerDeck.aiHand.begin() + pick);
                playerDeck.hand.push_back(sent);
                handSlotAssign.push_back(1);
                newCardJustDrawn = true; cardAnimClock.restart(); isAnimatingCard = true;
                cardBirthActive = true; cardBirthClock.restart(); cardBirthInit = false;
                std::cout << "[以地事秦] AI送出橙卡回应！" << std::endl;
                consumeActionPoint(false);
                settleActionPoints();
                return;
            }
        }

        if (!isAiThinking) {
            isAiThinking = true;
            aiThinkClock.restart();
        }

        // 等待思考时间
        if (aiThinkClock.getElapsedTime().asSeconds() < 0.8f) return;

        // ── 盲目效果：AI 随机决策 ──
        bool aiBlindActive = false;
        for (const auto& c : playerDeck.aiHand)
            if (c.cardColor == 1 && c.transferred && c.effect == CardEffect::BLIND)
                { aiBlindActive = true; break; }

        // ── 决策：先出牌再下棋？──
        bool hasCardAP = hasValidActionPoint(false);
        bool shouldPlay = hasCardAP && !playerDeck.aiHand.empty() && !aiOnlyDrop;
        if (aiOnlyCard) shouldPlay = hasCardAP && !playerDeck.aiHand.empty();
        if (shouldPlay && !aiBlindActive && !aiOnlyCard) {
            shouldPlay = aiPlayer->shouldPlayCard(playerDeck.aiHand, chessboard, hasCardAP) &&
                         aiPlayer->shouldPlayBeforeDrop(playerDeck.aiHand, chessboard);
        } else if (shouldPlay && aiBlindActive) {
            shouldPlay = (rand() % 100) < 40;
        }
        if (shouldPlay) {
            int idx = aiBlindActive ? (rand() % (int)playerDeck.aiHand.size())
                                    : aiPlayer->chooseCardToPlay(playerDeck.aiHand, chessboard);
            if (aiOnlyCard && idx < 0 && !playerDeck.aiHand.empty())
                idx = rand() % (int)playerDeck.aiHand.size(); // 强制出牌兜底
            if (idx >= 0 && idx < static_cast<int>(playerDeck.aiHand.size())) {
                startAICardPlay(idx);
                return;
            }
        }

        // ── AI 下棋 ──
        if (aiOnlyCard && !playerDeck.aiHand.empty()) return; // 有手牌时跳过下棋
        auto move = aiPlayer->calculateBestMove(chessboard);
        int aiRow = move.first;
        int aiCol = move.second;

        if (aiRow >= 0 && aiRow < 15 && aiCol >= 0 && aiCol < 15) {
            chessboard.placePieceByAI(aiRow, aiCol, currentTurn, aiRow, aiCol);
            chessboard.startDropAnim(aiRow, aiCol, currentTurn);

            if (dropSound) {
                dropSound->play();
                std::cout << "[Audio] AI Drop sound played." << std::endl;
            }

            chessboard.recordMove(aiRow, aiCol, currentTurn);
            isAiThinking = false;

            // 消耗行动点
            consumeActionPoint(true);

            // Pattern 检测 → AI 抽牌
            int dir = -1;
            int pattern = chessboard.checkPattern(aiRow, aiCol, dir);
            // 盲目延后：AI 三子连星延长移除回合
            if (pattern >= 3) {
                for (auto& c : playerDeck.aiHand)
                    if (c.cardColor == 1 && c.transferred && c.effect == CardEffect::BLIND)
                        { c.value++; std::cout << "[Blind] AI 三连，盲目延后至 " << c.value << " 回合" << std::endl; }
            }
            if (pattern == 3 || pattern == 4) {
                size_t aiBefore = playerDeck.aiHand.size();
                playerDeck.drawCardForAI();
                if (playerDeck.aiHand.size() > aiBefore) {
                    chessboard.markPatternUsed(aiRow, aiCol, dir);
                }
                std::cout << "[AI] 达成 " << pattern << " 连，抽牌！AI手牌: " << playerDeck.aiHand.size() << std::endl;
            }

            // AI 四子连星不影响战斗音乐

            // 🌟【胜利制重构】多方向5连检测，每方向放一个帧棋子
            int aiColor = (playerColorPref == 1) ? 2 : 1;
            for (int pass = 0; pass < 4; ++pass) {
                if (!chessboard.checkWin(aiRow, aiCol)) break;
                int sr, sc, er, ec;
                chessboard.getWinLine(sr, sc, er, ec);
                chessboard.markWinCellsAsScored(sr, sc, er, ec);
                awardFramePiece(aiColor);
                chessboard.clearWinLine();
            }
            if (isGameOver) {
                std::cout << "[Info] AI wins at (" << aiRow << "," << aiCol << ")" << std::endl;
                return;
            }
            // 帧棋子动画播放中，暂停 AI 决策
            if (isBusyAnimating) return;

            // ── 决策：下棋后出牌？──
            hasCardAP = hasValidActionPoint(false);
            bool shouldPlayAfter = hasCardAP && !playerDeck.aiHand.empty() && !aiOnlyDrop;
            if (aiOnlyCard) shouldPlayAfter = hasCardAP && !playerDeck.aiHand.empty();
            if (shouldPlayAfter && !aiBlindActive && !aiOnlyCard)
                shouldPlayAfter = aiPlayer->shouldPlayCard(playerDeck.aiHand, chessboard, hasCardAP);
            else if (shouldPlayAfter && aiBlindActive)
                shouldPlayAfter = (rand() % 100) < 40;
            if (shouldPlayAfter) {
                int idx = aiBlindActive ? (rand() % (int)playerDeck.aiHand.size())
                                        : aiPlayer->chooseCardToPlay(playerDeck.aiHand, chessboard);
                if (aiOnlyCard && idx < 0 && !playerDeck.aiHand.empty())
                    idx = rand() % (int)playerDeck.aiHand.size();
                if (idx >= 0 && idx < static_cast<int>(playerDeck.aiHand.size())) {
                    startAICardPlay(idx);
                    return;
                }
            }

            // 结算行动点
            settleActionPoints();

            // FORCE_DROP 场景：AP 未耗尽，AI 继续回合
            if (currentState == GameState::GAME_PVE && currentTurn != playerColorPref) {
                isAiThinking = true;
                aiThinkClock.restart();
                std::cout << "[AI] 仍有行动点，继续回合" << std::endl;
            }
        } else {
            std::cerr << "[Error] AI returned invalid move (" << aiRow << "," << aiCol << ")." << std::endl;
            isAiThinking = false;
            currentTurn = playerColorPref;
            turnClock.restart();
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
    // 先清全屏黑 → 黑边；再在 2560×1440 区域画游戏背景
    window.clear(sf::Color::Black);
    window.setView(gameView);
    sf::RectangleShape gameBG({2560.f, 1440.f});
    gameBG.setFillColor(sf::Color(250, 215, 175));
    window.draw(gameBG);

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

    // 🌟 界面切换过渡遮罩
    if (transitioning) {
        float t2 = transitionClock.getElapsedTime().asSeconds();
        float alpha = (t2 < 0.25f) ? 255.f * (t2 / 0.25f) : 255.f * (1.f - (t2 - 0.25f) / 0.25f);
        if (alpha > 0.f) {
            sf::RectangleShape overlay({2560.f, 1440.f});
            overlay.setPosition({0.f, 0.f});
            overlay.setFillColor(sf::Color(0, 0, 0, (uint8_t)alpha));
            window.draw(overlay);
        }
    }

    window.display();
}

void GameEngine::renderMenu() {
    // ── 第 0 层：星空帧动画背景（25fps, 播完循环）──
    // ── Universe 背景（本界面独立加载，每帧缓存）──
    {
        static GameState lastBgState = GameState::MENU;
        if (currentState != lastBgState) { bgCurrentFrame = -1; lastBgState = currentState; }
        float bgElapsed = bgFrameClock.getElapsedTime().asSeconds();
        int frame = static_cast<int>(bgElapsed * 25.f) % BG_TOTAL_FRAMES + 1;
        if (frame != bgCurrentFrame) {
            char path[128]; snprintf(path, sizeof(path),
                "assets/Universe/frame_%05d.jpg", frame);
            if (bgTexA.loadFromFile(getEngineAssetPath(path))) {
                bgSprA.setTexture(bgTexA, true);
                float sw = 2560.f / bgTexA.getSize().x;
                float sh = 1440.f / bgTexA.getSize().y;
                bgSprA.setScale({sw, sh});
                bgSprA.setPosition({0.f, 0.f});
                bgSprA.setColor(sf::Color(255, 255, 255, 255));
            }
            bgCurrentFrame = frame;
        }
        window.draw(bgSprA);
    }

    // ── 抛物线动画计算 ──
    const float DURATION = 30.f;  // 单程 30 秒
    float elapsed = menuPieceClock.getElapsedTime().asSeconds();
    float raw = fmodf(elapsed, 2.f * DURATION);
    float t = (raw < DURATION) ? (raw / DURATION) : (2.f - raw / DURATION);  // 0→1→0 循环
    float et = t * t * (3.f - 2.f * t);  // smoothstep
    float arc = 300.f * sinf(3.14159f * et);  // 抛物线弧高 300px

    // 旋转：2°/s，白顺黑逆
    float rotAngle = elapsed * 3.f;

    // 白子：左下 (50,1480) → 右上 (1840,0)
    float wx = 50.f  + (1840.f - 50.f)  * et;
    float wy = 1480.f + (0.f - 1480.f) * et - arc;
    // 黑子：右上 (2510,-40) → 左下 (720,1440)
    float bx = 2510.f + (720.f - 2510.f) * et;
    float by = -40.f  + (1440.f - (-40.f)) * et - arc;

    // ── 第 1 层：白子 ──
    float ww = static_cast<float>(menuWhiteTex.getSize().x);
    float wh = static_cast<float>(menuWhiteTex.getSize().y);
    menuWhiteSpr.setScale({1013.f / ww, 1003.5f / wh});
    menuWhiteSpr.setPosition({wx, wy});
    menuWhiteSpr.setRotation(sf::degrees(rotAngle));       // 顺时针
    window.draw(menuWhiteSpr);

    // ── 第 2 层：画中画 (PIP) ──
    pipRT.clear(sf::Color::Transparent);
    pipRT.setView(pipView);

    float dt = pipAnimClock.getElapsedTime().asSeconds();

    // ── 动画 1：读卡器 + 卡牌 ──
    if (pipAnimIndex == 1 && cardReaderBottomSprite && cardReaderTopSprite) {
        float rs = 0.35f;  // 读卡器在 PIP 中的缩放
        cardReaderBottomSprite->setScale({rs, rs});
        cardReaderBottomSprite->setPosition({640.f, 100.f});
        pipRT.draw(*cardReaderBottomSprite);
        cardReaderBottomSprite->setScale({0.8f, 0.8f});  // 恢复
        cardReaderBottomSprite->setPosition({300.f, 210.f}); // 恢复

        // ── 第 2 层：卡牌动画 ──
        float cs = rs * 0.45f;
        float cardX = 640.f, cardY = 360.f;
        float cardScale = cs;
        uint8_t cardAlpha = 255;
        bool showCard = true;
        bool showReader = true;

        // ──── 动画 1：连击读卡演示 ────
        if (pipAnimIndex == 1) {
            switch (pipAnimState) {
            case 0: { // MOVE_DOWN (2s)
                float targetY = 200.f, startY = 360.f, dur = 2.f;
                float t = (dt < dur) ? dt / dur : 1.f;
                t = t * t * (3.f - 2.f * t);
                cardY = startY + (targetY - startY) * t;
                if (dt >= dur + 0.2f) { pipAnimState = 1; pipAnimClock.restart(); }
                break;
            }
            case 1: { // INSERT + SHATTER：边插入读卡器边粉碎 (0.5s)
                if (!pipFragsInit) {
                    pipFrags = cardFragCache; pipFragsTex = 0;
                    for (auto& frag : pipFrags) {
                        frag.released = false; frag.alpha = 255.f; frag.fadeTimer = 0.f;
                        frag.rotation = 0.f;
                        frag.vel = {(rand()%160-80)*1.f, -(rand()%100+50)*1.f};
                        frag.rotSpeed = (rand()%720-360)*1.f;
                        frag.pos = {640.f, 200.f};
                    }
                    pipFragsInit = true; pipFragsLastT = 0.f;
                }
                float t = dt / 0.8f; if (t > 1.f) t = 1.f;
                float st = t * t * (3.f - 2.f * t);
                float curY = 200.f + (100.f - 200.f) * st;
                cardY = 100.f;
                float pipClipH = newCardTexture.getSize().y * (1.f - st);
                float pipScale = cs;
                int clipH = (int)pipClipH;
                if (clipH > 0) {
                    newCardSprite->setScale({pipScale, pipScale});
                    newCardSprite->setOrigin({newCardTexture.getSize().x/2.f, newCardTexture.getSize().y/2.f});
                    newCardSprite->setTextureRect(sf::IntRect({0,0}, {(int)newCardTexture.getSize().x, clipH}));
                    newCardSprite->setPosition({640.f, curY});
                    newCardSprite->setColor(sf::Color(255,255,255,255));
                    pipRT.draw(*newCardSprite);
                }
                float dtF = t - pipFragsLastT; pipFragsLastT = t;
                if (dtF < 0.f) dtF = 0.f; if (dtF > 0.1f) dtF = 0.08f;
                for (auto& frag : pipFrags) {
                    if (!frag.released && (float)frag.texRect.position.y > pipClipH) {
                        frag.released = true;
                        float fcx=frag.texRect.size.x/2.f, fcy=frag.texRect.size.y/2.f;
                        sf::Vector2u ts = newCardTexture.getSize();
                        frag.pos.x = 640.f + (frag.texRect.position.x + fcx - ts.x/2.f) * pipScale;
                        frag.pos.y = curY + (frag.texRect.position.y + fcy - ts.y/2.f) * pipScale;
                    }
                    if (frag.released) {
                        frag.vel.y += 800.f*dtF; frag.pos += frag.vel*dtF;
                        frag.rotation += frag.rotSpeed*dtF;
                        frag.fadeTimer += dtF; float fp=frag.fadeTimer/0.85f; if(fp>1.f)fp=1.f;
                        frag.alpha = 255.f*(1.f-fp*fp*(3.f-2.f*fp));
                        float fcx=frag.texRect.size.x/2.f, fcy=frag.texRect.size.y/2.f;
                        newCardSprite->setOrigin({fcx,fcy}); newCardSprite->setTextureRect(frag.texRect);
                        newCardSprite->setPosition(frag.pos); newCardSprite->setRotation(sf::degrees(frag.rotation));
                        newCardSprite->setColor(sf::Color(255,255,255,(uint8_t)frag.alpha));
                        pipRT.draw(*newCardSprite);
                        uint8_t g=(uint8_t)((1.f-fp)*100.f);
                        if(g>0){newCardSprite->setColor(sf::Color(g,g,g,(uint8_t)frag.alpha));pipRT.draw(*newCardSprite,sf::BlendAdd);}
                    }
                }
                newCardSprite->setOrigin({newCardTexture.getSize().x/2.f, newCardTexture.getSize().y/2.f});
                // 文字逐行淡出：每行根据卡牌上的位置映射擦除阈值
                {   auto txtFade = [&](float screenY) -> uint8_t {
                        float texY = newCardTexture.getSize().y/2.f + (screenY - curY) / pipScale;
                        float threshold = texY / newCardTexture.getSize().y;
                        float clipFrac = 1.f - st; // 擦除线位置（1=完整, 0=全碎）
                        if (threshold < clipFrac - 0.03f) return 255;
                        if (threshold > clipFrac + 0.03f) return 0;
                        return (uint8_t)(255.f * (clipFrac + 0.03f - threshold) / 0.06f);
                    };
                    uiText.setFont(cardFont);
                    float scT = pipScale / 0.36f;
                    uint8_t na = txtFade(curY - 164.f * scT);
                    if (na > 0) { uiText.setCharacterSize((int)(36.f * scT)); uiText.setFillColor(sf::Color(255,255,255,na));
                        uiText.setString(L"连击"); uiText.setPosition({640.f - 52.f * scT, curY - 164.f * scT}); pipRT.draw(uiText); }
                    uint8_t da = txtFade(curY - 90.f * scT);
                    if (da > 0) { uiText.setCharacterSize((int)(27.f * scT)); uiText.setFillColor(sf::Color(230,230,230,da));
                        uiText.setString(L"给予两次落子数"); uiText.setPosition({640.f - 88.f * scT, curY - 90.f * scT}); pipRT.draw(uiText); }
                    uiText.setFont(font);
                }
                showCard = false;
                if (dt >= 0.8f) {
                    for (auto& f : pipFrags) pipDecayFrags.push_back(f);
                    pipAnimState = 2; pipAnimClock.restart(); pipFragsInit = false;
                }
                break;
            }
            case 2: // SHOW_PAUSE (0.3s)
                showCard = false;
                if (dt >= 0.3f) { pipAnimState = 3; pipAnimClock.restart(); }
                break;
            case 3: { // SHOW_APPEAR → 碎片聚合
                if (!pipShowFragsInit) {
                    std::cout << "[PIP Show] 初始化展示碎片" << std::endl;
                    pipShowFrags = cardFragCache;
                    for (auto& frag : pipShowFrags) {
                        frag.released = false; frag.alpha = 0.f; frag.fadeTimer = 0.f;
                        frag.rotation = (rand()%60-30)*1.f;
                        frag.pos.x = 640.f + (rand()%200-100)*1.f;
                        frag.pos.y = 650.f + (rand()%80)*1.f;
                    }
                    pipShowFragsInit = true; pipFragsLastT = 0.f;
                }
                float t = dt / 1.2f; if (t > 1.f) t = 1.f;
                float pipScale2 = cs * 2.5f;
                float wipeY = newCardTexture.getSize().y * t;
                int visH = (int)wipeY;
                float dtF2 = t - pipFragsLastT; pipFragsLastT = t;
                if (dtF2 < 0.f) dtF2 = 0.f; if (dtF2 > 0.1f) dtF2 = 0.04f;
                newCardSprite->setScale({pipScale2, pipScale2});
                for (auto& frag : pipShowFrags) {
                    float fcx = frag.texRect.size.x/2.f, fcy = frag.texRect.size.y/2.f;
                    sf::Vector2u ts2 = newCardTexture.getSize();
                    float tx = 640.f + (frag.texRect.position.x + fcx - ts2.x/2.f) * pipScale2;
                    float ty = 450.f + (frag.texRect.position.y + fcy - ts2.y/2.f) * pipScale2;
                    if (!frag.released && (float)frag.texRect.position.y < wipeY + ts2.y*0.06f && (float)frag.texRect.position.y >= wipeY - ts2.y*0.02f) {
                        frag.released = true; frag.targetPos = {tx, ty}; frag.fadeTimer = 0.f;
                        frag.pos.x = tx + (rand()%40-20)*1.f; frag.pos.y = ty + (rand()%30+20)*1.f;
                    }
                    if (frag.released) {
                        frag.fadeTimer += dtF2; float prog = frag.fadeTimer/0.3f; if (prog>1.f) prog=1.f;
                        float eased = prog*prog*(3.f-2.f*prog);
                        frag.pos.x += (frag.targetPos.x-frag.pos.x)*0.12f;
                        frag.pos.y += (frag.targetPos.y-frag.pos.y)*0.12f;
                        frag.alpha = 255.f*eased;
                        newCardSprite->setOrigin({fcx,fcy}); newCardSprite->setTextureRect(frag.texRect);
                        newCardSprite->setPosition(frag.pos); newCardSprite->setRotation(sf::degrees(frag.rotation*(1.f-eased)));
                        newCardSprite->setColor(sf::Color(255,255,255,(uint8_t)frag.alpha));
                        pipRT.draw(*newCardSprite);
                        float gb = eased<1.f ? eased*60.f : 0.f;
                        if (gb>0.f){newCardSprite->setColor(sf::Color((uint8_t)gb,(uint8_t)gb,(uint8_t)gb,(uint8_t)frag.alpha));pipRT.draw(*newCardSprite, sf::BlendAdd);}
                    }
                }
                newCardSprite->setOrigin({newCardTexture.getSize().x/2.f, newCardTexture.getSize().y/2.f});
                newCardSprite->setTextureRect(sf::IntRect({0,0},{(int)newCardTexture.getSize().x,(int)newCardTexture.getSize().y}));
                newCardSprite->setRotation(sf::degrees(0.f));
                newCardSprite->setColor(sf::Color(255,255,255,255));
                // 完整裁剪纹理盖在碎片上消除分割线
                if (visH > 0) {
                    newCardSprite->setScale({pipScale2, pipScale2});
                    newCardSprite->setOrigin({newCardTexture.getSize().x/2.f, newCardTexture.getSize().y/2.f});
                    newCardSprite->setTextureRect(sf::IntRect({0,0}, {(int)newCardTexture.getSize().x, visH}));
                    newCardSprite->setPosition({640.f, 450.f});
                    newCardSprite->setColor(sf::Color(255,255,255,255));
                    pipRT.draw(*newCardSprite);
                }
                // 文字逐行淡入：每行根据卡牌上的位置映射擦除阈值
                {   auto txtFadeIn = [&](float screenY) -> uint8_t {
                        float texY = newCardTexture.getSize().y/2.f + (screenY - 450.f) / pipScale2;
                        float threshold = texY / newCardTexture.getSize().y;
                        if (t > threshold + 0.05f) return 255;
                        if (t < threshold - 0.05f) return 0;
                        return (uint8_t)(255.f * (t - threshold + 0.05f) / 0.1f);
                    };
                    uiText.setFont(cardFont);
                    float scS = pipScale2 / 0.36f;
                    uint8_t na = txtFadeIn(450.f - 164.f * scS);
                    if (na>0) { uiText.setCharacterSize((int)(36.f*scS)); uiText.setFillColor(sf::Color(255,255,255,na));
                        uiText.setString(L"连击"); uiText.setPosition({640.f-52.f*scS, 450.f-164.f*scS}); pipRT.draw(uiText); }
                    uint8_t da = txtFadeIn(450.f - 90.f * scS);
                    if (da>0) { uiText.setCharacterSize((int)(27.f*scS)); uiText.setFillColor(sf::Color(230,230,230,da));
                        uiText.setString(L"给予两次落子数"); uiText.setPosition({640.f-88.f*scS, 450.f-90.f*scS}); pipRT.draw(uiText); }
                    uiText.setFont(font);
                }
                showCard = false;
                if (dt >= 1.2f) { pipAnimState = 4; pipAnimClock.restart(); pipShowFragsInit = false; }
                break;
            }
            case 4: // SHOW_DISPLAY → 卡片定型
                cardX = 640.f; cardY = 450.f; cardScale = cs * 2.5f;
                if (dt >= 1.f) { pipAnimState = 5; pipAnimClock.restart(); }
                break;
            case 5: { // SHOW_FADE → 纯透明度淡化
                float t = dt / 0.5f; if (t > 1.f) t = 1.f;
                cardX = 640.f; cardY = 450.f; cardScale = cs * 2.5f;
            cardAlpha = (uint8_t)(255 * (1.f - t));
            if (dt >= 0.5f) { pipAnimNext(); pipFragsInit = false; }
                break;
            }
            }
        }  // end anim 1

        // 绘制卡牌（非特殊绘制的状态）
        if (showCard && cardAlpha > 0) {
            sf::Vector2u ct2 = newCardTexture.getSize();
            newCardSprite->setScale({cardScale, cardScale});
            newCardSprite->setPosition({cardX, cardY});
            newCardSprite->setTextureRect(sf::IntRect({0,0}, {(int)ct2.x, (int)ct2.y}));
            newCardSprite->setColor(sf::Color(255,255,255,cardAlpha));
            pipRT.draw(*newCardSprite);
            // 文字
            uiText.setFont(cardFont);
            uiText.setCharacterSize((int)(36.f * cardScale / 0.36f));
            uiText.setFillColor(sf::Color(255,255,255,cardAlpha));
            uiText.setString(L"连击");
            uiText.setPosition({cardX - 52.f * cardScale / 0.36f, cardY - 164.f * cardScale / 0.36f});
            pipRT.draw(uiText);
            uiText.setCharacterSize((int)(27.f * cardScale / 0.36f));
            uiText.setFillColor(sf::Color(230,230,230,cardAlpha));
            uiText.setString(L"给予两次落子数");
            uiText.setPosition({cardX - 88.f * cardScale / 0.36f, cardY - 90.f * cardScale / 0.36f});
            pipRT.draw(uiText);
            uiText.setFont(font);
        }
        newCardSprite->setScale({0.36f, 0.36f});

        // ── PIP 衰减碎片 ──
        if (!pipDecayFrags.empty()) {
            newCardSprite->setScale({cs, cs});
            for (size_t di = 0; di < pipDecayFrags.size(); ) {
                auto& frag = pipDecayFrags[di];
                if (frag.alpha > 0.f) {
                    frag.vel.y += 800.f * 0.016f; frag.pos += frag.vel * 0.016f;
                    frag.fadeTimer += 0.016f; float fp = frag.fadeTimer/0.85f; if (fp>1.f)fp=1.f;
                    frag.alpha = 255.f*(1.f-fp*fp*(3.f-2.f*fp));
                }
                if (frag.alpha <= 0.f) { pipDecayFrags.erase(pipDecayFrags.begin()+di); continue; }
                float fcx=frag.texRect.size.x/2.f, fcy=frag.texRect.size.y/2.f;
                newCardSprite->setOrigin({fcx,fcy}); newCardSprite->setTextureRect(frag.texRect);
                newCardSprite->setPosition(frag.pos);
                newCardSprite->setColor(sf::Color(255,255,255,(uint8_t)frag.alpha));
                pipRT.draw(*newCardSprite);
                ++di;
            }
            newCardSprite->setOrigin({newCardTexture.getSize().x/2.f, newCardTexture.getSize().y/2.f});
            newCardSprite->setTextureRect(sf::IntRect({0,0},{(int)newCardTexture.getSize().x,(int)newCardTexture.getSize().y}));
            newCardSprite->setColor(sf::Color(255,255,255,255));
        }

        // ── 第 3 层：CardReader Top ──
        if (showReader) {
            cardReaderTopSprite->setScale({rs, rs});
            cardReaderTopSprite->setPosition({640.f, 100.f});
            pipRT.draw(*cardReaderTopSprite);
            cardReaderTopSprite->setScale({0.8f, 0.8f});
            cardReaderTopSprite->setPosition({300.f, 210.f});
        }
    }
    // ──── 动画 2：棋盘下棋演示 ────
    else if (pipAnimIndex == 2) {
        pipTotalDur = 10.f;
        // 预定义走法序列（黑1白2，黑先）
        struct Move { float t; int r; int c; int p; };
        static const Move moves[] = {
            {0.0f, 7,7,1},  {1.0f, 7,8,2}, {2.0f, 7,6,1},
            {3.0f, 8,7,2},  {4.0f, 7,5,1},  // 黑三连 (7,5-7,6-7,7)
        };
        const int moveCount = 5;

        // 状态 0：清空棋盘
        if (pipAnimState == 0) {
            chessboard.reset();
            pipAnimState = 1;
        }
        // 逐步落子
        if (pipAnimState >= 1 && pipAnimState <= moveCount) {
            int idx = pipAnimState - 1;
            if (dt >= moves[idx].t) {
                chessboard.placeDirect(moves[idx].r, moves[idx].c, moves[idx].p);
                pipAnimState++;
                // 三连达成后启抽卡动画（moveCount=5已达成）
                if (pipAnimState > moveCount) pipAnimState = 6;
            }
        }
        // 抽卡出生动画（1.5s，在卡槽1）
        if (pipAnimState == 6) {
            float cardGenT = dt - moves[moveCount-1].t;  // 从最后一子落完后计时
            if (cardGenT < 0.f) cardGenT = 0.f;
            if (cardGenT > 1.5f) { pipAnimState = 7; }
        }
        // 展示到结束
        if (pipAnimState == 7 && dt >= pipTotalDur) {
            pipAnimNext();
        }

        sf::View boardView(sf::FloatRect({300.f, 0.f}, {2560.f, 1440.f}));
        pipRT.setView(boardView);
        chessboard.draw(pipRT);
        // 卡槽
        if (cardSlotSprite)  pipRT.draw(*cardSlotSprite);
        if (cardSlotSprite2) pipRT.draw(*cardSlotSprite2);
        // 抽卡动画 / 卡牌展示
        if (pipAnimState == 6 || pipAnimState == 7) {
            float t = dt - moves[moveCount-1].t;
            if (t < 0.f) t = 0.f;
            if (pipAnimState == 6) {
                if (!pipFragsInit) {
                    pipFrags = cardFragCache; pipFragsTex = 0;
                    for (auto& frag : pipFrags) {
                        frag.released = false; frag.alpha = 0.f; frag.fadeTimer = 0.f;
                        frag.rotation = (rand()%60-30)*1.f;
                        frag.pos.x = 2260.f + (rand()%160-80)*1.f;
                        frag.pos.y = 500.f + (rand()%80)*1.f;
                    }
                    pipFragsInit = true; pipFragsLastT = 0.f;
                }
                float rawT2 = t / 1.0f; if (rawT2 > 1.f) rawT2 = 1.f;
                sf::Vector2u ct = newCardTexture.getSize();
                float wipeY2 = ct.y * rawT2;
                int visH2 = (int)wipeY2;
                if (visH2 > 0) {
                    newCardSprite->setScale({0.36f, 0.36f});
                    newCardSprite->setOrigin({ct.x/2.f, ct.y/2.f});
                    newCardSprite->setTextureRect(sf::IntRect({0,0}, {(int)ct.x, visH2}));
                    newCardSprite->setPosition({2260.f, 321.f});
                    newCardSprite->setColor(sf::Color(255,255,255,255));
                    pipRT.draw(*newCardSprite);
                }
                float dtF3 = rawT2 - pipFragsLastT; pipFragsLastT = rawT2;
                if (dtF3 < 0.f) dtF3 = 0.f; if (dtF3 > 0.1f) dtF3 = 0.04f;
                for (auto& frag : pipFrags) {
                    float fcx = frag.texRect.size.x/2.f, fcy = frag.texRect.size.y/2.f;
                    float tx = 2260.f + (frag.texRect.position.x + fcx - ct.x/2.f) * 0.36f;
                    float ty = 321.f + (frag.texRect.position.y + fcy - ct.y/2.f) * 0.36f;
                    if (!frag.released && (float)frag.texRect.position.y < wipeY2 + ct.y*0.06f && (float)frag.texRect.position.y >= wipeY2 - ct.y*0.02f) {
                        frag.released = true; frag.targetPos = {tx, ty}; frag.fadeTimer = 0.f;
                        frag.pos.x = tx + (rand()%40-20)*1.f; frag.pos.y = ty + (rand()%30+20)*1.f;
                    }
                    if (frag.released) {
                        frag.fadeTimer += dtF3; float prog = frag.fadeTimer/0.3f; if (prog>1.f) prog=1.f;
                        float eased = prog*prog*(3.f-2.f*prog);
                        frag.pos.x += (frag.targetPos.x-frag.pos.x)*0.12f;
                        frag.pos.y += (frag.targetPos.y-frag.pos.y)*0.12f;
                        frag.alpha = 255.f*eased;
                        newCardSprite->setOrigin({fcx,fcy}); newCardSprite->setTextureRect(frag.texRect);
                        newCardSprite->setPosition(frag.pos); newCardSprite->setRotation(sf::degrees(frag.rotation*(1.f-eased)));
                        newCardSprite->setColor(sf::Color(255,255,255,(uint8_t)frag.alpha));
                        pipRT.draw(*newCardSprite);
                        float gb2 = eased<1.f ? eased*60.f : 0.f;
                        if (gb2>0.f){newCardSprite->setColor(sf::Color((uint8_t)gb2,(uint8_t)gb2,(uint8_t)gb2,(uint8_t)frag.alpha));pipRT.draw(*newCardSprite, sf::BlendAdd);}
                    }
                }
                newCardSprite->setOrigin({ct.x/2.f, ct.y/2.f});
                // 文字逐行淡入
                {   auto txtA2 = [&](float yOff) -> uint8_t {
                        float th = ct.y*0.5f - yOff/0.36f;
                        float lineT = th / ct.y; if (lineT<0.f) lineT=0.f;
                        if (rawT2 < lineT) return 0;
                        float f = (rawT2 - lineT)/0.08f; if (f>1.f) f=1.f;
                        return (uint8_t)(255.f*f);
                    };
                    uiText.setFont(cardFont);
                    uint8_t na2 = txtA2(164.f);
                    if (na2>0){uiText.setCharacterSize(28);uiText.setFillColor(sf::Color(255,255,255,na2));uiText.setString(L"连击");uiText.setPosition({2260.f-52.f,321.f-164.f});pipRT.draw(uiText);}
                    uint8_t da2 = txtA2(90.f);
                    if (da2>0){uiText.setCharacterSize(22);uiText.setFillColor(sf::Color(230,230,230,da2));uiText.setString(L"给予两次落子数");uiText.setPosition({2260.f-88.f,321.f-90.f});pipRT.draw(uiText);}
                    uiText.setFont(font);
                }
                if (rawT2 >= 1.f) { pipAnimState = 7; pipFragsInit = false; }
            } else {
                sf::Vector2u ct = newCardTexture.getSize();
                newCardSprite->setScale({0.36f, 0.36f});
                newCardSprite->setPosition({2260.f, 321.f});
                newCardSprite->setTextureRect(sf::IntRect({0,0}, {(int)ct.x, (int)ct.y}));
                newCardSprite->setColor(sf::Color(255,255,255,255));
                pipRT.draw(*newCardSprite);
                // 文字
                uiText.setFont(cardFont);
                uiText.setCharacterSize(28);
                uiText.setFillColor(sf::Color(255,255,255,255));
                uiText.setString(L"连击");
                uiText.setPosition({2260.f - 52.f, 321.f - 164.f});
                pipRT.draw(uiText);
                uiText.setCharacterSize(22);
                uiText.setFillColor(sf::Color(230,230,230,255));
                uiText.setString(L"给予两次落子数");
                uiText.setPosition({2260.f - 88.f, 321.f - 90.f});
                pipRT.draw(uiText);
                uiText.setFont(font);
            }
        }
        newCardSprite->setScale({0.36f, 0.36f});
        pipRT.setView(pipView);
    }
    // ──── 动画 3：疫病传播演示 ────
    else if (pipAnimIndex == 3) {
        pipTotalDur = 10.f;
        // 预定义棋盘状态
        struct Step { float t; int r; int c; int action; }; // action: 0=放白子, 1=感染, 2=销毁, 3=结束
        static const Step steps[] = {
            // 初始布置白子方阵 (行1-4, 列1-5)
            {0.0f, 1,1,0}, {0.0f, 1,2,0}, {0.0f, 1,3,0}, {0.0f, 1,4,0}, {0.0f, 1,5,0},
            {0.0f, 2,1,0}, {0.0f, 2,2,0}, {0.0f, 2,3,0}, {0.0f, 2,4,0}, {0.0f, 2,5,0},
            {0.0f, 3,1,0}, {0.0f, 3,2,0}, {0.0f, 3,3,0}, {0.0f, 3,4,0}, {0.0f, 3,5,0},
            {0.0f, 4,1,0}, {0.0f, 4,2,0}, {0.0f, 4,3,0}, {0.0f, 4,4,0}, {0.0f, 4,5,0},
            // 初始感染 (2,3)
            {1.5f, 2,3,1},
            // 回合1：感染源死亡 + 四邻感染
            {3.0f, 2,3,2}, {3.0f, 1,3,1}, {3.0f, 3,3,1}, {3.0f, 2,2,1}, {3.0f, 2,4,1},
            // 回合2：两个感染棋子死亡 + 更多传播
            {5.0f, 1,3,2}, {5.0f, 2,2,2}, {5.0f, 1,2,1}, {5.0f, 1,4,1}, {5.0f, 3,2,1},
            {6.5f, 3,3,2},
        };
        const int stepCount = sizeof(steps) / sizeof(steps[0]);

        if (pipAnimState == 0) {
            chessboard.reset();
            memset(infected, 0, sizeof(infected));
            pipAnimState = 1;
        }

        // 执行步骤（按时间顺序逐个触发）
        for (int i = pipAnimState - 1; i < stepCount; ++i) {
            if (dt >= steps[i].t) {
                if (steps[i].action == 0)
                    chessboard.placeDirect(steps[i].r, steps[i].c, 2);
                else if (steps[i].action == 1)
                    infected[steps[i].r][steps[i].c] = true;
                else if (steps[i].action == 2) {
                    chessboard.placeDirect(steps[i].r, steps[i].c, 0);
                    infected[steps[i].r][steps[i].c] = false;
                }
            } else break;
        }
        // pipAnimState 追踪已执行的步骤数
        int executed = 0;
        for (int i = 0; i < stepCount; ++i)
            if (dt >= steps[i].t) ++executed;
        pipAnimState = executed + 1;
        // 全部完成且停留时间足够后切换
        if (executed >= stepCount && dt >= 10.f && pipAnimState < 999) {
            pipAnimState = 999; // 防止重复触发
            pipAnimNext();
        }

        // 绘制棋盘（放大局部视角，保持 16:9）
        sf::View boardView(sf::FloatRect({-200.f, -150.f}, {1600.f, 900.f}));
        pipRT.setView(boardView);
        chessboard.draw(pipRT);

        // 绘制疫病标记
        if (virusTex.getSize().x > 0) {
            for (int r = 0; r < 15; ++r) {
                for (int c = 0; c < 15; ++c) {
                    if (!infected[r][c]) continue;
                    float ix, iy;
                    chessboard.getGridPosition(r, c, ix, iy);
                    float vs = 18.f / virusTex.getSize().x;
                    virusSpr.setScale({vs, vs});
                    virusSpr.setPosition({ix + 16.f, iy - 16.f});
                    virusSpr.setColor(sf::Color(255, 255, 255, 220));
                    pipRT.draw(virusSpr);
                }
            }
        }

        pipRT.setView(pipView);
    }
    pipRT.display();
    pipSpr->setTexture(pipRT.getTexture(), true);
    pipSpr->setOrigin({pipRT.getSize().x / 2.f, pipRT.getSize().y / 2.f});
    pipSpr->setPosition(pipPos);
    float combinedAlpha = pipFadeAlpha * pipBounceAlpha * 0.6f;
    uint8_t fa = static_cast<uint8_t>(255.f * combinedAlpha);
    pipSpr->setColor(sf::Color(255, 255, 255, fa));
    window.draw(*pipSpr);

    // ── 第 3 层：黑子 ──
    float bw = static_cast<float>(menuBlackTex.getSize().x);
    float bh = static_cast<float>(menuBlackTex.getSize().y);
    menuBlackSpr.setScale({1013.f / bw, 1003.5f / bh});
    menuBlackSpr.setPosition({bx, by});
    menuBlackSpr.setRotation(sf::degrees(-rotAngle));
    window.draw(menuBlackSpr);

    // ── 第 4 层：UI 渲染在最上面 ──
    float tw = static_cast<float>(mainTitleTex.getSize().x);
    float th = static_cast<float>(mainTitleTex.getSize().y);
    float titleScale = 800.f / tw;  // 宽 400×2 等比缩放
    mainTitleSpr.setScale({titleScale, titleScale});
    mainTitleSpr.setPosition({WINDOW_WIDTH * 0.5f, 320.f});
    window.draw(mainTitleSpr);

    // UI_Frame 按钮背景图（等比例缩放，悬停放大）
    float ffw = static_cast<float>(uiFrameTex.getSize().x);
    float ffh = static_cast<float>(uiFrameTex.getSize().y);
    float frameScale = 120.6f / ffh;
    // 检测鼠标悬停
    sf::Vector2i mp = sf::Mouse::getPosition(window);
    sf::Vector2f mpos = window.mapPixelToCoords(mp);
    int hoverIdx = -1;
    float hw = 320.f * 1.1f;       // 检测区宽度
    float hh = 67.f * 1.8f;        // 检测区高度
    for (size_t i = 0; i < menuButtonBackgrounds.size(); ++i) {
        sf::Vector2f hp = menuButtonBackgrounds[i].getPosition();
        if (mpos.x > hp.x && mpos.x < hp.x + hw && mpos.y > hp.y && mpos.y < hp.y + hh)
            { hoverIdx = static_cast<int>(i); break; }
    }
    // 悬停按钮抖动（20Hz, ±3px）
    float jx = 0.f, jy = 0.f;
    if (hoverIdx >= 0) {
        jx = (static_cast<float>(rand()) / RAND_MAX * 2.f - 1.f);
        jy = (static_cast<float>(rand()) / RAND_MAX * 2.f - 1.f);
    }
    for (size_t i = 0; i < menuButtonBackgrounds.size(); ++i) {
        sf::Vector2f bpos = menuButtonBackgrounds[i].getPosition();
        sf::Vector2f bsize = menuButtonBackgrounds[i].getSize();
        bool hover = (static_cast<int>(i) == hoverIdx);
        float s = hover ? frameScale * 1.05f : frameScale;
        float ox = hover ? 10.f : 0.f;
        float oy = hover ? -10.f : 0.f;
        float hx = hover ? jx : 0.f;
        float hy = hover ? jy : 0.f;
        uiFrameSpr.setScale({s, s});
        uiFrameSpr.setPosition({bpos.x + bsize.x / 2.f - ox + hx, bpos.y + bsize.y / 2.f + oy + hy});
        uiFrameSpr.setColor(sf::Color(255, 255, 255, 255));
        window.draw(uiFrameSpr);
    }
    for (auto& bg : menuButtonBackgrounds) window.draw(bg);  // 透明定位层
    for (size_t i = 0; i < menuButtons.size(); ++i) {
        auto& btn = menuButtons[i];
        bool hover = (static_cast<int>(i) == hoverIdx);
        if (hover) {
            btn.setCharacterSize(50);  // 48 × 1.05
            sf::Vector2f op = btn.getPosition();
            btn.setPosition({op.x - 6.f + jx, op.y - 10.f + jy});
            window.draw(btn);
            btn.setPosition(op);
            btn.setCharacterSize(48);
        } else {
            window.draw(btn);
        }
    }

    // ── 第 5 层：乱码卡片（最上层，UI 之上，支持拖拽旋转）──
    {
        sf::Vector2i mp2 = sf::Mouse::getPosition(window);
        sf::Vector2f mpos2 = window.mapPixelToCoords(mp2);
        const float cardW = 800.f * 0.432f, cardH = 1112.f * 0.432f;
        const float halfW = cardW / 2.f, halfH = cardH / 2.f;

        float a = glitchAngle;
        sf::Vector2f longAxis(std::sin(a), -std::cos(a));
        sf::Vector2f mouseDelta = mpos2 - glitchPrevMouse;

        if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left)) {
            sf::FloatRect cardRect({glitchCardPos.x - halfW, glitchCardPos.y - halfH}, {cardW, cardH});
            if (!glitchDragging && cardRect.contains(mpos2)) {
                glitchDragging = true;
                grabPoint = mpos2;
                sf::Vector2f worldOff = grabPoint - glitchCardPos;
                float ca = std::cos(-glitchAngle), sa = std::sin(-glitchAngle);
                grabOffset.x = worldOff.x * ca - worldOff.y * sa;
                grabOffset.y = worldOff.x * sa + worldOff.y * ca;
                glitchVelocity = {0, 0};
                glitchAngularVel = 0;
                lastMouseDir = {0, 0};
            }
            if (glitchDragging) {
                float ca2 = std::cos(glitchAngle), sa2 = std::sin(glitchAngle);
                sf::Vector2f rotatedOffset;
                rotatedOffset.x = grabOffset.x * ca2 - grabOffset.y * sa2;
                rotatedOffset.y = grabOffset.x * sa2 + grabOffset.y * ca2;
                glitchCardPos = mpos2 - rotatedOffset;

                float angleDiff = 0.f, rotSpeed = 0.f;
                float mdLen = std::sqrt(mouseDelta.x * mouseDelta.x + mouseDelta.y * mouseDelta.y);
                if (mdLen > 0.5f) {
                    // 指数移动平均平滑鼠标方向，抑制单帧抖动
                    sf::Vector2f rawDir(mouseDelta.x / mdLen, mouseDelta.y / mdLen);
                    const float SMOOTH = 0.35f;
                    if (lastMouseDir.x == 0.f && lastMouseDir.y == 0.f) {
                        lastMouseDir = rawDir;  // 首帧直接采纳
                    } else {
                        lastMouseDir.x = lastMouseDir.x + (rawDir.x - lastMouseDir.x) * SMOOTH;
                        lastMouseDir.y = lastMouseDir.y + (rawDir.y - lastMouseDir.y) * SMOOTH;
                    }
                }
                if (lastMouseDir.x != 0.f || lastMouseDir.y != 0.f) {
                    float sideFlip = (grabOffset.y > 0.f) ? -1.f : 1.f;
                    float cross = longAxis.x * lastMouseDir.y * sideFlip - longAxis.y * lastMouseDir.x * sideFlip;
                    float dot   = longAxis.x * lastMouseDir.x * sideFlip + longAxis.y * lastMouseDir.y * sideFlip;
                    angleDiff = std::atan2(cross, dot);
                    float leverDist = std::sqrt(grabOffset.x * grabOffset.x + grabOffset.y * grabOffset.y);
                    rotSpeed = leverDist / (halfH * 1.5f);
                    if (rotSpeed > 1.f) rotSpeed = 1.f;
                    glitchAngle += angleDiff * rotSpeed * 0.20f;
                }

                if (glitchCardPos.x < halfW)      glitchCardPos.x = halfW;
                if (glitchCardPos.x > 2560-halfW) glitchCardPos.x = 2560-halfW;
                if (glitchCardPos.y < halfH)      glitchCardPos.y = halfH;
                if (glitchCardPos.y > 1440-halfH) glitchCardPos.y = 1440-halfH;

                glitchVelocity = mouseDelta;
                glitchAngularVel = angleDiff * rotSpeed * 0.5f;
            }
        } else {
            if (glitchDragging) { glitchDragging = false; }
            glitchCardPos += glitchVelocity;
            glitchAngle += glitchAngularVel;
            if (glitchCardPos.x - halfW < 0.f)     { glitchCardPos.x = halfW;      glitchVelocity.x = -glitchVelocity.x * 0.6f; glitchVelocity.y *= 0.8f; }
            if (glitchCardPos.x + halfW > 2560.f)  { glitchCardPos.x = 2560.f-halfW; glitchVelocity.x = -glitchVelocity.x * 0.6f; glitchVelocity.y *= 0.8f; }
            if (glitchCardPos.y - halfH < 0.f)     { glitchCardPos.y = halfH;      glitchVelocity.y = -glitchVelocity.y * 0.6f; glitchVelocity.x *= 0.8f; }
            if (glitchCardPos.y + halfH > 1440.f)  { glitchCardPos.y = 1440.f-halfH; glitchVelocity.y = -glitchVelocity.y * 0.6f; glitchVelocity.x *= 0.8f; }
            glitchVelocity *= 0.97f;
            glitchAngularVel *= 0.98f;
            if (std::abs(glitchVelocity.x) < 0.1f && std::abs(glitchVelocity.y) < 0.1f) glitchVelocity = {0,0};
            if (std::abs(glitchAngularVel) < 0.0001f) glitchAngularVel = 0;
        }
        glitchPrevMouse = mpos2;

        // 渲染位置平滑追赶物理位置（提高视觉帧率）
        glitchRenderPos.x += (glitchCardPos.x - glitchRenderPos.x) * 0.45f;
        glitchRenderPos.y += (glitchCardPos.y - glitchRenderPos.y) * 0.45f;

        // 乱码文字刷新
        const std::wstring pool = L"连击给两次数落子隐忍迫使敌为胜途方承六星受笼络销毁己一个棋转化破釜沉舟将手牌放回库根据量疫病指定颗患上概率死亡试图传染给其他也风险隔离在后痊愈盲目以地事秦持有者需要传送张橙卡出不送牌则接下回合只能下撑过本消退";
        int poolSize = (int)pool.size();
        if (menuGlitchClock.getElapsedTime().asSeconds() >= 0.08f) {
            for (auto& c : glitchName) c = pool[rand() % poolSize];
            for (auto& c : glitchDesc) if (c != L'\n') c = pool[rand() % poolSize];
            menuGlitchClock.restart();
        }

        // 渲染卡片（带旋转，位置用插值平滑坐标）
        sf::Vector2u ct3 = newCardTexture.getSize();
        newCardSprite->setScale({0.432f, 0.432f});
        newCardSprite->setPosition(glitchRenderPos);
        newCardSprite->setRotation(sf::radians(glitchAngle));
        newCardSprite->setTextureRect(sf::IntRect({0,0}, {(int)ct3.x, (int)ct3.y}));
        newCardSprite->setColor(sf::Color(255,255,255,255));
        window.draw(*newCardSprite);
        newCardSprite->setRotation(sf::degrees(0));  // 重置

        // 乱码文字（跟随渲染位置）
        float gs = 0.432f / 0.36f;
        float cx = glitchRenderPos.x, cy = glitchRenderPos.y;
        sf::Transform rotTx;
        rotTx.rotate(sf::radians(glitchAngle), glitchRenderPos);

        uiText.setFont(cardFont);
        uiText.setCharacterSize((int)(28.f * gs));
        uiText.setFillColor(sf::Color(255,255,255,200));
        uiText.setString(sf::String(glitchName));
        uiText.setPosition({cx - 52.f * gs - 30.f, cy - 164.f * gs});
        window.draw(uiText, rotTx);
        uiText.setCharacterSize((int)(24.f * gs));
        uiText.setFillColor(sf::Color(230,230,230,180));
        float descBaseY = cy - 90.f * gs;
        std::wstring line; int lineNum = 0;
        for (auto c : glitchDesc) {
            if (c == L'\n') { uiText.setString(sf::String(line)); uiText.setPosition({cx - 88.f * gs, descBaseY + lineNum * 20.f * gs}); window.draw(uiText, rotTx); line.clear(); lineNum++; }
            else line += c;
        }
        if (!line.empty()) { uiText.setString(sf::String(line)); uiText.setPosition({cx - 88.f * gs, descBaseY + lineNum * 20.f * gs}); window.draw(uiText, rotTx); }
        uiText.setFont(font);
    }
}

void GameEngine::renderPVEConfig() {
    // ── 背景：星空帧动画（与主菜单一致）──
    // ── Universe 背景（本界面独立加载，每帧缓存）──
    {
        static GameState lastBgState = GameState::MENU;
        if (currentState != lastBgState) { bgCurrentFrame = -1; lastBgState = currentState; }
        float bgElapsed = bgFrameClock.getElapsedTime().asSeconds();
        int frame = static_cast<int>(bgElapsed * 25.f) % BG_TOTAL_FRAMES + 1;
        if (frame != bgCurrentFrame) {
            char path[128]; snprintf(path, sizeof(path),
                "assets/Universe/frame_%05d.jpg", frame);
            if (bgTexA.loadFromFile(getEngineAssetPath(path))) {
                bgSprA.setTexture(bgTexA, true);
                float sw = 2560.f / bgTexA.getSize().x;
                float sh = 1440.f / bgTexA.getSize().y;
                bgSprA.setScale({sw, sh});
                bgSprA.setPosition({0.f, 0.f});
                bgSprA.setColor(sf::Color(255, 255, 255, 255));
            }
            bgCurrentFrame = frame;
        }
        window.draw(bgSprA);
    }

    // ── 按钮参数（与主菜单一致）──
    const float centerX = WINDOW_WIDTH * 0.5f;
    const float btnW = 320.f;
    const float btnH = 67.f;
    const float hitW = btnW * 1.1f;
    const float hitH = btnH * 1.8f;
    const float btnX = centerX - btnW * 0.5f;
    const float hitX = centerX - hitW * 0.5f;
    const float menuStartY = 543.f;

    // 四个按钮的 Y 坐标（间距 137）
    const float btnY[4] = { menuStartY, menuStartY + 137.f, menuStartY + 274.f, menuStartY + 411.f };
    const std::wstring btnLabels[4] = {
        (aiDifficultySetting == 1) ? L"AI难度:初级" :
        (aiDifficultySetting == 2) ? L"AI难度:明智" :
                                     L"AI难度:大神",
        (playerColorPref == 1) ? L"执子:执黑" : L"执子:执白",
        L"进入游戏",
        L"返回主菜单"
    };

    // ── 标题 ──
    uiText.setFont(cardFont);
    uiText.setCharacterSize(78);
    uiText.setFillColor(sf::Color(200, 160, 230));
    uiText.setString(sf::String(L"对战配置"));
    sf::FloatRect titleBounds = uiText.getLocalBounds();
    uiText.setPosition(sf::Vector2f(centerX - (titleBounds.position.x + titleBounds.size.x) * 0.5f, 380.f));
    window.draw(uiText);

    // ── 悬停检测 ──
    sf::Vector2i mp = sf::Mouse::getPosition(window);
    sf::Vector2f mpos = window.mapPixelToCoords(mp);
    int hoverIdx = -1;
    for (int i = 0; i < 4; ++i) {
        float hy = btnY[i] - (hitH - btnH) * 0.5f;
        if (mpos.x > hitX && mpos.x < hitX + hitW && mpos.y > hy && mpos.y < hy + hitH)
            { hoverIdx = i; break; }
    }
    float jx = 0.f, jy = 0.f;
    if (hoverIdx >= 0) {
        jx = (static_cast<float>(rand()) / RAND_MAX * 2.f - 1.f);
        jy = (static_cast<float>(rand()) / RAND_MAX * 2.f - 1.f);
    }

    // ── UI_Frame 按钮背景 ──
    float ffw = static_cast<float>(uiFrameTex.getSize().x);
    float ffh = static_cast<float>(uiFrameTex.getSize().y);
    float frameScale = 120.6f / ffh;
    for (int i = 0; i < 4; ++i) {
        float by = btnY[i] - (hitH - btnH) * 0.5f;       // 检测矩形顶端
        sf::Vector2f bpos(hitX, by);
        sf::Vector2f bsize(hitW, hitH);
        bool hover = (i == hoverIdx);
        float s = hover ? frameScale * 1.05f : frameScale;
        float ox = hover ? 10.f : 0.f;
        float oy = hover ? -10.f : 0.f;
        float hx = hover ? jx : 0.f;
        float hy2 = hover ? jy : 0.f;
        uiFrameSpr.setScale({s, s});
        uiFrameSpr.setPosition({bpos.x + bsize.x / 2.f - ox + hx, bpos.y + bsize.y / 2.f + oy + hy2});
        uiFrameSpr.setColor(sf::Color(255, 255, 255, 255));
        window.draw(uiFrameSpr);
    }

    // ── 按钮文字 ──
    uiText.setFont(cardFont);
    uiText.setCharacterSize(48);
    uiText.setFillColor(sf::Color(50, 15, 70));
    for (int i = 0; i < 4; ++i) {
        bool hover = (i == hoverIdx);
        uiText.setString(sf::String(btnLabels[i]));
        sf::FloatRect tb = uiText.getLocalBounds();
        float tx = centerX - (tb.position.x + tb.size.x) * 0.5f;
        float ty = btnY[i] + (btnH - tb.size.y) * 0.5f - tb.position.y;
        if (hover) {
            uiText.setCharacterSize(50);
            uiText.setPosition({tx - 6.f + jx, ty - 10.f + jy});
            window.draw(uiText);
            uiText.setPosition({tx, ty});
            uiText.setCharacterSize(48);
        } else {
            uiText.setPosition({tx, ty});
            window.draw(uiText);
        }
    }

}

// ── AI 出牌动画 ──
void GameEngine::startAICardPlay(int handIndex) {
    if (handIndex < 0 || handIndex >= static_cast<int>(playerDeck.aiHand.size())) return;

    showcasedCard = playerDeck.aiHand[handIndex];
    aiPlayingCardIndex = handIndex;
    aiCardIsPurpleTransfer = (showcasedCard.cardColor == 1);  // 紫卡走传送流程
    aiCardPlayState = AICardPlayState::RISING;
    aiCardPlayClock.restart();
    aiCardAnimPos = {300.f, 1230.f};  // CardPortal 位置
    isBusyAnimating = true;
    isTurnPaused = true;
    pauseClock.restart();
    std::cout << "[AI] 开始出牌: " << showcasedCard.name.c_str()
              << (aiCardIsPurpleTransfer ? " [紫卡→传送]" : "") << std::endl;
}

void GameEngine::updateAICardAnimation() {
    float elapsed = aiCardPlayClock.getElapsedTime().asSeconds();

    switch (aiCardPlayState) {
    case AICardPlayState::RISING: {
        // 从 CardPortal(300,1230) 滑到 (300,720) — 与人族 MOVE_TO_200 同款缓动
        float t = elapsed / 0.55f;
        if (t > 1.f) t = 1.f;
        float eased = t * t * (3.f - 2.f * t); // smoothstep（先快后慢）
        aiCardAnimPos.x = 300.f;
        aiCardAnimPos.y = 1230.f + (720.f - 1230.f) * eased;
        if (t >= 1.f) {
            aiCardPlayState = AICardPlayState::PAUSE_AT_CENTER;
            aiCardPlayClock.restart();
        }
        break;
    }
    case AICardPlayState::PAUSE_AT_CENTER: {
        // 停在 Y=720 展示 1 秒；紫卡跳过 Reader 走传送展示
        aiCardAnimPos = {300.f, 720.f};
        if (elapsed >= 1.0f) {
            if (aiCardIsPurpleTransfer) {
                // 🌟 紫卡不走 Reader，淡出后走中央展示
                aiCardPlayState = AICardPlayState::PURPLE_FADE_OUT;
                purpleShowcaseClock.restart();
            } else {
                aiCardPlayState = AICardPlayState::TO_READER;
            }
            aiCardPlayClock.restart();
        }
        break;
    }
    case AICardPlayState::TO_READER: {
        // 从 (300,720) 滑到 (300,400) — 与人族 MOVE_TO_200 同款缓动
        float t = elapsed / 0.4f;
        if (t > 1.f) t = 1.f;
        float eased = t * t * (3.f - 2.f * t); // smoothstep
        aiCardAnimPos.x = 300.f;
        aiCardAnimPos.y = 720.f + (400.f - 720.f) * eased;
        if (t >= 1.f) {
            aiCardPlayState = AICardPlayState::ANNIHILATING;
            aiCardPlayClock.restart();
            // 初始化碎片
            cardFragActive = (showcasedCard.cardColor == 1) ? cardFragCachePurple : cardFragCache;
            cardShatterTex = showcasedCard.cardColor;
            cardShatterLastT = 0.f;
            for (auto& frag : cardFragActive) {
                frag.released = false; frag.alpha = 255.f; frag.fadeTimer = 0.f;
                frag.rotation = 0.f;
                frag.vel = {(rand()%160-80)*1.f, -(rand()%100+50)*1.f};
                frag.rotSpeed = (rand()%720-360)*1.f;
            }
        }
        break;
    }
    case AICardPlayState::ANNIHILATING: {
        // 边上升边粉碎 (300,400)→(300,210)，0.5s
        float t = elapsed / 0.5f; if (t > 1.f) t = 1.f;
        float eased = t * t * (3.f - 2.f * t);
        aiCardAnimPos.x = 300.f;
        aiCardAnimPos.y = 400.f + (210.f - 400.f) * eased;
        if (t >= 1.f) {
            playerDeck.aiDiscardCard(aiPlayingCardIndex);
            aiPlayingCardIndex = -1;
            showcaseState = CardShowcaseState::PAUSE;
            showcaseClock.restart();
            aiCardPlayState = AICardPlayState::SHOWCASING;
            aiCardPlayClock.restart();
        }
        break;
    }
    case AICardPlayState::SHOWCASING: {
        // 等待展示 + 延迟效果全部完成（计时由 FADE_OUT/deferred handler 管理）
        if (showcaseState == CardShowcaseState::NONE) {
            if (aiCardIsPurpleTransfer) {
                // 🌟 紫卡：中央展示完成，进入卡槽生成阶段
                aiCardPlayState = AICardPlayState::PURPLE_SLOT_GEN;
                aiCardPlayClock.restart();
            } else if (!isBusyAnimating) {
                aiCardPlayState = AICardPlayState::IDLE;
                std::cout << "[AI] 出牌动画完成" << std::endl;
            }
        }
        break;
    }
    // ── 🌟 紫卡传送：y=720 淡出 → 中央展示 → 玩家卡槽生成 ──
    case AICardPlayState::PURPLE_FADE_OUT: {
        // 0.5s 透明度淡出，然后触发中央展示
        aiCardAnimPos = {300.f, 720.f};
        if (elapsed >= 0.5f) {
            // 从 AI 手牌移除
            playerDeck.aiDiscardCard(aiPlayingCardIndex);
            aiPlayingCardIndex = -1;
            // 触发中央展示（紫卡/橙卡共用，通过 aiCardIsPurpleTransfer 区分精灵）
            showcaseState = CardShowcaseState::PAUSE;
            showcaseClock.restart();
            aiCardPlayState = AICardPlayState::SHOWCASING;
            aiCardPlayClock.restart();
            std::cout << "[AI] 紫卡淡出完成，进入中央展示" << std::endl;
        }
        break;
    }
    case AICardPlayState::PURPLE_SLOT_GEN: {
        // 等待出生动画完成
        if (!isAnimatingCard) {
            aiCardPlayState = AICardPlayState::IDLE;
            aiCardIsPurpleTransfer = false;
            isBusyAnimating = false;
            if (isTurnPaused) {
                turnTimePaused += pauseClock.getElapsedTime().asSeconds();
                isTurnPaused = false;
            }
            consumeActionPoint(false);
            triggerBattleMusic02();
            addCrisisTime(30.f);
            settleActionPoints();
            std::cout << "[AI] 紫卡传送完成" << std::endl;
        }
        break;
    }
    default: break;
    }
}

// ── 紫卡传送动画 ──
void GameEngine::startPurpleCardSend(int handIndex) {
    if (handIndex < 0 || handIndex >= static_cast<int>(playerDeck.hand.size())) return;
    purpleSendState = PurpleSendState::MOVE_TO_POS;
    purpleSendClock.restart();
    purpleSendIndex = handIndex;
    purpleSendPos = newCardSprite->getPosition(); // 从当前释放位置出发
    isBusyAnimating = true;
    cardBirthActive = false; isAnimatingCard = false;
    showcaseState = CardShowcaseState::NONE; // 终止任何展示
    annihilateState = CardAnnihilateState::NONE; // 终止任何湮灭
    if (!isTurnPaused) { isTurnPaused = true; pauseClock.restart(); }
    std::cout << "[PurpleSend] 紫卡传送启动" << std::endl;
}

void GameEngine::updatePurpleCardSend() {
    float elapsed = purpleSendClock.getElapsedTime().asSeconds();
    switch (purpleSendState) {
    case PurpleSendState::MOVE_TO_POS: {
        // 指数缓动到 Portal 检测区中心（与橙卡 MOVE_TO_200 同款）
        sf::Vector2f target(300.f, 1060.f);
        purpleSendPos.x += (target.x - purpleSendPos.x) * 0.07f;
        purpleSendPos.y += (target.y - purpleSendPos.y) * 0.07f;
        float dist = std::sqrt((target.x - purpleSendPos.x) * (target.x - purpleSendPos.x) +
                               (target.y - purpleSendPos.y) * (target.y - purpleSendPos.y));
        if (dist < 0.5f) {
            purpleSendPos = target;
            purpleSendState = PurpleSendState::PAUSE_BEFORE;
            purpleSendClock.restart();
        }
        break;
    }
    case PurpleSendState::PAUSE_BEFORE:
        purpleSendPos = {300.f, 1060.f};
        if (elapsed >= 0.2f) {
            purpleSendState = PurpleSendState::PAUSE_AFTER;
            purpleSendClock.restart();
        }
        break;
    case PurpleSendState::PAUSE_AFTER:
        purpleSendPos = {300.f, 1060.f}; // 定住（与橙卡 PAUSE_AFTER 对称）
        if (elapsed >= 0.5f) {
            purpleSendState = PurpleSendState::MOVE_TO_PORTAL;
            purpleSendClock.restart();
        }
        break;
    case PurpleSendState::MOVE_TO_PORTAL: {
        float t = elapsed / 0.35f;
        if (t > 1.f) t = 1.f;
        float eased = t * t * (3.f - 2.f * t); // smoothstep
        purpleSendPos.x = 300.f;
        purpleSendPos.y = 1060.f + (1230.f - 1060.f) * eased; // 下滑→Portal
        if (t >= 1.f) {
            // 传送完成：卡牌给敌方，立即清理
            if (purpleSendIndex >= 0 && purpleSendIndex < static_cast<int>(playerDeck.hand.size())) {
                Card sentCard = playerDeck.hand[purpleSendIndex];
                sentCard.transferred = true; // 打上传送标记
                playerDeck.hand.erase(playerDeck.hand.begin() + purpleSendIndex);
                handSlotAssign.erase(handSlotAssign.begin() + purpleSendIndex);
                playerDeck.aiHand.push_back(sentCard);
                std::cout << "[PurpleSend] 紫卡已传送给敌方（已标记）" << std::endl;
                triggerBattleMusic02();
                addCrisisTime(30.f);
            }
            purpleSendState = PurpleSendState::IDLE;
            purpleSendIndex = -1;
            isBusyAnimating = false;
            cardBirthActive = false; isAnimatingCard = false; // 终止出生动画防后续异常
            if (isTurnPaused) {
                turnTimePaused += pauseClock.getElapsedTime().asSeconds();
                isTurnPaused = false;
            }
            consumeActionPoint(false);
            settleActionPoints();
        }
        break;
    }
    default: break;
    }
}

void GameEngine::renderGameplay() {
    // ── 盲目效果：乱码文字池 ──
    const std::wstring blindPool = L"连击给两次数落子隐忍迫使敌为胜途方承六星受笼络销毁己一个棋转化破釜沉舟将手牌放回库根据量疫病指定颗患上概率死亡试图传染给其他也风险隔离在后痊愈盲目以地事秦持有者需要传送张橙卡出不送牌则接下回合只能下撑过本消退";
    auto scrambleText = [&](const std::wstring& src) {
        std::wstring out = src;
        for (auto& ch : out) if (ch != L'\n') ch = blindPool[rand() % blindPool.size()];
        return out;
    };
    bool playerBlindActive = false;
    for (const auto& c : playerDeck.hand)
        if (c.cardColor == 1 && c.transferred && c.effect == CardEffect::BLIND)
            { playerBlindActive = true; break; }
    bool aiBlindForDebug = false;
    for (const auto& c : playerDeck.aiHand)
        if (c.cardColor == 1 && c.transferred && c.effect == CardEffect::BLIND)
            { aiBlindForDebug = true; break; }

    // ── 背景：星空帧动画 ──
    // ── Universe 背景（本界面独立加载，每帧缓存）──
    {
        static GameState lastBgState = GameState::MENU;
        if (currentState != lastBgState) { bgCurrentFrame = -1; lastBgState = currentState; }
        float bgElapsed = bgFrameClock.getElapsedTime().asSeconds();
        int frame = static_cast<int>(bgElapsed * 25.f) % BG_TOTAL_FRAMES + 1;
        if (frame != bgCurrentFrame) {
            char path[128]; snprintf(path, sizeof(path),
                "assets/Universe/frame_%05d.jpg", frame);
            if (bgTexA.loadFromFile(getEngineAssetPath(path))) {
                bgSprA.setTexture(bgTexA, true);
                float sw = 2560.f / bgTexA.getSize().x;
                float sh = 1440.f / bgTexA.getSize().y;
                bgSprA.setScale({sw, sh});
                bgSprA.setPosition({0.f, 0.f});
                bgSprA.setColor(sf::Color(255, 255, 255, 255));
            }
            bgCurrentFrame = frame;
        }
        window.draw(bgSprA);
    }

    // 1. 渲染棋盘
    chessboard.draw(window);

    // 1.5 落子抛物线动画（棋盘之上、UI 之下）
    chessboard.drawDropAnim(window);

    // ── 疫病标记渲染（virus.png）──
    if (virusTex.getSize().x > 0) {
        for (int r = 0; r < 15; ++r) {
            for (int c = 0; c < 15; ++c) {
                if (!infected[r][c]) continue;
                if (chessboard.getPiece(r, c) == 0) continue;
                float ix, iy;
                chessboard.getGridPosition(r, c, ix, iy);
                float vScale = 24.f / virusTex.getSize().x;
                virusSpr.setScale({vScale, vScale});
                virusSpr.setPosition({ix + 22.f, iy - 22.f});
                virusSpr.setColor(sf::Color(255, 255, 255, 220));
                window.draw(virusSpr);
            }
        }
    }

    // ── 🌟 选子悬停预览（疫病/笼络销毁/笼络转化）统一淡入淡出 ──
    if (isSelectingPiece) {
        // 1. 判断当前选子模式（三种互斥）
        int hoverMode = 0; // 0=无, 1=疫病virus, 2=笼络销毁skull, 3=笼络转化ring
        if (selectPieceStep == 10 && virusTex.getSize().x > 0) hoverMode = 1;
        else if (selectPieceStep == 1 && skullTex.getSize().x > 0) hoverMode = 2;
        else if (selectPieceStep == 2 && !isBusyAnimating && ringTex.getSize().x > 0) hoverMode = 3;

        if (hoverMode > 0) {
            sf::Vector2i mpix = sf::Mouse::getPosition(window);
            sf::Vector2f mpos = window.mapPixelToCoords(mpix);
            const float BW = 15.f * 72.f;
            float bx = (2560.f - BW) * 0.5f + 36.f;
            float by = (1440.f - BW) * 0.5f + 36.f;
            int hc = static_cast<int>(std::round((mpos.x - bx) / 72.f));
            int hr = static_cast<int>(std::round((mpos.y - by) / 72.f));

            // 2. 检查悬停棋子合法性
            int curRow = -1, curCol = -1;
            if (hr >= 0 && hr < 15 && hc >= 0 && hc < 15) {
                int piece = chessboard.getPiece(hr, hc);
                bool valid = false;
                if (hoverMode == 1)      valid = (piece != 0 && piece != selectPiecePlayer);
                else if (hoverMode == 2) valid = (piece == selectPiecePlayer);
                else if (hoverMode == 3) valid = (piece != 0 && piece != selectPiecePlayer);
                if (valid) { curRow = hr; curCol = hc; }
            }

            // 3. 淡入淡出状态机
            if (curRow >= 0) {
                if (curRow != hoverCellRow || curCol != hoverCellCol) {
                    hoverEnterClock.restart();
                    hoverIsLeaving = false;
                    hoverCellRow = curRow; hoverCellCol = curCol;
                }
            } else if (hoverCellRow >= 0 && !hoverIsLeaving) {
                hoverLeaveClock.restart();
                hoverIsLeaving = true;
            }

            // 4. 计算当前 alpha（0.15s 淡入/淡出）
            const uint8_t BASE = 204;
            uint8_t alpha = 0;
            if (hoverIsLeaving) {
                float lt = hoverLeaveClock.getElapsedTime().asSeconds();
                float fade = 1.f - (lt / 0.15f);
                if (fade <= 0.f) { hoverCellRow = -1; hoverCellCol = -1; hoverIsLeaving = false; }
                else alpha = static_cast<uint8_t>(BASE * fade);
            } else if (hoverCellRow >= 0) {
                float et = hoverEnterClock.getElapsedTime().asSeconds();
                float fade = et / 0.15f;
                if (fade > 1.f) fade = 1.f;
                alpha = static_cast<uint8_t>(BASE * fade);
            }

            // 5. 绘制对应图标
            if (alpha > 0 && hoverCellRow >= 0) {
                float ix, iy;
                chessboard.getGridPosition(hoverCellRow, hoverCellCol, ix, iy);
                sf::Sprite* spr = nullptr;
                const sf::Texture* tex = nullptr;
                if (hoverMode == 1)      { spr = &virusSpr; tex = &virusTex; }
                else if (hoverMode == 2) { spr = &skullSpr; tex = &skullTex; }
                else if (hoverMode == 3) { spr = &ringSpr;  tex = &ringTex;  }
                if (spr && tex && tex->getSize().x > 0) {
                    float s = 60.f / tex->getSize().x;
                    spr->setScale({s, s});
                    spr->setPosition({ix, iy});
                    spr->setColor(sf::Color(255, 255, 255, alpha));
                    window.draw(*spr);
                }
            }
        } else {
            // 模式不存在或切换时重置悬停状态
            hoverCellRow = -1; hoverCellCol = -1; hoverIsLeaving = false;
        }
    }

    // ChessFrame 装饰框（棋盘之上、卡牌之下）
    // 🌟【胜利制重构】初始无棋子，随5连达成逐步放置对应颜色棋子
    if (chessFrameTex.getSize().x > 0) {
        float cfs = 720.f / chessFrameTex.getSize().x;
        chessFrameSpr.setScale({cfs, cfs});
        chessFrameSpr.setPosition({1280.f, 80.f});
        chessFrameSpr.setColor(sf::Color(255, 255, 255, 255));
        window.draw(chessFrameSpr);
        chessFrameSpr.setPosition({1280.f, 1360.f});
        window.draw(chessFrameSpr);

        // 确定上下框棋子颜色
        int topColor, bottomColor;
        if (currentState == GameState::GAME_PVE) {
            topColor = playerColorPref;                   // 玩家颜色
            bottomColor = (playerColorPref == 1) ? 2 : 1; // AI 颜色
        } else { // GAME_PVP
            topColor = 1;    // 黑方=上框
            bottomColor = 2; // 白方=下框
        }

        auto drawFramePiece = [&](int color, float x, float y) {
            sf::Sprite& sp = (color == 1) ? chessboard.getBlackSprite()
                                          : chessboard.getWhiteSprite();
            const sf::Texture& tex = sp.getTexture();
            float s = 96.f / tex.getSize().x;
            sp.setScale({s, s});
            sp.setPosition({x, y});
            sp.setColor(sf::Color::White);
            window.draw(sp);
        };

        const float pxBase = 1005.f, pxSpacing = 138.f;
        // 上框：playerFramePieces 个棋子（左→右）
        for (int p = 0; p < playerFramePieces && p < 5; ++p)
            drawFramePiece(topColor, pxBase + p * pxSpacing, 80.f);
        // 下框：enemyFramePieces 个棋子（左→右）
        for (int p = 0; p < enemyFramePieces && p < 5; ++p)
            drawFramePiece(bottomColor, pxBase + p * pxSpacing, 1360.f);
    }

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
        if (cardPortalBottomSprite != nullptr) {
            window.draw(*cardPortalBottomSprite);
        }
    }

    // 2. 渲染计时器（watch + 数字，在传送动画和卡牌下层）
    if (watchTex.getSize().x > 0) {
        float ws = 480.f / watchTex.getSize().x;
        watchSpr.setScale({ws, ws});
        watchSpr.setPosition({234.f, 720.f});
        watchSpr.setColor(sf::Color(255, 255, 255, 255));
        window.draw(watchSpr);
    }
    int remainingTime = static_cast<int>(std::ceil(45.f - getEffectiveTurnTime()));
    if (remainingTime < 0) remainingTime = 0;
    if (remainingTime > 45) remainingTime = 45;
    int tens = remainingTime / 10;
    int ones = remainingTime % 10;
    float digitScale = 48.f / digitTex[0].getSize().x;
    float digitX = 285.f, digitY = 725.f, digitSpacing = 60.f;
    if (digitSpr[tens] && digitTex[tens].getSize().x > 0) {
        digitSpr[tens]->setScale({digitScale, digitScale});
        digitSpr[tens]->setPosition({digitX, digitY});
        digitSpr[tens]->setColor(sf::Color::White);
        window.draw(*digitSpr[tens]);
    }
    if (digitSpr[ones] && digitTex[ones].getSize().x > 0) {
        digitSpr[ones]->setScale({digitScale, digitScale});
        digitSpr[ones]->setPosition({digitX + digitSpacing, digitY});
        digitSpr[ones]->setColor(sf::Color::White);
        window.draw(*digitSpr[ones]);
    }

    // ── 紫卡传送动画（Bottom 之上、Top 之下）──
    if (purpleSendState != PurpleSendState::IDLE &&
        purpleSendIndex >= 0 && purpleSendIndex < static_cast<int>(playerDeck.hand.size())) {
        bool isP = (playerDeck.hand[purpleSendIndex].cardColor == 1);
        sf::Sprite* psSpr = (isP && purpleCardSprite) ? purpleCardSprite : newCardSprite;
        sf::Texture& psTex = (isP && purpleCardSprite) ? purpleCardTexture : newCardTexture;
        sf::Vector2u ts = psTex.getSize();
        psSpr->setScale({0.36f, 0.36f});
        psSpr->setPosition(purpleSendPos);
        psSpr->setTextureRect(sf::IntRect({0,0}, {(int)ts.x, (int)ts.y}));
        psSpr->setColor(sf::Color(255, 255, 255, 255));
        window.draw(*psSpr);
        const auto& pc = playerDeck.hand[purpleSendIndex];
        uiText.setFont(cardFont);
        uiText.setCharacterSize(28);
        uiText.setFillColor(sf::Color(255, 255, 255, 255));
        uiText.setString(pc.name);
        uiText.setPosition({purpleSendPos.x - 52.f, purpleSendPos.y - 164.f});
        window.draw(uiText);
        uiText.setCharacterSize(22);
        uiText.setFillColor(sf::Color(230, 230, 230, 255));
        uiText.setString(pc.description);
        uiText.setPosition({purpleSendPos.x - 88.f, purpleSendPos.y - 90.f});
        window.draw(uiText);
        uiText.setFont(font);
    }

    // ── AI 出牌动画（Bottom 之上、Top 之下）──
    // 跳过 PURPLE_SLOT_GEN（卡已在玩家卡槽）和 SHOWCASING（由中央展示处理）
    if (aiCardPlayState != AICardPlayState::IDLE &&
        aiCardPlayState != AICardPlayState::SHOWCASING &&
        aiCardPlayState != AICardPlayState::PURPLE_SLOT_GEN) {

        // 🌟 紫卡传送全程使用紫卡底纹
        bool isPurple = aiCardIsPurpleTransfer;
        sf::Sprite*  useSpr = (isPurple && purpleCardSprite) ? purpleCardSprite : newCardSprite;
        sf::Texture& useTex = (isPurple && purpleCardSprite) ? purpleCardTexture : newCardTexture;
        sf::Vector2u ts = useTex.getSize();

        if (aiCardPlayState == AICardPlayState::PURPLE_FADE_OUT) {
            // 🌟 紫卡在 y=720 透明度淡出（0.5s）
            float ft = purpleShowcaseClock.getElapsedTime().asSeconds();
            float alpha = 1.f - (ft / 0.5f);
            if (alpha < 0.f) alpha = 0.f;
            useSpr->setScale({0.36f, 0.36f});
            useSpr->setPosition({300.f, 720.f});
            useSpr->setTextureRect(sf::IntRect({0,0}, {(int)ts.x, (int)ts.y}));
            useSpr->setColor(sf::Color(255, 255, 255, (uint8_t)(255 * alpha)));
            window.draw(*useSpr);
        } else if (aiCardPlayState == AICardPlayState::ANNIHILATING && !aiCardIsPurpleTransfer) {
            // 🌟 AI 卡牌边上升边粉碎
            float t2 = aiCardPlayClock.getElapsedTime().asSeconds() / 0.5f;
            if (t2 > 1.f) t2 = 1.f;
            float eased2 = t2 * t2 * (3.f - 2.f * t2);
            float curY2 = 400.f + (210.f - 400.f) * eased2;
            sf::Sprite* spr = (cardShatterTex == 1 && purpleCardSprite) ? purpleCardSprite : newCardSprite;
            sf::Texture& tx2 = (cardShatterTex == 1) ? purpleCardTexture : newCardTexture;
            sf::Vector2u ts2 = tx2.getSize();
            if (ts2.x > 0 && ts2.y > 0 && !cardFragActive.empty()) {
                float clipH2 = ts2.y * (1.f - eased2);
                spr->setScale({0.36f, 0.36f});
                int clip = (int)clipH2;
                if (clip > 0) { spr->setOrigin({ts2.x/2.f, ts2.y/2.f}); spr->setTextureRect({{0,0},{(int)ts2.x,clip}}); spr->setPosition({300.f, curY2}); spr->setColor({255,255,255,255}); window.draw(*spr); }
                float dt2 = t2 - cardShatterLastT; cardShatterLastT = t2;
                if (dt2 < 0.f) dt2 = 0.f; if (dt2 > 0.1f) dt2 = 0.05f;
                for (auto& frag : cardFragActive) {
                    if (!frag.released) { float ft2=(float)frag.texRect.position.y; if (ft2 > clipH2) { frag.released=true;
                        float fcx=frag.texRect.size.x/2.f, fcy=frag.texRect.size.y/2.f;
                        frag.pos.x=300.f+(frag.texRect.position.x+fcx-ts2.x/2.f)*0.36f;
                        frag.pos.y=curY2+(frag.texRect.position.y+fcy-ts2.y/2.f)*0.36f;
                    }}
                    if (frag.released) { frag.vel.y+=800.f*dt2; frag.pos+=frag.vel*dt2; frag.rotation+=frag.rotSpeed*dt2; frag.fadeTimer+=dt2;
                        float fp=frag.fadeTimer/0.85f; if(fp>1.f)fp=1.f; frag.alpha=255.f*(1.f-fp*fp*(3.f-2.f*fp));
                        float fcx=frag.texRect.size.x/2.f, fcy=frag.texRect.size.y/2.f;
                        spr->setOrigin({fcx,fcy}); spr->setTextureRect(frag.texRect); spr->setPosition(frag.pos);
                        spr->setRotation(sf::degrees(frag.rotation)); spr->setColor(sf::Color(255,255,255,(uint8_t)frag.alpha)); window.draw(*spr);
                        uint8_t g=(uint8_t)((1.f-fp)*100.f); if(g>0){spr->setColor(sf::Color(g,g,g,(uint8_t)frag.alpha));window.draw(*spr,sf::BlendAdd);}
                    }
                }
                spr->setOrigin({ts2.x/2.f,ts2.y/2.f}); spr->setTextureRect({{0,0},{(int)ts2.x,(int)ts2.y}}); spr->setRotation(sf::degrees(0.f)); spr->setColor({255,255,255,255});
            }
        } else {
            // ── 普通 AI 出牌 / 紫卡滑动：小卡沿路径移动 ──
            useSpr->setScale({0.36f, 0.36f});
            useSpr->setPosition(aiCardAnimPos);
            useSpr->setTextureRect(sf::IntRect({0,0}, {(int)ts.x, (int)ts.y}));
            useSpr->setColor(sf::Color(255, 255, 255, 255));
            window.draw(*useSpr);
            uiText.setFont(cardFont);
            uiText.setCharacterSize(28);
            uiText.setFillColor(sf::Color(255, 255, 255, 255));
            uiText.setString(showcasedCard.name);
            uiText.setPosition({aiCardAnimPos.x - 52.f, aiCardAnimPos.y - 164.f});
            window.draw(uiText);
            // 描述逐行（5行以内，与卡槽渲染一致）
            uiText.setCharacterSize(22);
            uiText.setFillColor(sf::Color(230, 230, 230, 255));
            {
                std::wstring desc = showcasedCard.description;
                size_t pos = 0; int li = 0; const float lh = 26.f;
                while (pos <= desc.size() && li < 6) {
                    size_t nl = desc.find(L'\n', pos);
                    if (nl == std::wstring::npos) nl = desc.size();
                    uiText.setString(sf::String(desc.substr(pos, nl - pos)));
                    uiText.setPosition({aiCardAnimPos.x - 88.f, aiCardAnimPos.y - 90.f + li * lh});
                    window.draw(uiText);
                    pos = nl + 1; li++;
                }
            }
            uiText.setFont(font);
        }
    }

    // 🌟【动态交互层：多卡堆叠渲染】
    if (currentState == GameState::GAME_PVP || currentState == GameState::GAME_PVE) {
        if (newCardSprite != nullptr && !playerDeck.hand.empty()) {

            // 重置卡牌精灵状态（防展示残留 scale=0.76 污染手牌渲染）
            sf::Vector2u nts = newCardTexture.getSize();
            newCardSprite->setScale({0.36f, 0.36f});
            newCardSprite->setOrigin({nts.x / 2.f, nts.y / 2.f});
            newCardSprite->setTextureRect(sf::IntRect({0, 0}, {(int)nts.x, (int)nts.y}));
            if (purpleCardSprite) {
                sf::Vector2u pts = purpleCardTexture.getSize();
                purpleCardSprite->setScale({0.36f, 0.36f});
                purpleCardSprite->setOrigin({pts.x / 2.f, pts.y / 2.f});
                purpleCardSprite->setTextureRect(sf::IntRect({0, 0}, {(int)pts.x, (int)pts.y}));
            }

            // 【时序捕获】：抽卡标记触发出生动画（比 handSize 变化更可靠）
            if (newCardJustDrawn) {
                cardAnimClock.restart();
                isAnimatingCard = true;
                newCardJustDrawn = false;
                // 🌟 启动碎片聚合出生动画
                cardBirthActive = true;
                cardBirthClock.restart();
                cardBirthInit = false;
            }

            // 🌟 卡牌出生碎片聚合：首次渲染帧初始化
            if (cardBirthActive && !cardBirthInit && !playerDeck.hand.empty()) {
                int lastIdx = (int)playerDeck.hand.size() - 1;
                bool isP3 = (playerDeck.hand[lastIdx].cardColor == 1);
                cardBirthFrags = isP3 ? cardFragCachePurple : cardFragCache;
                cardBirthTex = playerDeck.hand[lastIdx].cardColor;
                for (auto& frag : cardBirthFrags) {
                    frag.released = false; frag.alpha = 0.f; frag.fadeTimer = 0.f;
                    frag.rotation = (rand() % 60 - 30) * 1.f;
                    frag.pos.x = 2260.f + (rand() % 200 - 100) * 1.f; // 卡槽附近散落
                    frag.pos.y = 1200.f + (rand() % 200) * 1.f;        // 下方散落
                }
                cardBirthInit = true;
            }

            // 🌟 破釜沉舟退牌：首次渲染帧初始化所有卡牌碎片
            if (isReturningHandToDeck && !returnDeckInit) {
                cardDecayFrags.clear();
                size_t n = playerDeck.hand.size();
                returnDeckFrags.resize(n);
                returnDeckPos.resize(n);
                returnDeckTex.resize(n);
                for (size_t k = 0; k < n; ++k) {
                    bool isP = (playerDeck.hand[k].cardColor == 1);
                    returnDeckFrags[k] = isP ? cardFragCachePurple : cardFragCache;
                    returnDeckTex[k] = playerDeck.hand[k].cardColor;
                    for (auto& frag : returnDeckFrags[k]) {
                        frag.released = false; frag.alpha = 255.f; frag.fadeTimer = 0.f;
                        frag.rotation = 0.f;
                        frag.vel = {(rand()%200-100)*1.f, -(rand()%200+100)*1.f};
                        frag.rotSpeed = (rand()%720-360)*1.f;
                    }
                }
                returnDeckInit = true;
            }

            // 🌟 紫卡诅咒到期移除：首次渲染帧初始化碎片
            if (curseRemoving && !curseRemoveFragsInit && curseRemovingIdx >= 0 && curseRemovingIdx < (int)playerDeck.hand.size()) {
                cardDecayFrags.clear(); // 清掉可能残留的旧衰减碎片
                bool isP2 = (playerDeck.hand[curseRemovingIdx].cardColor == 1);
                curseRemoveFrags = isP2 ? cardFragCachePurple : cardFragCache;
                curseRemoveTex = playerDeck.hand[curseRemovingIdx].cardColor;
                for (auto& frag : curseRemoveFrags) {
                    frag.released = false; frag.alpha = 255.f; frag.fadeTimer = 0.f;
                    frag.rotation = 0.f;
                    frag.vel = {(rand()%200-100)*1.f, -(rand()%200+100)*1.f};
                    frag.rotSpeed = (rand()%720-360)*1.f;
                }
                curseRemoveFragsInit = true;
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
            sf::Vector2f targetPos(2260.f, 240.f);  // 默认槽1，防未初始化
            // 同步保护：两向量必须等长（湮灭期间跳过，避免干扰 activeIdx）
            if (annihilateState == CardAnnihilateState::NONE &&
                handSlotAssign.size() != playerDeck.hand.size()) {
                handSlotAssign.resize(playerDeck.hand.size(), 1);
            }
            sf::Vector2f preLoopPos = newCardSprite->getPosition();

            for (int slot = 1; slot <= 2; ++slot) {
                float baseX = 2260.f;
                float baseY = (slot == 1) ? 320.f : 960.f;
                int stackIdx = 0;
                for (size_t i = 0; i < playerDeck.hand.size(); ++i) {
                    if (handSlotAssign[i] != slot) continue;
                    sf::Vector2f offset(stackIdx * 6.f, stackIdx * 6.f);
                    sf::Vector2f cardPos(baseX + offset.x, baseY + offset.y);

                    if (static_cast<int>(i) == activeIdx) {
                        targetPos = cardPos; // 活跃卡回归点
                    } else {
                        // 紫卡传送期间跳过该卡静态渲染
                        if (purpleSendIndex == static_cast<int>(i) && purpleSendState != PurpleSendState::IDLE) continue;
                        // 静态卡牌：根据 cardColor 选择纹理
                        bool isPurple = (playerDeck.hand[i].cardColor == 1);
                        sf::Sprite* cardSpr = (isPurple && purpleCardSprite) ? purpleCardSprite : newCardSprite;
                        sf::Texture& cardTex = isPurple ? purpleCardTexture : newCardTexture;
                        cardSpr->setScale({0.36f, 0.36f});
                        cardSpr->setPosition(cardPos);
                        sf::Vector2u texSize = cardTex.getSize();

                        bool isCurseRm = (curseRemoving && (int)i == curseRemovingIdx);
                        if (isReturningHandToDeck || isCurseRm) {
                            // 🌟 碎片销毁（退牌 / 诅咒移除）
                            float rt = isCurseRm ? curseRemoveClock.getElapsedTime().asSeconds() / 2.0f
                                                 : returnHandToDeckClock.getElapsedTime().asSeconds() / 2.0f;
                            if (rt > 1.f) rt = 1.f;
                            float t2 = rt * rt * (3.f - 2.f * rt);
                            float clipH2 = texSize.y * (1.f - t2);
                            int clipH = (int)clipH2;
                            if (clipH > 0) {
                                cardSpr->setOrigin({texSize.x / 2.f, texSize.y / 2.f});
                                cardSpr->setTextureRect(sf::IntRect({0, 0}, {(int)texSize.x, clipH}));
                                cardSpr->setPosition(cardPos);
                                cardSpr->setColor(sf::Color(255, 255, 255, 255));
                                window.draw(*cardSpr);
                            }
                            // 碎片渲染
                            bool hasFrags2 = (isCurseRm && curseRemoveFragsInit) || (isReturningHandToDeck && returnDeckInit && i < (int)returnDeckFrags.size());
                            if (hasFrags2) {
                                auto& frags = isCurseRm ? curseRemoveFrags : returnDeckFrags[i];
                                int fragTex = isCurseRm ? curseRemoveTex : returnDeckTex[i];
                                sf::Sprite* spF = (fragTex == 1 && purpleCardSprite) ? purpleCardSprite : newCardSprite;
                                sf::Texture& txF = (fragTex == 1) ? purpleCardTexture : newCardTexture;
                                sf::Vector2u tsF = txF.getSize();
                                if (tsF.x > 0 && tsF.y > 0) {
                                    spF->setScale({0.36f, 0.36f});
                                    for (auto& frag : frags) {
                                        if (!frag.released) {
                                            if ((float)frag.texRect.position.y > clipH2) {
                                                frag.released = true;
                                                float fcx = frag.texRect.size.x / 2.f;
                                                float fcy = frag.texRect.size.y / 2.f;
                                                frag.pos.x = cardPos.x + (frag.texRect.position.x + fcx - tsF.x / 2.f) * 0.36f;
                                                frag.pos.y = cardPos.y + (frag.texRect.position.y + fcy - tsF.y / 2.f) * 0.36f;
                                            }
                                        }
                                        if (frag.released) {
                                            frag.vel.y += 800.f * 0.016f;
                                            frag.pos += frag.vel * 0.016f;
                                            frag.rotation += frag.rotSpeed * 0.016f;
                                            frag.fadeTimer += 0.016f;
                                            float fp = frag.fadeTimer / 0.85f;
                                            if (fp > 1.f) fp = 1.f;
                                            frag.alpha = 255.f * (1.f - fp * fp * (3.f - 2.f * fp));
                                            float fcx = frag.texRect.size.x / 2.f;
                                            float fcy = frag.texRect.size.y / 2.f;
                                            spF->setOrigin({fcx, fcy});
                                            spF->setTextureRect(frag.texRect);
                                            spF->setPosition(frag.pos);
                                            spF->setRotation(sf::degrees(frag.rotation));
                                            spF->setColor(sf::Color(255, 255, 255, (uint8_t)frag.alpha));
                                            window.draw(*spF);
                                            uint8_t glow = (uint8_t)((1.f - fp) * 120.f);
                                            if (glow > 0) { spF->setColor(sf::Color(glow, glow, glow, (uint8_t)frag.alpha)); window.draw(*spF, sf::BlendAdd); }
                                        }
                                    }
                                    spF->setOrigin({tsF.x / 2.f, tsF.y / 2.f});
                                    spF->setTextureRect(sf::IntRect({0, 0}, {(int)tsF.x, (int)tsF.y}));
                                    spF->setRotation(sf::degrees(0.f));
                                    spF->setColor(sf::Color(255, 255, 255, 255));
                                }
                            }
                            // 文字逐行：扫描线扫到时 0.1s 淡出
                            if ((returnDeckInit || (isCurseRm && curseRemoveFragsInit)) && i < (int)playerDeck.hand.size()) {
                                const auto& sc = playerDeck.hand[i];
                                const float fadeZone2 = texSize.y * 0.05f; // 0.1s
                                auto textAlpha = [&](float screenY) -> uint8_t {
                                    float texY = texSize.y/2.f + (screenY - cardPos.y) / 0.36f;
                                    if (texY <= clipH2 - fadeZone2) return 255;
                                    if (texY >= clipH2) return 0;
                                    return (uint8_t)(255.f * (clipH2 - texY) / fadeZone2);
                                };
                                uiText.setFont(cardFont);
                                uint8_t na = textAlpha(cardPos.y - 164.f);
                                if (na > 0) {
                                    uiText.setCharacterSize(28);
                                    uiText.setFillColor(sf::Color(255,255,255,na));
                                    uiText.setString(playerBlindActive ? scrambleText(sc.name) : sc.name);
                                    uiText.setPosition({cardPos.x-52.f, cardPos.y-164.f});
                                    window.draw(uiText);
                                }
                                std::wstring desc = sc.description;
                                size_t dp = 0; int dl = 0; const float lH = 26.f;
                                while (dp <= desc.size()) {
                                    size_t nl = desc.find(L'\n', dp);
                                    if (nl == std::wstring::npos) nl = desc.size();
                                    float sy = cardPos.y - 90.f + dl * lH;
                                    uint8_t da = textAlpha(sy);
                                    if (da > 0) {
                                        uiText.setCharacterSize(22);
                                        uiText.setFillColor(sf::Color(230,230,230,da));
                                        uiText.setString(sf::String(desc.substr(dp, nl-dp)));
                                        uiText.setPosition({cardPos.x-88.f, sy});
                                        window.draw(uiText);
                                    }
                                    dp = nl+1; dl++;
                                }
                                uiText.setFont(font);
                            }
                            cardSpr->setTextureRect(sf::IntRect({0, 0}, {(int)texSize.x, (int)texSize.y}));
                        } else {
                            cardSpr->setTextureRect(sf::IntRect({0, 0}, {static_cast<int>(texSize.x), static_cast<int>(texSize.y)}));
                            cardSpr->setColor(sf::Color(255, 255, 255, 255));
                            window.draw(*cardSpr, sf::BlendAlpha);
                            // 文字逐行渲染（动画与静态统一间距）
                            {
                                const auto& sc = playerDeck.hand[i];
                                const float lineH = 26.f;
                                uiText.setFont(cardFont);
                                // 名称
                                uiText.setCharacterSize(28);
                                uiText.setFillColor(sf::Color(255, 255, 255, 255));
                                uiText.setString(playerBlindActive ? scrambleText(sc.name) : sc.name);
                                uiText.setPosition({cardPos.x - 52.f, cardPos.y - 164.f});
                                window.draw(uiText);
                                // 描述逐行
                                uiText.setCharacterSize(22);
                                uiText.setFillColor(sf::Color(230, 230, 230, 255));
                                std::wstring desc = playerBlindActive ? scrambleText(sc.description) : sc.description;
                                size_t pos = 0; int li = 0;
                                while (pos <= desc.size()) {
                                    size_t nl = desc.find(L'\n', pos);
                                    if (nl == std::wstring::npos) nl = desc.size();
                                    uiText.setString(sf::String(desc.substr(pos, nl - pos)));
                                    uiText.setPosition({cardPos.x - 88.f, cardPos.y - 90.f + li * lineH});
                                    window.draw(uiText);
                                    pos = nl + 1; li++;
                                }
                                uiText.setFont(font);
                            }
                        }
                    }
                    stackIdx++;
                }
            }

            // 恢复精灵到活跃卡的正确位置（湮灭期间也必须恢复，否则静态循环残留坐标污染状态机）
            if (isCardAttachedToMouse) {
                sf::Vector2i pix = sf::Mouse::getPosition(window);
                sf::Vector2f wpos = window.mapPixelToCoords(pix);
                newCardSprite->setPosition(wpos - cardMouseOffset);
            } else {
                newCardSprite->setPosition(preLoopPos);
            }

            // ── 顶卡：完整状态机 + 动画 + 文字 ──

            // 紫卡传送期间跳过活跃卡渲染（由紫卡动画块接管）
            if (purpleSendState != PurpleSendState::IDLE && activeIdx == purpleSendIndex) {
                // skip active card rendering
            } else {

            // 紫卡活跃时临时切换精灵纹理
            bool activeIsPurple = (activeIdx >= 0 && activeIdx < static_cast<int>(playerDeck.hand.size()) &&
                                   playerDeck.hand[activeIdx].cardColor == 1);
            const sf::Texture& savedTex = newCardSprite->getTexture();
            if (activeIsPurple && purpleCardSprite) {
                newCardSprite->setTexture(purpleCardTexture);
            }

            // ============================================================
            // 🌟【新复合精调版】：支持鼠标吸附、弹回以及全新的湮灭仪式计算
            // ============================================================
            {
            sf::Vector2f cardPos;
            bool cardAlreadyRendered = false;  // MOVE_TO_ZERO 完成帧防双重渲染

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
                            // 【阶段 1】：点中后，在原地停顿 0.2 秒 → 滑向读卡器
                            if (elapsed >= 0.2f) {
                                annihilateState = CardAnnihilateState::MOVE_TO_200;
                                annihilateClock.restart();
                            }
                            break;

                        case CardAnnihilateState::SHATTER: {
                            // 🌟 边上升边粉碎：cardShatterPos 从 (300,540) → (300,210)，0.7s smoothstep
                            float rawT = cardShatterClock.getElapsedTime().asSeconds() / 0.8f;
                            if (rawT > 1.f) rawT = 1.f;
                            float t = rawT * rawT * (3.f - 2.f * rawT); // smoothstep
                            // 动画位置：向上滑入读卡器
                            cardShatterPos = {300.f, 540.f + (210.f - 540.f) * t};

                            // 选用正确的卡牌精灵
                            sf::Sprite* sp = (cardShatterTex == 1 && purpleCardSprite) ? purpleCardSprite : newCardSprite;
                            sf::Texture& tx = (cardShatterTex == 1 && purpleCardSprite) ? purpleCardTexture : newCardTexture;
                            sf::Vector2u ts = tx.getSize();
                            if (ts.x > 0 && ts.y > 0 && !cardFragActive.empty()) {
                                float scale = 0.36f;
                                sp->setScale({scale, scale});
                                float clipHeight = ts.y * (1.f - t);
                                sf::Vector2f pos = cardShatterPos;

                                // 1. 完整部分（裁剪纹理，无缝）
                                int clipH = (int)clipHeight;
                                if (clipH > 0) {
                                    sp->setOrigin({ts.x / 2.f, ts.y / 2.f});
                                    sp->setTextureRect(sf::IntRect({0, 0}, {(int)ts.x, clipH}));
                                    sp->setPosition(pos);
                                    sp->setColor(sf::Color(255, 255, 255, 255));
                                    window.draw(*sp);
                                }

                                // 2. dt 计算
                                float dt2 = rawT - cardShatterLastT;
                                cardShatterLastT = rawT;
                                if (dt2 < 0.f) dt2 = 0.f; if (dt2 > 0.1f) dt2 = 0.016f;

                                // 3. 粉碎碎片
                                for (auto& frag : cardFragActive) {
                                    if (!frag.released) {
                                        if ((float)frag.texRect.position.y > clipHeight) {
                                            frag.released = true;
                                            float fcx = frag.texRect.size.x / 2.f;
                                            float fcy = frag.texRect.size.y / 2.f;
                                            frag.pos.x = pos.x + (frag.texRect.position.x + fcx - ts.x / 2.f) * scale;
                                            frag.pos.y = pos.y + (frag.texRect.position.y + fcy - ts.y / 2.f) * scale;
                                        }
                                    }
                                    if (frag.released) {
                                        frag.vel.y += 800.f * dt2;
                                        frag.pos += frag.vel * dt2;
                                        frag.rotation += frag.rotSpeed * dt2;
                                        frag.fadeTimer += dt2;
                                        float fp = frag.fadeTimer / 0.85f;
                                        if (fp > 1.f) fp = 1.f;
                                        frag.alpha = 255.f * (1.f - fp * fp * (3.f - 2.f * fp));
                                        float fcx = frag.texRect.size.x / 2.f;
                                        float fcy = frag.texRect.size.y / 2.f;
                                        sp->setOrigin({fcx, fcy});
                                        sp->setTextureRect(frag.texRect);
                                        sp->setPosition(frag.pos);
                                        sp->setRotation(sf::degrees(frag.rotation));
                                        sp->setColor(sf::Color(255, 255, 255, (uint8_t)frag.alpha));
                                        window.draw(*sp);
                                        uint8_t glow3 = (uint8_t)((1.f - fp) * 120.f);
                                        if (glow3 > 0) { sp->setColor(sf::Color(glow3, glow3, glow3, (uint8_t)frag.alpha)); window.draw(*sp, sf::BlendAdd); }
                                    }
                                }
                                sp->setOrigin({ts.x / 2.f, ts.y / 2.f});
                                sp->setTextureRect(sf::IntRect({0, 0}, {(int)ts.x, (int)ts.y}));
                                sp->setRotation(sf::degrees(0.f));
                                sp->setColor(sf::Color(255, 255, 255, 255));
                                // 文字逐行淡出
                                if (attachedCardIndex >= 0 && attachedCardIndex < (int)playerDeck.hand.size()) {
                                    const auto& sc2 = playerDeck.hand[attachedCardIndex];
                                    auto txtFade2 = [&](float screenY) -> uint8_t {
                                        float texY = ts.y/2.f + (screenY - pos.y) / 0.36f;
                                        float clipFrac = 1.f - t;
                                        float th = texY / ts.y;
                                        if (th < clipFrac - 0.02f) return 255;
                                        if (th > clipFrac + 0.02f) return 0;
                                        return (uint8_t)(255.f * (clipFrac + 0.02f - th) / 0.04f);
                                    };
                                    uiText.setFont(cardFont);
                                    uint8_t na2 = txtFade2(pos.y - 164.f);
                                    if (na2 > 0) { uiText.setCharacterSize(28); uiText.setFillColor(sf::Color(255,255,255,na2));
                                        uiText.setString(playerBlindActive ? scrambleText(sc2.name) : sc2.name);
                                        uiText.setPosition(sf::Vector2f(pos.x-52.f, pos.y-164.f)); window.draw(uiText); }
                                    std::wstring desc2 = sc2.description;
                                    size_t dp2 = 0; int dl2 = 0;
                                    while (dp2 <= desc2.size()) {
                                        size_t nl2 = desc2.find(L'\n', dp2);
                                        if (nl2 == std::wstring::npos) nl2 = desc2.size();
                                        float sy2 = pos.y - 90.f + dl2 * 26.f;
                                        uint8_t da2 = txtFade2(sy2);
                                        if (da2 > 0) { uiText.setCharacterSize(22); uiText.setFillColor(sf::Color(230,230,230,da2));
                                            uiText.setString(sf::String(desc2.substr(dp2, nl2-dp2)));
                                            uiText.setPosition(sf::Vector2f(pos.x-88.f, sy2)); window.draw(uiText); }
                                        dp2 = nl2+1; dl2++;
                                    }
                                    uiText.setFont(font);
                                }
                            }
                            cardAlreadyRendered = true;

                            if (rawT >= 1.f) {
                                if (!playerDeck.hand.empty() && attachedCardIndex >= 0) {
                                    showcasedCard = playerDeck.hand[attachedCardIndex];
                                    playerDeck.discardCard(showcasedCard);
                                    playerDeck.hand.erase(playerDeck.hand.begin() + attachedCardIndex);
                                    handSlotAssign.erase(handSlotAssign.begin() + attachedCardIndex);
                                    attachedCardIndex = -1;
                                    showcaseState = CardShowcaseState::PAUSE;
                                    showcaseClock.restart();
                                    newCardSprite->setPosition({300.f, 540.f});
                                    annihilateState = CardAnnihilateState::NONE;
                                }
                            }
                            break;
                        }

                        case CardAnnihilateState::MOVE_TO_200: {
                            // 🎯 设定第二阶段的目标过渡坐标
                            sf::Vector2f targetAnnihilatePos(300.f, 540.f);

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
                            // 【阶段 3】：在 (300, 540) 停顿 0.2 秒 → 边上升边粉碎
                            cardPos = {300.f, 540.f};
                            if (elapsed >= 0.2f) {
                                const Card& c = playerDeck.hand[attachedCardIndex];
                                cardFragActive = (c.cardColor == 1) ? cardFragCachePurple : cardFragCache;
                                cardShatterTex = c.cardColor;
                                cardShatterPos = cardPos;
                                for (auto& frag : cardFragActive) {
                                    frag.released = false; frag.alpha = 255.f; frag.fadeTimer = 0.f;
                                    frag.rotation = 0.f;
                                    frag.vel = {(rand()%200-100)*1.f, -(rand()%200+100)*1.f};
                                    frag.rotSpeed = (rand()%720-360)*1.f;
                                    frag.pos = cardPos;
                                }
                                cardShatterClock.restart();
                                cardShatterLastT = 0.f;
                                annihilateState = CardAnnihilateState::SHATTER;
                            }
                            break;

                        case CardAnnihilateState::MOVE_TO_ZERO: {
                            // 【阶段 4】：缓动滑入读卡器（只计算坐标，绘制交统一路径）
                            float startY = 400.f;  // MOVE_TO_200 落点
                            float targetY = 210.f; // CardReader Y
                            float duration = 0.35f;
                            float t = elapsed / duration;
                            if (t > 1.f) t = 1.f;
                            float eased = t * t * (3.f - 2.f * t);  // smoothstep
                            float curY = startY + (targetY - startY) * eased;
                            cardPos = {300.f, curY};

                            if (t >= 1.f) {
                                if (!playerDeck.hand.empty() && attachedCardIndex >= 0) {
                                    showcasedCard = playerDeck.hand[attachedCardIndex];
                                    playerDeck.discardCard(showcasedCard);
                                    playerDeck.hand.erase(playerDeck.hand.begin() + attachedCardIndex);
                                    handSlotAssign.erase(handSlotAssign.begin() + attachedCardIndex);
                                    attachedCardIndex = -1;
                                    std::cout << "[Annihilate] 卡牌已吸入读卡器，正式湮灭！" << std::endl;
                                    showcaseState = CardShowcaseState::PAUSE;
                                    showcaseClock.restart();
                                    cardAlreadyRendered = true;  // 防本帧二次渲染
                                    newCardSprite->setPosition({300.f, 540.f});
                                    annihilateState = CardAnnihilateState::NONE;
                                }
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
                
                if (annihilateState != CardAnnihilateState::PAUSE_FINAL
                    && !cardAlreadyRendered) {
                // 更新卡牌位置并锁死缩放比例
                newCardSprite->setScale({0.36f, 0.36f});
                newCardSprite->setPosition(cardPos);

                // 获取活跃卡的实际纹理尺寸
                bool activeIsPurple = (activeIdx >= 0 && activeIdx < (int)playerDeck.hand.size() && playerDeck.hand[activeIdx].cardColor == 1);
                sf::Vector2u texSize = (activeIsPurple && purpleCardSprite) ? purpleCardTexture.getSize() : newCardTexture.getSize();

                bool isCurseActive2 = (curseRemoving && activeIdx == curseRemovingIdx);
                if (isReturningHandToDeck || isCurseActive2) {
                    // 🌟 碎片销毁（退牌 / 诅咒移除，槽顶卡版）
                    float rt2 = isCurseActive2 ? curseRemoveClock.getElapsedTime().asSeconds() / 2.0f
                                               : returnHandToDeckClock.getElapsedTime().asSeconds() / 2.0f;
                    if (rt2 > 1.f) rt2 = 1.f;
                    float t2 = rt2 * rt2 * (3.f - 2.f * rt2);
                    float clipH2 = texSize.y * (1.f - t2);
                    int clipH = (int)clipH2;
                    sf::Sprite* spTop = (activeIsPurple && purpleCardSprite) ? purpleCardSprite : newCardSprite;
                    if (clipH > 0) {
                        spTop->setOrigin({texSize.x / 2.f, texSize.y / 2.f});
                        spTop->setTextureRect(sf::IntRect({0, 0}, {(int)texSize.x, clipH}));
                        spTop->setPosition(cardPos);
                        spTop->setColor(sf::Color(255, 255, 255, 255));
                        window.draw(*spTop);
                    }
                    bool isCurseActive = (curseRemoving && activeIdx == curseRemovingIdx);
                    bool hasFrags3 = (isCurseActive && curseRemoveFragsInit) || (isReturningHandToDeck && returnDeckInit && activeIdx >= 0 && activeIdx < (int)returnDeckFrags.size());
                    if (hasFrags3) {
                        auto& frags = isCurseActive ? curseRemoveFrags : returnDeckFrags[activeIdx];
                        int fragTex = isCurseActive ? curseRemoveTex : returnDeckTex[activeIdx];
                        sf::Sprite* spF2 = (fragTex == 1 && purpleCardSprite) ? purpleCardSprite : newCardSprite;
                        sf::Texture& txF2 = (fragTex == 1) ? purpleCardTexture : newCardTexture;
                        sf::Vector2u tsF = txF2.getSize();
                        if (tsF.x > 0 && tsF.y > 0) {
                            spF2->setScale({0.36f, 0.36f});
                            for (auto& frag : frags) {
                                if (!frag.released) {
                                    if ((float)frag.texRect.position.y > clipH2) {
                                        frag.released = true;
                                        float fcx = frag.texRect.size.x / 2.f;
                                        float fcy = frag.texRect.size.y / 2.f;
                                        frag.pos.x = cardPos.x + (frag.texRect.position.x + fcx - tsF.x / 2.f) * 0.36f;
                                        frag.pos.y = cardPos.y + (frag.texRect.position.y + fcy - tsF.y / 2.f) * 0.36f;
                                    }
                                }
                                if (frag.released) {
                                    frag.vel.y += 800.f * 0.016f;
                                    frag.pos += frag.vel * 0.016f;
                                    frag.rotation += frag.rotSpeed * 0.016f;
                                    frag.fadeTimer += 0.016f;
                                    float fp = frag.fadeTimer / 0.85f;
                                    if (fp > 1.f) fp = 1.f;
                                    frag.alpha = 255.f * (1.f - fp * fp * (3.f - 2.f * fp));
                                    float fcx = frag.texRect.size.x / 2.f;
                                    float fcy = frag.texRect.size.y / 2.f;
                                    spF2->setOrigin({fcx, fcy});
                                    spF2->setTextureRect(frag.texRect);
                                    spF2->setPosition(frag.pos);
                                    spF2->setRotation(sf::degrees(frag.rotation));
                                    spF2->setColor(sf::Color(255, 255, 255, (uint8_t)frag.alpha));
                                    window.draw(*spF2);
                                    uint8_t glow2 = (uint8_t)((1.f - fp) * 120.f);
                                    if (glow2 > 0) { spF2->setColor(sf::Color(glow2, glow2, glow2, (uint8_t)frag.alpha)); window.draw(*spF2, sf::BlendAdd); }
                                }
                            }
                            spF2->setOrigin({tsF.x / 2.f, tsF.y / 2.f});
                            spF2->setTextureRect(sf::IntRect({0, 0}, {(int)tsF.x, (int)tsF.y}));
                            spF2->setRotation(sf::degrees(0.f));
                            spF2->setColor(sf::Color(255, 255, 255, 255));
                        }
                    }
                    // 文字逐行：扫描线扫到时 0.1s 淡出（槽顶卡）
                    if ((returnDeckInit || (isCurseActive2 && curseRemoveFragsInit)) && activeIdx >= 0 && activeIdx < (int)playerDeck.hand.size()) {
                        const auto& sc = playerDeck.hand[activeIdx];
                        const float fadeZone3 = texSize.y * 0.05f;
                        auto textAlpha2 = [&](float screenY) -> uint8_t {
                            float texY2 = texSize.y/2.f + (screenY - cardPos.y) / 0.36f;
                            if (texY2 <= clipH2 - fadeZone3) return 255;
                            if (texY2 >= clipH2) return 0;
                            return (uint8_t)(255.f * (clipH2 - texY2) / fadeZone3);
                        };
                        uiText.setFont(cardFont);
                        uint8_t na2 = textAlpha2(cardPos.y - 164.f);
                        if (na2 > 0) {
                            uiText.setCharacterSize(28);
                            uiText.setFillColor(sf::Color(255,255,255,na2));
                            uiText.setString(playerBlindActive ? scrambleText(sc.name) : sc.name);
                            uiText.setPosition({cardPos.x-52.f, cardPos.y-164.f});
                            window.draw(uiText);
                        }
                        std::wstring desc = sc.description;
                        size_t dp = 0; int dl = 0; const float lH = 26.f;
                        while (dp <= desc.size()) {
                            size_t nl = desc.find(L'\n', dp);
                            if (nl == std::wstring::npos) nl = desc.size();
                            float sy = cardPos.y - 90.f + dl * lH;
                            uint8_t da2 = textAlpha2(sy);
                            if (da2 > 0) {
                                uiText.setCharacterSize(22);
                                uiText.setFillColor(sf::Color(230,230,230,da2));
                                uiText.setString(sf::String(desc.substr(dp, nl-dp)));
                                uiText.setPosition({cardPos.x-88.f, sy});
                                window.draw(uiText);
                            }
                            dp = nl+1; dl++;
                        }
                        uiText.setFont(font);
                    }
                    spTop->setTextureRect(sf::IntRect({0, 0}, {(int)texSize.x, (int)texSize.y}));
                    spTop->setOrigin({texSize.x / 2.f, texSize.y / 2.f});
                    spTop->setRotation(sf::degrees(0.f));
                    spTop->setColor(sf::Color(255, 255, 255, 255));
                    // 如果 spTop 是 purpleCardSprite 还需要恢复 newCardSprite
                    if (activeIsPurple && purpleCardSprite) {
                        sf::Vector2u nt = newCardTexture.getSize();
                        newCardSprite->setTextureRect(sf::IntRect({0, 0}, {(int)nt.x, (int)nt.y}));
                        newCardSprite->setOrigin({nt.x / 2.f, nt.y / 2.f});
                        newCardSprite->setColor(sf::Color(255, 255, 255, 255));
                    }
                } else if (cardBirthActive && cardBirthInit && activeIdx == (int)playerDeck.hand.size() - 1) {
                // 🌟 碎片聚合出生：自上往下扫描，碎片从下方飞入聚合
                float rawT = cardBirthClock.getElapsedTime().asSeconds() / 1.5f;
                if (rawT > 1.f) rawT = 1.f;
                float t2 = (rawT < 0.667f) ? (rawT / 0.667f) : 1.f; // 前 1.0s 聚合，后 0.5s 衰减
                sf::Sprite* spB = (cardBirthTex == 1 && purpleCardSprite) ? purpleCardSprite : newCardSprite;
                sf::Texture& txB = (cardBirthTex == 1) ? purpleCardTexture : newCardTexture;
                sf::Vector2u tsB = txB.getSize();
                if (tsB.x > 0 && tsB.y > 0 && !cardBirthFrags.empty()) {
                    float wipeY = tsB.y * t2;
                    spB->setScale({0.36f, 0.36f});
                    // 已聚合部分（擦除线以上）：无缝裁剪纹理
                    int visH = (int)wipeY;
                    if (visH > 0) {
                        spB->setOrigin({tsB.x / 2.f, tsB.y / 2.f});
                        spB->setTextureRect(sf::IntRect({0, 0}, {(int)tsB.x, visH}));
                        spB->setPosition(cardPos);
                        spB->setColor(sf::Color(255, 255, 255, 255));
                        window.draw(*spB);
                    }
                    // 聚合碎片（擦除线以下正在飞入）
                    for (auto& frag : cardBirthFrags) {
                        float fcx = frag.texRect.size.x / 2.f;
                        float fcy = frag.texRect.size.y / 2.f;
                        float tx = cardPos.x + (frag.texRect.position.x + fcx - tsB.x / 2.f) * 0.36f;
                        float ty = cardPos.y + (frag.texRect.position.y + fcy - tsB.y / 2.f) * 0.36f;
                        float fragTop = (float)frag.texRect.position.y;
                        if (!frag.released && fragTop < wipeY + tsB.y * 0.08f && fragTop >= wipeY - tsB.y * 0.02f) {
                            frag.released = true;
                            frag.targetPos = {tx, ty};
                            frag.fadeTimer = 0.f;
                            // 散落起始位置紧贴目标下方 30~80px
                            frag.pos.x = tx + (rand() % 60 - 30) * 1.f;
                            frag.pos.y = ty + (rand() % 50 + 30) * 1.f;
                        }
                        if (frag.released) {
                            frag.fadeTimer += 0.016f;
                            float prog = frag.fadeTimer / 0.35f;
                            if (prog > 1.f) prog = 1.f;
                            float eased = prog * prog * (3.f - 2.f * prog);
                            frag.pos.x += (frag.targetPos.x - frag.pos.x) * 0.12f;
                            frag.pos.y += (frag.targetPos.y - frag.pos.y) * 0.12f;
                            frag.alpha = 255.f * eased;
                            spB->setOrigin({fcx, fcy});
                            spB->setTextureRect(frag.texRect);
                            spB->setPosition(frag.pos);
                            spB->setRotation(sf::degrees(frag.rotation * (1.f - eased)));
                            spB->setColor(sf::Color(255, 255, 255, (uint8_t)frag.alpha));
                            window.draw(*spB);
                            // 亮度：飞行中渐增，到达后 0.5s 缓慢暗下来
                            float glowB = 0.f;
                            if (eased < 1.f) {
                                glowB = eased * 80.f;
                            } else {
                                float tAfter = frag.fadeTimer - 0.35f;
                                float f2 = tAfter / 0.25f; if (f2 > 1.f) f2 = 1.f;
                                glowB = 80.f * (1.f - f2);
                            }
                            if (glowB > 0.f) { spB->setColor(sf::Color((uint8_t)glowB, (uint8_t)glowB, (uint8_t)glowB, (uint8_t)frag.alpha)); window.draw(*spB, sf::BlendAdd); }
                        }
                    }
                    spB->setOrigin({tsB.x / 2.f, tsB.y / 2.f});
                    spB->setTextureRect(sf::IntRect({0, 0}, {(int)tsB.x, (int)tsB.y}));
                    spB->setRotation(sf::degrees(0.f));
                    spB->setColor(sf::Color(255, 255, 255, 255));
                    spB->setPosition(cardPos);
                    // 文字逐行：扫描线扫到时 0.1s 淡入
                    if (!playerDeck.hand.empty()) {
                        const auto& sc = playerDeck.hand[activeIdx];
                        const float fadeZone = tsB.y * 0.067f; // 0.1s
                        auto textAlphaB = [&](float screenY) -> uint8_t {
                            float texY2 = tsB.y/2.f + (screenY - cardPos.y) / 0.36f;
                            if (wipeY >= texY2 + fadeZone) return 255;
                            if (wipeY <= texY2) return 0;
                            return (uint8_t)(255.f * (wipeY - texY2) / fadeZone);
                        };
                        uiText.setFont(cardFont);
                        uint8_t na = textAlphaB(cardPos.y - 164.f);
                        if (na > 0) {
                            uiText.setCharacterSize(28);
                            uiText.setFillColor(sf::Color(255,255,255,na));
                            uiText.setString(playerBlindActive ? scrambleText(sc.name) : sc.name);
                            uiText.setPosition({cardPos.x-52.f, cardPos.y-164.f});
                            window.draw(uiText);
                        }
                        std::wstring desc = sc.description;
                        size_t dp = 0; int dl = 0; const float lH = 26.f;
                        while (dp <= desc.size()) {
                            size_t nl = desc.find(L'\n', dp);
                            if (nl == std::wstring::npos) nl = desc.size();
                            float sy = cardPos.y - 90.f + dl * lH;
                            uint8_t da = textAlphaB(sy);
                            if (da > 0) {
                                uiText.setCharacterSize(22);
                                uiText.setFillColor(sf::Color(230,230,230,da));
                                uiText.setString(sf::String(desc.substr(dp, nl-dp)));
                                uiText.setPosition({cardPos.x-88.f, sy});
                                window.draw(uiText);
                            }
                            dp = nl+1; dl++;
                        }
                        uiText.setFont(font);
                    }
                }
                cardAlreadyRendered = true;
                if (rawT >= 0.667f) { cardBirthActive = false; isAnimatingCard = false; }
                } else {
                // 🌟 动效一：连续插值替代固定50段量化，消除帧率拍频闪烁
                float currentHeightPercent = animTime / 1.0f;  // 0.0 → 1.0 连续增长
                if (currentHeightPercent > 1.f) currentHeightPercent = 1.f;
                if (animTime > 1.0f) currentHeightPercent = 1.f;

                // ⏳ 弹回阶段过了停顿期后，强制全显
                if (isReturningToSlot && returnDelayClock.getElapsedTime().asSeconds() >= 0.3f) {
                    currentHeightPercent = 1.f;
                }
                // 🔒 湮灭期间强制全显，抑制出生动画裁剪/白光闪烁
                if (annihilateState != CardAnnihilateState::NONE) {
                    currentHeightPercent = 1.f;
                }
                int currentRectHeight = static_cast<int>(texSize.y * currentHeightPercent);

                // 应用 50 阶微级阶梯裁剪矩形
                newCardSprite->setTextureRect(sf::IntRect({0, 0}, {static_cast<int>(texSize.x), currentRectHeight}));

                // 🌟 动效二：【1.5秒纯白褪去变原色】（湮灭期间跳过）
                if (animTime <= 1.5f && annihilateState == CardAnnihilateState::NONE && isAnimatingCard) {
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

                    // 文字跟随卡牌裁剪边缘逐行出现（0.1s 淡入+亮→原色）
                    float halfH = texSize.y * 0.18f;
                    auto lineTh = [&](float yOff) { return (halfH - yOff) / (halfH * 2.f); };
                    auto drawLine = [&](const sf::String& str, float x, float y, int size,
                                        sf::Color normColor, float threshold) {
                        float lineAge = animTime - threshold; // 这行已可见多久
                        if (lineAge < 0.f) return;
                        float fade = lineAge / 0.1f; if (fade > 1.f) fade = 1.f;
                        uint8_t a = static_cast<uint8_t>(255.f * fade);
                        uint8_t r = static_cast<uint8_t>(normColor.r + (255 - normColor.r) * (1.f - fade));
                        uint8_t g = static_cast<uint8_t>(normColor.g + (255 - normColor.g) * (1.f - fade));
                        uint8_t b = static_cast<uint8_t>(normColor.b + (255 - normColor.b) * (1.f - fade));
                        uiText.setCharacterSize(size);
                        uiText.setFillColor(sf::Color(r, g, b, a));
                        uiText.setString(str);
                        uiText.setPosition({x, y});
                        window.draw(uiText);
                    };
                    // 卡牌名
                    drawLine(sf::String(playerBlindActive ? scrambleText(currentCard.name) : currentCard.name), cardPos.x - 52.f, cardPos.y - 164.f,
                             28, sf::Color(255, 255, 255), lineTh(164.f));
                    // 描述逐行（行距与静态卡牌统一：26px）
                    const float lineH = 26.f;
                    std::wstring desc = playerBlindActive ? scrambleText(currentCard.description) : currentCard.description;
                    size_t pos = 0; int li = 0;
                    while (pos <= desc.size()) {
                        size_t nl = desc.find(L'\n', pos);
                        if (nl == std::wstring::npos) nl = desc.size();
                        float ly = 90.f - li * lineH;
                        drawLine(sf::String(desc.substr(pos, nl - pos)), cardPos.x - 88.f,
                                 cardPos.y - ly, 22, sf::Color(230, 230, 230), lineTh(ly));
                        pos = nl + 1; li++;
                    }

                    uiText.setFont(font); // 恢复主系统字体
                }
                } // end if (isReturningHandToDeck) else
            } // 顶卡状态机作用域结束
        }
        // 恢复精灵纹理（紫卡临时切换）
        if (activeIsPurple) {
            newCardSprite->setTexture(savedTex, true);
        }
        } // end if purpleSendState skip
    }
    }

    // ============================================================================
    // ── 卡牌碎片衰减（动画结束后继续淡出）──
    if (!cardDecayFrags.empty()) {
        sf::Sprite* spCd = (cardDecayTex == 1 && purpleCardSprite) ? purpleCardSprite : newCardSprite;
        sf::Texture& txCd = (cardDecayTex == 1) ? purpleCardTexture : newCardTexture;
        sf::Vector2u tsCd = txCd.getSize();
        if (tsCd.x > 0 && tsCd.y > 0) {
            spCd->setScale({0.36f, 0.36f});
            for (size_t di = 0; di < cardDecayFrags.size(); ) {
                auto& frag = cardDecayFrags[di];
                float fp = 1.f;
                if (frag.alpha > 0.f) {
                    frag.vel.y += 800.f * 0.016f;
                    frag.pos += frag.vel * 0.016f;
                    frag.fadeTimer += 0.016f;
                    fp = frag.fadeTimer / 0.85f;
                    if (fp > 1.f) fp = 1.f;
                    frag.alpha = 255.f * (1.f - fp * fp * (3.f - 2.f * fp));
                }
                if (frag.alpha <= 0.f) {
                    cardDecayFrags.erase(cardDecayFrags.begin() + di);
                    continue;
                }
                float fcx = frag.texRect.size.x / 2.f;
                float fcy = frag.texRect.size.y / 2.f;
                spCd->setOrigin({fcx, fcy});
                spCd->setTextureRect(frag.texRect);
                spCd->setPosition(frag.pos);
                spCd->setColor(sf::Color(255, 255, 255, (uint8_t)frag.alpha));
                window.draw(*spCd);
                uint8_t glow4 = (uint8_t)((1.f - fp) * 120.f);
                if (glow4 > 0) { spCd->setColor(sf::Color(glow4, glow4, glow4, (uint8_t)frag.alpha)); window.draw(*spCd, sf::BlendAdd); }
                ++di;
            }
            spCd->setOrigin({tsCd.x / 2.f, tsCd.y / 2.f});
            spCd->setTextureRect(sf::IntRect({0, 0}, {(int)tsCd.x, (int)tsCd.y}));
            spCd->setColor(sf::Color(255, 255, 255, 255));
        }
    }

    // 🌟 卡牌效果展示动画（湮灭完成后，画面中央展示）
    //    流程: PAUSE(0.5s) → APPEAR(1.2s) → DISPLAY(2s) → FADE_OUT(1s) → 触发效果
    // ============================================================================
    if (showcaseState != CardShowcaseState::NONE && newCardSprite != nullptr) {
        float t = showcaseClock.getElapsedTime().asSeconds();
        sf::Vector2f centerPos(1280.f, 720.f);
        // 🌟 紫卡传送展示使用独立紫卡精灵，不污染橙卡纹理
        bool showcaseIsPurple = aiCardIsPurpleTransfer;
        sf::Sprite*  showcaseSpr = (showcaseIsPurple && purpleCardSprite) ? purpleCardSprite : newCardSprite;
        sf::Texture& showcaseTex = (showcaseIsPurple && purpleCardSprite) ? purpleCardTexture : newCardTexture;
        sf::Vector2u texSize = showcaseTex.getSize();
        const float TARGET_SCALE = 0.76f;
        const float START_SCALE  = 0.10f;
        const float NORMAL_SCALE = 0.36f;              // 卡槽正常缩放
        const float SCALE_RATIO  = TARGET_SCALE / NORMAL_SCALE;  // ≈ 2.111
        const int   NAME_FONT    = static_cast<int>(28 * SCALE_RATIO);  // ≈ 59
        const int   DESC_FONT    = static_cast<int>(22 * SCALE_RATIO);  // ≈ 46
        const float NAME_OFF_X   = 52.f * SCALE_RATIO;  // ≈ 110
        const float NAME_OFF_Y   = 164.f * SCALE_RATIO;  // ≈ 346
        const float DESC_OFF_X   = 88.f * SCALE_RATIO;  // ≈ 186
        const float DESC_OFF_Y   = 90.f * SCALE_RATIO;  // ≈ 190

        switch (showcaseState) {

            case CardShowcaseState::PAUSE:
                // 湮灭后 0.3s 空白停顿
                showcaseFragsInit = false;
                if (t >= 0.3f) {
                    showcaseState = CardShowcaseState::APPEAR;
                    showcaseClock.restart();
                }
                break;

            case CardShowcaseState::APPEAR: {
                // 🌟 碎片聚合展示（自上往下扫描，碎片飞入）
                if (!showcaseFragsInit) {
                    showcaseFrags = (showcasedCard.cardColor == 1) ? cardFragCachePurple : cardFragCache;
                    showcaseFragsTex = showcasedCard.cardColor;
                    for (auto& frag : showcaseFrags) {
                        frag.released = false; frag.alpha = 0.f; frag.fadeTimer = 0.f;
                        frag.rotation = (rand() % 60 - 30) * 1.f;
                        frag.pos.x = centerPos.x + (rand() % 300 - 150) * 1.f;
                        frag.pos.y = centerPos.y + (rand() % 100 + 80) * 1.f;
                    }
                    showcaseFragsInit = true;
                }
                float rawT = t / 1.2f;
                if (rawT > 1.f) rawT = 1.f;
                float wipeY = texSize.y * rawT;
                int visH = (int)wipeY;
                if (visH > 0) {
                    showcaseSpr->setScale({TARGET_SCALE, TARGET_SCALE});
                    showcaseSpr->setOrigin({texSize.x / 2.f, texSize.y / 2.f});
                    showcaseSpr->setTextureRect(sf::IntRect({0, 0}, {(int)texSize.x, visH}));
                    showcaseSpr->setPosition(centerPos);
                    showcaseSpr->setColor(sf::Color(255, 255, 255, 255));
                    window.draw(*showcaseSpr);
                }
                for (auto& frag : showcaseFrags) {
                    float fcx = frag.texRect.size.x / 2.f;
                    float fcy = frag.texRect.size.y / 2.f;
                    float tx = centerPos.x + (frag.texRect.position.x + fcx - texSize.x / 2.f) * TARGET_SCALE;
                    float ty = centerPos.y + (frag.texRect.position.y + fcy - texSize.y / 2.f) * TARGET_SCALE;
                    float fragTop = (float)frag.texRect.position.y;
                    if (!frag.released && fragTop < wipeY + texSize.y * 0.06f && fragTop >= wipeY - texSize.y * 0.02f) {
                        frag.released = true;
                        frag.targetPos = {tx, ty};
                        frag.fadeTimer = 0.f;
                        frag.pos.x = tx + (rand() % 60 - 30) * 1.f;
                        frag.pos.y = ty + (rand() % 50 + 30) * 1.f;
                    }
                    if (frag.released) {
                        frag.fadeTimer += 0.016f;
                        float prog = frag.fadeTimer / 0.3f;
                        if (prog > 1.f) prog = 1.f;
                        float eased = prog * prog * (3.f - 2.f * prog);
                        frag.pos.x += (frag.targetPos.x - frag.pos.x) * 0.12f;
                        frag.pos.y += (frag.targetPos.y - frag.pos.y) * 0.12f;
                        frag.alpha = 255.f * eased;
                        showcaseSpr->setScale({TARGET_SCALE, TARGET_SCALE});
                        showcaseSpr->setOrigin({fcx, fcy});
                        showcaseSpr->setTextureRect(frag.texRect);
                        showcaseSpr->setPosition(frag.pos);
                        showcaseSpr->setRotation(sf::degrees(frag.rotation * (1.f - eased)));
                        showcaseSpr->setColor(sf::Color(255, 255, 255, (uint8_t)frag.alpha));
                        window.draw(*showcaseSpr);
                        // 亮度效果
                        float glowB = 0.f;
                        if (eased < 1.f) { glowB = eased * 60.f; }
                        else { float ta = frag.fadeTimer - 0.3f; float f2 = ta / 0.2f; if (f2 > 1.f) f2 = 1.f; glowB = 60.f * (1.f - f2); }
                        if (glowB > 0.f) { showcaseSpr->setColor(sf::Color((uint8_t)glowB, (uint8_t)glowB, (uint8_t)glowB, (uint8_t)frag.alpha)); window.draw(*showcaseSpr, sf::BlendAdd); }
                    }
                }
                showcaseSpr->setOrigin({texSize.x / 2.f, texSize.y / 2.f});
                showcaseSpr->setTextureRect(sf::IntRect({0, 0}, {(int)texSize.x, (int)texSize.y}));
                showcaseSpr->setRotation(sf::degrees(0.f));
                showcaseSpr->setColor(sf::Color(255, 255, 255, 255));

                // 文字跟随裁剪边缘出现（0.1s 淡入+亮→原色）
                float sHalfH = texSize.y * TARGET_SCALE * 0.5f;
                auto sLineTh = [&](float yOff) { return (sHalfH - yOff) / (sHalfH * 2.f); };
                auto sDrawLine = [&](const sf::String& str, float x, float y, int size,
                                     sf::Color normColor, float threshold) {
                    float lineAge = t - threshold;
                    if (lineAge < 0.f) return;
                    float fade = lineAge / 0.1f; if (fade > 1.f) fade = 1.f;
                    uint8_t a = static_cast<uint8_t>(255.f * fade);
                    uint8_t r = static_cast<uint8_t>(normColor.r + (255 - normColor.r) * (1.f - fade));
                    uint8_t g = static_cast<uint8_t>(normColor.g + (255 - normColor.g) * (1.f - fade));
                    uint8_t b = static_cast<uint8_t>(normColor.b + (255 - normColor.b) * (1.f - fade));
                    uiText.setFont(cardFont);
                    uiText.setCharacterSize(size);
                    uiText.setFillColor(sf::Color(r, g, b, a));
                    uiText.setString(str);
                    uiText.setPosition({x, y});
                    window.draw(uiText);
                };
                sDrawLine(sf::String(showcasedCard.name), centerPos.x - NAME_OFF_X,
                          centerPos.y - NAME_OFF_Y, NAME_FONT, sf::Color(255, 255, 255), sLineTh(NAME_OFF_Y));
                // 描述逐行（行距统一）
                float sLineH = DESC_FONT * 1.15f;
                std::wstring desc = showcasedCard.description;
                size_t pos = 0; int li = 0;
                while (pos <= desc.size()) {
                    size_t nl = desc.find(L'\n', pos);
                    if (nl == std::wstring::npos) nl = desc.size();
                    float ly = DESC_OFF_Y - li * sLineH;
                    sDrawLine(sf::String(desc.substr(pos, nl - pos)), centerPos.x - DESC_OFF_X,
                              centerPos.y - ly, DESC_FONT, sf::Color(230, 230, 230), sLineTh(ly));
                    pos = nl + 1; li++;
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
                showcaseSpr->setScale({TARGET_SCALE, TARGET_SCALE});
                showcaseSpr->setPosition(centerPos);
                showcaseSpr->setTextureRect(sf::IntRect({0, 0}, {static_cast<int>(texSize.x), static_cast<int>(texSize.y)}));
                showcaseSpr->setColor(sf::Color(255, 255, 255, 255));
                window.draw(*showcaseSpr, sf::BlendAlpha);

                uiText.setFont(cardFont);
                uiText.setCharacterSize(NAME_FONT);
                uiText.setFillColor(sf::Color(255, 255, 255, 255));
                uiText.setString(showcasedCard.name);
                uiText.setPosition({centerPos.x - NAME_OFF_X, centerPos.y - NAME_OFF_Y});
                window.draw(uiText);

                // 描述逐行
                uiText.setCharacterSize(DESC_FONT);
                uiText.setFillColor(sf::Color(230, 230, 230, 255));
                float dLineH = DESC_FONT * 1.15f;
                std::wstring desc = showcasedCard.description;
                size_t pos = 0; int li = 0;
                while (pos <= desc.size()) {
                    size_t nl = desc.find(L'\n', pos);
                    if (nl == std::wstring::npos) nl = desc.size();
                    uiText.setString(sf::String(desc.substr(pos, nl - pos)));
                    uiText.setPosition({centerPos.x - DESC_OFF_X, centerPos.y - DESC_OFF_Y + li * dLineH});
                    window.draw(uiText);
                    pos = nl + 1; li++;
                }
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

                showcaseSpr->setScale({TARGET_SCALE, TARGET_SCALE});
                showcaseSpr->setPosition(centerPos);
                showcaseSpr->setTextureRect(sf::IntRect({0, 0}, {static_cast<int>(texSize.x), static_cast<int>(texSize.y)}));

                // BlendAlpha 基底（含透明度衰减）
                showcaseSpr->setColor(sf::Color(255, 255, 255, currentAlpha));
                window.draw(*showcaseSpr, sf::BlendAlpha);

                // BlendAdd 白光叠层（含透明度衰减，同步熄灭）
                uint8_t whiteEmit = static_cast<uint8_t>(255 * whiteProgress * alphaProgress);
                showcaseSpr->setColor(sf::Color(whiteEmit, whiteEmit, whiteEmit, 255));
                window.draw(*showcaseSpr, sf::BlendAdd);

                // 文字同步淡出（逐行）
                float fLineH = DESC_FONT * 1.15f;
                uiText.setFont(cardFont);
                uiText.setCharacterSize(NAME_FONT);
                uiText.setFillColor(sf::Color(255, 255, 255, currentAlpha));
                uiText.setString(showcasedCard.name);
                uiText.setPosition({centerPos.x - NAME_OFF_X, centerPos.y - NAME_OFF_Y});
                window.draw(uiText);

                uiText.setCharacterSize(DESC_FONT);
                uiText.setFillColor(sf::Color(230, 230, 230, currentAlpha));
                std::wstring desc = showcasedCard.description;
                size_t pos = 0; int li = 0;
                while (pos <= desc.size()) {
                    size_t nl = desc.find(L'\n', pos);
                    if (nl == std::wstring::npos) nl = desc.size();
                    uiText.setString(sf::String(desc.substr(pos, nl - pos)));
                    uiText.setPosition({centerPos.x - DESC_OFF_X, centerPos.y - DESC_OFF_Y + li * fLineH});
                    window.draw(uiText);
                    pos = nl + 1; li++;
                }
                uiText.setFont(font);

                if (t >= 0.4f) {
                    // 🌟 紫卡传送：不走 applyCardEffect，转为加入玩家手牌
                    if (aiCardIsPurpleTransfer) {
                        Card purpleCard = showcasedCard;
                        purpleCard.transferred = true;
                        playerDeck.hand.push_back(purpleCard);
                        handSlotAssign.push_back(1);
                        newCardJustDrawn = true;
                        cardAnimClock.restart();
                        isAnimatingCard = true;
                        std::cout << "[Showcase] 紫卡传送——消退完成，卡牌转移至玩家手牌" << std::endl;
                    } else {
                        std::cout << "[Showcase] 消退完成，正式触发卡牌效果 —— "
                                  << showcasedCard.name.c_str() << std::endl;
                        bool immediate = applyCardEffect(showcasedCard);
                        if (immediate) {
                            // 即时效果：恢复计时、消耗AP、结算、解锁
                            if (isTurnPaused) {
                                turnTimePaused += pauseClock.getElapsedTime().asSeconds();
                                isTurnPaused = false;
                            }
                            consumeActionPoint(false);
                            settleActionPoints();
                            isBusyAnimating = false;
                        }
                    }
                    // 延迟效果（破釜沉舟/笼络）：保持 isTurnPaused 和 isBusyAnimating，
                    // 由各自的 deferred completion handler 负责解锁和结算
                    showcaseState = CardShowcaseState::NONE;
                }
                break;
            }
        }
    }
    // ============================================================================

    if ((currentState == GameState::GAME_PVP || currentState == GameState::GAME_PVE) && !isPaused) {
        window.draw(detectionZone);
        window.draw(portalDetectionZone);
    }

    // ============================================================================
    // 🌟【三明治夹层渲染 - 第 3 层：Top 滑盖】
    // 最后把读卡器的上盖面板盖在最上面，遮挡住通过它下方的卡牌
    // ============================================================================
    if (currentState == GameState::GAME_PVP || currentState == GameState::GAME_PVE) {
        if (cardReaderTopSprite != nullptr) {
            window.draw(*cardReaderTopSprite);
        }
        if (cardPortalTopSprite != nullptr) {
            window.draw(*cardPortalTopSprite);
        }
    }

    // 🌟【预落子圆环浮现逻辑】
    if (currentState == GameState::GAME_PVP || currentState == GameState::GAME_PVE) {
        if (!(currentState == GameState::GAME_PVE && isAiThinking)) {
            // 只有在鼠标没有抓取卡牌 且 不在动画期间 时，才绘制预落子提示环
            if (!isCardAttachedToMouse && !isBusyAnimating && !isPaused) {
                chessboard.drawHoverRing(window, currentTurn);
            }
        }
    }

    // ── 调试面板鼠标追踪（始终运行，避免 static 残留）──
    static bool dbgClicked = false;
    bool mouseDown = sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);
    if (!mouseDown) dbgClicked = false;

    // ── F10 AI 手牌调试面板 ──
    if (showAIDebug) {
        float dbgX = 20.f, dbgY = 80.f, dbgW = 280.f;
        int lines = static_cast<int>(playerDeck.aiHand.size());
        const float lineH = 24.f;
        const float padX = 10.f, padY = 6.f;
        const float titleH = 22.f;
        sf::Vector2f dbgMpos = window.mapPixelToCoords(sf::Mouse::getPosition(window));

        // === 区域 1：AI 手牌列表 ===
        float sec1H = titleH + lines * lineH + padY * 2;
        sf::RectangleShape bg1({dbgW, sec1H});
        bg1.setPosition({dbgX, dbgY});
        bg1.setFillColor(sf::Color(20, 20, 40, 200));
        bg1.setOutlineColor(sf::Color(100, 100, 180, 150));
        bg1.setOutlineThickness(1.f);
        window.draw(bg1);
        // 标题行
        uiText.setFont(font);
        uiText.setCharacterSize(18);
        uiText.setFillColor(sf::Color(255, 220, 100));
        uiText.setString(sf::String(L"[AI手牌] " + std::to_wstring(lines) + L"张  点击=立即打出"));
        uiText.setPosition({dbgX + padX, dbgY + padY});
        window.draw(uiText);
        // 分隔线
        sf::RectangleShape sep1({dbgW - padX * 2, 1.f});
        sep1.setPosition({dbgX + padX, dbgY + titleH + padY});
        sep1.setFillColor(sf::Color(120, 120, 180, 120));
        window.draw(sep1);
        // 卡牌条目
        std::vector<sf::FloatRect> aiCardRects;
        for (int i = 0; i < lines; ++i) {
            float ty = dbgY + titleH + padY + 4.f + i * lineH;
            sf::RectangleShape rowBg({dbgW - padX * 2, lineH - 2.f});
            rowBg.setPosition({dbgX + padX, ty - 1.f});
            rowBg.setFillColor(sf::Color(40, 40, 70, 120));
            window.draw(rowBg);
            uiText.setCharacterSize(18);
            uiText.setFillColor(sf::Color(200, 210, 255));
            // 调试面板始终显示真实卡名（盲目等紫卡加 * 标记）
            std::wstring aiName = playerDeck.aiHand[i].name;
            if (playerDeck.aiHand[i].cardColor == 1 && playerDeck.aiHand[i].transferred)
                aiName = L"*" + aiName;
            uiText.setString(sf::String(aiName));
            uiText.setPosition({dbgX + padX + 4.f, ty});
            window.draw(uiText);
            aiCardRects.push_back(sf::FloatRect({dbgX, ty}, {dbgW, lineH}));
        }

        // === 区域 2：全部卡牌模板（点击生成新卡入槽）===
        static const Card cardTemplates[] = {
            {1, L"连击", L"给予两次落子数", CardEffect::FORCE_DROP, 0, 0},
            {3, L"隐忍", L"迫使敌方承受：\n六子连星为胜途", CardEffect::CHANGE_WIN_RULE, 6, 0},
            {4, L"笼络", L"销毁己方一个棋子\n转化敌方一个棋子", CardEffect::CONVERT_PIECE, 0, 0},
            {5, L"破釜沉舟", L"将手牌放回牌库\n根据放回的数量\n销毁敌方棋子", CardEffect::SACRIFICE_HAND, 0, 0},
            {6, L"疫病", L"指定一颗敌方棋子\n使其患上疫病\n患病棋子每回合都\n有概率死亡并试图\n传染给其他棋子\n玩家棋子也有患病\n风险", CardEffect::PLAGUE, 0, 0},
            {7, L"隔离", L"使患病棋子在三回\n合后痊愈", CardEffect::QUARANTINE, 4, 0},
            {8, L"盲目", L"持有者将无法辨清\n自己的手牌\n三回合后消退\n且三子连星将导致\n消退时间延后", CardEffect::BLIND, 4, 1},
            {9, L"以地事秦", L"持有者需要传送一\n张橙卡给出牌者\n不送牌则接下来三\n回合只能下棋\n送卡或撑过三回合\n后本牌消退", CardEffect::YIDISHIQIN, 3, 1},
        };
        const int tmplCount = sizeof(cardTemplates) / sizeof(cardTemplates[0]);
        float sec2Y = dbgY + sec1H + 6.f;
        float sec2H = titleH + tmplCount * lineH + padY * 2;
        sf::RectangleShape bg2({dbgW, sec2H});
        bg2.setPosition({dbgX, sec2Y});
        bg2.setFillColor(sf::Color(40, 20, 20, 200));
        bg2.setOutlineColor(sf::Color(180, 100, 100, 150));
        bg2.setOutlineThickness(1.f);
        window.draw(bg2);
        uiText.setCharacterSize(18);
        uiText.setFillColor(sf::Color(255, 180, 140));
        uiText.setString(sf::String(L"[卡牌模板] 点击=生成新卡入槽"));
        uiText.setPosition({dbgX + padX, sec2Y + padY});
        window.draw(uiText);
        sf::RectangleShape sep2({dbgW - padX * 2, 1.f});
        sep2.setPosition({dbgX + padX, sec2Y + titleH + padY});
        sep2.setFillColor(sf::Color(180, 120, 120, 120));
        window.draw(sep2);
        std::vector<sf::FloatRect> tmplRects;
        for (int i = 0; i < tmplCount; ++i) {
            float ty = sec2Y + titleH + padY + 4.f + i * lineH;
            uiText.setCharacterSize(18);
            // 橙卡/紫卡用不同颜色
            uiText.setFillColor(cardTemplates[i].cardColor == 1
                ? sf::Color(220, 160, 255) : sf::Color(255, 200, 160));
            uiText.setString(sf::String(cardTemplates[i].name));
            uiText.setPosition({dbgX + padX + 4.f, ty});
            window.draw(uiText);
            tmplRects.push_back(sf::FloatRect({dbgX, ty}, {dbgW, lineH}));
        }
        // === 区域 3：生成新卡加入 AI 手牌 ===
        float sec3Y = sec2Y + sec2H + 6.f;
        float sec3H = titleH + tmplCount * lineH + padY * 2;
        sf::RectangleShape bg3({dbgW, sec3H});
        bg3.setPosition({dbgX, sec3Y});
        bg3.setFillColor(sf::Color(20, 20, 40, 200));
        bg3.setOutlineColor(sf::Color(100, 100, 180, 150));
        bg3.setOutlineThickness(1.f);
        window.draw(bg3);
        uiText.setCharacterSize(18);
        uiText.setFillColor(sf::Color(180, 200, 255));
        uiText.setString(sf::String(L"[生成卡→AI] 点击=加入AI手牌"));
        uiText.setPosition({dbgX + padX, sec3Y + padY});
        window.draw(uiText);
        sf::RectangleShape sep3({dbgW - padX * 2, 1.f});
        sep3.setPosition({dbgX + padX, sec3Y + titleH + padY});
        sep3.setFillColor(sf::Color(120, 120, 180, 120));
        window.draw(sep3);
        std::vector<sf::FloatRect> aiGenRects;
        for (int i = 0; i < tmplCount; ++i) {
            float ty = sec3Y + titleH + padY + 4.f + i * lineH;
            uiText.setCharacterSize(18);
            uiText.setFillColor(cardTemplates[i].cardColor == 1
                ? sf::Color(220, 160, 255) : sf::Color(255, 200, 160));
            uiText.setString(sf::String(cardTemplates[i].name));
            uiText.setPosition({dbgX + padX + 4.f, ty});
            window.draw(uiText);
            aiGenRects.push_back(sf::FloatRect({dbgX, ty}, {dbgW, lineH}));
        }

        // === 区域 4：牌库信息 ===
        float sec4Y = sec3Y + sec3H + 6.f;
        float sec4H = titleH + padY;
        sf::RectangleShape bg4({dbgW, sec4H});
        bg4.setPosition({dbgX, sec4Y});
        bg4.setFillColor(sf::Color(20, 40, 20, 200));
        bg4.setOutlineColor(sf::Color(100, 180, 100, 150));
        bg4.setOutlineThickness(1.f);
        window.draw(bg4);
        uiText.setCharacterSize(16);
        uiText.setFillColor(sf::Color(180, 255, 180));
        uiText.setString(sf::String(L"牌库:" + std::to_wstring(playerDeck.deck.size()) +
                                    L"  弃牌:" + std::to_wstring(playerDeck.discardPile.size())));
        uiText.setPosition({dbgX + padX, sec4Y + padY});
        window.draw(uiText);

        // === 区域 5：无敌按钮 ===
        float sec5Y = sec4Y + sec4H + 6.f;
        float sec5H = lineH + padY * 2;
        sf::RectangleShape bg5({dbgW, sec5H});
        bg5.setPosition({dbgX, sec5Y});
        bg5.setFillColor(playerInvincible ? sf::Color(80, 20, 20, 220) : sf::Color(20, 20, 20, 200));
        bg5.setOutlineColor(playerInvincible ? sf::Color(255, 80, 80, 200) : sf::Color(120, 120, 120, 150));
        bg5.setOutlineThickness(1.f);
        window.draw(bg5);
        uiText.setCharacterSize(18);
        uiText.setFillColor(playerInvincible ? sf::Color(255, 100, 100) : sf::Color(200, 200, 200));
        uiText.setString(sf::String(playerInvincible ? L"[无敌: ON] 点击关闭" : L"[无敌: OFF] 点击开启"));
        uiText.setPosition({dbgX + padX, sec5Y + padY});
        window.draw(uiText);
        sf::FloatRect invRect({dbgX, sec5Y}, {dbgW, sec5H});

        // === 区域 6：无敌 Plus 按钮 ===
        float sec6Y = sec5Y + sec5H + 6.f;
        float sec6H = lineH + padY * 2;
        sf::RectangleShape bg6({dbgW, sec6H});
        bg6.setPosition({dbgX, sec6Y});
        bg6.setFillColor(playerInvinciblePlus ? sf::Color(80, 0, 80, 220) : sf::Color(20, 20, 20, 200));
        bg6.setOutlineColor(playerInvinciblePlus ? sf::Color(255, 80, 255, 200) : sf::Color(120, 120, 120, 150));
        bg6.setOutlineThickness(1.f);
        window.draw(bg6);
        uiText.setCharacterSize(18);
        uiText.setFillColor(playerInvinciblePlus ? sf::Color(255, 150, 255) : sf::Color(200, 200, 200));
        uiText.setString(sf::String(playerInvinciblePlus ? L"[无敌Plus: ON] 点击关闭" : L"[无敌Plus: OFF] 点击开启"));
        uiText.setPosition({dbgX + padX, sec6Y + padY});
        window.draw(uiText);
        sf::FloatRect invPlusRect({dbgX, sec6Y}, {dbgW, sec6H});

        // === 区域 7：AI 只会下棋 ===
        float sec7Y = sec6Y + sec6H + 6.f;
        float sec7H = lineH + padY * 2;
        sf::RectangleShape bg7({dbgW, sec7H});
        bg7.setPosition({dbgX, sec7Y});
        bg7.setFillColor(aiOnlyDrop ? sf::Color(20, 60, 20, 220) : sf::Color(20, 20, 20, 200));
        bg7.setOutlineColor(aiOnlyDrop ? sf::Color(100, 255, 100, 200) : sf::Color(120, 120, 120, 150));
        bg7.setOutlineThickness(1.f);
        window.draw(bg7);
        uiText.setCharacterSize(18);
        uiText.setFillColor(aiOnlyDrop ? sf::Color(100, 255, 100) : sf::Color(200, 200, 200));
        uiText.setString(sf::String(aiOnlyDrop ? L"[只会下棋: ON] 点击关闭" : L"[只会下棋: OFF] 点击开启"));
        uiText.setPosition({dbgX + padX, sec7Y + padY});
        window.draw(uiText);
        sf::FloatRect onlyDropRect({dbgX, sec7Y}, {dbgW, sec7H});

        // === 区域 8：AI 只会出牌 ===
        float sec8Y = sec7Y + sec7H + 6.f;
        float sec8H = lineH + padY * 2;
        sf::RectangleShape bg8({dbgW, sec8H});
        bg8.setPosition({dbgX, sec8Y});
        bg8.setFillColor(aiOnlyCard ? sf::Color(60, 20, 20, 220) : sf::Color(20, 20, 20, 200));
        bg8.setOutlineColor(aiOnlyCard ? sf::Color(255, 100, 100, 200) : sf::Color(120, 120, 120, 150));
        bg8.setOutlineThickness(1.f);
        window.draw(bg8);
        uiText.setCharacterSize(18);
        uiText.setFillColor(aiOnlyCard ? sf::Color(255, 100, 100) : sf::Color(200, 200, 200));
        uiText.setString(sf::String(aiOnlyCard ? L"[只会出牌: ON] 点击关闭" : L"[只会出牌: OFF] 点击开启"));
        uiText.setPosition({dbgX + padX, sec8Y + padY});
        window.draw(uiText);
        sf::FloatRect onlyCardRect({dbgX, sec8Y}, {dbgW, sec8H});

        // === 统一点击处理（防连点，放最后确保所有区域已声明）===
        if (mouseDown && !dbgClicked) {
            dbgClicked = true;
            bool done = false;
            if (onlyCardRect.contains(dbgMpos) && !done) {
                aiOnlyCard = !aiOnlyCard;
                if (aiOnlyCard) aiOnlyDrop = false;
                std::cout << "[Debug] AI只会出牌: " << (aiOnlyCard ? "ON" : "OFF") << std::endl;
                done = true;
            }
            if (onlyDropRect.contains(dbgMpos) && !done) {
                aiOnlyDrop = !aiOnlyDrop;
                if (aiOnlyDrop) aiOnlyCard = false;
                std::cout << "[Debug] AI只会下棋: " << (aiOnlyDrop ? "ON" : "OFF") << std::endl;
                done = true;
            }
            if (invPlusRect.contains(dbgMpos)) {
                playerInvinciblePlus = !playerInvinciblePlus;
                int aiC = (playerColorPref == 1) ? 2 : 1;
                chessboard.setWinCondition(aiC, playerInvinciblePlus ? 16 : (playerInvincible ? 16 : 5));
                chessboard.setWinCondition(playerColorPref, playerInvinciblePlus ? 16 : 5);
                std::cout << "[Debug] 无敌Plus: " << (playerInvinciblePlus ? "ON" : "OFF") << std::endl;
                done = true;
            }
            if (invRect.contains(dbgMpos) && !done) {
                playerInvincible = !playerInvincible;
                playerInvinciblePlus = false;
                int aiC = (playerColorPref == 1) ? 2 : 1;
                chessboard.setWinCondition(aiC, playerInvincible ? 16 : 5);
                chessboard.setWinCondition(playerColorPref, 5);
                std::cout << "[Debug] 无敌: " << (playerInvincible ? "ON(AI需15连)" : "OFF") << std::endl;
                done = true;
            }
            for (size_t i = 0; i < aiCardRects.size() && !done; ++i) {
                if (aiCardRects[i].contains(dbgMpos) && i < playerDeck.aiHand.size()) {
                    std::cout << "[Debug] AI 立即打出: " << playerDeck.aiHand[i].name.c_str() << std::endl;
                    // 走完整 AI 出牌动画（Portal→Reader→展示→效果）
                    startAICardPlay(static_cast<int>(i));
                    done = true;
                }
            }
            for (size_t i = 0; i < tmplRects.size() && !done; ++i) {
                if (tmplRects[i].contains(dbgMpos)) {
                    playerDeck.hand.push_back(cardTemplates[i]);
                    handSlotAssign.push_back(1);
                    newCardJustDrawn = true; cardAnimClock.restart(); isAnimatingCard = true;
                    std::cout << "[Debug] 生成 " << cardTemplates[i].name.c_str() << " → 玩家卡槽" << std::endl;
                    done = true;
                }
            }
            for (size_t i = 0; i < aiGenRects.size() && !done; ++i) {
                if (aiGenRects[i].contains(dbgMpos)) {
                    playerDeck.aiHand.push_back(cardTemplates[i]);
                    std::cout << "[Debug] 生成 " << cardTemplates[i].name.c_str() << " → AI 手牌" << std::endl;
                    done = true;
                }
            }
        }
    }

    // ── 帧棋子动画（碎片聚合 → 停顿 → 飞行缩放）—— 最上层渲染 ──
    if (fpAnimState != FramePieceAnimState::IDLE && fpAnimTex && fpAnimTex->getSize().x > 0) {
        float t = fpAnimClock.getElapsedTime().asSeconds();
        sf::Vector2f centerPos(1280.f, 720.f);
        sf::Vector2u ts = fpAnimTex->getSize();
        const float BIG_SCALE = 384.f / ts.x;
        const float SMALL_SCALE = 96.f / ts.x;

        if (fpAnimState == FramePieceAnimState::AGGREGATE) {
            float rawT = t / 1.0f; if (rawT > 1.f) rawT = 1.f;
            float wipeY = ts.y * rawT;
            int visH = (int)wipeY;
            if (visH > 0) {
                sf::Sprite s(*fpAnimTex);
                s.setOrigin({ts.x / 2.f, ts.y / 2.f});
                s.setTextureRect(sf::IntRect({0, 0}, {(int)ts.x, visH}));
                s.setScale({BIG_SCALE, BIG_SCALE});
                s.setPosition(centerPos);
                s.setColor(sf::Color::White);
                window.draw(s);
            }
            if (!fpAnimInit) { fpAnimInit = true; }
            for (auto& frag : fpAnimFrags) {
                float fcx = frag.texRect.size.x / 2.f;
                float fcy = frag.texRect.size.y / 2.f;
                float tx = centerPos.x + (frag.texRect.position.x + fcx - ts.x / 2.f) * BIG_SCALE;
                float ty = centerPos.y + (frag.texRect.position.y + fcy - ts.y / 2.f) * BIG_SCALE;
                float fragTop = (float)frag.texRect.position.y;
                if (!frag.released && fragTop < wipeY + ts.y * 0.06f && fragTop >= wipeY - ts.y * 0.02f) {
                    frag.released = true;
                    frag.targetPos = {tx, ty};
                    frag.fadeTimer = 0.f;
                    frag.pos.x = tx + (rand() % 60 - 30) * 1.f;
                    frag.pos.y = ty + (rand() % 50 + 30) * 1.f;
                }
                if (frag.released) {
                    frag.fadeTimer += 0.016f;
                    float prog = frag.fadeTimer / 0.3f; if (prog > 1.f) prog = 1.f;
                    float eased = prog * prog * (3.f - 2.f * prog);
                    frag.pos.x += (frag.targetPos.x - frag.pos.x) * 0.12f;
                    frag.pos.y += (frag.targetPos.y - frag.pos.y) * 0.12f;
                    frag.alpha = 255.f * eased;
                    sf::Sprite s(*fpAnimTex);
                    s.setScale({BIG_SCALE, BIG_SCALE});
                    s.setOrigin({fcx, fcy});
                    s.setTextureRect(frag.texRect);
                    s.setPosition(frag.pos);
                    s.setRotation(sf::degrees(frag.rotation * (1.f - eased)));
                    s.setColor(sf::Color(255, 255, 255, (uint8_t)frag.alpha));
                    window.draw(s);
                    float glow = (eased < 1.f) ? eased * 80.f : 0.f;
                    if (glow > 0.f) {
                        s.setColor(sf::Color((uint8_t)glow, (uint8_t)glow, (uint8_t)glow, (uint8_t)frag.alpha));
                        window.draw(s, sf::BlendAdd);
                    }
                }
            }
            if (rawT >= 1.f) {
                fpAnimState = FramePieceAnimState::HOLD;
                fpAnimClock.restart();
            }
        }
        else if (fpAnimState == FramePieceAnimState::HOLD) {
            sf::Sprite s(*fpAnimTex);
            s.setOrigin({ts.x / 2.f, ts.y / 2.f});
            s.setScale({BIG_SCALE, BIG_SCALE});
            s.setPosition(centerPos);
            s.setColor(sf::Color::White);
            window.draw(s);
            if (t >= 0.5f) {
                fpAnimState = FramePieceAnimState::FLY;
                fpAnimClock.restart();
            }
        }
        else if (fpAnimState == FramePieceAnimState::FLY) {
            float rawT = t / 0.5f; if (rawT > 1.f) rawT = 1.f;
            float eased = rawT * rawT * (3.f - 2.f * rawT);
            float curX = centerPos.x + (fpAnimTargetX - centerPos.x) * eased;
            float curY = centerPos.y + (fpAnimTargetY - centerPos.y) * eased;
            float curScale = BIG_SCALE + (SMALL_SCALE - BIG_SCALE) * eased;
            if (rawT >= 1.f) {
                sf::Sprite& sp = (fpAnimColor == 1) ? chessboard.getBlackSprite()
                                                    : chessboard.getWhiteSprite();
                const sf::Texture& ttx = sp.getTexture();
                float fs = 96.f / ttx.getSize().x;
                sp.setScale({fs, fs});
                sp.setPosition({fpAnimTargetX, fpAnimTargetY});
                sp.setColor(sf::Color::White);
                window.draw(sp);
                completeFramePieceAnim();
            } else {
                sf::Sprite s(*fpAnimTex);
                s.setOrigin({ts.x / 2.f, ts.y / 2.f});
                s.setScale({curScale, curScale});
                s.setPosition({curX, curY});
                s.setColor(sf::Color::White);
                window.draw(s);
            }
        }
    }

    // ── 暂停覆盖层（淡入/淡出各 0.5s）──
    float fadeElapsed = pauseFadeClock.getElapsedTime().asSeconds();
    float fadeT = fadeElapsed / 0.5f;
    if (fadeT > 1.f) fadeT = 1.f;
    if (!isPaused) fadeT = 1.f - fadeT;  // 退出时反向

    // 下落动画（进入：从上方1200px落下；退出：回到上方1200px）
    float dropT = fadeElapsed / 0.5f;
    if (dropT > 1.f) dropT = 1.f;
    float easedDrop = 1.f - (1.f - dropT) * (1.f - dropT) * (1.f - dropT);
    float dropOffset = isPaused ? (-1200.f * (1.f - easedDrop))   // 进入：-1200→0 落下
                                : (-1200.f * easedDrop);           // 退出：0→-1200 飞回
    float elemAlpha = isPaused ? easedDrop : (1.f - easedDrop);    // 元素透明度（缓出）

    if (isPaused || fadeT > 0.f) {
        uint8_t overlayAlpha = static_cast<uint8_t>(160.f * fadeT);
        sf::RectangleShape overlay({2560.f, 1440.f});
        overlay.setPosition({0.f, 0.f});
        overlay.setFillColor(sf::Color(0, 0, 0, overlayAlpha));
        window.draw(overlay);

        uint8_t ea = static_cast<uint8_t>(255.f * elemAlpha);

        // SettingsMenu 背景图（带下落+淡入）
        if (settingsMenuLoaded) {
            float smw = static_cast<float>(settingsMenuTex.getSize().x);
            float smScale = 800.f / smw * 1.5f;
            settingsMenuSpr.setScale({smScale, smScale});
            settingsMenuSpr.setPosition({1280.f, 680.f + dropOffset});
            settingsMenuSpr.setColor(sf::Color(255, 255, 255, ea));
            window.draw(settingsMenuSpr);
        }

        const float centerX = WINDOW_WIDTH * 0.5f;
        const float btnW = 320.f;
        const float btnH = 67.f;
        const float hitW = btnW * 1.1f;
        const float hitH = btnH * 1.8f;
        const float hitX = centerX - hitW * 0.5f;
        const float menuStartY = 623.f;
        const int   btnCount = 4;
        const float btnY[4] = { menuStartY, menuStartY + 137.f, menuStartY + 324.f, menuStartY + 511.f };
        const std::wstring labels[4] = { L"认输放弃", L"重开一局", L"返回主页", L"退出游戏" };
        const float btnOffX[4] = { -100.f, 0.f, 100.f, 240.f };
        const float btnOffY[4] = { -50.f, 0.f, 0.f, 0.f };

        // 悬停检测（跟随下落动画）
        sf::Vector2i mp = sf::Mouse::getPosition(window);
        sf::Vector2f mpos = window.mapPixelToCoords(mp);
        int hoverIdx = -1;
        for (int i = 0; i < btnCount; ++i) {
            float hx = hitX + btnOffX[i];
            float hy = btnY[i] - (hitH - btnH) * 0.5f + btnOffY[i] + dropOffset;
            if (mpos.x > hx && mpos.x < hx + hitW && mpos.y > hy && mpos.y < hy + hitH)
                { hoverIdx = i; break; }
        }

        // 悬停抖动（60Hz, ±2px）
        float jx = 0.f, jy = 0.f;
        if (hoverIdx >= 0) {
            jx = (static_cast<float>(rand()) / RAND_MAX * 2.f - 1.f);
            jy = (static_cast<float>(rand()) / RAND_MAX * 2.f - 1.f);
        }

        float ffw = static_cast<float>(uiFrameTex.getSize().x);
        float ffh = static_cast<float>(uiFrameTex.getSize().y);
        float frameScale = 120.6f / ffh;

        // UI_Frame 按钮
        for (int i = 0; i < btnCount; ++i) {
            float by = btnY[i] - (hitH - btnH) * 0.5f + btnOffY[i] + dropOffset;
            sf::Vector2f bpos(hitX + btnOffX[i], by);
            sf::Vector2f bsize(hitW, hitH);
            bool hover = (i == hoverIdx);
            float s = hover ? frameScale * 1.05f : frameScale;
            float ox = hover ? 10.f : 0.f;
            float oy = hover ? -10.f : 0.f;
            float hx = hover ? jx : 0.f;
            float hy = hover ? jy : 0.f;
            uiFrameSpr.setScale({s, s});
            uiFrameSpr.setPosition({bpos.x + bsize.x / 2.f - ox + hx, bpos.y + bsize.y / 2.f + oy + hy});
            uiFrameSpr.setColor(sf::Color(255, 255, 255, ea));
            window.draw(uiFrameSpr);
        }
        uiFrameSpr.setColor(sf::Color(255, 255, 255, 255)); // 恢复，避免影响主菜单

        // 文字
        uiText.setFont(cardFont);
        uiText.setCharacterSize(48);
        uiText.setFillColor(sf::Color(50, 15, 70, ea));
        for (int i = 0; i < btnCount; ++i) {
            bool hover = (i == hoverIdx);
            uiText.setString(sf::String(labels[i]));
            sf::FloatRect tb = uiText.getLocalBounds();
            float tx = centerX - (tb.position.x + tb.size.x) * 0.5f + btnOffX[i];
            float ty = btnY[i] + (btnH - tb.size.y) * 0.5f - tb.position.y + btnOffY[i] + dropOffset;
            if (hover) {
                uiText.setCharacterSize(50);
                uiText.setPosition({tx - 6.f + jx, ty - 10.f + jy});
                window.draw(uiText);
                uiText.setPosition({tx, ty});
                uiText.setCharacterSize(48);
            } else {
                uiText.setPosition({tx, ty});
                window.draw(uiText);
            }
        }
    }
}

// ============================================================================
// 🌟 【胜利制重构】帧棋子奖励 — 每次5连放一个棋子，集满5个获胜
// ============================================================================
// ============================================================================
// 🌟 【帧棋子动画】入队待处理，若空闲则启动动画
// ============================================================================
void GameEngine::awardFramePiece(int scoringPlayer) {
    pendingFramePieces.push_back({scoringPlayer});
    std::cout << "[Frame] 待处理帧棋子: " << pendingFramePieces.size()
              << " (scoringPlayer=" << scoringPlayer << ")" << std::endl;
    if (fpAnimState == FramePieceAnimState::IDLE) {
        startNextFramePieceAnim();
    }
}

// ============================================================================
// 🌟 【帧棋子动画】从队列取出，计算目标，初始化碎片
// ============================================================================
void GameEngine::startNextFramePieceAnim() {
    if (pendingFramePieces.empty()) return;
    auto pfp = pendingFramePieces.front();

    // 确定归属（上框/下框）+ 目标槽位索引
    int targetSlot; float targetY;
    if (currentState == GameState::GAME_PVE) {
        if (pfp.scoringPlayer == playerColorPref) {
            targetSlot = playerFramePieces; targetY = 80.f;
        } else {
            targetSlot = enemyFramePieces; targetY = 1360.f;
        }
    } else { // GAME_PVP
        if (pfp.scoringPlayer == 1) {
            targetSlot = playerFramePieces; targetY = 80.f;
        } else {
            targetSlot = enemyFramePieces; targetY = 1360.f;
        }
    }
    fpAnimTargetX = 1005.f + targetSlot * 138.f;
    fpAnimTargetY = targetY;

    // 棋子颜色
    fpAnimColor = pfp.scoringPlayer;

    // 选择 HR 纹理
    fpAnimTex = (fpAnimColor == 1) ? &menuBlackTex : &menuWhiteTex;

    // 初始化碎片：复制缓存，随机化起始位置
    fpAnimFrags = (fpAnimColor == 1) ? fpFragCacheBlack : fpFragCacheWhite;
    sf::Vector2f centerPos(1280.f, 720.f);
    for (auto& frag : fpAnimFrags) {
        frag.released = false; frag.alpha = 0.f; frag.fadeTimer = 0.f;
        frag.rotation = (rand() % 40 - 20) * 1.f;
        frag.pos.x = centerPos.x + (rand() % 300 - 150) * 1.f;
        frag.pos.y = centerPos.y + (rand() % 200 + 100) * 1.f;
    }
    fpAnimInit = false;

    fpAnimState = FramePieceAnimState::AGGREGATE;
    fpAnimClock.restart();
    isBusyAnimating = true;
    std::cout << "[FrameAnim] 启动动画 color=" << fpAnimColor
              << " target=(" << fpAnimTargetX << "," << fpAnimTargetY << ")" << std::endl;
}

// ============================================================================
// 🌟 【帧棋子动画】动画完成，计数器 +1，检查胜负，处理队列
// ============================================================================
void GameEngine::completeFramePieceAnim() {
    if (pendingFramePieces.empty()) return;
    auto pfp = pendingFramePieces.front();
    pendingFramePieces.erase(pendingFramePieces.begin());

    // 计数器 +1
    if (currentState == GameState::GAME_PVE) {
        if (pfp.scoringPlayer == playerColorPref)
            playerFramePieces++;
        else
            enemyFramePieces++;
    } else { // GAME_PVP
        if (pfp.scoringPlayer == 1)
            playerFramePieces++;
        else
            enemyFramePieces++;
    }

    std::cout << "[Frame] 上框: " << playerFramePieces
              << ", 下框: " << enemyFramePieces << std::endl;

    // 胜负检查
    bool isPVE = (currentState == GameState::GAME_PVE);
    if (playerFramePieces >= 5) {
        winReason = isPVE ? L"我方获胜" : L"黑方获胜";
        isGameOver = true;
        currentState = GameState::GAME_OVER;
        chessboard.clearWinLine();
        std::cout << "[Frame] 上框集满5个，" << (isPVE ? "玩家" : "黑方") << "获胜！" << std::endl;
    } else if (enemyFramePieces >= 5) {
        winReason = isPVE ? L"敌方获胜" : L"白方获胜";
        isGameOver = true;
        currentState = GameState::GAME_OVER;
        chessboard.clearWinLine();
        std::cout << "[Frame] 下框集满5个，" << (isPVE ? "AI" : "白方") << "获胜！" << std::endl;
    }

    // 清理当前动画状态
    fpAnimState = FramePieceAnimState::IDLE;
    fpAnimFrags.clear();

    // 继续处理队列或恢复游戏
    if (!isGameOver && !pendingFramePieces.empty()) {
        startNextFramePieceAnim();
    } else if (!isGameOver) {
        isBusyAnimating = false;
        settleActionPoints();
        if (currentState == GameState::GAME_PVE && currentTurn != playerColorPref
            && currentTurnActionPoints.size() == 1) {
            isAiThinking = true;
            aiThinkClock.restart();
        }
    }
}

void GameEngine::renderGameOver() {
    // ── 背景：星空帧动画 ──
    {
        static GameState lastBgState = GameState::MENU;
        if (currentState != lastBgState) { bgCurrentFrame = -1; lastBgState = currentState; }
        float bgElapsed = bgFrameClock.getElapsedTime().asSeconds();
        int frame = static_cast<int>(bgElapsed * 25.f) % BG_TOTAL_FRAMES + 1;
        if (frame != bgCurrentFrame) {
            char path[128]; snprintf(path, sizeof(path), "assets/Universe/frame_%05d.jpg", frame);
            if (bgTexA.loadFromFile(getEngineAssetPath(path))) {
                bgSprA.setTexture(bgTexA, true);
                float sw = 2560.f / bgTexA.getSize().x;
                float sh = 1440.f / bgTexA.getSize().y;
                bgSprA.setScale({sw, sh});
                bgSprA.setPosition({0.f, 0.f});
                bgSprA.setColor(sf::Color(255, 255, 255, 255));
            }
            bgCurrentFrame = frame;
        }
        window.draw(bgSprA);
    }

    chessboard.draw(window);

    // 半透明遮罩
    sf::RectangleShape cover({2560.f, 1440.f});
    cover.setFillColor(sf::Color(0, 0, 0, 180));
    window.draw(cover);

    const float centerX = WINDOW_WIDTH * 0.5f;

    // 标题
    uiText.setFont(cardFont);
    uiText.setCharacterSize(78);
    uiText.setFillColor(sf::Color(200, 160, 230));
    uiText.setString(sf::String(L"对局结束"));
    sf::FloatRect titleBounds = uiText.getLocalBounds();
    uiText.setPosition(sf::Vector2f(centerX - (titleBounds.position.x + titleBounds.size.x) * 0.5f, 380.f));
    window.draw(uiText);

    // 胜负文字
    uiText.setFont(cardFont);
    uiText.setCharacterSize(48);
    uiText.setFillColor(sf::Color::White);
    uiText.setString(sf::String(winReason));
    sf::FloatRect resultBounds = uiText.getLocalBounds();
    uiText.setPosition(sf::Vector2f(centerX - (resultBounds.position.x + resultBounds.size.x) * 0.5f, 520.f));
    window.draw(uiText);

    // UI_Frame 按钮
    float ffw = static_cast<float>(uiFrameTex.getSize().x);
    float ffh = static_cast<float>(uiFrameTex.getSize().y);
    float frameScale = 120.6f / ffh;
    uiFrameSpr.setScale({frameScale, frameScale});
    uiFrameSpr.setPosition({centerX, 680.f});
    uiFrameSpr.setColor(sf::Color(255, 255, 255, 255));
    window.draw(uiFrameSpr);

    // 按钮文字（垂直居中于 Y=680）
    uiText.setFont(cardFont);
    uiText.setCharacterSize(48);
    uiText.setFillColor(sf::Color(50, 15, 70));
    uiText.setString(sf::String(L"回到主菜单"));
    sf::FloatRect resetBounds = uiText.getLocalBounds();
    uiText.setPosition(sf::Vector2f(centerX - (resetBounds.position.x + resetBounds.size.x) * 0.5f,
        680.f - resetBounds.size.y * 0.5f - resetBounds.position.y));
    window.draw(uiText);
}

void GameEngine::renderSettings() {
    // ── 背景：星空帧动画（与主菜单一致）──
    // ── Universe 背景（本界面独立加载，每帧缓存）──
    {
        static GameState lastBgState = GameState::MENU;
        if (currentState != lastBgState) { bgCurrentFrame = -1; lastBgState = currentState; }
        float bgElapsed = bgFrameClock.getElapsedTime().asSeconds();
        int frame = static_cast<int>(bgElapsed * 25.f) % BG_TOTAL_FRAMES + 1;
        if (frame != bgCurrentFrame) {
            char path[128]; snprintf(path, sizeof(path),
                "assets/Universe/frame_%05d.jpg", frame);
            if (bgTexA.loadFromFile(getEngineAssetPath(path))) {
                bgSprA.setTexture(bgTexA, true);
                float sw = 2560.f / bgTexA.getSize().x;
                float sh = 1440.f / bgTexA.getSize().y;
                bgSprA.setScale({sw, sh});
                bgSprA.setPosition({0.f, 0.f});
                bgSprA.setColor(sf::Color(255, 255, 255, 255));
            }
            bgCurrentFrame = frame;
        }
        window.draw(bgSprA);
    }

    // ── 按钮参数（与主菜单一致）──
    const float centerX = WINDOW_WIDTH * 0.5f;
    const float btnW = 320.f;
    const float btnH = 67.f;
    const float hitW = btnW * 1.1f;
    const float hitH = btnH * 1.8f;
    const float hitX = centerX - hitW * 0.5f;
    const float menuStartY = 543.f;

    // 三个按钮的 Y 坐标（间距 137）
    const float btnY[3] = { menuStartY, menuStartY + 137.f, menuStartY + 274.f };
    const std::wstring btnLabels[3] = {
        isProfessionalMode ? L"专业模式:开启" : L"专业模式:关闭",
        isFullscreen ? L"全屏模式:开启" : L"全屏模式:关闭",
        L"返回主菜单"
    };

    // ── 标题 ──
    uiText.setFont(cardFont);
    uiText.setCharacterSize(78);
    uiText.setFillColor(sf::Color(200, 160, 230));
    uiText.setString(sf::String(L"游戏设置"));
    sf::FloatRect titleBounds = uiText.getLocalBounds();
    uiText.setPosition(sf::Vector2f(centerX - (titleBounds.position.x + titleBounds.size.x) * 0.5f, 380.f));
    window.draw(uiText);

    // ── 悬停检测 ──
    sf::Vector2i mp = sf::Mouse::getPosition(window);
    sf::Vector2f mpos = window.mapPixelToCoords(mp);
    int hoverIdx = -1;
    for (int i = 0; i < 3; ++i) {
        float hy = btnY[i] - (hitH - btnH) * 0.5f;
        if (mpos.x > hitX && mpos.x < hitX + hitW && mpos.y > hy && mpos.y < hy + hitH)
            { hoverIdx = i; break; }
    }
    float jx = 0.f, jy = 0.f;
    if (hoverIdx >= 0) {
        jx = (static_cast<float>(rand()) / RAND_MAX * 2.f - 1.f);
        jy = (static_cast<float>(rand()) / RAND_MAX * 2.f - 1.f);
    }

    // ── UI_Frame 按钮背景 ──
    float ffw = static_cast<float>(uiFrameTex.getSize().x);
    float ffh = static_cast<float>(uiFrameTex.getSize().y);
    float frameScale = 120.6f / ffh;
    for (int i = 0; i < 3; ++i) {
        float by = btnY[i] - (hitH - btnH) * 0.5f;
        sf::Vector2f bpos(hitX, by);
        sf::Vector2f bsize(hitW, hitH);
        bool hover = (i == hoverIdx);
        float s = hover ? frameScale * 1.05f : frameScale;
        float ox = hover ? 10.f : 0.f;
        float oy = hover ? -10.f : 0.f;
        float hx = hover ? jx : 0.f;
        float hy2 = hover ? jy : 0.f;
        uiFrameSpr.setScale({s, s});
        uiFrameSpr.setPosition({bpos.x + bsize.x / 2.f - ox + hx, bpos.y + bsize.y / 2.f + oy + hy2});
        uiFrameSpr.setColor(sf::Color(255, 255, 255, 255));
        window.draw(uiFrameSpr);
    }

    // ── 按钮文字 ──
    uiText.setFont(cardFont);
    uiText.setCharacterSize(48);
    uiText.setFillColor(sf::Color(50, 15, 70));
    for (int i = 0; i < 3; ++i) {
        bool hover = (i == hoverIdx);
        uiText.setString(sf::String(btnLabels[i]));
        sf::FloatRect tb = uiText.getLocalBounds();
        float tx = centerX - (tb.position.x + tb.size.x) * 0.5f;
        float ty = btnY[i] + (btnH - tb.size.y) * 0.5f - tb.position.y;
        if (hover) {
            uiText.setCharacterSize(50);
            uiText.setPosition({tx - 6.f + jx, ty - 10.f + jy});
            window.draw(uiText);
            uiText.setPosition({tx, ty});
            uiText.setCharacterSize(48);
        } else {
            uiText.setPosition({tx, ty});
            window.draw(uiText);
        }
    }
}

void GameEngine::renderInfo() {
    if (currentState == GameState::HELP) {
        // ── 背景：星空帧动画 ──
        float bgElapsed = bgFrameClock.getElapsedTime().asSeconds();
        int frame = static_cast<int>(bgElapsed * 25.f) % BG_TOTAL_FRAMES + 1;
        char path[128]; snprintf(path, sizeof(path),
            "assets/Universe/frame_%05d.jpg", frame);
        if (bgTexA.loadFromFile(getEngineAssetPath(path))) {
            bgSprA.setTexture(bgTexA, true);
            float sw = 2560.f / bgTexA.getSize().x;
            float sh = 1440.f / bgTexA.getSize().y;
            bgSprA.setScale({sw, sh});
            bgSprA.setPosition({0.f, 0.f});
            bgSprA.setColor(sf::Color(255, 255, 255, 255));
            // ── Universe 背景（本界面独立加载，每帧缓存）──
    {
        static GameState lastBgState = GameState::MENU;
        if (currentState != lastBgState) { bgCurrentFrame = -1; lastBgState = currentState; }
        float bgElapsed = bgFrameClock.getElapsedTime().asSeconds();
        int frame = static_cast<int>(bgElapsed * 25.f) % BG_TOTAL_FRAMES + 1;
        if (frame != bgCurrentFrame) {
            char path[128]; snprintf(path, sizeof(path),
                "assets/Universe/frame_%05d.jpg", frame);
            if (bgTexA.loadFromFile(getEngineAssetPath(path))) {
                bgSprA.setTexture(bgTexA, true);
                float sw = 2560.f / bgTexA.getSize().x;
                float sh = 1440.f / bgTexA.getSize().y;
                bgSprA.setScale({sw, sh});
                bgSprA.setPosition({0.f, 0.f});
                bgSprA.setColor(sf::Color(255, 255, 255, 255));
            }
            bgCurrentFrame = frame;
        }
        window.draw(bgSprA);
    }
        }

        const float centerX = WINDOW_WIDTH * 0.5f;

        // ── 标题 ──
        uiText.setFont(cardFont);
        uiText.setCharacterSize(78);
        uiText.setFillColor(sf::Color(200, 160, 230));
        uiText.setString(sf::String(L"游戏规则"));
        sf::FloatRect titleBounds = uiText.getLocalBounds();
        uiText.setPosition(sf::Vector2f(centerX - (titleBounds.position.x + titleBounds.size.x) * 0.5f, 380.f));
        window.draw(uiText);

        // ── 正文（分行）──
        uiText.setFont(cardFont);
        uiText.setCharacterSize(48);
        uiText.setFillColor(sf::Color::White);

        const std::wstring lines[] = {
            L"五子棋规则：",
            L"在 15×15 的棋盘上，黑白双方轮流落子，",
            L"先达成五连者获胜。",
            L"",
            L"卡牌系统：",
            L"三连抽卡 → 拖至读卡器 → 触发效果",
            L"",
            L"点击屏幕任意位置返回",
        };
        const float lineH = 62.f;
        const float startY = 560.f;
        for (int i = 0; i < 8; ++i) {
            uiText.setString(sf::String(lines[i]));
            sf::FloatRect tb = uiText.getLocalBounds();
            uiText.setPosition(sf::Vector2f(centerX - (tb.position.x + tb.size.x) * 0.5f, startY + i * lineH));
            window.draw(uiText);
        }
        return;
    }

    // ── HISTORY：星空背景 + 统一字体风格 ──
    {
        float bgElapsed = bgFrameClock.getElapsedTime().asSeconds();
        int frame = static_cast<int>(bgElapsed * 25.f) % BG_TOTAL_FRAMES + 1;
        char path[128]; snprintf(path, sizeof(path),
            "assets/Universe/frame_%05d.jpg", frame);
        if (bgTexA.loadFromFile(getEngineAssetPath(path))) {
            bgSprA.setTexture(bgTexA, true);
            float sw = 2560.f / bgTexA.getSize().x;
            float sh = 1440.f / bgTexA.getSize().y;
            bgSprA.setScale({sw, sh});
            bgSprA.setPosition({0.f, 0.f});
            bgSprA.setColor(sf::Color(255, 255, 255, 255));
            // ── Universe 背景（本界面独立加载，每帧缓存）──
    {
        static GameState lastBgState = GameState::MENU;
        if (currentState != lastBgState) { bgCurrentFrame = -1; lastBgState = currentState; }
        float bgElapsed = bgFrameClock.getElapsedTime().asSeconds();
        int frame = static_cast<int>(bgElapsed * 25.f) % BG_TOTAL_FRAMES + 1;
        if (frame != bgCurrentFrame) {
            char path[128]; snprintf(path, sizeof(path),
                "assets/Universe/frame_%05d.jpg", frame);
            if (bgTexA.loadFromFile(getEngineAssetPath(path))) {
                bgSprA.setTexture(bgTexA, true);
                float sw = 2560.f / bgTexA.getSize().x;
                float sh = 1440.f / bgTexA.getSize().y;
                bgSprA.setScale({sw, sh});
                bgSprA.setPosition({0.f, 0.f});
                bgSprA.setColor(sf::Color(255, 255, 255, 255));
            }
            bgCurrentFrame = frame;
        }
        window.draw(bgSprA);
    }
        }

        const float centerX = WINDOW_WIDTH * 0.5f;

        // ── 标题 ──
        uiText.setFont(cardFont);
        uiText.setCharacterSize(78);
        uiText.setFillColor(sf::Color(200, 160, 230));
        uiText.setString(sf::String(L"历史记录"));
        sf::FloatRect titleBounds = uiText.getLocalBounds();
        uiText.setPosition(sf::Vector2f(centerX - (titleBounds.position.x + titleBounds.size.x) * 0.5f, 380.f));
        window.draw(uiText);

        // ── 文件列表 ──
        auto dir = getSavesDir();
        std::vector<std::filesystem::path> files;
        if (std::filesystem::exists(dir)) {
            for (auto &entry : std::filesystem::directory_iterator(dir)) {
                if (entry.is_regular_file()) files.push_back(entry.path());
            }
        }
        std::sort(files.begin(), files.end(), std::greater<>());

        historyItems.clear();

        const float listStartY = 560.f;
        const float lineH = 52.f;

        if (files.empty()) {
            uiText.setFont(cardFont);
            uiText.setCharacterSize(48);
            uiText.setFillColor(sf::Color::White);
            uiText.setString(sf::String(L"暂无历史记录"));
            sf::FloatRect tb = uiText.getLocalBounds();
            uiText.setPosition(sf::Vector2f(centerX - (tb.position.x + tb.size.x) * 0.5f, listStartY));
            window.draw(uiText);
        } else {
            int idx = 0;
            for (const auto& p : files) {
                if (idx >= 12) break;

                std::wstring nameW;
                std::string fn = p.filename().string();
                nameW.assign(fn.begin(), fn.end());

                uiText.setFont(cardFont);
                uiText.setCharacterSize(40);
                uiText.setFillColor(sf::Color(220, 200, 240));
                uiText.setString(sf::String(nameW));
                sf::FloatRect tb = uiText.getLocalBounds();
                float tx = centerX - (tb.position.x + tb.size.x) * 0.5f;
                float ty = listStartY + idx * lineH;
                uiText.setPosition({tx, ty});
                window.draw(uiText);

                // 保存点击检测区域
                sf::FloatRect hitRect({tx - 10.f, ty}, {tb.size.x + 20.f, lineH});
                historyItems.emplace_back(hitRect, p);
                idx++;
            }
        }

        // ── 底部提示 ──
        uiText.setFont(cardFont);
        uiText.setCharacterSize(36);
        uiText.setFillColor(sf::Color(180, 160, 200));
        uiText.setString(sf::String(L"点击条目加载复盘 · 点击空白返回"));
        sf::FloatRect tb = uiText.getLocalBounds();
        uiText.setPosition(sf::Vector2f(centerX - (tb.position.x + tb.size.x) * 0.5f, 1340.f));
        window.draw(uiText);
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
    // 以地事秦禁出牌
    if (!isPieceDrop && yiDiShiQinPenalty > 0) return false;
    for (const auto& ap : currentTurnActionPoints) {
        if (isPieceDrop && ap.canPieceDrop) return true;
        if (!isPieceDrop && ap.canPlayCard) return true;
    }
    return false;
}

// 3. 智能行动点扣除算法
bool GameEngine::consumeActionPoint(bool isPieceDrop) {
    if (isPieceDrop) {
        // 优先寻找"只能下棋，不能出牌"的【专能行动点】，从而将珍贵的"双能万能点"留到最后
        for (auto it = currentTurnActionPoints.begin(); it != currentTurnActionPoints.end(); ++it) {
            if (it->canPieceDrop && !it->canPlayCard) {
                currentTurnActionPoints.erase(it);
                std::cout << "[ActionPoint] 成功消耗 1 个【仅当下棋】的专属行动点。" << std::endl;
                return true;
            }
        }
        // 如果没有专属点，再被迫消耗"双能行动点"
        for (auto it = currentTurnActionPoints.begin(); it != currentTurnActionPoints.end(); ++it) {
            if (it->canPieceDrop && it->canPlayCard) {
                currentTurnActionPoints.erase(it);
                std::cout << "[ActionPoint] 成功消耗 1 个【双能】行动点。" << std::endl;
                return true;
            }
        }
    } 
    else {
        // 优先寻找"只能出牌，不能下棋"的【专能行动点】
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
        isAiThinking   = false; // 切回合时清除 AI 思考标记

        // 自动为下一个接管回合的玩家刷新基础行动点
        initActionPointsForTurn();
        // 切换回合时处理紫卡诅咒（每次换边都触发，不依赖 update 帧检测）
        processPurpleCurses();
    }
    else {
        std::cout << "[ActionPoint] 结算完成：仍有可用点数，请继续行动。⏳ 回合倒计时安全锁死，不予重置。" << std::endl;
    }
}

// ── 疫病处理：每回合传播 + 概率销毁 ──
void GameEngine::processInfection() {
    // 隔离倒计时
    if (quarantineTimer > 0) {
        quarantineTimer--;
        std::cout << "[Quarantine] 隔离剩余回合: " << quarantineTimer << std::endl;
        if (quarantineTimer == 0) {
            // 清除所有疫病
            for (int r = 0; r < 15; ++r)
                for (int c = 0; c < 15; ++c) {
                    infected[r][c] = false;
                    infectionActive[r][c] = false;
                }
            quarantineTimer = -1;
            std::cout << "[Quarantine] 隔离完成，所有疫病已清除！" << std::endl;
            return;
        }
    }

    // 收集当前感染棋子（仅活跃期才可传播）
    std::vector<std::pair<int,int>> currentInfected;
    std::vector<std::pair<int,int>> toActivate;  // 潜伏→活跃
    for (int r = 0; r < 15; ++r) {
        for (int c = 0; c < 15; ++c) {
            if (!infected[r][c]) continue;
            if (infectionActive[r][c]) {
                currentInfected.push_back({r, c});
            } else {
                toActivate.push_back({r, c});  // 本回合进入活跃，不传播
            }
        }
    }

    // 潜伏→活跃
    for (auto& p : toActivate) infectionActive[p.first][p.second] = true;

    if (currentInfected.empty() && toActivate.empty()) return;

    // 新感染标记（本回合新增，延迟写入避免连锁）
    bool newInfected[15][15] = {};
    std::vector<std::pair<int,int>> toDestroy;

    for (auto& p : currentInfected) {
        int r = p.first, c = p.second;
        // 敌方棋子10%概率销毁，我方棋子5%
        int pieceColor = chessboard.getPiece(r, c);
        int deathRate = (pieceColor == plagueOwner) ? 5 : 10;
        if ((rand() % 100) < deathRate) {
            toDestroy.push_back({r, c});
            continue;
        }

        // 四方向感染尝试
        int dr[] = {-1, 1, 0, 0};
        int dc[] = {0, 0, -1, 1};
        for (int d = 0; d < 4; ++d) {
            int nr = r + dr[d], nc = c + dc[d];
            if (nr < 0 || nr >= 15 || nc < 0 || nc >= 15) continue;
            int piece = chessboard.getPiece(nr, nc);
            if (piece == 0) continue;
            if (infected[nr][nc]) continue;
            if (newInfected[nr][nc]) continue;

            // 感染概率：敌方（非疫病发起方）60%，我方（疫病发起方）40%
            int infectThreshold = (piece == plagueOwner) ? 40 : 60;
            if ((rand() % 100) < infectThreshold) {
                newInfected[nr][nc] = true;
            }
        }
    }

    // 执行销毁
    for (auto& p : toDestroy) {
        int r = p.first, c = p.second;
        infected[r][c] = false; infectionActive[r][c] = false;
        if (chessboard.getPiece(r, c) != 0) {
            chessboard.placeDirect(r, c, 0);
            std::cout << "[Plague] 疫病销毁棋子 (" << r << "," << c << ")" << std::endl;
        }
    }

    // 应用新感染
    for (int r = 0; r < 15; ++r)
        for (int c = 0; c < 15; ++c)
            if (newInfected[r][c]) { infected[r][c] = true; infectionActive[r][c] = false; }
}

// ── 紫卡诅咒处理（盲目等）──
void GameEngine::processPurpleCurses() {
    bool isPlayerTurn = (currentTurn == playerColorPref ||
                         currentState == GameState::GAME_PVP);
    // 惩罚倒计时（每回合开始时减一次）
    if (yiDiShiQinPenalty > 0) {
        yiDiShiQinPenalty--;
        std::cout << "[以地事秦] 禁出牌剩余: " << yiDiShiQinPenalty << std::endl;
    }
    if (isPlayerTurn || currentState == GameState::GAME_PVP) {
        for (size_t i = 0; i < playerDeck.hand.size(); ) {
            auto& c = playerDeck.hand[i];
            if (c.cardColor == 1 && c.transferred && c.effect == CardEffect::BLIND) {
                c.value--;
                std::cout << "[Blind] 玩家盲目剩余: " << c.value << std::endl;
                if (c.value <= 0) {
                    if (!curseRemoving) {
                        curseRemoving = true;
                        curseRemovingIdx = (int)i;
                        curseRemoveClock.restart();
                        curseRemoveFragsInit = false;
                        std::cout << "[Blind] 玩家盲目到期，启动移除动画" << std::endl;
                    }
                    ++i;
                    continue;
                }
            }
            // 🌟 以地事秦
            if (c.cardColor == 1 && c.transferred && c.effect == CardEffect::YIDISHIQIN) {
                if (yiDiShiQinPenalizing) {
                    // 惩罚期：紫卡保留，倒计时结束才移除
                    if (yiDiShiQinPenalty <= 0) {
                        if (!curseRemoving) {
                            curseRemoving = true; curseRemovingIdx = (int)i;
                            curseRemoveClock.restart(); curseRemoveFragsInit = false;
                        }
                        yiDiShiQinActive = false; yiDiShiQinPenalizing = false;
                        std::cout << "[以地事秦] 惩罚结束，粉碎移除" << std::endl;
                        ++i; continue;
                    }
                } else if (yiDiShiQinResponded) {
                    // 已回应，粉碎移除
                    if (!curseRemoving) {
                        curseRemoving = true; curseRemovingIdx = (int)i;
                        curseRemoveClock.restart(); curseRemoveFragsInit = false;
                    }
                    yiDiShiQinActive = false; yiDiShiQinResponded = false;
                    std::cout << "[以地事秦] 已回应，粉碎移除" << std::endl;
                    ++i; continue;
                } else if (!yiDiShiQinActive) {
                    // 首次检测：开启响应窗口
                    yiDiShiQinActive = true;
                    yiDiShiQinResponded = false;
                    yiDiShiQinPenalty = 0;
                    std::cout << "[以地事秦] 响应窗口开启" << std::endl;
                } else {
                    // 响应窗口结束，未送牌 → 启动惩罚，紫卡保留
                    yiDiShiQinPenalizing = true;
                    yiDiShiQinPenalty = 3;
                    std::cout << "[以地事秦] 未回应，三回合禁出牌" << std::endl;
                }
            }
            ++i;
        }
    }
    if (!isPlayerTurn || currentState == GameState::GAME_PVP) {
        for (size_t i = 0; i < playerDeck.aiHand.size(); ) {
            auto& c = playerDeck.aiHand[i];
            if (c.cardColor == 1 && c.transferred && c.effect == CardEffect::BLIND) {
                c.value--;
                std::cout << "[Blind] AI 盲目剩余: " << c.value << std::endl;
                if (c.value <= 0) {
                    playerDeck.aiHand.erase(playerDeck.aiHand.begin() + i);
                    std::cout << "[Blind] AI 盲目已移除" << std::endl;
                    continue;
                }
            }
            // 🌟 AI 以地事秦（AI不会主动送牌回应，直接进惩罚）
            if (c.cardColor == 1 && c.transferred && c.effect == CardEffect::YIDISHIQIN) {
                if (yiDiShiQinPenalizing) {
                    if (yiDiShiQinPenalty <= 0) {
                        playerDeck.aiHand.erase(playerDeck.aiHand.begin() + i);
                        yiDiShiQinActive = false; yiDiShiQinPenalizing = false;
                        std::cout << "[以地事秦] AI惩罚结束，移除" << std::endl;
                        continue;
                    }
                } else if (!yiDiShiQinActive) {
                    yiDiShiQinActive = true; yiDiShiQinResponded = false;
                    yiDiShiQinPenalty = 0;
                    std::cout << "[以地事秦] AI响应窗口开启（AI不回应）" << std::endl;
                } else {
                    yiDiShiQinPenalizing = true; yiDiShiQinPenalty = 3;
                    std::cout << "[以地事秦] AI未回应，三回合禁出牌" << std::endl;
                }
            }
            ++i;
        }
    }
}

// 6. 卡牌效果分发器：湮灭完成后由 GameEngine 统一调度
// 🌟 破釜沉舟销毁阶段：退牌动画完成后，随机销毁敌方棋子
void GameEngine::executeSacrificeDestroy(int destroyCount) {
    int enemy = (currentTurn == 1) ? 2 : 1;
    std::vector<std::pair<int,int>> enemyPieces;
    for (int r = 0; r < 15; ++r)
        for (int c = 0; c < 15; ++c)
            if (chessboard.getPiece(r, c) == enemy)
                enemyPieces.push_back({r, c});
    // 随机打乱
    for (size_t k = 0; k < enemyPieces.size(); ++k) {
        int sw = rand() % enemyPieces.size();
        std::swap(enemyPieces[k], enemyPieces[sw]);
    }
    int actual = std::min(destroyCount, (int)enemyPieces.size());
    pendingDestroys.clear();
    for (int k = 0; k < actual; ++k)
        pendingDestroys.push_back(enemyPieces[k]);
    isBulkDestroying = !pendingDestroys.empty();
    if (isBulkDestroying) {
        auto& p = pendingDestroys.back();
        chessboard.startDestroyAnim(p.first, p.second);
        isBusyAnimating = true;
        std::cout << "[CardEffect] 破釜沉舟销毁阶段：销毁" << actual << "颗敌方棋子。" << std::endl;
    } else {
        isBusyAnimating = false;
    }
}

bool GameEngine::applyCardEffect(const Card& card) {
    std::cout << "[CardEffect] 触发卡牌效果 —— ID:" << card.id
              << " 名称:" << card.name.c_str()
              << " 效果类型:" << static_cast<int>(card.effect) << std::endl;

    switch (card.effect) {
        case CardEffect::FORCE_DROP:
            addActionPoint(true, false);
            addActionPoint(true, false);
            std::cout << "[CardEffect] 连击发动！获得 2 次额外落子机会。" << std::endl;
            return true;
        case CardEffect::CHANGE_WIN_RULE: {
            int opponent = (currentTurn == 1) ? 2 : 1;
            chessboard.setWinCondition(opponent, card.value);
            std::cout << "[CardEffect] 隐忍发动！敌方（玩家" << opponent
                      << "）胜利条件改为 " << card.value << " 子连星。" << std::endl;
            return true;
        }
        case CardEffect::CONVERT_PIECE: {
            isSelectingPiece = true;
            selectPieceStep  = 1;
            selectPiecePlayer = currentTurn;
            std::cout << "[CardEffect] 笼络发动！请先选择己方棋子销毁。" << std::endl;
            return false;
        }
        case CardEffect::SACRIFICE_HAND: {
            bool isAI = (currentState == GameState::GAME_PVE && currentTurn != playerColorPref);
            int otherCards = isAI ? (int)playerDeck.aiHand.size() : (int)playerDeck.hand.size();
            int destroyCount = (otherCards >= 3) ? 5 : (otherCards == 2 ? 3 : (otherCards == 1 ? 2 : 1));
            if (otherCards > 0) {
                if (isAI) {
                    // AI 手牌直接退库，无动画
                    playerDeck.returnAiHandToDeck();
                    std::cout << "[AI] 破釜沉舟！" << otherCards
                              << "张AI手牌返还牌库，将销毁" << destroyCount << "颗敌方棋子。" << std::endl;
                    executeSacrificeDestroy(destroyCount);
                    return false;
                }
                isReturningHandToDeck = true;
                returnHandToDeckClock.restart();
                pendingSacrificeDestroys = destroyCount;
                isBusyAnimating = true;
                std::cout << "[CardEffect] 破釜沉舟！退牌动画启动，" << otherCards
                          << "张手牌返还牌库，将销毁" << destroyCount << "颗敌方棋子。" << std::endl;
            } else {
                executeSacrificeDestroy(destroyCount);
            }
            return false;
        }
        case CardEffect::PLAGUE: {
            bool isAI = (currentState == GameState::GAME_PVE && currentTurn != playerColorPref);
            if (isAI && aiPlayer) {
                // AI 自动选敌方棋子感染
                auto enemy = aiPlayer->chooseEnemyPieceToConvert(chessboard);
                if (enemy.first >= 0) {
                    infected[enemy.first][enemy.second] = true; infectionActive[enemy.first][enemy.second] = false;
                    plagueOwner = currentTurn;
                    std::cout << "[AI] 疫病发动！感染 (" << enemy.first << "," << enemy.second << ")" << std::endl;
                }
                return true;
            }
            // 玩家交互选子
            isSelectingPiece = true;
            selectPieceStep = 10;  // 用 10 表示疫病选子（区别于笼络的 1/2/3）
            selectPiecePlayer = currentTurn;
            std::cout << "[CardEffect] 疫病发动！请选择一颗敌方棋子感染。" << std::endl;
            return false;
        }
        case CardEffect::QUARANTINE:
            quarantineTimer = card.value;  // 3 回合
            std::cout << "[CardEffect] 隔离发动！" << quarantineTimer << " 回合后清除所有疫病。" << std::endl;
            return true;
        case CardEffect::REMOVE_OPPONENT:
            std::cout << "[CardEffect] REMOVE_OPPONENT 尚未实现，预留框架。" << std::endl;
            return true;
        case CardEffect::EXTRA_TURN:
            std::cout << "[CardEffect] EXTRA_TURN 尚未实现，预留框架。" << std::endl;
            return true;
        default:
            std::cout << "[CardEffect] 未知或无效果卡牌。" << std::endl;
            return true;
    }
    return true;
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
        std::cout << "[Audio] CardReader 检测 —— 开始串行过渡到 BATTLE_02" << std::endl;
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

// ── 帧棋子 HR 碎片缓存 ──
void GameEngine::initFramePieceFragCache() {
    auto gen = [](sf::Texture& tex, std::vector<Fragment>& out) {
        out.clear();
        sf::Vector2u ts = tex.getSize();
        if (ts.x == 0 || ts.y == 0) return;
        const int COLS = 36, ROWS = 36;  // 1296 fragments
        float cw = (float)ts.x / COLS;
        float rh = (float)ts.y / ROWS;
        for (int r = 0; r < ROWS; ++r) {
            for (int c = 0; c < COLS; ++c) {
                Fragment f;
                int rx = (int)(cw * c + cw * (rand() % 12 - 6) / 100.f);
                int ry = (int)(rh * r + rh * (rand() % 6  - 3) / 100.f);
                int rw = (int)(cw + cw * (rand() % 10 - 5) / 100.f);
                int rh2 = (int)(rh + rh * (rand() % 6  - 3) / 100.f);
                if (rx < 0) rx = 0; if (ry < 0) ry = 0;
                if (rx + rw > (int)ts.x) rw = ts.x - rx;
                if (ry + rh2 > (int)ts.y) rh2 = ts.y - ry;
                if (rw < 1) rw = 1; if (rh2 < 1) rh2 = 1;
                f.texRect = sf::IntRect({rx, ry}, {rw, rh2});
                f.released = false; f.alpha = 255.f;
                out.push_back(f);
            }
        }
    };
    gen(menuBlackTex, fpFragCacheBlack);
    gen(menuWhiteTex, fpFragCacheWhite);
    std::cout << "[FrameFragCache] HR碎片缓存: 黑=" << fpFragCacheBlack.size()
              << " 白=" << fpFragCacheWhite.size() << std::endl;
}

// ── 卡牌碎片缓存 ──
void GameEngine::initCardFragmentCache() {
    auto gen = [](sf::Texture& tex, std::vector<Fragment>& out) {
        out.clear();
        sf::Vector2u ts = tex.getSize();
        if (ts.x == 0 || ts.y == 0) return;
        const int COLS = 36, ROWS = 36;
        float cw = (float)ts.x / COLS;
        float rh = (float)ts.y / ROWS;
        for (int r = 0; r < ROWS; ++r) {
            for (int c = 0; c < COLS; ++c) {
                Fragment f;
                int rx = (int)(cw * c + cw * (rand() % 12 - 6) / 100.f);
                int ry = (int)(rh * r + rh * (rand() % 6  - 3) / 100.f);
                int rw = (int)(cw + cw * (rand() % 10 - 5) / 100.f);
                int rh2 =(int)(rh + rh * (rand() % 6  - 3) / 100.f);
                if (rx < 0) rx = 0; if (ry < 0) ry = 0;
                if (rx + rw > (int)ts.x) rw = ts.x - rx;
                if (ry + rh2 > (int)ts.y) rh2 = ts.y - ry;
                if (rw < 1) rw = 1; if (rh2 < 1) rh2 = 1;
                f.texRect = sf::IntRect({rx, ry}, {rw, rh2});
                f.released = false; f.alpha = 255.f;
                out.push_back(f);
            }
        }
    };
    gen(newCardTexture, cardFragCache);
    gen(purpleCardTexture, cardFragCachePurple);
    std::cout << "[CardFragment] 卡牌碎片缓存: 橙=" << cardFragCache.size()
              << " 紫=" << cardFragCachePurple.size() << std::endl;
}
