#ifndef AIPLAYER_H
#define AIPLAYER_H

#include "Chessboard.h"
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
        int& openEnds);

    int evaluatePosition(
        const Chessboard& chessboard,
        int row,
        int col,
        int pieceType);

    bool hasNeighbor(
        const Chessboard& chessboard,
        int row,
        int col);
};

#endif // AIPLAYER_H
