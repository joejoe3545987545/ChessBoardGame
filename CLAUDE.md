# CLAUDE.md — ChessBoardGame 项目指引

## 项目简介

ChessBoardGame 是一款结合**五子棋 + 卡牌**玩法的策略游戏。C++17 + SFML 3.0，CMake + Ninja 构建，MinGW 编译。1280×720 窗口，15×15 棋盘。

## 文档索引

| 文档 | 路径 | 内容 |
|------|------|------|
| 开发需求 | [docs/requirements.md](docs/requirements.md) | 核心玩法、功能清单、优先级 |
| 技术规范 | [docs/technical-spec.md](docs/technical-spec.md) | 架构、API、编码规范、文件结构 |
| 设计规范 | [docs/design-spec.md](docs/design-spec.md) | 视觉、交互、音效、字体规范 |
| 执行步骤 | [docs/implementation-steps.md](docs/implementation-steps.md) | 里程碑 checklist、下一步计划 |
| 开发日志 | [devlog/](devlog/) | 每日开发记录（按日期命名：`YYYY-MM-DD.md`） |

## 工作流程

### 每次会话开始
1. 检查 `devlog/` 中最新的日志文件，了解上次开发到哪里
2. 阅读 `docs/implementation-steps.md` 确认当前阶段和下一步任务
3. 如涉及新功能，先读 `docs/requirements.md` 和 `docs/design-spec.md`

### 每次会话结束
1. **必须在 `devlog/` 中创建或更新当日日志**（`YYYY-MM-DD.md`）
2. 日志格式：
   ```markdown
   # 开发日志 — YYYY-MM-DD

   ## 已完成
   - [x] 具体完成事项（含文件/行号引用）

   ## 待办
   - [ ] 发现的问题或下一步要做的事

   ## 备注
   重要决策、设计讨论、注意事项
   ```
3. 如有里程碑推进，更新 `docs/implementation-steps.md` 中的 checklist

### 编码规范（详见 `docs/technical-spec.md`）
- C++17，无平台特定扩展
- 成员变量无前缀，方法 camelCase
- 中文 UI 使用 `std::wstring`
- SFML 纹理与精灵分离管理（纹理在类成员，精灵用指针/`unique_ptr`）
- 复杂逻辑用分隔注释标记（`// ====...====`）
- 关键变量在声明处注释用途和单位

### 构建命令
```bash
cmake --build build          # 编译
./build/chessBoardGame.exe   # 运行（Windows）
```

## 核心架构速查

### 游戏状态机
`MENU → PVE_CONFIG → GAME_{PVP,PVE} → GAME_OVER`
（另有 SETTINGS / HELP / HISTORY）

### 行动点系统 (`include/GameEngine.h:78-90`)
- `ActionPoint { canPieceDrop, canPlayCard }` — 三模式判定
- `hasValidActionPoint(bool isPieceDrop)` — 行动前检查
- `consumeActionPoint(bool isPieceDrop)` — 消耗（优先专能点）
- `addActionPoint(canPiece, canCard)` — 卡牌效果赠送
- `settleActionPoints()` — AP 归零切回合，未归零继续
- `applyCardEffect(const Card&)` — 卡牌效果分发器

### 出牌流程
```
拖牌到 CardReader → AP检查 → 5阶段湮灭 → consumeAP → 触发效果 → settleAP
```

### 关键文件
- 游戏引擎（状态机/AP/事件/渲染）：`include/GameEngine.h` + `src/GameEngine.cpp`（~1700行）
- 棋盘逻辑：`include/Chessboard.h` + `src/Chessboard.cpp`
- 卡牌数据：`include/Card.h`（`Card` 结构体 + `CardEffect` 枚举）
- 牌库管理：`include/DeckManager.h` + `src/DeckManager.cpp`
- AI 玩家：`include/AIPlayer.h` + `src/AIPlayer.cpp`

## 当前状态

- **已完成**：棋盘落子、每方独立胜利条件、行动点框架、卡牌出牌流程、双卡槽+堆叠+存放、展示动画、动态音乐、计时暂停、无边框全屏、棋子动画
- **卡牌**：连击（FORCE_DROP）、隐忍（CHANGE_WIN_RULE→敌方六子连星）、笼络（CONVERT_PIECE）、破釜沉舟（SACRIFICE_HAND）
- **牌库**：随机抽卡 + 弃牌堆
- **渲染**：统一 activeIdx 管理、滑入式湮灭（smoothstep）、棋子销毁/转化动画
- **下一步**：AI 增强 → 牌库扩展 → JSON 配置
