#ifndef CHESSBOARD_H
#define CHESSBOARD_H

#include <SFML/Graphics.hpp>
#include <utility>
#include <vector>

// 一个简单的 Move 结构：行、列、执子（1 黑、2 白）
struct Move {
    int row;
    int col;
    int player;
    Move() : row(-1), col(-1), player(0) {}
    Move(int r, int c, int p) : row(r), col(c), player(p) {}
};

class Chessboard {
public:
    Chessboard();
    ~Chessboard();

    void reset();
    int checkPattern(int row, int col, int& outDir);  // outDir=触发方向索引, -1=无
    void markPatternUsed(int row, int col, int dir);   // 沿方向标记连子为已使用
    int getPiece(int row, int col) const;
    bool handleMouseClick(sf::Vector2i mousePos, int turn, int& outRow, int& outCol);
    void placePieceByAI(int row, int col, int turn, int& outRow, int& outCol);
    bool checkWin(int row, int col);
    void setWinCondition(int player, int cond);  // 设置某方胜利条件（默认 5）
    int  getWinCondition(int player) const;
    void draw(sf::RenderTarget& target);

    // 将棋盘网格坐标转换为像素坐标（外部也可用）
    void getGridPosition(int row, int col, float& outX, float& outY) const;

    // 获取胜利连线信息（外部查询用）
    void getWinLine(int& startRow, int& startCol, int& endRow, int& endCol) const;
    bool hasWinLine() const;
    void clearWinLine();

    // ================= 历史与复盘接口 =================
    // 在每次成功落子（玩家或 AI）后由外部调用记录
    void recordMove(int row, int col, int player);

    // 返回当前对局的完整 move 列表（按先后）
    std::vector<Move> getMoveHistory() const;

    // 直接用一系列 moves 初始化棋盘（用于加载复盘 - 不再记录为新历史）
    void applyMoveSequence(const std::vector<Move>& moves);

    // 回放控制：启动回放（清空棋盘并准备回放），单步回放，停止回放
    void startReplay(const std::vector<Move>& moves);
    // stepReplay 执行下一步并返回 true 表示还有下一步，false 表示回放完成
    bool stepReplay();
    void stopReplay();
    bool isReplaying() const;
    void drawHoverRing(sf::RenderWindow& window, int currentTurn);
    void placeDirect(int row, int col, int player);

    // 棋子动画（销毁/转化，可复用）
    void startDestroyAnim(int row, int col);
    void startConvertAnim(int row, int col, int fromColor, int toColor);
    bool isPieceAnimating() const;
    bool updatePieceAnim();   // true=本帧刚结束
    void finishPieceAnim();   // 清除动画状态（grid 更新后调用）
    int  animRow = -1, animCol = -1;

private:
    int grid[15][15];
    int usedMask[15][15] = {};  // 每位对应一个方向: 0=H 1=V 2=↘ 3=↗

    sf::Texture boardTexture;
    sf::Texture blackTexture;
    sf::Texture whiteTexture;

    sf::Sprite boardSprite;
    sf::Sprite blackSprite;
    sf::Sprite whiteSprite;

    // 胜利条件（每方独立，默认 5 子连星）
    int winCondition[3] = {0, 5, 5};  // [0] unused, [1]=黑, [2]=白

    // 胜利连线相关状态
    bool hasWin = false;
    int winStartRow = -1;
    int winStartCol = -1;
    int winEndRow = -1;
    int winEndCol = -1;

    // 找到以 (row,col) 为一员的那条胜利连线（若有），并记录端点
    void findWinLine(int row, int col);

    // 在 draw() 中调用，用于绘制胜利连线上每个棋子的高亮
    void drawWinLineHighlight(sf::RenderTarget& target, int startRow, int startCol, int endRow, int endCol);

    // ================= 历史数据 =================
    std::vector<Move> moveHistory;

    // replay state
    std::vector<Move> replayMoves;
    size_t replayIndex = 0;
    bool replaying = false;
    sf::Vector2i hoverGridPos = sf::Vector2i(-1, -1);

    // 棋子动画状态
    int  pieceAnimType = 0;       // 0=无, 1=销毁, 2=转化
    int  pieceAnimFrom = 0;       // 原颜色
    int  pieceAnimTo = 0;         // 目标颜色
    bool pieceAnimJustDone = false;
    sf::Clock pieceAnimClock;
};

#endif // CHESSBOARD_H
