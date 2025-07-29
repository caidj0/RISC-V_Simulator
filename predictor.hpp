#pragma once

#include <cstdint>

#include "bus.hpp"
#include "utils.hpp"

class Predictor : public Updatable {
   public:
    Wire<uint32_t> PC;
    Wire<PredictFeedbackBus> feedback;

    virtual bool branch() = 0;
};

class AlwaysBranchPredictor : public Predictor {
   public:
    bool branch() { return true; }
    void pull() {}
    void update() {}
};

class NeverBranchPredictor : public Predictor {
   public:
    bool branch() { return false; }
    void pull() {}
    void update() {}
};
