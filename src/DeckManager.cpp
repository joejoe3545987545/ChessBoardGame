#include <iostream>
#include "DeckManager.h"

DeckManager::DeckManager() {
    initDeck(); // 初始化牌库
}

void DeckManager::initDeck() {
    // 这里先建立一个框架，后续可以从 JSON 读取
    deck.push_back({1, L"连击", L"给予两次落子数", CardEffect::FORCE_DROP, 0});
    deck.push_back({2, L"规则加固", L"胜利条件变为6子", CardEffect::CHANGE_WIN_RULE, 6});
}

void DeckManager::playCard(int index, Chessboard& board) {
    Card c = hand[index];
    switch(c.effect) {
        case CardEffect::FORCE_DROP:
            // TODO: 调用落子逻辑
            break;
        case CardEffect::CHANGE_WIN_RULE:
            // TODO: 修改全局胜利条件变量
            break;
            // ... 更多效果 ...
    }
    hand.erase(hand.begin() + index); // 出牌后移除
}

void DeckManager::drawCard() {
    if (!deck.empty()) {
        hand.push_back(deck.front()); // 将牌库顶部的牌移到手牌
        deck.pop_front();
        std::cout << "[CardSystem] 成功抽卡！当前手牌数: " << hand.size() << std::endl;
    } else {
        std::cout << "[CardSystem] 牌库空了！" << std::endl;
    }
}



void DeckManager::resetDeck() {
    hand.clear(); // 清空手牌
    deck.clear(); // 清空残余牌库
    initDeck();   // 🌟 重新调用你写好的初始化，倒满 2 张初始卡牌
    std::cout << "[CardSystem] 牌库与手牌已彻底重置洗牌！" << std::endl;
}