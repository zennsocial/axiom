#include "FormType.h"

#include <cassert>

using namespace MaximCommon;

std::string MaximCommon::formType2String(FormType type) {
    switch (type) {
        case FormType::LINEAR:
            return "lin";
        case FormType::OSCILLATOR:
            return "osc";
        case FormType::CONTROL:
            return "control";
        case FormType::NOTE:
            return "note";
        case FormType::DB:
            return "db";
        case FormType::Q:
            return "q";
        case FormType::SECONDS:
            return "secs";
        case FormType::BEATS:
            return "beats";
    }

    assert(false);
    throw;
}