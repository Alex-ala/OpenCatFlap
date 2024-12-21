#include <OCFHelper.h>

OCFDirection parseDirection(String direction)
{
    if (direction == "in") return OCFDirection::IN;
    if (direction == "out") return OCFDirection::OUT;
    if (direction == "both") return OCFDirection::BOTH;
    log_d("Failed to parse direction %s", direction);
    return OCFDirection::NONE;
}

const char* writeDirection(OCFDirection dir){
    if (dir == OCFDirection::IN){
        return "in";
    }else if (dir == OCFDirection::OUT){
        return "out";
    }else if (dir == OCFDirection::BOTH){
        return "both";
    }
    return "none";
}