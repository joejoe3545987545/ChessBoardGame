#ifndef DECKMANAGER_H
#define DECKMANAGER_H

#include "Card.h"
#include "Chessboard.h"
#include <vector>
#include <deque>

class DeckManager {
public:
    DeckManager();
    void drawCard(); // 从牌库随机抽一张到玩家手牌
    void drawCardForAI(); // 从牌库随机抽一张到 AI 手牌
    void discardCard(const Card& card); // 弃牌堆
    void aiDiscardCard(int index); // AI 弃掉 aiHand[index]
    void returnHandToDeck(); // 玩家手牌全部回牌库
    void returnAiHandToDeck(); // AI 手牌全部回牌库
    void playCard(int index, Chessboard& board); // 出牌并触发效果
    void resetDeck();

    std::vector<Card> hand;        // 玩家手牌
    std::vector<Card> aiHand;      // AI 手牌（不可见，无卡槽）
    std::deque<Card> deck;         // 牌库（共享）
    std::vector<Card> discardPile; // 弃牌堆（共享）

private:
    void initDeck(); // 初始化初始卡牌池
};

#endif