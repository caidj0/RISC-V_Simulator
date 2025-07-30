#pragma once

#include <bitset>
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
            return total_predict + fb.valid_flag;
        };

        correct_predict <= [&]() -> size_t {
            PredictFeedbackBus fb = feedback;
            return correct_predict + (fb.valid_flag && !fb.is_mispredicted);
        };
    }

    size_t totalPredict() const { return total_predict; }

    size_t correctPredict() const { return correct_predict; }

    virtual bool branch() const = 0;

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
    bool branch() const { return true; }
};

class NeverBranchPredictor : public Predictor {
   public:
    bool branch() const { return false; }
};

enum BinaryPredictState { StronglyB, WeaklyB, WeaklyNo, StronglyNo };

template <size_t Bits, BinaryPredictState InitState>
    requires(Bits <= 32)
class BinaryPredictor : public Predictor {
    Reg<BinaryPredictState> states[1U << Bits];

   public:
    BinaryPredictor() : Predictor() {
        for (size_t i = 0; i < (1U << Bits); i++) {
            states[i] = InitState;

            states[i] <= [&, i]() -> BinaryPredictState {
                PredictFeedbackBus fb = feedback;

                if (!fb.valid_flag || (fb.PC & ((1U << Bits) - 1)) != i) {
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

    bool branch() const {
        BinaryPredictState state = states[PC & ((1U << Bits) - 1)];
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

// (M, 2) 分支预测器
template <size_t Bits, size_t M>
    requires(Bits <= 32 && M > 0)
class CorrelatingPredictor : public Predictor {
    Reg<std::bitset<M>> histories[1U << Bits];
    Reg<BinaryPredictState> states[1U << M];

   public:
    CorrelatingPredictor() : Predictor() {
        for (size_t i = 0; i < (1U << Bits); i++) {
            histories[i] <= [&, i]() -> std::bitset<M> {
                PredictFeedbackBus fb = feedback;

                if (!fb.valid_flag || (fb.PC & ((1U << Bits) - 1)) != i) {
                    return histories[i];
                }

                auto ret = std::bitset<M>(histories[i]) << 1;
                ret[0] = fb.predict_branch ^ fb.is_mispredicted;
                return ret;
            };
        }

        for (size_t i = 0; i < (1U << M); i++) {
            states[i] <= [&, i]() -> BinaryPredictState {
                PredictFeedbackBus fb = feedback;
                size_t state_index =
                    std::bitset<M>(histories[fb.PC & ((1U << Bits) - 1)]).to_ulong();

                if (!fb.valid_flag || state_index != i) {
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

    bool branch() const {
        BinaryPredictState state =
            states[std::bitset<M>(histories[PC & ((1U << Bits) - 1)]).to_ulong()];
        return state == StronglyB || state == WeaklyB;
    }

    void pull() {
        Predictor::pull();
        for (auto &history : histories) {
            history.pull();
        }
        for (auto &state: states) {
            state.pull();
        }
    }

    void update() {
        Predictor::update();
        for (auto &history : histories) {
            history.update();
        }
        for (auto &state: states) {
            state.update();
        }
    }
};