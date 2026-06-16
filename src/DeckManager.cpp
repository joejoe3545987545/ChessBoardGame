#include <iostream>
#include <cstdlib>
#include "DeckManager.h"

DeckManager::DeckManager() {
    initDeck();
}

void DeckManager::initDeck() {
    deck.push_back({1, L"连击", L"给予两次落子数", CardEffect::FORCE_DROP, 0});
    deck.push_back({3, L"隐忍", L"迫使敌方承受：\n六子连星为胜途", CardEffect::CHANGE_WIN_RULE, 6});
    deck.push_back({4, L"笼络", L"销毁己方一个棋子\n转化敌方一个棋子", CardEffect::CONVERT_PIECE, 0});
    deck.push_back({5, L"破釜沉舟", L"将手牌放回牌库\n根据放回的数量\n销毁敌方棋子", CardEffect::SACRIFICE_HAND, 0});
}

void DeckManager::playCard(int index, Chessboard& board) {
    Card c = hand[index];
    switch(c.effect) {
        case CardEffect::FORCE_DROP:
            // TODO
            break;
        case CardEffect::CHANGE_WIN_RULE:
            // TODO
            break;
    }
    hand.erase(hand.begin() + index);
}

void DeckManager::drawCard() {
    if (!deck.empty()) {
        int idx = rand() % static_cast<int>(deck.size());
        hand.push_back(deck[idx]);
        deck.erase(deck.begin() + idx);
        std::cout << "[CardSystem] 随机抽卡！当前手牌数: " << hand.size()
                  << " 剩余牌库: " << deck.size() << std::endl;
    } else {
        std::cout << "[CardSystem] 牌库空了！" << std::endl;
    }
}

void DeckManager::returnHandToDeck() {
    for (auto& c : hand) deck.push_back(c);
    hand.clear();
    std::cout << "[CardSystem] 手牌已全部回到牌库，牌库大小: " << deck.size() << std::endl;
}

void DeckManager::discardCard(const Card& card) {
    discardPile.push_back(card);
    std::cout << "[CardSystem] 卡牌进入弃牌堆: " << card.name.c_str()
              << " 弃牌堆大小: " << discardPile.size() << std::endl;
}

void DeckManager::resetDeck() {
    hand.clear();
    deck.clear();
    discardPile.clear();
    initDeck();
    std::cout << "[CardSystem] 牌库、手牌、弃牌堆已彻底重置！" << std::endl;
}