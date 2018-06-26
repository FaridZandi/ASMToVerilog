#include "asmblock.h"

ASMBlock::ASMBlock(int id){
    this->id = id;
}

void ASMBlock::print()
{
    std::cout << "state " << id << " default code : " << default_code << std::endl;


    std::cout << "conditional codes : " << std::endl;

    for(auto conditional_code : conditional_codes){

        std::cout << "when ";

        for(int i = 0; i < conditional_code.conditions.size(); i++){
            std::cout << conditional_code.conditions[i].condition << " is " << conditional_code.conditions[i].value;

            if(i != conditional_code.conditions.size() - 1){
                std::cout << " and " ;
            }
        }

        std::cout << " => " << conditional_code.code;

        std::cout << std::endl;
    }

    std::cout << "next states : " << std::endl;

    for(auto next_state : next_states){

        std::cout << "when ";

        for(int i = 0; i < next_state.conditions.size(); i++){
            std::cout << next_state.conditions[i].condition << " is " << next_state.conditions[i].value;

            if(i != next_state.conditions.size() - 1){
                std::cout << " and " << std::endl;
            }
        }

        std::cout << " => " << next_state.next_state_id;

        std::cout << std::endl;
    }



}

