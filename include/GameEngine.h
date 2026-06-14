#ifndef GAMEENGINE_H
#define GAMEENGINE_H

#include <SFML/Graphics.hpp>
#include "Chessboard.h"
#include "AIPlayer.h"
#include "DeckManager.h"
#include <vector>
#include <string>
#include <filesystem>
#include <SFML/Audio.hpp>

// 状态枚举
enum class GameState {
    MENU,
    PVE_CONFIG,
    GAME_PVP,
    GAME_PVE,
    SETTINGS,
    HELP,
    HISTORY,
    GAME_OVER
};

class GameEngine {
public:
    GameEngine();
    ~GameEngine();
    void run();

private:
    // 事件处理
    void processEvents();
    void update();
    void render();

    // 事件处理函数
    void handleMenuClick(sf::Vector2i mousePos);
    void handlePVEConfigClick(sf::Vector2i mousePos);
    bool handleHUDClick(sf::Vector2i mousePos);
    void handleSettingsClick(sf::Vector2i mousePos);
    void handleHistoryClick(sf::Vector2i mousePos);

    // UI 初始化
    void initUI();
    
    // 全屏控制
    void toggleFullscreen();

    // 🌟 【新增】分离的渲染函数
    void renderMenu();
    void renderPVEConfig();
    void renderGameplay();
    void renderGameOver();
    void renderSettings();
    void renderInfo();

    // ===== UI 元素 =====
    sf::RenderWindow window;
    GameState currentState;
    Chessboard chessboard;
    AIPlayer* aiPlayer;

    // ===== 窗口和显示设置 =====
    bool isFullscreen = false;
    static const unsigned int WINDOW_WIDTH = 1280;
    static const unsigned int WINDOW_HEIGHT = 720;

    // ===== 游戏状态 =====
    int currentTurn;          // 1: 黑, 2: 白
    bool isProfessionalMode;
    std::wstring winReason;
    bool isGameOver;
    bool isCardAttachedToMouse = false; // 🌟 标记卡牌是否吸附在鼠标上
    sf::Vector2f cardMouseOffset = {0.f, 0.f};
    DeckManager playerDeck;

    // ===== 人机对战设置 =====
    int playerColorPref;      // 1: 玩家执黑, 2: 玩家执白
    int aiDifficultySetting;  // 1: 初级, 2: 明智, 3: 大神

    // ===== AI 相关 =====
    bool isAiThinking;
    sf::Clock turnClock;
    sf::Clock aiThinkClock;

    // ===== 文字和按钮 =====
    sf::Font font;            // 原有的主UI系统字体
    sf::Font cardFont;          // 🌟 新增：专门给大卡牌文字使用的个性化专属字体
    sf::Text uiText;
    sf::Text timerText;

    std::vector<sf::Text> menuButtons;
    std::vector<sf::RectangleShape> menuButtonBackgrounds;
    std::vector<sf::RectangleShape> gameButtons;
    std::vector<sf::Text> gameButtonTexts;

    // ============= 历史/复盘相关 =============
    // 是否已为当前对局自动保存过（避免重复）
    bool savedThisGame = false;

    // 列表项用于点击检测（在 renderInfo 填充）
    std::vector<std::pair<sf::FloatRect, std::filesystem::path>> historyItems;

    // 回放控制（GameEngine 使用这些触发 Chessboard 的 stepReplay）
    sf::Clock replayClock;
    float replayStepSeconds = 0.6f; // 每步间隔（秒）

    //添加音频
    sf::SoundBuffer dropBuffer;
    sf::Sound* dropSound;

    void initAudio();
    
    // 背景音乐
    sf::Music menuMusic; 
    sf::Music configMusic;
    sf::Music battleMusic;
    sf::Music winMusic;
    sf::Music failMusic;

    // 🌟【新增：魔幻像素卡牌底底纹与精灵】
    sf::Texture newCardTexture;
    sf::Sprite* newCardSprite = nullptr;
    // 🌟【新增：卡槽纹理与精灵】
    sf::Texture cardSlotTexture;
    sf::Sprite* cardSlotSprite = nullptr;
    
    // 🌟【新增：卡牌高精动效时序控制器】
    sf::Clock cardAnimClock;      // 记录卡牌动画已经播了多久
    bool isAnimatingCard = false; // 当前卡牌是否正处于出场动画中
    bool lastHandEmpty = true;    // 辅助判定：手牌从无到有的瞬间触发时钟
    // 🌟【新增：卡牌弹回动画状态机控制变量】
    bool isReturningToSlot = false;   // 卡牌当前是否正处于“弹回卡槽”的动画中
    sf::Clock returnDelayClock;       // 计时器：用来处理点击后的 0.2 秒静止停顿
    sf::Vector2f returnStartPos;      // 记录玩家松开点击时，卡牌在屏幕上的那一瞬间的物理坐标
    // 🌟【新增：读卡器夹层素材】
    std::unique_ptr<sf::Texture> cardReaderTopTexture;
    std::unique_ptr<sf::Sprite> cardReaderTopSprite;

    std::unique_ptr<sf::Texture> cardReaderBottomTexture;
    std::unique_ptr<sf::Sprite> cardReaderBottomSprite;
};

#endif // GAMEENGINE_H
