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
├── CMakePresets.json         # CMake 预设配置
├── CLAUDE.md                 # AI 助手指引（本体系入口）
├── include/                  # 头文件
│   ├── GameEngine.h          # 游戏引擎（状态机、AP 系统、渲染、事件）
│   ├── Chessboard.h          # 棋盘逻辑（网格、落子、胜利判定）
│   ├── Card.h                # 卡牌数据结构
│   ├── DeckManager.h         # 牌库管理（抽牌、出牌、重置）
│   └── AIPlayer.h            # AI 玩家
├── src/                      # 实现文件
│   ├── GameEngine.cpp        # 游戏引擎实现（~1700 行）
│   ├── Chessboard.cpp        # 棋盘实现
│   ├── DeckManager.cpp       # 牌库实现
│   └── AIPlayer.cpp          # AI 实现
├── assets/                   # 资源文件（纹理、音频、字体）
├── saves/                    # 棋局复盘存档（.txt）
├── build/                    # 构建输出
├── docs/                     # 项目文档
│   ├── requirements.md       # 开发需求
│   ├── technical-spec.md     # 技术规范（本文件）
│   ├── design-spec.md        # 设计规范
│   └── implementation-steps.md # 执行步骤
└── devlog/                   # 开发日志
    └── YYYY-MM-DD.md         # 每日开发记录
```

## 核心架构

### 游戏状态机
```
MENU → PVE_CONFIG → GAME_PVP / GAME_PVE → GAME_OVER
                    ↑           ↓
                    └───────────┘
                    SETTINGS / HELP / HISTORY
```

枚举定义于 `include/GameEngine.h:14-23`：
```cpp
enum class GameState { MENU, PVE_CONFIG, GAME_PVP, GAME_PVE, SETTINGS, HELP, HISTORY, GAME_OVER };
```

### 主循环
```cpp
void GameEngine::run() {
    while (window.isOpen()) {
        processEvents();  // 事件处理
        update();         // 状态更新
        render();         // 渲染
    }
}
```

### 行动点系统

位置：`include/GameEngine.h:78-90`、`src/GameEngine.cpp:1562-1701`

核心 API：
| 方法 | 功能 |
|------|------|
| `initActionPointsForTurn()` | 每回开始，赠送 1 个双能点 |
| `hasValidActionPoint(bool isPieceDrop)` | 检查是否有支持某行动的点 |
| `consumeActionPoint(bool isPieceDrop)` | 消耗行动点（优先专能点） |
| `addActionPoint(bool canPiece, bool canCard)` | 卡牌效果赠送 AP |
| `settleActionPoints()` | 总体结算（AP=0 切回合） |
| `applyCardEffect(const Card&)` | 卡牌效果分发器 |

### 卡牌数据结构

`include/Card.h`：
```cpp
enum class CardEffect { NONE, FORCE_DROP, CHANGE_WIN_RULE, REMOVE_OPPONENT, EXTRA_TURN };

struct Card {
    int id;
    std::wstring name;
    std::wstring description;
    CardEffect effect;
    int value;  // 效果参数
};
```

### 出牌流程

```
抓牌 → 拖到 CardReader → hasValidActionPoint(false)?
  ├─ false → 弹回卡槽
  └─ true → 5阶段湮灭动画 → consumeActionPoint(false)
            → applyCardEffect(card) → settleActionPoints()
```

## 编码规范

- C++17 标准，不使用平台特定扩展
- 类成员变量无前缀，方法名 camelCase
- 游戏实体使用宽字符串 `std::wstring`（中文 UI）
- SFML 对象生命周期：纹理与精灵分离管理，纹理在类中声明，精灵用指针/`unique_ptr`
- 在类声明处注释关键变量的用途和单位
- 复杂逻辑段落用分隔注释标记（`// ====...====`）

## 窗口规格

| 属性 | 值 |
|------|-----|
| 分辨率 | 1280 × 720 |
| 棋盘网格 | 15 × 15 |
| 格距 | 36 px |
| 卡牌物理尺寸 | 144 × 200 px |
| 卡牌缩放 | 0.18 |
| CardReader 检测半径 | 120 px |
| 每回合时限 | 45 秒 |
