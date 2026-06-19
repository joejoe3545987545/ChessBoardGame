#include "Chessboard.h"
#include <iostream>
#include <cmath>
#include <filesystem> // 确保引入了此头文件

namespace {
    const float STEP           = 72.0f;
    const int GRID_COUNT       = 15;
    const float GRID_SPAN      = (GRID_COUNT - 1) * STEP;
    const float BOARD_WIDTH    = GRID_SPAN + STEP;
    const float BOARD_HEIGHT   = BOARD_WIDTH;
    const float START_X        = (2560.f - BOARD_WIDTH) * 0.5f + STEP * 0.5f;
    const float START_Y        = (1440.f - BOARD_HEIGHT) * 0.5f + STEP * 0.5f;

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
            blackTexture.setSmooth(false);
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
            whiteTexture.setSmooth(false);
            whiteSprite.setTexture(whiteTexture, true);
            sf::Vector2u size = whiteTexture.getSize();
            whiteSprite.setOrigin({size.x / 2.f, size.y / 2.f});
        }
    }

    initFragmentCache();
    reset();
}

Chessboard::~Chessboard() {}

// ── 预切分棋子纹理为 8×8 随机碎片 ──
void Chessboard::initFragmentCache() {
    auto genFragments = [](sf::Texture& tex, std::vector<Fragment>& out) {
        out.clear();
        sf::Vector2u ts = tex.getSize();
        if (ts.x == 0 || ts.y == 0) return;
        const int COLS = 12, ROWS = 12;
        float cw = (float)ts.x / COLS;
        float rh = (float)ts.y / ROWS;
        for (int r = 0; r < ROWS; ++r) {
            for (int c = 0; c < COLS; ++c) {
                Fragment f;
                // 基础网格 + 随机扰动（Y方向收紧保持逐行擦除线清晰）
                int rx = (int)(cw * c + cw * (rand() % 16 - 8) / 100.f);
                int ry = (int)(rh * r + rh * (rand() % 8  - 4) / 100.f);
                int rw = (int)(cw + cw * (rand() % 14 - 7) / 100.f);
                int rh2 = (int)(rh + rh * (rand() % 8  - 4) / 100.f);
                // 夹紧到纹理边界
                if (rx < 0) rx = 0;
                if (ry < 0) ry = 0;
                if (rx + rw > (int)ts.x) rw = ts.x - rx;
                if (ry + rh2 > (int)ts.y) rh2 = ts.y - ry;
                if (rw < 1) rw = 1;
                if (rh2 < 1) rh2 = 1;
                f.texRect = sf::IntRect({rx, ry}, {rw, rh2});
                // 随机物理参数（运行时赋值）
                f.vel = {0.f, 0.f};
                f.rotSpeed = 0.f;
                f.released = false;
                f.alpha = 255.f;
                f.rotation = 0.f;
                f.pos = {0.f, 0.f};
                out.push_back(f);
            }
        }
    };
    genFragments(blackTexture, blackFragments);
    genFragments(whiteTexture, whiteFragments);
    std::cout << "[Fragment] 碎片缓存已生成: 黑=" << blackFragments.size()
              << " 白=" << whiteFragments.size() << std::endl;
}

void Chessboard::reset() {
    for (int i = 0; i < 15; ++i) {
        for (int j = 0; j < 15; ++j) {
            grid[i][j] = 0;
            usedMask[i][j] = 0;
        }
    }
    clearWinLine();
    winCondition[1] = 5;
    winCondition[2] = 5;
    pieceAnimType = 0;     // 清除棋子动画
    animRow = animCol = -1;
    dropAnimating = false;   // 清除落子动画
    activeFragments.clear();
    convertFragments.clear();
    decayFragments.clear();
    decayColor = 0;
    fragLastElapsed = 0.f;
    convFragLastElapsed = 0.f;

    // 清除历史和回放状态
    moveHistory.clear();
    stopReplay();
}

int Chessboard::checkPattern(int row, int col, int& outDir) {
    int turn = grid[row][col];
    outDir = -1;
    if (turn == 0) return 0;

    int dx[] = {1, 0, 1, 1};
    int dy[] = {0, 1, 1, -1};
    for (int i = 0; i < 4; ++i) {
        // 沿该方向收集所有连续同色棋子
        std::vector<std::pair<int,int>> line;
        for (int r = row, c = col; r >= 0 && r < 15 && c >= 0 && c < 15 && grid[r][c] == turn; r += dy[i], c += dx[i])
            line.push_back({r, c});
        // 去掉当前棋子（前面已加），换反方向
        // 实际上上面的循环已包含当前棋子，直接整理：从一端到另一端扫描

        int count = 1;
        int r1 = row + dy[i], c1 = col + dx[i];
        while (r1 >= 0 && r1 < 15 && c1 >= 0 && c1 < 15 && grid[r1][c1] == turn) { count++; r1 += dy[i]; c1 += dx[i]; }
        int r2 = row - dy[i], c2 = col - dx[i];
        while (r2 >= 0 && r2 < 15 && c2 >= 0 && c2 < 15 && grid[r2][c2] == turn) { count++; r2 -= dy[i]; c2 -= dx[i]; }

        if (count >= 3) {
            // 搜索是否存在 3 连段全部未被该方向标记
            int bit = (1 << i);
            // 找线段起点（最末端）
            int sr = row, sc = col;
            while (true) {
                int nr = sr - dy[i], nc = sc - dx[i];
                if (nr >= 0 && nr < 15 && nc >= 0 && nc < 15 && grid[nr][nc] == turn)
                    { sr = nr; sc = nc; }
                else break;
            }
            // 从起点扫描，检查每个连续 3 段
            int cr = sr, cc = sc;
            while (cr >= 0 && cr < 15 && cc >= 0 && cc < 15 && grid[cr][cc] == turn) {
                // 取 cr,cc 开始的连续 3 子
                int r[3], c[3]; bool valid = true;
                for (int k = 0; k < 3; ++k) {
                    int nr = cr + k * dy[i], nc = cc + k * dx[i];
                    if (nr < 0 || nr >= 15 || nc < 0 || nc >= 15 || grid[nr][nc] != turn)
                        { valid = false; break; }
                    r[k] = nr; c[k] = nc;
                }
                if (valid) {
                    bool allUnmarked = true;
                    for (int k = 0; k < 3; ++k)
                        if (usedMask[r[k]][c[k]] & bit) { allUnmarked = false; break; }
                    if (allUnmarked) {
                        outDir = i;
                        return (count >= 3) ? ((count >= 4) ? 4 : 3) : 0;
                    }
                }
                cr += dy[i]; cc += dx[i];
            }
        }
    }
    return 0;
}

void Chessboard::markPatternUsed(int row, int col, int dir) {
    if (dir < 0 || dir >= 4) return;
    int turn = grid[row][col];
    if (turn == 0) return;
    int dx[] = {1, 0, 1, 1};
    int dy[] = {0, 1, 1, -1};
    int bit = (1 << dir);

    // 找线段起点
    int sr = row, sc = col;
    while (true) {
        int nr = sr - dy[dir], nc = sc - dx[dir];
        if (nr >= 0 && nr < 15 && nc >= 0 && nc < 15 && grid[nr][nc] == turn)
            { sr = nr; sc = nc; }
        else break;
    }
    // 从起点扫描，找到第一个全未标记的 3 连段，标记之
    int cr = sr, cc = sc;
    while (cr >= 0 && cr < 15 && cc >= 0 && cc < 15 && grid[cr][cc] == turn) {
        int r[3], c[3]; bool valid = true;
        for (int k = 0; k < 3; ++k) {
            int nr = cr + k * dy[dir], nc = cc + k * dx[dir];
            if (nr < 0 || nr >= 15 || nc < 0 || nc >= 15 || grid[nr][nc] != turn)
                { valid = false; break; }
            r[k] = nr; c[k] = nc;
        }
        if (valid) {
            bool allUnmarked = true;
            for (int k = 0; k < 3; ++k)
                if (usedMask[r[k]][c[k]] & bit) { allUnmarked = false; break; }
            if (allUnmarked) {
                for (int k = 0; k < 3; ++k)
                    usedMask[r[k]][c[k]] |= bit;
                return;
            }
        }
        cr += dy[dir]; cc += dx[dir];
    }
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
        const float maxClickDistance = 40.0f;

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
void Chessboard::draw(sf::RenderTarget& target) {
    // 1. 🌟 棋盘底图（中心原点，方便缩放校准）
    sf::Vector2u boardTextureSize = boardTexture.getSize();
    if (boardTextureSize.x > 0 && boardTextureSize.y > 0) {
        boardSprite.setOrigin({boardTextureSize.x / 2.f, boardTextureSize.y / 2.f});
        boardSprite.setScale({
            1.315f * BOARD_WIDTH  / static_cast<float>(boardTextureSize.x),
            1.3f * BOARD_HEIGHT / static_cast<float>(boardTextureSize.y)
        });
    }
    float boardCenterX = START_X + GRID_SPAN * 0.5f + 0.0f;
    float boardCenterY = START_Y + GRID_SPAN * 0.5f + 14.f;
    boardSprite.setPosition({boardCenterX, boardCenterY});
    target.draw(boardSprite);

    // 2. 棋子渲染（含动画效果）
    for (int i = 0; i < 15; ++i) {
        for (int j = 0; j < 15; ++j) {
            bool hasDecay = (i == animRow && j == animCol && pieceAnimType == 0 &&
                             (!activeFragments.empty() || !convertFragments.empty()));
            bool isAnimPiece = hasDecay || (i == animRow && j == animCol && pieceAnimType != 0);
            int displayColor = grid[i][j];
            if (displayColor == 0 && !isAnimPiece) continue;
            // 落子动画期间跳过该格的静态棋子（由 drawDropAnim 绘制漂浮棋子）
            if (i == dropAnimRow && j == dropAnimCol && dropAnimating) continue;

            // 获取动画进度
            float t = 0.f;
            if (isAnimPiece) {
                float raw = pieceAnimClock.getElapsedTime().asSeconds() / 1.0f;
                if (raw > 1.f) raw = 1.f;
                t = raw * raw * (3.f - 2.f * raw); // smoothstep 缓进缓出
            }

            auto drawPiece = [&](int color, float clipHeight, uint8_t alpha = 255) {
                if (color == 0 || alpha == 0) return;
                sf::Sprite& sp = (color == 1) ? blackSprite : whiteSprite;
                sf::Texture& tx = (color == 1) ? blackTexture : whiteTexture;
                sf::Vector2u ts = tx.getSize();
                if (ts.x == 0 || ts.y == 0) return;
                sp.setScale({68.4f / (float)ts.x, 68.4f / (float)ts.y});
                float px, py;
                getGridPosition(i, j, px, py);
                sp.setPosition({px, py});
                int ch = (int)(ts.y * clipHeight);
                sp.setTextureRect(sf::IntRect({0, 0}, {(int)ts.x, ch}));
                sp.setColor(sf::Color(255, 255, 255, alpha));
                target.draw(sp);
                sp.setTextureRect(sf::IntRect({0, 0}, {(int)ts.x, (int)ts.y}));
                sp.setColor(sf::Color(255, 255, 255, 255)); // 恢复
            };

            if (isAnimPiece) {
                if (pieceAnimType == 1) {
                    // ── 🌟 碎片销毁动画 ──
                    sf::Sprite& sp = (pieceAnimFrom == 1) ? blackSprite : whiteSprite;
                    sf::Texture& tx = (pieceAnimFrom == 1) ? blackTexture : whiteTexture;
                    sf::Vector2u ts = tx.getSize();
                    if (ts.x > 0 && ts.y > 0 && !activeFragments.empty()) {
                        float scaleX = 68.4f / (float)ts.x;
                        float scaleY = 68.4f / (float)ts.y;
                        // 擦除线（纹理空间，自下往上）
                        float clipHeight = ts.y * (1.f - t);
                        float px, py;
                        getGridPosition(i, j, px, py);

                        // 1. 画未粉碎部分：完整裁剪纹理，无缝
                        int clipH = (int)clipHeight;
                        if (clipH > 0) {
                            sp.setOrigin({ts.x / 2.f, ts.y / 2.f});
                            sp.setScale({scaleX, scaleY});
                            sp.setTextureRect(sf::IntRect({0, 0}, {(int)ts.x, clipH}));
                            sp.setPosition({px, py});
                            sp.setColor(sf::Color(255, 255, 255, 255));
                            target.draw(sp);
                        }

                        // 2. dt 计算
                        float rawElapsed = pieceAnimClock.getElapsedTime().asSeconds();
                        float dt = rawElapsed - fragLastElapsed;
                        if (dt < 0.f) dt = 0.f;
                        if (dt > 0.1f) dt = 0.016f;
                        fragLastElapsed = rawElapsed;

                        // 3. 遍历碎片：标记释放 + 物理更新 + 绘制已释放碎片
                        for (auto& frag : activeFragments) {
                            if (!frag.released) {
                                float fragTop = (float)frag.texRect.position.y;
                                if (fragTop > clipHeight) {
                                    frag.released = true;
                                    float fcx = frag.texRect.size.x / 2.f;
                                    float fcy = frag.texRect.size.y / 2.f;
                                    frag.pos.x = px + (frag.texRect.position.x + fcx - ts.x / 2.f) * scaleX;
                                    frag.pos.y = py + (frag.texRect.position.y + fcy - ts.y / 2.f) * scaleY;
                                }
                            }
                            if (frag.released) {
                                // 物理更新
                                frag.vel.y += 800.f * dt;
                                frag.pos += frag.vel * dt;
                                frag.rotation += frag.rotSpeed * dt;
                                frag.fadeTimer += dt;
                                float fp = frag.fadeTimer / 0.85f;
                                if (fp > 1.f) fp = 1.f;
                                frag.alpha = 255.f * (1.f - fp * fp * (3.f - 2.f * fp));
                                // 绘制
                                float fcx = frag.texRect.size.x / 2.f;
                                float fcy = frag.texRect.size.y / 2.f;
                                sp.setOrigin({fcx, fcy});
                                sp.setTextureRect(frag.texRect);
                                sp.setPosition(frag.pos);
                                sp.setRotation(sf::degrees(frag.rotation));
                                sp.setColor(sf::Color(255, 255, 255, (uint8_t)frag.alpha));
                                target.draw(sp);
                            }
                        }
                        // 恢复精灵默认状态
                        sp.setOrigin({ts.x / 2.f, ts.y / 2.f});
                        sp.setTextureRect(sf::IntRect({0, 0}, {(int)ts.x, (int)ts.y}));
                        sp.setRotation(sf::degrees(0.f));
                        sp.setColor(sf::Color(255, 255, 255, 255));
                    }
                } else if (pieceAnimType == 2) {
                    // ── 转化动画：旧色粉碎 + 新色自下往上生长 ──
                    sf::Sprite& spFrom = (pieceAnimFrom == 1) ? blackSprite : whiteSprite;
                    sf::Texture& txFrom = (pieceAnimFrom == 1) ? blackTexture : whiteTexture;
                    sf::Sprite& spTo   = (pieceAnimTo   == 1) ? blackSprite : whiteSprite;
                    sf::Texture& txTo   = (pieceAnimTo   == 1) ? blackTexture : whiteTexture;
                    sf::Vector2u ts = txFrom.getSize();
                    sf::Vector2u tsTo = txTo.getSize();
                    if (ts.x > 0 && ts.y > 0 && tsTo.x > 0 && tsTo.y > 0 &&
                        !activeFragments.empty()) {
                        float wipeFrac = 1.f - t;
                        float clipFrom = ts.y * wipeFrac;
                        float clipTo   = tsTo.y * wipeFrac;
                        float scaleX = 68.4f / (float)ts.x;
                        float scaleY = 68.4f / (float)ts.y;
                        float toScaleX = 68.4f / (float)tsTo.x;
                        float toScaleY = 68.4f / (float)tsTo.y;
                        float px, py;
                        getGridPosition(i, j, px, py);
                        float rawElapsed = pieceAnimClock.getElapsedTime().asSeconds();
                        float dt = rawElapsed - fragLastElapsed;
                        if (dt < 0.f) dt = 0.f; if (dt > 0.1f) dt = 0.016f;
                        fragLastElapsed = rawElapsed;

                        // ── 底层：新色，自下往上生长（origin=可见底部中心，position=格子底部）──
                        float visH = tsTo.y - clipTo;
                        if (visH > 0.f) {
                            spTo.setOrigin({tsTo.x / 2.f, visH}); // 可见区域底部中心
                            spTo.setScale({toScaleX, toScaleY});
                            spTo.setTextureRect(sf::IntRect({0, (int)clipTo}, {(int)tsTo.x, (int)visH}));
                            spTo.setPosition({px, py + tsTo.y / 2.f * toScaleY}); // 格子底部
                            spTo.setColor(sf::Color(255, 255, 255, 255));
                            target.draw(spTo);
                        }
                        spTo.setOrigin({tsTo.x / 2.f, tsTo.y / 2.f});
                        spTo.setTextureRect(sf::IntRect({0, 0}, {(int)tsTo.x, (int)tsTo.y}));

                        // ── 上层：旧色粉碎（同销毁动画）──
                        int clipH = (int)clipFrom;
                        if (clipH > 0) {
                            spFrom.setOrigin({ts.x / 2.f, ts.y / 2.f});
                            spFrom.setScale({scaleX, scaleY});
                            spFrom.setTextureRect(sf::IntRect({0, 0}, {(int)ts.x, clipH}));
                            spFrom.setPosition({px, py});
                            spFrom.setColor(sf::Color(255, 255, 255, 255));
                            target.draw(spFrom);
                        }
                        for (auto& frag : activeFragments) {
                            if (!frag.released) {
                                if ((float)frag.texRect.position.y > clipFrom) {
                                    frag.released = true;
                                    float fcx = frag.texRect.size.x / 2.f;
                                    float fcy = frag.texRect.size.y / 2.f;
                                    frag.pos.x = px + (frag.texRect.position.x + fcx - ts.x / 2.f) * scaleX;
                                    frag.pos.y = py + (frag.texRect.position.y + fcy - ts.y / 2.f) * scaleY;
                                }
                            }
                            if (frag.released) {
                                frag.vel.y += 800.f * dt;
                                frag.pos += frag.vel * dt;
                                frag.rotation += frag.rotSpeed * dt;
                                frag.fadeTimer += dt;
                                float fp = frag.fadeTimer / 0.85f;
                                if (fp > 1.f) fp = 1.f;
                                frag.alpha = 255.f * (1.f - fp * fp * (3.f - 2.f * fp));
                                float fcx = frag.texRect.size.x / 2.f;
                                float fcy = frag.texRect.size.y / 2.f;
                                spFrom.setOrigin({fcx, fcy});
                                spFrom.setTextureRect(frag.texRect);
                                spFrom.setPosition(frag.pos);
                                spFrom.setRotation(sf::degrees(frag.rotation));
                                spFrom.setColor(sf::Color(255, 255, 255, (uint8_t)frag.alpha));
                                target.draw(spFrom);
                            }
                        }
                        spFrom.setTextureRect(sf::IntRect({0, 0}, {(int)ts.x, (int)ts.y}));
                        spFrom.setOrigin({ts.x / 2.f, ts.y / 2.f});
                        spFrom.setRotation(sf::degrees(0.f));
                        spFrom.setColor(sf::Color(255, 255, 255, 255));

                        // ── 新色聚合碎片（旧色之后画，在旧色完整部分之后但在旧碎片之下）──
                        float dtConv = rawElapsed - convFragLastElapsed;
                        if (dtConv < 0.f) dtConv = 0.f; if (dtConv > 0.1f) dtConv = 0.016f;
                        convFragLastElapsed = rawElapsed;
                        float releaseZone = tsTo.y * 0.25f;
                        for (auto& frag : convertFragments) {
                            float fcx = frag.texRect.size.x / 2.f;
                            float fcy = frag.texRect.size.y / 2.f;
                            float tx = px + (frag.texRect.position.x + fcx - tsTo.x / 2.f) * toScaleX;
                            float ty = py + (frag.texRect.position.y + fcy - tsTo.y / 2.f) * toScaleY;
                            float fragBot = frag.texRect.position.y + frag.texRect.size.y;
                            if (!frag.released && fragBot > clipTo && fragBot - clipTo < releaseZone) {
                                frag.released = true;
                                frag.targetPos = {tx, ty};
                                frag.assembleTimer = 0.f;
                            }
                            if (frag.released) {
                                frag.assembleTimer += dtConv;
                                float prog = frag.assembleTimer / 0.35f;
                                if (prog > 1.f) prog = 1.f;
                                float eased = prog * prog * (3.f - 2.f * prog);
                                frag.pos.x += (frag.targetPos.x - frag.pos.x) * 0.12f;
                                frag.pos.y += (frag.targetPos.y - frag.pos.y) * 0.12f;
                                frag.alpha = 255.f * eased;
                                spTo.setOrigin({fcx, fcy});
                                spTo.setTextureRect(frag.texRect);
                                spTo.setPosition(frag.pos);
                                spTo.setRotation(sf::degrees(frag.rotation * (1.f - eased)));
                                spTo.setColor(sf::Color(255, 255, 255, (uint8_t)frag.alpha));
                                target.draw(spTo);
                            }
                        }
                        spTo.setOrigin({tsTo.x / 2.f, tsTo.y / 2.f});
                        spTo.setTextureRect(sf::IntRect({0, 0}, {(int)tsTo.x, (int)tsTo.y}));
                        spTo.setRotation(sf::degrees(0.f));
                        spTo.setColor(sf::Color(255, 255, 255, 255));
                    }
                }
            } else if (hasDecay) {
                // ── 碎片残留衰减：动画结束但碎片继续下落/飞入直到自然淡出 ──
                float rawD = pieceAnimClock.getElapsedTime().asSeconds();
                float dtD = rawD - fragLastElapsed;
                if (dtD < 0.f) dtD = 0.f; if (dtD > 0.1f) dtD = 0.016f;
                fragLastElapsed = rawD;
                bool allDone = true;
                // 粉碎碎片（旧色）
                if (!activeFragments.empty()) {
                    sf::Sprite& sp = (pieceAnimFrom == 1) ? blackSprite : whiteSprite;
                    sf::Texture& tx = (pieceAnimFrom == 1) ? blackTexture : whiteTexture;
                    sf::Vector2u tsD = tx.getSize();
                    if (tsD.x > 0 && tsD.y > 0) {
                        for (auto& frag : activeFragments) {
                            if (frag.alpha <= 0.f) continue;
                            allDone = false;
                            frag.vel.y += 800.f * dtD;
                            frag.pos += frag.vel * dtD;
                            frag.rotation += frag.rotSpeed * dtD;
                            frag.fadeTimer += dtD;
                            float fp2 = frag.fadeTimer / 0.85f;
                            if (fp2 > 1.f) fp2 = 1.f;
                            frag.alpha = 255.f * (1.f - fp2 * fp2 * (3.f - 2.f * fp2));
                            float fcx = frag.texRect.size.x / 2.f;
                            float fcy = frag.texRect.size.y / 2.f;
                            sp.setOrigin({fcx, fcy});
                            sp.setTextureRect(frag.texRect);
                            sp.setPosition(frag.pos);
                            sp.setRotation(sf::degrees(frag.rotation));
                            sp.setColor(sf::Color(255, 255, 255, (uint8_t)frag.alpha));
                            target.draw(sp);
                        }
                        sp.setOrigin({tsD.x / 2.f, tsD.y / 2.f});
                        sp.setTextureRect(sf::IntRect({0, 0}, {(int)tsD.x, (int)tsD.y}));
                        sp.setRotation(sf::degrees(0.f));
                        sp.setColor(sf::Color(255, 255, 255, 255));
                    }
                }
                if (allDone) {
                    activeFragments.clear();
                    convertFragments.clear();
                    animRow = animCol = -1;
                }
            } else {
                drawPiece(displayColor, 1.f);
            }
        }
    }

    // ── 残留衰减碎片（无网格位置，独立物理播放直到淡出）──
    if (!decayFragments.empty()) {
        sf::Sprite& spD = (decayColor == 1) ? blackSprite : whiteSprite;
        sf::Texture& txD = (decayColor == 1) ? blackTexture : whiteTexture;
        sf::Vector2u tsD = txD.getSize();
        if (tsD.x > 0 && tsD.y > 0) {
            for (size_t di = 0; di < decayFragments.size(); ) {
                auto& frag = decayFragments[di];
                if (frag.alpha > 0.f) {
                    // 继续物理更新（固定 0.016s dt）
                    frag.vel.y += 800.f * 0.016f;
                    frag.pos += frag.vel * 0.016f;
                    frag.fadeTimer += 0.016f;
                    float fp3 = frag.fadeTimer / 0.85f;
                    if (fp3 > 1.f) fp3 = 1.f;
                    frag.alpha = 255.f * (1.f - fp3 * fp3 * (3.f - 2.f * fp3));
                }
                if (frag.alpha <= 0.f) {
                    decayFragments.erase(decayFragments.begin() + di);
                    continue;
                }
                float fcx = frag.texRect.size.x / 2.f;
                float fcy = frag.texRect.size.y / 2.f;
                spD.setOrigin({fcx, fcy});
                spD.setTextureRect(frag.texRect);
                spD.setPosition(frag.pos);
                spD.setColor(sf::Color(255, 255, 255, (uint8_t)frag.alpha));
                target.draw(spD);
                ++di;
            }
            spD.setOrigin({tsD.x / 2.f, tsD.y / 2.f});
            spD.setTextureRect(sf::IntRect({0, 0}, {(int)tsD.x, (int)tsD.y}));
            spD.setColor(sf::Color(255, 255, 255, 255));
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

        target.draw(winLine);

        // 绘制更粗的线条效果（通过绘制多条线）
        sf::VertexArray winLineThick(sf::PrimitiveType::Lines);
        float offsetX = (endY - startY) / 5.0f;
        float offsetY = (endX - startX) / 5.0f;

        winLineThick.append(sf::Vertex{ .position = {startX - offsetX, startY + offsetY}, .color = sf::Color(255, 215, 0, 180) });
        winLineThick.append(sf::Vertex{ .position = {endX - offsetX, endY + offsetY},     .color = sf::Color(255, 215, 0, 180) });

        winLineThick.append(sf::Vertex{ .position = {startX + offsetX, startY - offsetY}, .color = sf::Color(255, 215, 0, 180) });
        winLineThick.append(sf::Vertex{ .position = {endX + offsetX, endY - offsetY},     .color = sf::Color(255, 215, 0, 180) });

        target.draw(winLineThick);

        // 在五个连线棋子上绘制圆形高亮
        drawWinLineHighlight(target, winStartRow, winStartCol, winEndRow, winEndCol);
    }
}

// ============================================================================
// 🌟 【辅助】绘制胜利线上的棋子高亮
// ============================================================================
void Chessboard::drawWinLineHighlight(sf::RenderTarget& target, int startRow, int startCol, int endRow, int endCol) {
    // 计算方向向量
    int dr = (endRow > startRow) ? 1 : (endRow < startRow) ? -1 : 0;
    int dc = (endCol > startCol) ? 1 : (endCol < startCol) ? -1 : 0;

    float x, y;
    int r = startRow, c = startCol;

    // 绘制五个连线棋子的高亮圆圈
    for (int i = 0; i < 5; ++i) {
        getGridPosition(r, c, x, y);

        sf::CircleShape highlight(44.f);
        highlight.setOrigin({44.f, 44.f});
        highlight.setPosition({x, y});
        highlight.setFillColor(sf::Color(0, 0, 0, 0));  // 透明填充
        highlight.setOutlineColor(sf::Color(255, 215, 0, 200));  // 金黄色边框
        highlight.setOutlineThickness(6.f);

        target.draw(highlight);

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
            if (distance < 36.0f && grid[i][j] == 0) {
                hoverGridPos = sf::Vector2i(i, j);
                break;
            }
        }
        if (hoverGridPos.x != -1) break;
    }

    // 4. 如果成功捕捉到了满足要求的预落子点，开始上色渲染
    if (hoverGridPos.x != -1) {
        float ringRadius = 20.0f;
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
        hoverRing.setOutlineThickness(3.f); 

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

// ── 棋子动画 ──
void Chessboard::startDestroyAnim(int row, int col) {
    // 将旧粉碎碎片移到衰减缓冲区（聚合碎片直接清除）
    for (auto& f : activeFragments)
        if (f.alpha > 0.f) decayFragments.push_back(f);
    if (!activeFragments.empty()) decayColor = pieceAnimFrom;
    activeFragments.clear();
    convertFragments.clear();

    animRow = row; animCol = col;
    pieceAnimType = 1;
    pieceAnimFrom = grid[row][col];
    pieceAnimJustDone = false;
    pieceAnimClock.restart();
    fragLastElapsed = 0.f;
    // 从缓存复制碎片并重置运行时状态
    const auto& cache = (pieceAnimFrom == 1) ? blackFragments : whiteFragments;
    activeFragments = cache; // 复制整个布局
    float px, py;
    getGridPosition(row, col, px, py);
    for (auto& frag : activeFragments) {
        frag.pos = {px, py};
        // 随机物理参数
        frag.vel = {(rand() % 200 - 100) * 1.f, -(rand() % 200 + 100) * 1.f}; // vx±100, vy -100~-300
        frag.rotSpeed = (rand() % 720 - 360) * 1.f; // -360~360 度/秒
        frag.alpha = 255.f;
        frag.rotation = 0.f;
        frag.released = false;
        frag.fadeTimer = 0.f;
    }
}

void Chessboard::startConvertAnim(int row, int col, int fromColor, int toColor) {
    animRow = row; animCol = col;
    pieceAnimType = 2;
    pieceAnimFrom = fromColor;
    pieceAnimTo   = toColor;
    pieceAnimJustDone = false;
    pieceAnimClock.restart();
    fragLastElapsed = 0.f;
    convFragLastElapsed = 0.f;

    // ── 粉碎方（旧色）──
    const auto& cacheFrom = (fromColor == 1) ? blackFragments : whiteFragments;
    activeFragments = cacheFrom;
    for (auto& frag : activeFragments) {
        frag.released = false;
        frag.alpha = 255.f;
        frag.rotation = 0.f;
        frag.fadeTimer = 0.f;
        frag.vel = {(rand() % 200 - 100) * 1.f, -(rand() % 200 + 100) * 1.f};
        frag.rotSpeed = (rand() % 720 - 360) * 1.f;
    }

    // ── 聚合方（新色）──
    const auto& cacheTo = (toColor == 1) ? blackFragments : whiteFragments;
    convertFragments = cacheTo;
    float px, py;
    getGridPosition(row, col, px, py);
    for (auto& frag : convertFragments) {
        frag.released = false;
        frag.alpha = 0.f;
        frag.rotation = (rand() % 60 - 30) * 1.f; // 初始小角度旋转
        frag.assembleTimer = 0.f;
        frag.fadeTimer = 0.f;
        // 散落起始位置（格子下方随机）
        frag.pos.x = px + (rand() % 120 - 60) * 1.f;
        frag.pos.y = py + (rand() % 100 + 40) * 1.f;
    }
}

bool Chessboard::isPieceAnimating() const {
    return pieceAnimType != 0;
}

bool Chessboard::updatePieceAnim() {
    if (pieceAnimType == 0) return false;
    if (pieceAnimJustDone)  return false; // 防重入
    if (pieceAnimClock.getElapsedTime().asSeconds() >= 1.0f) {
        pieceAnimJustDone = true;
        return true;
    }
    return false;
}

void Chessboard::finishPieceAnim() {
    // 粉碎碎片移到衰减缓冲区（继续下落淡出）
    for (auto& f : activeFragments)
        if (f.alpha > 0.f) decayFragments.push_back(f);
    if (!activeFragments.empty()) decayColor = pieceAnimFrom;
    // 聚合碎片不衰减——动画结束时已在目标位置，直接清除
    activeFragments.clear();
    convertFragments.clear();
    pieceAnimType = 0;
    pieceAnimJustDone = false;
    animRow = animCol = -1;
}

// ===== 落子抛物线动画 =====
void Chessboard::startDropAnim(int row, int col, int player) {
    if (replaying) return;  // 复盘时不播放落子动画
    dropAnimating = true;
    dropAnimRow = row;
    dropAnimCol = col;
    dropAnimPlayer = player;
    dropAnimClock.restart();
}

bool Chessboard::isDropAnimating() const {
    return dropAnimating;
}

void Chessboard::drawDropAnim(sf::RenderTarget& target) {
    if (!dropAnimating) return;

    const float DURATION = 0.35f;
    float raw = dropAnimClock.getElapsedTime().asSeconds() / DURATION;
    if (raw > 1.f) raw = 1.f;  // 钳制，确保末帧正常绘制再结束

    // 缓入：先快后慢（cubic ease-out）
    float t = 1.f - (1.f - raw) * (1.f - raw) * (1.f - raw);

    // 目标格子中心坐标
    float targetX, targetY;
    getGridPosition(dropAnimRow, dropAnimCol, targetX, targetY);

    // 起始位置：偏右 5px，偏上 10px
    float startX = targetX + 5.f;
    float startY = targetY - 10.f;

    // 抛物线弧高
    const float ARC_HEIGHT = 25.f;

    // 位移（弧顶高于起点，然后落到终点）
    float x = startX + (targetX - startX) * t;
    float y = startY + (targetY - startY) * t - ARC_HEIGHT * 4.f * t * (1.f - t);

    // 缩放：1.1 → 1.0
    float scale = 1.1f - 0.1f * t;

    // 绘制棋子
    sf::Sprite& sp = (dropAnimPlayer == 1) ? blackSprite : whiteSprite;
    sf::Texture& tx = (dropAnimPlayer == 1) ? blackTexture : whiteTexture;
    sf::Vector2u ts = tx.getSize();
    if (ts.x == 0 || ts.y == 0) return;

    float pieceSize = 68.4f * scale;
    sp.setScale({pieceSize / (float)ts.x, pieceSize / (float)ts.y});
    sp.setPosition({x, y});
    sp.setTextureRect(sf::IntRect({0, 0}, {(int)ts.x, (int)ts.y}));
    sp.setColor(sf::Color(255, 255, 255, 255));
    target.draw(sp);

    // 动画结束后标记完成，下一帧由 draw() 接管静态渲染
    if (dropAnimClock.getElapsedTime().asSeconds() >= DURATION) {
        dropAnimating = false;
    }
}
