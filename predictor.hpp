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

enum BinaryPredictState { StronglyB, WeaklyB, WeaklyNo, StronglyNo };

template <size_t HighestBit, BinaryPredictState InitState>
    requires(HighestBit < 32)
class BinaryPredictor : public Predictor {
    Reg<BinaryPredictState> states[2U << HighestBit];

   public:
    BinaryPredictor() : Predictor() {
        for (size_t i = 0; i < (2U << HighestBit); i++) {
            states[i] = InitState;

            states[i] <= [&, i]() -> BinaryPredictState {
                PredictFeedbackBus fb = feedback;

                if (!fb.vaild_flag || (fb.PC & ((2U << HighestBit) - 1)) != i) {
                    return states[i];
                }

                bool should_branch = fb.predict_branch ^ fb.is_mispredicted;

                switch (states[i]) {
                    case StronglyB:
                        return should_branch ? StronglyB : WeaklyB;
                    case WeaklyB:
                        return should_branch ? StronglyB : WeaklyNo;
                    case WeaklyNo:
                        return should_branch ? WeaklyB : StronglyNo;
                    default:  // StronglyNo
                        return should_branch ? WeaklyNo : StronglyNo;
                }
            };
        }
    }

    bool branch() {
        BinaryPredictState state = states[PC & ((2U << HighestBit) - 1)];
        return state == StronglyB || state == WeaklyB;
    }

    void pull() {
        Predictor::pull();
        for (auto &state : states) {
            state.pull();
        }
    }

    void update() {
        Predictor::update();
        for (auto &state : states) {
            state.update();
        }
    }
};