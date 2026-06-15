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

    // 战斗音乐控制
    void triggerBattleMusic02();            // 切到 battle_02（CardReader 触发）
    void addCrisisTime(float seconds = 30); // 危机延长 battle_02 时长

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
    bool isBusyAnimating = false;       // 🌟 卡牌动画期间锁定玩家操作和 AI
    bool isCardAttachedToMouse = false; // 🌟 标记卡牌是否吸附在鼠标上
    sf::Vector2f cardMouseOffset = {0.f, 0.f};
    DeckManager playerDeck;
    std::vector<int> handSlotAssign;  // 与 hand 平行：1=槽1, 2=槽2
    int attachedCardIndex = -1;       // 当前吸附的是 hand 中哪张卡（-1=无）

    struct ActionPoint {
        bool canPieceDrop = false; // 判定条件 1：是否允许下棋
        bool canPlayCard = false;  // 判定条件 2：是否允许出牌
    };

    // 当前回合正在行动的玩家所拥有的“行动点蓄水池”
    std::vector<ActionPoint> currentTurnActionPoints;

    void initActionPointsForTurn();                  // 每一轮新回合开始：自动赠送1个双能点
    bool hasValidActionPoint(bool isPieceDrop) const; // 判定当前点数是否支持某种行动
    bool consumeActionPoint(bool isPieceDrop);       // 消耗行动点（含智能防卡手消耗算法）
    void addActionPoint(bool canPiece, bool canCard); // 供卡牌效果调用：额外赠送特定行动点
    void settleActionPoints();                       // 🌟 总体行动点结算中心（决定回合是否结束）
    void applyCardEffect(const Card& card);         // 🌟 卡牌效果分发器（湮灭完成后触发）

    // ===== 人机对战设置 =====
    int playerColorPref;      // 1: 玩家执黑, 2: 玩家执白
    int aiDifficultySetting;  // 1: 初级, 2: 明智, 3: 大神

    // ===== AI 相关 =====
    bool isAiThinking;
    sf::Clock turnClock;
    sf::Clock aiThinkClock;

    // ===== 回合计时器暂停（卡牌动画期间不计时）=====
    float turnTimePaused = 0.f;    // 累计暂停时长（秒）
    bool  isTurnPaused   = false;  // 当前是否处于暂停中
    sf::Clock pauseClock;          // 当前暂停段计时器

    float getEffectiveTurnTime() const;  // 有效回合时间 = 实际 - 暂停

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
    sf::Music battleMusic02;  // 战斗曲 2（读卡器触发切歌用）
    sf::Music winMusic;
    sf::Music failMusic;

    // ===== 战斗音乐状态机 =====
    enum class BattleMusicState { NORMAL, SWITCHING_TO_02, BATTLE_02, SWITCHING_TO_01 };
    BattleMusicState battleMusicState = BattleMusicState::NORMAL;
    sf::Clock crossfadeClock;
    sf::Clock battleTimer;         // battle_02 剩余时长计时
    float battle02Remaining = 0.f;
    static constexpr float CROSSFADE_DURATION   = 1.0f;
    static constexpr float BATTLE_02_BASE_TIME  = 30.f;
    static constexpr float BATTLE_VOLUME        = 40.f;

    // 🌟【新增：魔幻像素卡牌底底纹与精灵】
    sf::Texture newCardTexture;
    sf::Sprite* newCardSprite = nullptr;
    // 🌟【新增：卡槽纹理与精灵】
    sf::Texture cardSlotTexture;
    sf::Sprite* cardSlotSprite = nullptr;
    sf::Texture cardSlotTexture2;
    sf::Sprite* cardSlotSprite2 = nullptr;    // 第二卡槽（下方，当前空置）

    // 多卡堆叠相关
    bool newCardJustDrawn = false;  // 本帧刚抽到卡，触发出生动画（比 handSize 变化更可靠）

    // 🌟【新增：CardReader正下方的圆形检测区域】
    sf::CircleShape detectionZone;
    float zoneRadius = 120.0f; // 暂定半径为50像素，后续可以根据实际大小微调

    // 🌟【新增：卡牌高精动效时序控制器】
    sf::Clock cardAnimClock;      // 记录卡牌动画已经播了多久
    bool isAnimatingCard = false; // 当前卡牌是否正处于出场动画中
    // 🌟【新增：卡牌弹回动画状态机控制变量】
    bool isReturningToSlot = false;   // 卡牌当前是否正处于“弹回卡槽”的动画中
    sf::Clock returnDelayClock;       // 计时器：用来处理点击后的 0.2 秒静止停顿
    sf::Vector2f returnStartPos;      // 记录玩家松开点击时，卡牌在屏幕上的那一瞬间的物理坐标
    
    enum class CardAnnihilateState {
        NONE,            // 常态（没有触发湮灭）
        PAUSE_BEFORE,    // 阶段 1：点中后，在原地停顿 0.2 秒
        MOVE_TO_200,     // 阶段 2：位移到 (640, 200)
        PAUSE_AFTER,     // 阶段 3：在 (640, 200) 停顿 0.5 秒
        MOVE_TO_ZERO,     // 阶段 4：缓慢向上移动到 (640, 0)
        PAUSE_FINAL      // 新增阶段 5：在 70 的位置定格停顿 0.5 秒
    };

    CardAnnihilateState annihilateState = CardAnnihilateState::NONE; // 当前湮灭状态
    sf::Clock annihilateClock;                                      // 湮灭专用独立时钟

    // 🌟 卡牌效果展示状态机（湮灭 → 中央展示 → 消退 → 触发效果）
    enum class CardShowcaseState {
        NONE,       // 无展示
        PAUSE,      // 湮灭后 0.5s 空白停顿
        APPEAR,     // 中央出现：小→大 + 阶梯裁剪 + 亮→正常
        DISPLAY,    // 2s 完整静止展示
        FADE_OUT    // 原色→极亮 + 透明度 255→0
    };
    CardShowcaseState showcaseState = CardShowcaseState::NONE;
    Card showcasedCard;                 // 存储被展示的卡牌数据（pop 后保留）
    sf::Clock showcaseClock;            // 展示阶段独立时钟

    // 🌟【新增：读卡器夹层素材】
    std::unique_ptr<sf::Texture> cardReaderTopTexture;
    std::unique_ptr<sf::Sprite> cardReaderTopSprite;

    std::unique_ptr<sf::Texture> cardReaderBottomTexture;
    std::unique_ptr<sf::Sprite> cardReaderBottomSprite;
};

#endif // GAMEENGINE_H
