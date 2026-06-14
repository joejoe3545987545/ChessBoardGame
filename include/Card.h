#ifndef CARD_H
#define CARD_H

#include <string>

// 卡牌效果类型枚举，方便后续扩展
enum class CardEffect {
    NONE,
    FORCE_DROP,         // 强制落子
    CHANGE_WIN_RULE,    // 修改胜利条件
    REMOVE_OPPONENT,    // 移除对方棋子
    EXTRA_TURN          // 额外回合
};

struct Card {
    int id;
    std::wstring name;
    std::wstring description;
    CardEffect effect;
    int value; // 效果参数，比如修改规则为6时，value=6
};

#endif