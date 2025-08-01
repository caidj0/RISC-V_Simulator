#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "ALU.hpp"
#include "ROB.hpp"
#include "bus.hpp"
#include "memory.hpp"
#include "predictor.hpp"
#include "regs.hpp"
#include "rs.hpp"
#include "utils.hpp"

template <typename PredictorType, typename MemoryType, size_t ROBLength,
          size_t N_MemRS, size_t N_ALU>
    requires(std::derived_from<PredictorType, Predictor> &&
             std::derived_from<MemoryType, BaseMemory> && ROBLength > 0 &&
             N_MemRS > 0 && N_ALU > 0)
class CPU {
    Reg<uint32_t> PC;
    Regs regs;
    ReorderBuffer<ROBLength> rob;
    MemoryType mem;
    ReservationStation<MemBus> mem_rs[N_MemRS + 1];
    ALU alus[N_ALU + 1];
    ReservationStation<ALUBus> alu_rs[N_ALU + 1];
    PredictorType predictor;

    Reg<uint64_t> cycle_time;

    Wire<uint32_t> next_PC;
    Wire<uint32_t> full_instruction;
    Reg<bool> valid_instruction;
    Wire<RSBus> rs_bus;
    Wire<ExecuteType> execute_type;
    Wire<size_t> rs_index;
    Wire<bool> issue;

    const std::vector<Updatable *> updatables;
    const std::vector<CDBSource *> cdb_sources;

    void pullAndUpdate();

    CommonDataBus CDBSelect() const;
    size_t MemRSSelect() const;
    size_t ALURSSelect() const;

    RegValueBus regValue(uint8_t index) const;

    void PCInit();
    void baseWireInit();
    void regInit();
    void robInit();
    void memInit();
    void aluInit();
    void predictorInit();

   public:
    CPU();

    bool step(uint8_t &ret);

    PredictorStatistics predictorStatistics() const;
    MemoryStatistics memoryStatistics() const;
    size_t cycleTime() const;
};

template <typename PredictorType, typename MemoryType, size_t ROBLength,
          size_t N_MemRS, size_t N_ALU>
    requires(std::derived_from<PredictorType, Predictor> &&
             std::derived_from<MemoryType, BaseMemory> && ROBLength > 0 &&
             N_MemRS > 0 && N_ALU > 0)
void CPU<PredictorType, MemoryType, ROBLength, N_MemRS, N_ALU>::baseWireInit() {
    execute_type = LAM(getExecuteType(get_op(full_instruction)));
    rs_index = [&]() -> size_t {
        switch (execute_type) {
            case ALU_T:
                return ALURSSelect();
            case Mem_T:
                return MemRSSelect();
            default:
                return 0;
        }
    };

    issue = [&]() -> bool {
        return valid_instruction &&
               get_opType(get_op(full_instruction)) != OpType::Unknown &&
               rob.get_index() != 0 &&
               (execute_type == None_T || rs_index != 0);
    };

    rs_bus = [&]() -> RSBus {
        if (!issue) {
            return RSBus();
        }

        RSBus ret{};
        ret.reorder_index = rob.get_index();

        auto rs1 = get_rs1(full_instruction);
        auto rs2 = get_rs2(full_instruction);
        auto op = get_op(full_instruction);
        auto op_type = get_opType(op);
        auto subop = get_subop(full_instruction);
        auto imm = get_imm(full_instruction);
        auto shamt = get_shamt(full_instruction);

        // 设置 qj vj
        switch (op_type) {
            case Unknown:
                assert(false);
            case U:
                ret.vj = op == 0b0110111 ? 0 : PC;
                break;
            case J:
                ret.vj = PC;
                break;
            case I:
                if (op == 0b1100111) {
                    ret.vj = PC;
                } else {
                    RegValueBus vb = regValue(rs1);
                    ret.vj = vb.v;
                    ret.qj = vb.q;
                }
                break;
            case S:
            case B:
            case R:
                RegValueBus vb = regValue(rs1);
                ret.vj = vb.v;
                ret.qj = vb.q;
                break;
        }

        // 设置 qk vk
        switch (op_type) {
            case Unknown:
                assert(false);
            case U:
                ret.vk = imm;
                break;
            case J:
                ret.vk = 4;
                break;
            case I:
                if (op == 0b1100111) {
                    ret.vk = 4;
                } else {
                    if (subop == 0b001 || subop == 0b101) {
                        ret.vk = shamt;
                    } else {
                        ret.vk = imm;
                    }
                }
                break;
            case S:
                ret.vk = imm;
                break;
            case B:
            case R:
                RegValueBus vb = regValue(rs2);
                ret.vk = vb.v;
                ret.qk = vb.q;
                break;
        }

        // 设置 subop
        if (op_type == B) {
            switch (subop) {
                case 0b000:
                case 0b001:
                    ret.subop = 0b000;  // sub
                    break;
                case 0b100:
                case 0b101:
                    ret.subop = 0b010;  // slt
                    break;
                case 0b110:
                case 0b111:
                    ret.subop = 0b011;  // sltu
                    break;
                default:
                    ret.subop = 0b000;
            }
        } else if (op_type == U || op_type == J || op_type == S) {
            ret.subop = 0b000;
        } else {
            ret.subop = subop;
        }

        ret.variant_flag = get_variant_flag(full_instruction);
        ret.imm = imm;

        return ret;
    };
}

template <typename PredictorType, typename MemoryType, size_t ROBLength,
          size_t N_MemRS, size_t N_ALU>
    requires(std::derived_from<PredictorType, Predictor> &&
             std::derived_from<MemoryType, BaseMemory> && ROBLength > 0 &&
             N_MemRS > 0 && N_ALU > 0)
void CPU<PredictorType, MemoryType, ROBLength, N_MemRS, N_ALU>::regInit() {
    regs.commit_bus = LAM(rob.regCommit());
    regs.issue_bus = [&]() -> RegIssueBus {
        if (issue) {
            return RegIssueBus{get_rd(full_instruction), rob.get_index()};
        }
        return RegIssueBus();
    };
    regs.clear = LAM(rob.clear());
}

template <typename PredictorType, typename MemoryType, size_t ROBLength,
          size_t N_MemRS, size_t N_ALU>
    requires(std::derived_from<PredictorType, Predictor> &&
             std::derived_from<MemoryType, BaseMemory> && ROBLength > 0 &&
             N_MemRS > 0 && N_ALU > 0)
void CPU<PredictorType, MemoryType, ROBLength, N_MemRS, N_ALU>::PCInit() {
    next_PC = [&]() -> uint32_t {
        auto relocate = rob.PCRelocate();
        uint32_t address = PC;
        uint32_t offset = 0;
        if (relocate.flag) {
            address = relocate.address;
            offset = relocate.offset;
        } else if (issue) {
            offset = 4;
            switch (get_op(full_instruction)) {
                case 0b1101111U: /* jal */
                    offset = get_imm(full_instruction);
                    break;
                case 0b1100011U: /* branch */
                    if (predictor.branch()) {
                        offset = get_imm(full_instruction);
                    }
                    break;
                case 0b1100111U: /* jalr */
                    RegValueBus rb = regValue(get_rs1(full_instruction));
                    if (rb.q == 0) {
                        address = rb.v;
                        offset = get_imm(full_instruction);
                    }
                    break;
            }
        }

        return (address + offset) & 0xFFFFFFFEU;
    };

    PC <= LAM(next_PC);
}

template <typename PredictorType, typename MemoryType, size_t ROBLength,
          size_t N_MemRS, size_t N_ALU>
    requires(std::derived_from<PredictorType, Predictor> &&
             std::derived_from<MemoryType, BaseMemory> && ROBLength > 0 &&
             N_MemRS > 0 && N_ALU > 0)
void CPU<PredictorType, MemoryType, ROBLength, N_MemRS, N_ALU>::robInit() {
    rob.PC = LAM(PC);
    rob.add_instruction = LAM(issue);
    rob.branched = LAM(predictor.branch());
    rob.full_instruction = LAM(full_instruction);
    rob.cdb = LAM(CDBSelect());
}

template <typename PredictorType, typename MemoryType, size_t ROBLength,
          size_t N_MemRS, size_t N_ALU>
    requires(std::derived_from<PredictorType, Predictor> &&
             std::derived_from<MemoryType, BaseMemory> && ROBLength > 0 &&
             N_MemRS > 0 && N_ALU > 0)
void CPU<PredictorType, MemoryType, ROBLength, N_MemRS, N_ALU>::memInit() {
    for (size_t i = 1; i <= N_MemRS; i++) {
        mem_rs[i].new_instruction = [&, i]() -> RSBus {
            return (execute_type == Mem_T && rs_index == i) ? rs_bus : RSBus();
        };
        mem_rs[i].cdb = LAM(CDBSelect());
        mem_rs[i].clear = LAM(rob.clear());
    }

    mem.cdb = LAM(CDBSelect());
    mem.PC = LAM(next_PC);
    mem.clear = LAM(rob.clear());
    mem.write_bus = LAM(rob.store());
    mem.read_bus = [&]() -> MemBus {
        return BusSelect<MemBus>(
            mem_rs, [&](ReservationStation<MemBus> &x) -> MemBus {
                return x.execute(rob);
            });
    };
}

template <typename PredictorType, typename MemoryType, size_t ROBLength,
          size_t N_MemRS, size_t N_ALU>
    requires(std::derived_from<PredictorType, Predictor> &&
             std::derived_from<MemoryType, BaseMemory> && ROBLength > 0 &&
             N_MemRS > 0 && N_ALU > 0)
void CPU<PredictorType, MemoryType, ROBLength, N_MemRS, N_ALU>::aluInit() {
    for (size_t i = 1; i <= N_ALU; i++) {
        alu_rs[i].new_instruction = [&, i]() -> RSBus {
            return (execute_type == ALU_T && rs_index == i) ? rs_bus : RSBus();
        };
        alu_rs[i].cdb = LAM(CDBSelect());
        alu_rs[i].clear = LAM(rob.clear());
    }

    for (size_t i = 1; i <= N_ALU; i++) {
        alus[i].cdb = LAM(CDBSelect());
        alus[i].clear = LAM(rob.clear());
        alus[i].bus = [&, i]() { return alu_rs[i].execute(rob); };
    }
}

template <typename PredictorType, typename MemoryType, size_t ROBLength,
          size_t N_MemRS, size_t N_ALU>
    requires(std::derived_from<PredictorType, Predictor> &&
             std::derived_from<MemoryType, BaseMemory> && ROBLength > 0 &&
             N_MemRS > 0 && N_ALU > 0)
void CPU<PredictorType, MemoryType, ROBLength, N_MemRS,
         N_ALU>::predictorInit() {
    predictor.PC = LAM(PC);
    predictor.feedback = LAM(rob.predictFeedback());
}

template <typename PredictorType, typename MemoryType, size_t ROBLength,
          size_t N_MemRS, size_t N_ALU>
    requires(std::derived_from<PredictorType, Predictor> &&
             std::derived_from<MemoryType, BaseMemory> && ROBLength > 0 &&
             N_MemRS > 0 && N_ALU > 0)
CPU<PredictorType, MemoryType, ROBLength, N_MemRS, N_ALU>::CPU()
    : PC(),
      regs(),
      rob(regs),
      mem(),
      mem_rs(),
      alus(),
      alu_rs(),
      cycle_time(0),
      updatables(collectPointer<Updatable>(cycle_time, PC, regs, rob, mem,
                                           mem_rs, alus, alu_rs, predictor,
                                           valid_instruction)),
      cdb_sources(collectPointer<CDBSource>(mem, alus)) {
    cycle_time <= LAM(cycle_time + 1);
    full_instruction = LAM(mem.get_instruction());
    valid_instruction = false;
    valid_instruction <= [&]() -> bool { return true; };

    baseWireInit();
    PCInit();
    regInit();
    robInit();
    memInit();
    aluInit();
    predictorInit();
}

template <typename PredictorType, typename MemoryType, size_t ROBLength,
          size_t N_MemRS, size_t N_ALU>
    requires(std::derived_from<PredictorType, Predictor> &&
             std::derived_from<MemoryType, BaseMemory> && ROBLength > 0 &&
             N_MemRS > 0 && N_ALU > 0)
bool CPU<PredictorType, MemoryType, ROBLength, N_MemRS, N_ALU>::step(
    uint8_t &ret) {
    if (rob.commit() && rob.front().full_instruction == 0x0ff00513U) {
        ret = regs.reg(10);
        return true;
    }
    pullAndUpdate();
    return false;
}

template <typename PredictorType, typename MemoryType, size_t ROBLength,
          size_t N_MemRS, size_t N_ALU>
    requires(std::derived_from<PredictorType, Predictor> &&
             std::derived_from<MemoryType, BaseMemory> && ROBLength > 0 &&
             N_MemRS > 0 && N_ALU > 0)
void CPU<PredictorType, MemoryType, ROBLength, N_MemRS,
         N_ALU>::pullAndUpdate() {
#ifdef TRACE
    bool commit = rob.commit();
    uint32_t commit_PC = rob.front().PC;
#endif
    for (auto &x : updatables) {
        x->pull();
    }

    for (auto &x : updatables) {
        x->update();
    }
#ifdef TRACE
    if (commit) {
        std::cout << std::format("Commit PC: 0x{:08X}, regs: ", commit_PC);
        for (int i = 0; i < 32; i++) {
            std::cout << std::format("0x{:08X} ", regs.reg(i));
        }
        std::cout << std::endl;
    }
#endif
}

template <typename PredictorType, typename MemoryType, size_t ROBLength,
          size_t N_MemRS, size_t N_ALU>
    requires(std::derived_from<PredictorType, Predictor> &&
             std::derived_from<MemoryType, BaseMemory> && ROBLength > 0 &&
             N_MemRS > 0 && N_ALU > 0)
CommonDataBus
CPU<PredictorType, MemoryType, ROBLength, N_MemRS, N_ALU>::CDBSelect() const {
    return BusSelect<CommonDataBus>(cdb_sources,
                                    [](CDBSource *x) { return x->CDBOut(); });
}

template <typename PredictorType, typename MemoryType, size_t ROBLength,
          size_t N_MemRS, size_t N_ALU>
    requires(std::derived_from<PredictorType, Predictor> &&
             std::derived_from<MemoryType, BaseMemory> && ROBLength > 0 &&
             N_MemRS > 0 && N_ALU > 0)
size_t CPU<PredictorType, MemoryType, ROBLength, N_MemRS, N_ALU>::MemRSSelect()
    const {
    for (size_t i = 1; i <= N_MemRS; i++) {
        if (!mem_rs[i].is_busy()) {
            return i;
        }
    }
    return 0;
}

template <typename PredictorType, typename MemoryType, size_t ROBLength,
          size_t N_MemRS, size_t N_ALU>
    requires(std::derived_from<PredictorType, Predictor> &&
             std::derived_from<MemoryType, BaseMemory> && ROBLength > 0 &&
             N_MemRS > 0 && N_ALU > 0)
size_t CPU<PredictorType, MemoryType, ROBLength, N_MemRS, N_ALU>::ALURSSelect()
    const {
    for (size_t i = 1; i <= N_ALU; i++) {
        if (!alu_rs[i].is_busy()) {
            return i;
        }
    }
    return 0;
}

// 如果 reorder 为 0，直接返回寄存器的值；如果 reorder 不为零，则去 rob
// 里面找该条记录，如果 ready 则返回对应的值，否则返回 reorder
template <typename PredictorType, typename MemoryType, size_t ROBLength,
          size_t N_MemRS, size_t N_ALU>
    requires(std::derived_from<PredictorType, Predictor> &&
             std::derived_from<MemoryType, BaseMemory> && ROBLength > 0 &&
             N_MemRS > 0 && N_ALU > 0)
RegValueBus CPU<PredictorType, MemoryType, ROBLength, N_MemRS, N_ALU>::regValue(
    uint8_t index) const {
    auto reorder_index = regs.reorder(index);
    if (reorder_index == 0) {
        return RegValueBus{0, regs.reg(index)};
    } else {
        auto item = rob.getItem(reorder_index);
        if (item.ready) {
            return RegValueBus{0, item.value};
        }

        CommonDataBus cdb_fetched = CDBSelect();
        if (cdb_fetched.reorder_index == reorder_index) {
            return RegValueBus{0, cdb_fetched.data};
        }

        return RegValueBus{reorder_index, 0};
    }
}

template <typename PredictorType, typename MemoryType, size_t ROBLength,
          size_t N_MemRS, size_t N_ALU>
    requires(std::derived_from<PredictorType, Predictor> &&
             std::derived_from<MemoryType, BaseMemory> && ROBLength > 0 &&
             N_MemRS > 0 && N_ALU > 0)
PredictorStatistics CPU<PredictorType, MemoryType, ROBLength, N_MemRS,
                        N_ALU>::predictorStatistics() const {
    return predictor.predictorStatistics();
}

template <typename PredictorType, typename MemoryType, size_t ROBLength,
          size_t N_MemRS, size_t N_ALU>
    requires(std::derived_from<PredictorType, Predictor> &&
             std::derived_from<MemoryType, BaseMemory> && ROBLength > 0 &&
             N_MemRS > 0 && N_ALU > 0)
MemoryStatistics CPU<PredictorType, MemoryType, ROBLength, N_MemRS,
                        N_ALU>::memoryStatistics() const {
    return mem.memoryStatistics();
}

template <typename PredictorType, typename MemoryType, size_t ROBLength,
          size_t N_MemRS, size_t N_ALU>
    requires(std::derived_from<PredictorType, Predictor> &&
             std::derived_from<MemoryType, BaseMemory> && ROBLength > 0 &&
             N_MemRS > 0 && N_ALU > 0)
size_t CPU<PredictorType, MemoryType, ROBLength, N_MemRS, N_ALU>::cycleTime()
    const {
    return cycle_time;
}