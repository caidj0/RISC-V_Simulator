#pragma once

#include <cstddef>
#include <cstdint>

#include "bus.hpp"
#include "utils.hpp"

class Predictor : public Updatable {
    Reg<size_t> total_predict;
    Reg<size_t> correct_predict;

   public:
    Wire<uint32_t> PC;
    Wire<PredictFeedbackBus> feedback;

    Predictor() {
        total_predict <= [&]() -> size_t {
            PredictFeedbackBus fb = feedback;
            return total_predict + fb.vaild_flag;
        };

        correct_predict <= [&]() -> size_t {
            PredictFeedbackBus fb = feedback;
            return correct_predict + (fb.vaild_flag && !fb.is_mispredicted);
        };
    }

    size_t totalPredict() const { return total_predict; }

    size_t correctPredict() const { return correct_predict; }

    virtual bool branch() = 0;

    virtual void pull() {
        total_predict.pull();
        correct_predict.pull();
    }

    virtual void update() {
        total_predict.update();
        correct_predict.update();
    }
};

class AlwaysBranchPredictor : public Predictor {
   public:
    bool branch() { return true; }
};

class NeverBranchPredictor : public Predictor {
   public:
    bool branch() { return false; }
};