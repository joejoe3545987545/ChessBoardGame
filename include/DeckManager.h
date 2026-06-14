#ifndef DECKMANAGER_H
#define DECKMANAGER_H

#include "Card.h"
#include "Chessboard.h"
#include <vector>
#include <deque>

class DeckManager {
public:
    DeckManager();
    void drawCard(); // 从牌库抽一张
    void playCard(int index, Chessboard& board); // 出牌并触发效果
    void resetDeck();

    std::vector<Card> hand; // 当前手牌
    std::deque<Card> deck;  // 牌库

private:
    void initDeck(); // 初始化初始卡牌池
};

#endif