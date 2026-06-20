# 技术规范 — ChessBoardGame

## 技术栈

| 层级 | 技术 |
|------|------|
| 语言 | C++17 |
| 图形/音频 | SFML 3.0 (Graphics, Window, System, Audio) |
| 构建系统 | CMake + Ninja |
| 编译器 | MinGW (Windows) |
| 版本控制 | Git |

## 项目结构

```
chessBoardGame/
├── main.cpp                  # 程序入口
├── CMakeLists.txt            # 顶层 CMake 构建文件
├── CLAUDE.md                 # AI 助手指引
├── include/
│   ├── GameEngine.h          # 游戏引擎（状态机、AP 系统、渲染、事件）~340 行
│   ├── Chessboard.h          # 棋盘逻辑（网格、落子、胜利判定、碎片动画）
│   ├── Card.h                # 卡牌数据结构
│   ├── DeckManager.h         # 牌库管理（抽牌、出牌、重置）
│   └── AIPlayer.h            # AI 玩家
├── src/
│   ├── GameEngine.cpp        # 游戏引擎实现 ~5400 行
│   ├── Chessboard.cpp        # 棋盘实现 ~1100 行
│   ├── DeckManager.cpp       # 牌库实现
│   └── AIPlayer.cpp          # AI 实现
├── assets/                   # 资源文件
│   ├── board_.png            # 棋盘背景
│   ├── black.png / white.png # 棋子纹理
│   ├── Card.png / Card_01.png # 橙/紫卡牌底纹
│   ├── CardSlot.png          # 卡槽背景
│   ├── UI_Frame.png          # UI 框架
│   ├── SettingsMenu.png      # 暂停菜单背景
│   ├── virus/skull/ring.png  # 悬停预览图标
│   └── Universe/             # 背景帧动画 (1500 帧)
├── saves/                    # 棋局复盘存档 (.txt)
├── build/                    # 构建输出
├── docs/                     # 项目文档
└── devlog/                   # 开发日志
```

## 核心架构

### 游戏状态机
```
MENU → PVE_CONFIG → GAME_PVP / GAME_PVE → GAME_OVER
                    ↑           ↓
                    └───────────┘
                    SETTINGS / HELP / HISTORY
```

### 卡牌效果枚举
```cpp
enum class CardEffect {
    NONE, FORCE_DROP, CHANGE_WIN_RULE, CONVERT_PIECE,
    SACRIFICE_HAND, REMOVE_OPPONENT, EXTRA_TURN,
    PLAGUE, QUARANTINE, BLIND, YIDISHIQIN
};
```

### 卡牌结构体
```cpp
struct Card {
    int id; std::wstring name, description;
    CardEffect effect; int value;
    int cardColor = 0;    // 0=橙卡, 1=紫卡
    bool transferred = false; // 紫卡已传送标记
};
```

### Fragment 结构体（碎片动画）
```cpp
struct Fragment {
    sf::Vector2f pos, vel, targetPos;
    sf::IntRect texRect;
    float rotation, rotSpeed, alpha, fadeTimer, assembleTimer;
    bool released;
};
```

### 关键状态机

#### 卡牌湮灭（橙卡出牌）
```
PAUSE_BEFORE(0.2s)→MOVE_TO_200→PAUSE_AFTER(0.2s)→SHATTER(0.8s 边上升边粉碎)→展示
```

#### 紫卡传送
```
MOVE_TO_POS→PAUSE_BEFORE(0.2s)→PAUSE_AFTER(0.5s)→MOVE_TO_PORTAL(0.35s 下滑)→传送完成
```

#### AI 出牌动画
```
RISING(0.55s)→PAUSE_AT_CENTER(1s)→TO_READER(0.4s)→ANNIHILATING(0.35s)→SHOWCASING
紫卡: RISING→PAUSE_AT_CENTER→PURPLE_FADE_OUT→中央展示→PURPLE_SLOT_GEN
```

#### 以地事秦状态机
```
IDLE → Active(响应窗口) → Responded(送牌粉碎移除)
                        → Penalizing(penalty=3, 禁出牌, 紫卡保留)
                        → Penalty=0(粉碎移除)
```

## 碎片动画系统

### 预计算缓存
- 棋子：`Chessboard::initFragmentCache()` → 12×12=144 碎片，±4% 扰动
- 卡牌：`GameEngine::initCardFragmentCache()` → 36×36=1296 碎片，±6% 扰动

### 动画参数
| 参数 | 值 |
|------|-----|
| 重力 | 800 px/s² |
| 水平漂移 | ±80 px/s（棋子）/ ±100 px/s（卡牌） |
| 旋转速度 | ±360 °/s |
| 淡出时长 | 0.85s smoothstep |
| BlendAdd 亮度 | 飞行中渐亮 0→120, 到达后缓暗 |

### 衰减缓冲区
- 棋子：`Chessboard::decayFragments`
- 卡牌：`GameEngine::cardDecayFrags`
- PIP：`GameEngine::pipDecayFrags`
- Fragment 在动画结束后移入缓冲区，继续物理+淡出，alpha=0 时自动移除

## API 参考

### Chessboard 新增 API
```cpp
void startDestroyAnim(int row, int col);    // 初始化碎片
void startConvertAnim(int r, int c, int from, int to); // 双碎片
void finishPieceAnim();                      // 不移除碎片，仅清 type
bool isPieceAnimating() const;
std::vector<Fragment> activeFragments;       // 粉碎方
std::vector<Fragment> convertFragments;      // 聚合方
std::vector<Fragment> decayFragments;        // 衰减缓冲
```

### GameEngine 新增 API
```cpp
// 卡牌碎片
void initCardFragmentCache();
std::vector<Fragment> cardFragCache, cardFragCachePurple;
std::vector<Fragment> cardFragActive, cardBirthFrags, showcaseFrags;

// 碎片动画状态
CardAnnihilateState annihilateState;  // 含 SHATTER
bool cardBirthActive, cardBirthInit;
bool curseRemoving; int curseRemovingIdx;
std::vector<Fragment> cardDecayFrags, curseRemoveFrags;

// 以地事秦
bool yiDiShiQinActive, yiDiShiQinResponded, yiDiShiQinPenalizing;
int  yiDiShiQinPenalty;

// 调试面板
bool aiOnlyDrop, aiOnlyCard;
```

## 编码规范
- C++17，无平台特定扩展
- 成员变量无前缀，方法 camelCase
- 中文 UI 使用 `std::wstring`
- SFML 纹理与精灵分离管理
- 复杂逻辑用分隔注释标记
- 关键变量在声明处注释用途和单位

## 构建命令
```bash
cmake --build build          # 编译
./build/chessBoardGame.exe   # 运行
```
