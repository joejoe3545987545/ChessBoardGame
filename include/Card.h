#ifndef CARD_H
#define CARD_H

#include <string>

// 卡牌效果类型枚举，方便后续扩展
enum class CardEffect {
    NONE,
    FORCE_DROP,         // 强制落子
    CHANGE_WIN_RULE,    // 修改胜利条件
    CONVERT_PIECE,      // 笼络：销毁己方一子 + 转化敌方一子
    SACRIFICE_HAND,     // 破釜沉舟：回手牌→销毁敌方棋子
    REMOVE_OPPONENT,    // 移除对方棋子
    EXTRA_TURN,         // 额外回合
    PLAGUE,             // 疫病：感染敌方棋子，每回合概率销毁+传播
    QUARANTINE          // 隔离：三回合后清除所有疫病标记
};

struct Card {
    int id;
    std::wstring name;
    std::wstring description;
    CardEffect effect;
    int value;         // 效果参数，比如修改规则为6时，value=6
    int cardColor = 0; // 0=橙卡（默认），1=紫卡
};

#endif