#ifndef AIPLAYER_H
#define AIPLAYER_H

#include "Chessboard.h"
#include "Card.h"
#include <utility>
#include <vector>

class AIPlayer
{
public:

    AIPlayer(
        int aiPieceType,
        int difficulty = 2);

    std::pair<int,int> calculateBestMove(
        const Chessboard& chessboard);

    void setConfig(
        int aiPieceType,
        int difficulty);

    // ── 卡牌决策（纯评估，无副作用）──
    bool shouldPlayCard(
        const std::vector<Card>& aiHand,
        const Chessboard& chessboard,
        bool hasCardPlayAP) const;

    int  chooseCardToPlay(
        const std::vector<Card>& aiHand,
        const Chessboard& chessboard) const;

    bool shouldPlayBeforeDrop(
        const std::vector<Card>& aiHand,
        const Chessboard& chessboard) const;

    // CONVERT_PIECE 自动选子
    std::pair<int,int> chooseOwnPieceToSacrifice(
        const Chessboard& chessboard) const;

    std::pair<int,int> chooseEnemyPieceToConvert(
        const Chessboard& chessboard) const;

private:

    int aiPiece;
    int playerPiece;
    int aiDifficulty;

    std::pair<int,int> playRandomMode(
        const Chessboard& chessboard);

    std::pair<int,int> playSmartMode(
        const Chessboard& chessboard);

    std::pair<int,int> playGodMode(
        const Chessboard& chessboard);

    void evaluateLine(
        const Chessboard& chessboard,
        int row,
        int col,
        int dr,
        int dc,
        int pieceType,
        int& count,
        int& openEnds) const;

    int evaluatePosition(
        const Chessboard& chessboard,
        int row,
        int col,
        int pieceType) const;

    bool hasNeighbor(
        const Chessboard& chessboard,
        int row,
        int col) const;

    // 卡牌启发式评分
    int  evaluateCard(const Card& card, const Chessboard& chessboard) const;
    int  countPieces(const Chessboard& chessboard, int pieceType) const;
};

#endif // AIPLAYER_H
