# 执行步骤 — ChessBoardGame

## 当前开发阶段：早期阶段（卡牌功能实现中）

## 已完成里程碑

### ✅ 基础棋盘系统
- 15×15 棋盘渲染与落子交互
- 五子连线胜利判定 + 高亮显示
- 三连/四连模式检测
- 落子历史记录与复盘回放

### ✅ 行动点框架
- 三模式判定（双能/仅下棋/仅出牌）
- 智能消耗算法（优先专能点）
- 总体结算中心（AP 归零切回合）
- 倒计时锁定机制

### ✅ 卡牌视觉系统
- 卡牌纹理、卡槽、CardReader 上下层
- 鼠标吸附/弹回动画
- 5 阶段湮灭动画特效

### ✅ 卡牌出牌流程（2026-06-16）
- 出牌前 AP 检查
- 湮灭完成 → 消耗 AP → 触发效果 → 结算
- FORCE_DROP 效果实现

---

## 下一步执行计划

### 阶段 1：完善现有卡牌效果

#### Step 1.1 — CHANGE_WIN_RULE 效果
- [ ] 在 `Chessboard` 中新增 `int winCondition = 5` 成员变量
- [ ] 修改 `Chessboard::checkWin()` 使用 `winCondition` 而非硬编码 5
- [ ] 在 `applyCardEffect()` 中实现：设置 `winCondition = card.value`
- [ ] 添加视觉效果提示胜利条件已改变
- **涉及文件**：`Chessboard.h`, `Chessboard.cpp`, `GameEngine.cpp`

#### Step 1.2 — REMOVE_OPPONENT 效果
- [ ] 设计交互：出牌后让玩家点击对方一颗棋子移除
- [ ] 或简化：自动随机移除一颗对方棋子
- [ ] 在 `applyCardEffect()` 中实现
- **涉及文件**：`Chessboard.h`, `GameEngine.cpp`

#### Step 1.3 — EXTRA_TURN 效果
- [ ] 在 `applyCardEffect()` 中实现：赠送 1 个双能点 + 保证回合不切换
- [ ] 设计实现方式：`addActionPoint(true, true)` 即可，结算时 AP 不为零自然继续
- **涉及文件**：`GameEngine.cpp`

### 阶段 2：多手牌支持
- [ ] 手牌 UI 重设计：当前只显示最新抽到的卡牌
- [ ] 多张手牌排列显示（如横向排列）
- [ ] 点击选择要出的卡牌（高亮选中）
- [ ] 选中后拖到 CardReader 出牌
- **涉及文件**：`GameEngine.h`, `GameEngine.cpp`

### 阶段 3：牌库扩展
- [ ] 创建 JSON 卡牌配置文件格式
- [ ] 实现 JSON 解析加载牌库
- [ ] 设计更多卡牌品种（≥10 张）
- **涉及文件**：`DeckManager.cpp`，新增 JSON 解析模块

### 阶段 4：AI 出牌
- [ ] AI 手牌评估逻辑
- [ ] AI 选择出牌时机
- [ ] AI 出牌操作（模拟拖牌到 CardReader）
- **涉及文件**：`AIPlayer.h`, `AIPlayer.cpp`, `GameEngine.cpp`

---

## 开发工作流

### 每次开发前
1. 阅读 `docs/requirements.md` 确认当前需求
2. 检查 `devlog/` 最新日志了解上下文
3. 阅读相关源码文件的现有实现

### 开发中
1. 遵循 `docs/technical-spec.md` 中的编码规范
2. 遵循 `docs/design-spec.md` 中的设计规范
3. 小步提交，每次 commit 聚焦单一改动

### 开发后
1. `cmake --build build` 编译通过
2. 更新 `devlog/YYYY-MM-DD.md` 记录当日完成事项
3. 更新本文档中的 checklist 状态
4. 如有架构/规范变更，更新对应的 docs 文件
