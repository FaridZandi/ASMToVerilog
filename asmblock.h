#ifndef ASMBLOCK_H
#define ASMBLOCK_H


#include <vector>
#include <iostream>

using namespace std;

struct condition{
    std::string condition;
    std::string value;
};

struct conditional_code{
    std::vector<condition> conditions;
    string code;
};

struct next_state{
    std::vector<condition> conditions;
    int next_state_id;
};

class ASMBlock{
public:

    int id;

    std::string default_code;

    std::vector<conditional_code> conditional_codes;

    std::vector<next_state> next_states;

    ASMBlock(int id);

<<<<<<< HEAD
=======
    void print();
>>>>>>> 02cd4fb3000c0525f42d5d799f8f473a8116fb42
};

#endif // ASMBLOCK_H
