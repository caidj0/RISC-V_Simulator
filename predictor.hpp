#pragma once

#include <bitset>
#include <concepts>
#include <cstddef>
#include <cstdint>

#include "bus.hpp"
#include "utils.hpp"

class Predictor : public Updatable {
    Reg<size_t> total_branch;
    Reg<size_t> correct_branch;
    Reg<size_t> total_jalr;
    Reg<size_t> correct_jalr;

   public:
    Wire<uint32_t> PC;
    Wire<PredictFeedbackBus> feedback;

    Predictor() {
        total_branch <= [&]() -> size_t {
            PredictFeedbackBus fb = feedback;
            return total_branch + (fb.type == PredictFeedbackBus::Branch);
        };

        correct_branch <= [&]() -> size_t {
            PredictFeedbackBus fb = feedback;
            return correct_branch + (fb.type == PredictFeedbackBus::Branch &&
                                     !fb.is_mispredicted);
        };

        total_jalr <= [&]() -> size_t {
            PredictFeedbackBus fb = feedback;
            return total_jalr + (fb.type == PredictFeedbackBus::Jalr);
        };

        correct_jalr <= [&]() -> size_t {
            PredictFeedbackBus fb = feedback;
            return correct_jalr +
                   (fb.type == PredictFeedbackBus::Jalr && !fb.is_mispredicted);
        };
    }

    PredictorStatistics predictorStatistics() const {
        return PredictorStatistics{total_branch, correct_branch, total_jalr,
                                   correct_jalr};
    }

    virtual bool branch()  = 0;

    virtual void pull() {
#ifdef PROFILE
        total_branch.pull();
        correct_branch.pull();
        total_jalr.pull();
        correct_jalr.pull();
#endif
    }

    virtual void update() {
#ifdef PROFILE
        total_branch.update();
        correct_branch.update();
        total_jalr.update();
        correct_jalr.update();
#endif
    }
};

class AlwaysBranchPredictor : public Predictor {
   public:
    bool branch()  { return true; }
};

class NeverBranchPredictor : public Predictor {
   public:
    bool branch()  { return false; }
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

                if (fb.type != PredictFeedbackBus::Branch ||
                    (fb.PC & ((1U << Bits) - 1)) != i) {
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

    bool branch()  {
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

                if (fb.type != PredictFeedbackBus::Branch ||
                    (fb.PC & ((1U << Bits) - 1)) != i) {
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
                    std::bitset<M>(histories[fb.PC & ((1U << Bits) - 1)])
                        .to_ulong();

                if (fb.type != PredictFeedbackBus::Branch || state_index != i) {
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

    bool branch()  {
        BinaryPredictState state =
            states[std::bitset<M>(histories[PC & ((1U << Bits) - 1)])
                       .to_ulong()];
        return state == StronglyB || state == WeaklyB;
    }

    void pull() {
        Predictor::pull();
        for (auto &history : histories) {
            history.pull();
        }
        for (auto &state : states) {
            state.pull();
        }
    }

    void update() {
        Predictor::update();
        for (auto &history : histories) {
            history.update();
        }
        for (auto &state : states) {
            state.update();
        }
    }
};

template <size_t Bits, typename Predictor1, typename Predictor2>
    requires(Bits <= 32 && std::derived_from<Predictor1, Predictor> &&
             std::derived_from<Predictor2, Predictor>)
class TournamentPredictor : public Predictor {
    Predictor1 predictor1;
    Predictor2 predictor2;
    Reg<BinaryPredictState> states[1U << Bits];

   public:
    TournamentPredictor() : Predictor() {
        predictor1.PC = LAM(PC);
        predictor2.PC = LAM(PC);
        predictor1.feedback = LAM(feedback);
        predictor2.feedback = LAM(feedback);

        for (size_t i = 0; i < (1U << Bits); i++) {
            states[i] <= [&, i]() -> BinaryPredictState {
                PredictFeedbackBus fb = feedback;
                if (fb.type != PredictFeedbackBus::Branch ||
                    (fb.PC & ((1U << Bits) - 1)) != i) {
                    return states[i];
                }

                switch (states[i]) {
                    case StronglyB:
                        return fb.is_mispredicted ? WeaklyB : StronglyB;
                    case WeaklyB:
                        return fb.is_mispredicted ? WeaklyNo : StronglyB;
                    case WeaklyNo:
                        return fb.is_mispredicted ? WeaklyB : StronglyNo;
                    default:  // StronglyNo
                        return fb.is_mispredicted ? WeaklyNo : StronglyNo;
                }
            };
        }
    }

    bool branch()  {
        BinaryPredictState bps = states[PC & ((1U << Bits) - 1)];
        return bps == StronglyB || bps == WeaklyB ? predictor1.branch()
                                                  : predictor2.branch();
    }

    void pull() {
        Predictor::pull();
        for (auto &state : states) {
            state.pull();
        }
        predictor1.pull();
        predictor2.pull();
    }

    void update() {
        Predictor::update();
        for (auto &state : states) {
            state.update();
        }
        predictor1.update();
        predictor2.update();
    }
};