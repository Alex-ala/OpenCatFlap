#include <OCFHelper.h>

OCFDirection parseDirection(String direction)
{
    if (direction == "in") return OCFDirection::IN;
    if (direction == "out") return OCFDirection::OUT;
    if (direction == "both") return OCFDirection::BOTH;
    log_d("Failed to parse direction %s", direction);
    return OCFDirection::NONE;
}

size_t copyString(char *dest, size_t dest_size, const char *src)
{
    if (dest == nullptr || dest_size == 0) return 0;
    if (src == nullptr)
    {
        dest[0] = '\0';
        return 0;
    }
    size_t len = strlen(src);
    if (len >= dest_size)
    {
        log_w("Truncating string of %u characters to %u", (unsigned)len, (unsigned)(dest_size - 1));
        len = dest_size - 1;
    }
    memcpy(dest, src, len);
    dest[len] = '\0';
    return len;
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