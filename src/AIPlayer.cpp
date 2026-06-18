#include "AIPlayer.h"
#include <random>
#include <algorithm>
#include <vector>
#include <cmath>
#include <iostream> // 可选：用于调试输出

AIPlayer::AIPlayer(int aiPieceType, int difficulty)
    : aiPiece(aiPieceType),
      aiDifficulty(difficulty)
{
    playerPiece = (aiPiece == 1) ? 2 : 1;
    // 使用随机设备初始化 mt19937（只初始化一次，见 static below）
}

// 如果你愿意把 rng 作为成员也可以，但静态放在 cpp 内更简单
static std::mt19937& globalRng() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    return gen;
}

void AIPlayer::setConfig(int aiPieceType, int difficulty)
{
    aiPiece = aiPieceType;
    playerPiece = (aiPiece == 1) ? 2 : 1;
    aiDifficulty = difficulty;
}

void AIPlayer::evaluateLine(
    const Chessboard& chessboard,
    int row,
    int col,
    int dr,
    int dc,
    int pieceType,
    int& count,
    int& openEnds) const
{
    count = 1;
    openEnds = 0;

    int r = row + dr;
    int c = col + dc;

    while (r >= 0 && r < 15 &&
           c >= 0 && c < 15 &&
           chessboard.getPiece(r, c) == pieceType)
    {
        count++;
        r += dr;
        c += dc;
    }

    if (r >= 0 && r < 15 &&
        c >= 0 && c < 15 &&
        chessboard.getPiece(r, c) == 0)
    {
        openEnds++;
    }

    r = row - dr;
    c = col - dc;

    while (r >= 0 && r < 15 &&
           c >= 0 && c < 15 &&
           chessboard.getPiece(r, c) == pieceType)
    {
        count++;
        r -= dr;
        c -= dc;
    }

    if (r >= 0 && r < 15 &&
        c >= 0 && c < 15 &&
        chessboard.getPiece(r, c) == 0)
    {
        openEnds++;
    }
}

bool AIPlayer::hasNeighbor(
    const Chessboard& chessboard,
    int row,
    int col) const
{
    for (int dr = -2; dr <= 2; dr++)
    {
        for (int dc = -2; dc <= 2; dc++)
        {
            if (dr == 0 && dc == 0)
                continue;

            int r = row + dr;
            int c = col + dc;

            if (r < 0 || r >= 15 ||
                c < 0 || c >= 15)
                continue;

            if (chessboard.getPiece(r, c) != 0)
                return true;
        }
    }

    return false;
}

int AIPlayer::evaluatePosition(
    const Chessboard& chessboard,
    int row,
    int col,
    int pieceType) const
{
    static const int directions[4][2] =
    {
        {1,0},
        {0,1},
        {1,1},
        {1,-1}
    };

    int score = 0;

    for (auto& dir : directions)
    {
        int count;
        int openEnds;

        evaluateLine(
            chessboard,
            row,
            col,
            dir[0],
            dir[1],
            pieceType,
            count,
            openEnds);

        if (count >= 5)
        {
            score += 1000000;
        }
        else if (count == 4)
        {
            if (openEnds == 2)
                score += 120000;
            else if (openEnds == 1)
                score += 15000;
        }
        else if (count == 3)
        {
            if (openEnds == 2)
                score += 6000;
            else if (openEnds == 1)
                score += 800;
        }
        else if (count == 2)
        {
            if (openEnds == 2)
                score += 300;
            else if (openEnds == 1)
                score += 50;
        }
    }

    int centerBonus =
        (7 - std::abs(row - 7))
        + (7 - std::abs(col - 7));

    score += centerBonus * 15;

    return score;
}

std::pair<int,int> AIPlayer::calculateBestMove(
    const Chessboard& chessboard)
{
    switch (aiDifficulty)
    {
        case 1:
            return playRandomMode(chessboard);

        case 2:
            return playSmartMode(chessboard);

        case 3:
            return playGodMode(chessboard);

        default:
            return playSmartMode(chessboard);
    }
}

std::pair<int,int> AIPlayer::playRandomMode(
    const Chessboard& chessboard)
{
    // 改进思路：
    // - 收集“有邻居”的候选位置（避免远空位）
    // - 对候选位置计算简单分数：attack + defend * defendWeight
    // - 取 topN（例如 8）较好候选，按分数做加权随机选择
    // - 保留少量纯随机概率（pureRandomChance）以保持初级随机性

    const double pureRandomChance = 0.10; // 10% 直接随机，越大越“蠢”
    const int topN = 8;
    const double defendWeight = 1.0; // 防守分权重，可调（初级可设低）

    std::vector<std::pair<std::pair<int,int>, int>> scored; // ((r,c), score)
    std::vector<std::pair<int,int>> empties;

    // 收集所有空位
    for (int r = 0; r < 15; r++)
    {
        for (int c = 0; c < 15; c++)
        {
            if (chessboard.getPiece(r,c) == 0)
            {
                empties.emplace_back(r,c);
            }
        }
    }

    if (empties.empty())
        return {7,7};

    // 如果棋盘几乎为空（初期），给中心更高概率
    bool veryEmpty = true;
    for (const auto& p : empties) {
        int r = p.first, c = p.second;
        // 如果有任何邻居说明不是非常空
        if (!(!hasNeighbor(chessboard, r, c))) {
            veryEmpty = false;
            break;
        }
    }

    // 优先收集有邻居的位置，避免远空位
    for (const auto& p : empties)
    {
        int r = p.first, c = p.second;
        if (!hasNeighbor(chessboard, r, c)) continue;
        int attack = evaluatePosition(chessboard, r, c, aiPiece);
        int defend = evaluatePosition(chessboard, r, c, playerPiece);
        int total = static_cast<int>(attack + defend * defendWeight);
        scored.push_back({{r,c}, total});
    }

    // 如果没有任何“有邻居”的候选（早期或极端空的局面），把一些关键位置加入候选：
    if (scored.empty()) {
        // 把中心与若干随机位置作为候选
        // Prefer center
        scored.push_back({{7,7}, 100});
        // Add up to 10 random empties (if exist)
        std::uniform_int_distribution<size_t> dist(0, empties.size()-1);
        size_t addCount = std::min<size_t>(10, empties.size());
        auto& rng = globalRng();
        for (size_t i = 0; i < addCount; ++i) {
            auto p = empties[dist(rng)];
            scored.push_back({p, 10});
        }
    }

    // 以评分排序（降序）
    std::sort(scored.begin(), scored.end(),
        [](const auto& a, const auto& b){
            return a.second > b.second;
        });

    // 10% 概率选择全局随机（保持初级随机性）
    std::uniform_real_distribution<double> uni01(0.0, 1.0);
    auto& rng = globalRng();
    if (uni01(rng) < pureRandomChance) {
        // 完全随机地从 empties 中选择一个（但如果 veryEmpty 值为真，倾向中心）
        if (veryEmpty) {
            return {7,7};
        }
        std::uniform_int_distribution<size_t> di(0, empties.size()-1);
        return empties[di(rng)];
    }

    // 取 topN 候选（如果 fewer，则取全部）
    int n = static_cast<int>(std::min<size_t>(topN, scored.size()));

    // 如果评分都很低（例如初期），可以放宽，直接选择中心或随机
    // 现在构造加权分布
    std::vector<double> weights;
    weights.reserve(n);
    double sum = 0.0;
    for (int i = 0; i < n; ++i) {
        // 为避免 0 权重，加上一个小 epsilon，同时将 score 转为正值
        double w = double(scored[i].second) + 1.0;
        if (w < 0.1) w = 0.1;
        weights.push_back(w);
        sum += w;
    }

    // 规范化并抽样
    std::discrete_distribution<int> dist(weights.begin(), weights.end());
    int idx = dist(rng); // index in 0..n-1
    auto chosen = scored[idx].first;
    return chosen;
}

std::pair<int,int> AIPlayer::playSmartMode(
    const Chessboard& chessboard)
{
    int bestScore = -1;
    std::pair<int,int> bestMove = {7,7};

    for (int r = 0; r < 15; r++)
    {
        for (int c = 0; c < 15; c++)
        {
            if (chessboard.getPiece(r,c) != 0)
                continue;

            if (!hasNeighbor(chessboard,r,c))
                continue;

            int attack =
                evaluatePosition(
                    chessboard,
                    r,
                    c,
                    aiPiece);

            int defend =
                evaluatePosition(
                    chessboard,
                    r,
                    c,
                    playerPiece);

            int total =
                attack * 2 +
                defend * 3;

            if (total > bestScore)
            {
                bestScore = total;
                bestMove = {r,c};
            }
        }
    }

    return bestMove;
}

std::pair<int,int> AIPlayer::playGodMode(
    const Chessboard& chessboard)
{
    int bestScore = -1;
    std::pair<int,int> bestMove = {7,7};

    for (int r = 0; r < 15; r++)
    {
        for (int c = 0; c < 15; c++)
        {
            if (chessboard.getPiece(r,c) != 0)
                continue;

            if (!hasNeighbor(chessboard,r,c))
                continue;

            int attack =
                evaluatePosition(
                    chessboard,
                    r,
                    c,
                    aiPiece);

            int defend =
                evaluatePosition(
                    chessboard,
                    r,
                    c,
                    playerPiece);

            int center =
                (7 - std::abs(r - 7))
                + (7 - std::abs(c - 7));

            int total =
                attack * 8 +
                defend * 6 +
                center * 40;

            if (r == 7 && c == 7)
            {
                total += 500;
            }

            if (total > bestScore)
            {
                bestScore = total;
                bestMove = {r,c};
            }
        }
    }

    return bestMove;
}

// ===== AI 卡牌决策 =====

int AIPlayer::countPieces(const Chessboard& chessboard, int pieceType) const {
    int count = 0;
    for (int r = 0; r < 15; ++r)
        for (int c = 0; c < 15; ++c)
            if (chessboard.getPiece(r, c) == pieceType) ++count;
    return count;
}

int AIPlayer::evaluateCard(const Card& card, const Chessboard& chessboard) const {
    // 紫卡优先出（负面效果给敌方）
    if (card.cardColor == 1) return 120;

    int score = 0;
    int ownPieces = countPieces(chessboard, aiPiece);
    int enemyPieces = countPieces(chessboard, playerPiece);

    switch (card.effect) {
    case CardEffect::FORCE_DROP:
        score = 60;
        if (ownPieces >= 10) score += 20;
        break;
    case CardEffect::CHANGE_WIN_RULE:
        score = 80;
        if (enemyPieces >= 8) score += 40;
        break;
    case CardEffect::CONVERT_PIECE:
        score = 50;
        if (ownPieces >= 5) score += 30;
        if (enemyPieces >= 8) score += 20;
        break;
    case CardEffect::SACRIFICE_HAND:
        score = 30;
        if (enemyPieces >= 10) score += 40;
        break;
    default:
        score = 20;
        break;
    }

    auto& rng = globalRng();
    std::uniform_int_distribution<int> noise(0, 1);
    switch (aiDifficulty) {
    case 1: score += (noise(rng) ? 30 : -30); break;
    case 2: score += (noise(rng) ? 15 : -15); break;
    default: break;
    }

    return score;
}

bool AIPlayer::shouldPlayCard(
    const std::vector<Card>& aiHand,
    const Chessboard& chessboard,
    bool hasCardPlayAP) const
{
    if (aiHand.empty() || !hasCardPlayAP) return false;

    int threshold = 80;
    if (aiDifficulty == 2) threshold = 60;
    if (aiDifficulty == 3) threshold = 40;

    int bestScore = -999;
    for (const auto& c : aiHand) {
        int s = evaluateCard(c, chessboard);
        if (s > bestScore) bestScore = s;
    }

    return bestScore >= threshold;
}

int AIPlayer::chooseCardToPlay(
    const std::vector<Card>& aiHand,
    const Chessboard& chessboard) const
{
    if (aiHand.empty()) return -1;

    int bestIdx = -1;
    int bestScore = -999;
    for (size_t i = 0; i < aiHand.size(); ++i) {
        int s = evaluateCard(aiHand[i], chessboard);
        if (s > bestScore) { bestScore = s; bestIdx = static_cast<int>(i); }
    }
    return bestIdx;
}

bool AIPlayer::shouldPlayBeforeDrop(
    const std::vector<Card>& aiHand,
    const Chessboard& chessboard) const
{
    for (const auto& c : aiHand) {
        if (c.effect == CardEffect::FORCE_DROP) return true;
        if (c.cardColor == 1) return true; // 紫卡优先发送
        if (c.effect == CardEffect::CHANGE_WIN_RULE &&
            countPieces(chessboard, playerPiece) >= 8) return true;
    }
    return false;
}

std::pair<int,int> AIPlayer::chooseOwnPieceToSacrifice(
    const Chessboard& chessboard) const
{
    int bestR = -1, bestC = -1;
    int worstScore = 999999;
    for (int r = 0; r < 15; ++r) {
        for (int c = 0; c < 15; ++c) {
            if (chessboard.getPiece(r, c) != aiPiece) continue;
            int sc = evaluatePosition(chessboard, r, c, aiPiece);
            if (sc < worstScore) { worstScore = sc; bestR = r; bestC = c; }
        }
    }
    return {bestR, bestC};
}

std::pair<int,int> AIPlayer::chooseEnemyPieceToConvert(
    const Chessboard& chessboard) const
{
    int bestR = -1, bestC = -1;
    int bestScore = -1;
    for (int r = 0; r < 15; ++r) {
        for (int c = 0; c < 15; ++c) {
            if (chessboard.getPiece(r, c) != playerPiece) continue;
            int sc = evaluatePosition(chessboard, r, c, playerPiece);
            if (sc > bestScore) { bestScore = sc; bestR = r; bestC = c; }
        }
    }
    return {bestR, bestC};
}
