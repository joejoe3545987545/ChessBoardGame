#include <iostream>
#include <cstdlib>
#include "DeckManager.h"

DeckManager::DeckManager() {
    initDeck();
}

void DeckManager::initDeck() {
    deck.push_back({1, L"连击", L"给予两次落子数", CardEffect::FORCE_DROP, 0});
    deck.push_back({1, L"连击", L"给予两次落子数", CardEffect::FORCE_DROP, 0});
    deck.push_back({3, L"隐忍", L"迫使敌方承受：\n六子连星为胜途", CardEffect::CHANGE_WIN_RULE, 6});
    deck.push_back({3, L"隐忍", L"迫使敌方承受：\n六子连星为胜途", CardEffect::CHANGE_WIN_RULE, 6});
    deck.push_back({4, L"笼络", L"销毁己方一个棋子\n转化敌方一个棋子", CardEffect::CONVERT_PIECE, 0});
    deck.push_back({4, L"笼络", L"销毁己方一个棋子\n转化敌方一个棋子", CardEffect::CONVERT_PIECE, 0});
    deck.push_back({5, L"破釜沉舟", L"将手牌放回牌库\n根据放回的数量\n销毁敌方棋子", CardEffect::SACRIFICE_HAND, 0});
    deck.push_back({5, L"破釜沉舟", L"将手牌放回牌库\n根据放回的数量\n销毁敌方棋子", CardEffect::SACRIFICE_HAND, 0});
    deck.push_back({6, L"疫病", L"指定一颗敌方棋子\n使其患上疫病\n患病棋子每回合都\n有概率死亡并试图\n传染给其他棋子\n玩家棋子也有患病\n风险", CardEffect::PLAGUE, 0});
    deck.push_back({6, L"疫病", L"指定一颗敌方棋子\n使其患上疫病\n患病棋子每回合都\n有概率死亡并试图\n传染给其他棋子\n玩家棋子也有患病\n风险", CardEffect::PLAGUE, 0});
    deck.push_back({7, L"隔离", L"使患病棋子在三回\n合后痊愈", CardEffect::QUARANTINE, 4});
    deck.push_back({7, L"隔离", L"使患病棋子在三回\n合后痊愈", CardEffect::QUARANTINE, 4});
    // 紫卡：盲目（cardColor=1, value=4 → 三回合后消退）
    deck.push_back({8, L"盲目", L"持有者将无法辨清\n自己的手牌\n三回合后消退\n且三子连星将导致\n消退时间延后", CardEffect::BLIND, 4, 1});
    deck.push_back({8, L"盲目", L"持有者将无法辨清\n自己的手牌\n三回合后消退\n且三子连星将导致\n消退时间延后", CardEffect::BLIND, 4, 1});
    // 紫卡：以地事秦（cardColor=1, value=3 → 三回合后消退）
    deck.push_back({9, L"以地事秦", L"持有者需要传送一\n张橙卡给出牌者\n不送牌则接下来三\n回合只能下棋\n送卡或撑过三回合\n后本牌消退", CardEffect::YIDISHIQIN, 3, 1});
    deck.push_back({9, L"以地事秦", L"持有者需要传送一\n张橙卡给出牌者\n不送牌则接下来三\n回合只能下棋\n送卡或撑过三回合\n后本牌消退", CardEffect::YIDISHIQIN, 3, 1});
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

// ── AI 手牌管理 ──
void DeckManager::drawCardForAI() {
    if (!deck.empty()) {
        int idx = rand() % static_cast<int>(deck.size());
        aiHand.push_back(deck[idx]);
        deck.erase(deck.begin() + idx);
        std::cout << "[CardSystem] AI 抽卡！AI手牌数: " << aiHand.size()
                  << " 剩余牌库: " << deck.size() << std::endl;
    } else {
        std::cout << "[CardSystem] 牌库空了，AI 无法抽卡！" << std::endl;
    }
}

void DeckManager::aiDiscardCard(int index) {
    if (index < 0 || index >= static_cast<int>(aiHand.size())) return;
    discardPile.push_back(aiHand[index]);
    std::cout << "[CardSystem] AI 弃牌: " << aiHand[index].name.c_str()
              << " 弃牌堆大小: " << discardPile.size() << std::endl;
    aiHand.erase(aiHand.begin() + index);
}

void DeckManager::returnAiHandToDeck() {
    for (auto& c : aiHand) deck.push_back(c);
    aiHand.clear();
    std::cout << "[CardSystem] AI 手牌已全部回到牌库，牌库大小: " << deck.size() << std::endl;
}

void DeckManager::resetDeck() {
    hand.clear();
    aiHand.clear();
    deck.clear();
    discardPile.clear();
    initDeck();
    std::cout << "[CardSystem] 牌库、手牌、弃牌堆已彻底重置！" << std::endl;
}