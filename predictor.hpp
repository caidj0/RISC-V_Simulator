#pragma once

#include "utils.hpp"

class Predictor : public Updatable {
   public:
    bool branch() { return true; }

    void pull() {}
    void update() {}
};