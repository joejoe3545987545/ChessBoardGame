#include "Chessboard.h"
#include <iostream>
#include <cmath>
#include <filesystem> // 确保引入了此头文件

namespace {
    const float STEP           = 36.0f;
    const int GRID_COUNT       = 15;
    const float GRID_SPAN      = (GRID_COUNT - 1) * STEP;
    const float BOARD_WIDTH    = GRID_SPAN + STEP;
    const float BOARD_HEIGHT   = BOARD_WIDTH;
    const float START_X        = (1280.f - BOARD_WIDTH) * 0.5f + STEP * 0.5f;
    const float START_Y        = (720.f - BOARD_HEIGHT) * 0.5f + STEP * 0.5f;

    // 🌟 引入与 GameEngine 一致的路径查找器
    std::string getResolvedPath(const std::string& relativePath) {
        if (std::filesystem::exists(relativePath)) return relativePath;
        std::string fallback = "../" + relativePath;
        if (std::filesystem::exists(fallback)) return fallback;
        return relativePath;
    }
}

Chessboard::Chessboard()
    : boardSprite(boardTexture),
      blackSprite(blackTexture),
      whiteSprite(whiteTexture)
{
    // 1. 加载棋盘底图
    if (boardTexture.loadFromFile(getResolvedPath("assets/board_.png"))) {
        boardTexture.setSmooth(true);
        // 🌟【核心修复】强行重新绑定纹理，并把第二个参数设为 true（重置纹理矩形大小）
        boardSprite.setTexture(boardTexture, true); 
        
        // 打印一下加载成功的图片的真实像素大小，看看是不是 (0,0)
        sf::Vector2u sz = boardTexture.getSize();
        std::cout << "[🎉 Success] Board texture loaded geometry: " << sz.x << "x" << sz.y << std::endl;
    } else {
        std::cerr << "[❌ Error] Cannot load background image texture from: " 
                  << getResolvedPath("assets/board_.png") << std::endl;
    }

    // 2. 加载黑子
    sf::Image blackImage;
    if (blackImage.loadFromFile(getResolvedPath("assets/black.png"))) {
        blackImage.createMaskFromColor(sf::Color(255, 255, 255), 0);
        if (blackTexture.loadFromImage(blackImage)) {
            blackTexture.setSmooth(true);
            blackSprite.setTexture(blackTexture, true);
            sf::Vector2u size = blackTexture.getSize();
            blackSprite.setOrigin({size.x / 2.f, size.y / 2.f});
        }
    }

    // 3. 加载白子（🌟 顺便修复这里：加上 getResolvedPath 确保路径安全）
    sf::Image whiteImage;
    if (whiteImage.loadFromFile(getResolvedPath("assets/white.png"))) {
        whiteImage.createMaskFromColor(sf::Color(255, 255, 255), 0);
        if (whiteTexture.loadFromImage(whiteImage)) {
            whiteTexture.setSmooth(true);
            whiteSprite.setTexture(whiteTexture, true);
            sf::Vector2u size = whiteTexture.getSize();
            whiteSprite.setOrigin({size.x / 2.f, size.y / 2.f});
        }
    }

    reset();
}

Chessboard::~Chessboard() {}

void Chessboard::reset() {
    for (int i = 0; i < 15; ++i) {
        for (int j = 0; j < 15; ++j) {
            grid[i][j] = 0;
        }
    }
    clearWinLine();
    winCondition[1] = 5;  // 黑方默认五子连星
    winCondition[2] = 5;  // 白方默认五子连星

    // 清除历史和回放状态
    moveHistory.clear();
    stopReplay();
}

int Chessboard::checkPattern(int row, int col) {
    int turn = grid[row][col];
    if (turn == 0) return 0;

    int dx[] = {1, 0, 1, 1};
    int dy[] = {0, 1, 1, -1};

    for (int i = 0; i < 4; ++i) {
        int count = 1;
        // 向两个方向统计连子
        int r1 = row + dy[i], c1 = col + dx[i];
        while (r1 >= 0 && r1 < 15 && c1 >= 0 && c1 < 15 && grid[r1][c1] == turn) { count++; r1 += dy[i]; c1 += dx[i]; }
        int r2 = row - dy[i], c2 = col - dx[i];
        while (r2 >= 0 && r2 < 15 && c2 >= 0 && c2 < 15 && grid[r2][c2] == turn) { count++; r2 -= dy[i]; c2 -= dx[i]; }

        if (count == 4) return 4; // 触发四连
        if (count == 3) return 3; // 触发三连
    }
    return 0;
}

int Chessboard::getPiece(int row, int col) const {
    if (row >= 0 && row < 15 && col >= 0 && col < 15) {
        return grid[row][col];
    }
    return 0;
}

void Chessboard::getGridPosition(int row, int col, float& outX, float& outY) const {
    if (row < 0 || row >= 15 || col < 0 || col >= 15) {
        outX = -1000.f;
        outY = -1000.f;
        return;
    }
    outX = START_X + static_cast<float>(col) * STEP;
    outY = START_Y + static_cast<float>(row) * STEP;
}

bool Chessboard::handleMouseClick(sf::Vector2i mousePos, int turn, int& outRow, int& outCol) {
    const float boardBottom = START_Y + GRID_SPAN + STEP * 0.5f;
    if (mousePos.y > boardBottom) return false;  // 超出棋盘区域，不处理

    int col = static_cast<int>(std::round((static_cast<float>(mousePos.x) - START_X) / STEP));
    int row = static_cast<int>(std::round((static_cast<float>(mousePos.y) - START_Y) / STEP));

    if (row >= 0 && row < 15 && col >= 0 && col < 15) {
        float centerX, centerY;
        getGridPosition(row, col, centerX, centerY);

        float distance = std::sqrt(std::pow(mousePos.x - centerX, 2) + std::pow(mousePos.y - centerY, 2));
        const float maxClickDistance = 20.0f;

        if (distance <= maxClickDistance && grid[row][col] == 0) {
            grid[row][col] = turn;
            outRow = row;
            outCol = col;
            return true;
        }
    }
    return false;
}

void Chessboard::placePieceByAI(int row, int col, int turn, int& outRow, int& outCol) {
    if (row >= 0 && row < 15 && col >= 0 && col < 15) {
        grid[row][col] = turn;
        outRow = row;
        outCol = col;
    } else {
        grid[7][7] = turn;
        outRow = 7;
        outCol = 7;
    }
}

// 直接放子（不记录历史、供回放或加载时使用）
void Chessboard::placeDirect(int row, int col, int player) {
    if (row >= 0 && row < 15 && col >= 0 && col < 15) {
        grid[row][col] = player;
    }
}

// ============================================================================
// 🌟 历史记录相关实现
// ============================================================================
void Chessboard::recordMove(int row, int col, int player) {
    if (row < 0 || col < 0) return;
    moveHistory.emplace_back(row, col, player);
}

std::vector<Move> Chessboard::getMoveHistory() const {
    return moveHistory;
}

void Chessboard::applyMoveSequence(const std::vector<Move>& moves) {
    reset();
    // 将 moves 依次直接放置（不再次 record）
    for (const auto& m : moves) {
        placeDirect(m.row, m.col, m.player);
    }
    // 更新胜利线信息（如果最后一步带来胜利）
    if (!moves.empty()) {
        const Move& last = moves.back();
        findWinLine(last.row, last.col);
    }
}

void Chessboard::startReplay(const std::vector<Move>& moves) {
    reset();
    replayMoves = moves;
    replayIndex = 0;
    replaying = true;
}

bool Chessboard::stepReplay() {
    if (!replaying) return false;
    if (replayIndex >= replayMoves.size()) {
        replaying = false;
        return false;
    }
    const Move& m = replayMoves[replayIndex++];
    placeDirect(m.row, m.col, m.player);

    // 每步后可以检查是否产生胜利（保持显示）
    if (replayIndex > 0) {
        const Move& last = replayMoves[replayIndex - 1];
        if (checkWin(last.row, last.col)) {
            // 找到胜利线并保持回放停止或继续（这里保留胜利高亮但继续回放）
        }
    }

    return replayIndex < replayMoves.size();
}

void Chessboard::stopReplay() {
    replayMoves.clear();
    replayIndex = 0;
    replaying = false;
}

bool Chessboard::isReplaying() const {
    return replaying;
}

// ============================================================================
// 🌟 【核心功能】检查胜利并记录胜利线
// ============================================================================
bool Chessboard::checkWin(int row, int col) {
    if (row < 0 || row >= 15 || col < 0 || col >= 15) return false;
    int turn = grid[row][col];
    if (turn == 0) return false;

    int dx[] = {1, 0, 1, 1};
    int dy[] = {0, 1, 1, -1};

    for (int i = 0; i < 4; ++i) {
        int count = 1;
        int r = row + dy[i];
        int c = col + dx[i];

        while (r >= 0 && r < 15 && c >= 0 && c < 15 && grid[r][c] == turn) {
            count++;
            r += dy[i];
            c += dx[i];
        }

        r = row - dy[i];
        c = col - dx[i];

        while (r >= 0 && r < 15 && c >= 0 && c < 15 && grid[r][c] == turn) {
            count++;
            r -= dy[i];
            c -= dx[i];
        }

        if (count >= winCondition[turn]) {
            // 🌟 找到胜利线！记录它的位置
            findWinLine(row, col);
            return true;
        }
    }
    return false;
}

// ============================================================================
// 🌟 【新增】找到胜利线的起点和终点
// ============================================================================
void Chessboard::findWinLine(int row, int col) {
    if (row < 0 || row >= 15 || col < 0 || col >= 15) return;

    int turn = grid[row][col];
    if (turn == 0) return;

    int dx[] = {1, 0, 1, 1};
    int dy[] = {0, 1, 1, -1};

    for (int i = 0; i < 4; ++i) {
        int count = 1;
        int r = row + dy[i];
        int c = col + dx[i];
        int endR = row, endC = col;

        // 向正方向扫描
        while (r >= 0 && r < 15 && c >= 0 && c < 15 && grid[r][c] == turn) {
            count++;
            endR = r;
            endC = c;
            r += dy[i];
            c += dx[i];
        }

        // 向反方向扫描
        r = row - dy[i];
        c = col - dx[i];
        int startR = row, startC = col;

        while (r >= 0 && r < 15 && c >= 0 && c < 15 && grid[r][c] == turn) {
            count++;
            startR = r;
            startC = c;
            r -= dy[i];
            c -= dx[i];
        }

        if (count >= winCondition[turn]) {
            hasWin = true;
            winStartRow = startR;
            winStartCol = startC;
            winEndRow = endR;
            winEndCol = endC;
            std::cout << "[✓] Win line found: (" << startR << "," << startC
                      << ") -> (" << endR << "," << endC << ")" << std::endl;
            return;
        }
    }
}

bool Chessboard::hasWinLine() const {
    return hasWin;
}

void Chessboard::getWinLine(int& startRow, int& startCol, int& endRow, int& endCol) const {
    startRow = winStartRow;
    startCol = winStartCol;
    endRow = winEndRow;
    endCol = winEndCol;
}

void Chessboard::clearWinLine() {
    hasWin = false;
    winStartRow = -1;
    winStartCol = -1;
    winEndRow = -1;
    winEndCol = -1;
}

// ============================================================================
// 绘制棋盘
// ============================================================================
void Chessboard::draw(sf::RenderWindow& window) {
    // 1. 🌟【底图替换】不再使用橘黄色色块，改用你准备好的 boardSprite
    // 计算底图的左上角绝对坐标
    float boardLeftX = START_X - STEP * 0.5f-27.5f;
    float boardTopY = START_Y - STEP * 0.5f-28.0f;
    boardSprite.setPosition({boardLeftX, boardTopY});

    // 🌟【自适应缩放】将底图完美缩放到标准 BOARD_WIDTH 和 BOARD_HEIGHT
    // 这样无论你的 board.png 尺寸是多少，都会严丝合缝地填满你的棋盘区域
    sf::Vector2u boardTextureSize = boardTexture.getSize();
    if (boardTextureSize.x > 0 && boardTextureSize.y > 0) {
        boardSprite.setScale({
            1.1f*(BOARD_WIDTH / static_cast<float>(boardTextureSize.x)), 
            1.1f*(BOARD_HEIGHT / static_cast<float>(boardTextureSize.y))
        });
    }
    // 🌟 渲染背景图
    window.draw(boardSprite);

    // 2. 棋子渲染（保持你原本的精灵图缩放及 grid 判定逻辑）
    for (int i = 0; i < 15; ++i) {
        for (int j = 0; j < 15; ++j) {
            if (grid[i][j] == 1 || grid[i][j] == 2) {
                sf::Sprite& targetSprite = (grid[i][j] == 1) ? blackSprite : whiteSprite;
                sf::Texture& targetTexture = (grid[i][j] == 1) ? blackTexture : whiteTexture;

                sf::Vector2u textureSize = targetTexture.getSize();
                if (textureSize.x > 0 && textureSize.y > 0) {
                    targetSprite.setScale({38.0f / static_cast<float>(textureSize.x), 38.0f / static_cast<float>(textureSize.y)});
                }

                float posX, posY;
                getGridPosition(i, j, posX, posY);

                if (posX >= 0 && posY >= 0) {
                    targetSprite.setPosition({posX, posY});
                    window.draw(targetSprite);
                }
            }
        }
    }

    // 5. 🌟 绘制胜利线高亮（你原本写得非常棒的发光与粗线条特效，完美保留）
    if (hasWin) {
        float startX, startY, endX, endY;
        getGridPosition(winStartRow, winStartCol, startX, startY);
        getGridPosition(winEndRow, winEndCol, endX, endY);

        // 绘制发光的胜利线
        sf::VertexArray winLine(sf::PrimitiveType::Lines);

        // 选择亮丽的颜色（金黄色/红色）
        sf::Color winLineColor(255, 215, 0, 255);  // 金黄色

        winLine.append(sf::Vertex{ .position = {startX, startY}, .color = winLineColor });
        winLine.append(sf::Vertex{ .position = {endX, endY},     .color = winLineColor });

        window.draw(winLine);

        // 绘制更粗的线条效果（通过绘制多条线）
        sf::VertexArray winLineThick(sf::PrimitiveType::Lines);
        float offsetX = (endY - startY) / 5.0f;
        float offsetY = (endX - startX) / 5.0f;

        winLineThick.append(sf::Vertex{ .position = {startX - offsetX, startY + offsetY}, .color = sf::Color(255, 215, 0, 180) });
        winLineThick.append(sf::Vertex{ .position = {endX - offsetX, endY + offsetY},     .color = sf::Color(255, 215, 0, 180) });

        winLineThick.append(sf::Vertex{ .position = {startX + offsetX, startY - offsetY}, .color = sf::Color(255, 215, 0, 180) });
        winLineThick.append(sf::Vertex{ .position = {endX + offsetX, endY - offsetY},     .color = sf::Color(255, 215, 0, 180) });

        window.draw(winLineThick);

        // 在五个连线棋子上绘制圆形高亮
        drawWinLineHighlight(window, winStartRow, winStartCol, winEndRow, winEndCol);
    }
}

// ============================================================================
// 🌟 【辅助】绘制胜利线上的棋子高亮
// ============================================================================
void Chessboard::drawWinLineHighlight(sf::RenderWindow& window, int startRow, int startCol, int endRow, int endCol) {
    // 计算方向向量
    int dr = (endRow > startRow) ? 1 : (endRow < startRow) ? -1 : 0;
    int dc = (endCol > startCol) ? 1 : (endCol < startCol) ? -1 : 0;

    float x, y;
    int r = startRow, c = startCol;

    // 绘制五个连线棋子的高亮圆圈
    for (int i = 0; i < 5; ++i) {
        getGridPosition(r, c, x, y);

        sf::CircleShape highlight(22.f);  // 比棋子略大的圆圈
        highlight.setOrigin({22.f, 22.f});
        highlight.setPosition({x, y});
        highlight.setFillColor(sf::Color(0, 0, 0, 0));  // 透明填充
        highlight.setOutlineColor(sf::Color(255, 215, 0, 200));  // 金黄色边框
        highlight.setOutlineThickness(3.f);

        window.draw(highlight);

        // 移动到下一个棋子
        r += dr;
        c += dc;
    }
}

// ============================================================================
// 🌟【新增】实现距离检测与预落子圆环绘制
// ============================================================================
// ============================================================================
// 🌟【完美适配全屏】实现距离检测与预落子圆环绘制
// ============================================================================
void Chessboard::drawHoverRing(sf::RenderWindow& window, int currentTurn) {
    // 1. 🌟【核心修复】：先获取鼠标的设备屏幕像素坐标
    sf::Vector2i mousePixelPos = sf::Mouse::getPosition(window);
    
    // 2. 🌟【核心修复】：通过 mapPixelToCoords 将全屏拉伸后的像素坐标，转换为游戏原本 1280x720 的标准逻辑坐标
    sf::Vector2f mousePos = window.mapPixelToCoords(mousePixelPos);

    // 重置悬停状态
    hoverGridPos = sf::Vector2i(-1, -1);

    // 3. 遍历 15x15 棋盘的所有交点
    for (int i = 0; i < GRID_COUNT; ++i) {
        for (int j = 0; j < GRID_COUNT; ++j) {
            float centerX, centerY;
            getGridPosition(i, j, centerX, centerY);

            // 🌟 此时 mousePos.x 和 mousePos.y 已经是完美的逻辑坐标，与 centerX/Y 完美匹配！
            float distance = std::sqrt(std::pow(mousePos.x - centerX, 2) + std::pow(mousePos.y - centerY, 2));

            // 如果距离小于 5 像素（全屏后建议可以改到 10.0f 或 12.0f，体验会更丝滑），且无子
            if (distance < 18.0f && grid[i][j] == 0) {
                hoverGridPos = sf::Vector2i(i, j);
                break;
            }
        }
        if (hoverGridPos.x != -1) break;
    }

    // 4. 如果成功捕捉到了满足要求的预落子点，开始上色渲染
    if (hoverGridPos.x != -1) {
        float ringRadius = 10.0f; // 保持你喜欢的精致小尺寸
        sf::CircleShape hoverRing(ringRadius);
        
        // 将原点设置为圆心，方便居中对齐交点坐标
        hoverRing.setOrigin({ringRadius, ringRadius});
        
        // 获取这个锁定点的真实坐标并固定圆环位置
        float ringX, ringY;
        getGridPosition(hoverGridPos.x, hoverGridPos.y, ringX, ringY);
        hoverRing.setPosition({ringX, ringY});

        // 🌟【核心修改 1】：里面填灰色（无论如何都是同一个色）
        // 这里采用灰色 (128, 128, 128)，并加上 180 的半透明度(Alpha通道)
        // 这样既有灰色质感，又不会完全死板地盖住底层的棋盘十字线
        hoverRing.setFillColor(sf::Color(128, 128, 128, 180)); 

        // 设置外边框粗细
        hoverRing.setOutlineThickness(1.5f); 

        // 🌟【核心修改 2】：外面画个环，颜色由玩家执子决定
        if (currentTurn == 1) {
            hoverRing.setOutlineColor(sf::Color::White); // 玩家执黑，外圈显现白色
        } else {
            hoverRing.setOutlineColor(sf::Color::Black); // 玩家执白，外圈显现黑色
        }

        // 将处理好的高级预落子点最终渲染到屏幕上
        window.draw(hoverRing);
    }
}

void Chessboard::setWinCondition(int player, int cond) {
    if (player >= 1 && player <= 2) {
        winCondition[player] = cond;
    }
}

int Chessboard::getWinCondition(int player) const {
    if (player >= 1 && player <= 2) {
        return winCondition[player];
    }
    return 5;
}
