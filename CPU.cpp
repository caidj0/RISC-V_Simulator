#include "CPU.hpp"

#include <cstddef>
#include <cstdint>

#include "bus.hpp"
#include "regs.hpp"
#include "utils.hpp"

ExecuteType getExecuteType(uint8_t op) {
    if (op == 0b0110111U /* lui */ || op == 0b0100011U /* sb, sh, sw */) {
        return None_T;
    }
    if (op == 0b0000011U /* lb, lh, lw, lbu, lhu */) {
        return Mem_T;
    }

    return ALU_T;
}

void CPU::baseWireInit() {
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
        return rob.next_index() != 0 &&
               (execute_type == None_T || rs_index != 0);
    };

    rs_bus = [&]() -> RSBus {
        RSBus ret;
        ret.reorder_index = rob.next_index();

        auto rs1 = get_rs1(full_instruction);
        auto rs2 = get_rs1(full_instruction);
        auto op = get_op(full_instruction);
        auto op_type = get_opType(op);
        auto subop = get_subop(full_instruction);
        auto imm = get_imm(full_instruction);
        auto shamt = get_shamt(full_instruction);

        // 设置 qj vj
        switch (op_type) {
            case U:
                ret.vj = op == 0b0110111 ? 0 : instruction_PC;
                break;
            case J:
                ret.vj = instruction_PC;
                break;
            case I:
                if (op == 0b1100111) {
                    ret.vj = instruction_PC;
                } else {
                    ret.vj = regs.reg(rs1);
                    ret.qj = regs.reorder(rs1);
                }
                break;
            case S:
            case B:
            case R:
                ret.vj = regs.reg(rs1);
                ret.qj = regs.reorder(rs1);
                break;
        }

        // 设置 qk vk
        switch (op_type) {
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
            case B:
            case R:
                ret.vk = regs.reg(rs2);
                ret.qk = regs.reorder(rs2);
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
        } else if (op_type == U || op_type == J || op_type == S ||
                   op == 0b0000011) {
            ret.subop = 0b000;
        } else {
            ret.subop = subop;
        }

        ret.variant_flag = get_variant_flag(full_instruction);
        ret.imm = imm;

        return ret;
    };
}

void CPU::regInit() {
    regs.commit_bus = LAM(rob.regCommit());
    regs.issue_bus = [&]() -> RegIssueBus {
        if (issue) {
            return RegIssueBus{get_rd(full_instruction), rob.next_index()};
        }
        return RegIssueBus();
    };
    regs.clear = LAM(rob.clear());
}

void CPU::PCInit() {
    PC <= [&]() -> uint32_t {
        auto relocate = rob.PCRelocate();
        uint32_t address = PC;
        uint32_t offset = 4;
        if (relocate.flag) {
            address = relocate.address;
            offset = relocate.offset;
        } else if (issue) {
            switch (get_op(full_instruction)) {
                case 0b1101111U: /* jalr */
                    address = instruction_PC;
                    offset = get_imm(full_instruction);
                    break;
                case 0b1100011U: /* branch */
                    if (predictor.branch()) {
                        address = instruction_PC;
                        offset = get_imm(full_instruction);
                    }
                    break;
            }
        }

        return address + offset;
    };
    instruction_PC <= LAM(PC);
}

void CPU::robInit() {
    rob.PC = LAM(instruction_PC);
    rob.add_instruction = LAM(issue);
    rob.branched = LAM(predictor.branch());
    rob.full_instruction = LAM(full_instruction);
    rob.cdb = LAM(CDBSelect());
}

void CPU::memInit() {
    for (size_t i = 1; i <= N_MemRS; i++) {
        mem_rs[i].new_instruction = [&, i]() -> RSBus {
            return (execute_type == Mem_T && rs_index == i) ? rs_bus : RSBus();
        };
        mem_rs[i].cdb = LAM(CDBSelect());
        mem_rs[i].clear = LAM(rob.clear());
    }

    mem.cdb = LAM(CDBSelect());
    mem.PC = LAM(issue ? PC : mem.PC);
    mem.clear = LAM(rob.clear());
    mem.write_bus = LAM(rob.store());
    mem.read_bus = [&]() -> MemBus {
        return BusSelect<MemBus>(
            mem_rs, [&](const ReservationStation<MemBus> &x) -> MemBus {
                return x.execute(rob);
            });
    };
}

void CPU::aluInit() {
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
        alus[i].bus = LAM(alu_rs[i].execute(rob));
    }
}

void CPU::predictorInit() {}

CPU::CPU()
    : PC(),
      regs(),
      rob(regs),
      mem(),
      mem_rs(),
      alus(),
      alu_rs(),
      cycle_time(0),
      updatables(collectPointer<Updatable>(cycle_time, PC, regs, rob, mem,
                                           mem_rs, alus, alu_rs, predictor)),
      cdb_sources(collectPointer<CDBSource>(mem, alus)) {
    cycle_time <= LAM(cycle_time + 1);
    full_instruction = LAM(mem.get_instruction());
    valid_instruction <= [&]() -> bool { return !rob.clear(); };

    baseWireInit();
    PCInit();
    regInit();
    robInit();
    memInit();
    aluInit();
    predictorInit();
}

bool CPU::step(uint8_t &ret) {
    if (rob.commit() && rob.front().full_instruction == 0x0ff00513U) {
        ret = regs.reg(10);
        return true;
    }
    pullAndUpdate();
    return false;
}

void CPU::pullAndUpdate() {
    for (auto &x : updatables) {
        x->pull();
    }

    for (auto &x : updatables) {
        x->update();
    }
}

CommonDataBus CPU::CDBSelect() const {
    return BusSelect<CommonDataBus>(cdb_sources,
                                    [](CDBSource *x) { return x->CDBOut(); });
}

size_t CPU::MemRSSelect() const {
    for (size_t i = 1; i <= N_MemRS; i++) {
        if (!mem_rs[i].is_busy()) {
            return i;
        }
    }
    return 0;
}

size_t CPU::ALURSSelect() const {
    for (size_t i = 1; i <= N_ALU; i++) {
        if (!alu_rs[i].is_busy()) {
            return i;
        }
    }
    return 0;
}