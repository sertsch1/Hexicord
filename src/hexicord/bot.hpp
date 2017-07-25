#include <string.h>
#include "json.hpp"

namespace Hexicord {
class Bot;

using json = nlohmannjson::json;
using handler_t = std::functon<Bot*, json>;

class Bot {
public:
    Bot();
    ~Bot();
private:
    std::string token;
};

}

