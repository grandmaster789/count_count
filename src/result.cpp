#include "result.h"

namespace cvc {
    BadResult::BadResult():
        std::logic_error("Bad result access")
    {
    }

    BadResult::BadResult(const char *what):
        std::logic_error(what)
    {
    }
}