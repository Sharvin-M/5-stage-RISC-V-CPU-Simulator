#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <array>
#include <cstdint>
#include <iomanip>

class RiscvPipeline
{
private:
    
    uint32_t pc = 0; /* Program Counter. */
    unsigned long clock_cycle = 0; /* Current Clock Cycle (basically clock_cyle++ represents one cpu tick).*/
    std::array<uint32_t, 32> data_memory{}; /* Array of 32 ints, each of which are 32 bytes large to represent data memory (off chip mem)*/
    std::array<uint32_t, 32> register_file{}; /* Array of 32 ints, each are 32 bytes in size, to represent the register file. This is where the registers are located. For example, sp, ra, x1, etc. ..*/
    std::vector<std::string> instruction_memory; /* A dynamic array of strings, each of which represents a 32-bit instruction saved as a string. This is read from the user input, which is a text file containing 32-bit instructions on each newline. */
    bool stall_pipeline = false; /* Is the pipeline currently stalled or not? Need to track this for pipelined structure. */
    int write_data; /* if the instruction writes back to the register, then save the data that was written in this global variable,  */

    // --- Struct Definitions ---
    struct ControlSignals
    {
        bool reg_write = false;
        bool branch = false;
        bool jump = false;
        bool alu_src = false;
        uint8_t alu_op = 0;
        bool mem_write = false;
        bool mem_to_reg = false;
        bool mem_read = false;
        uint8_t alu_ctrl = 0;
    };

    struct IF_ID
    {
        bool valid = false;
        uint32_t pc = 0;
        uint32_t next_pc = 0;
        std::string char_instr;
        uint32_t int_instr = 0;
    };

    struct ID_EX
    {
        bool valid = false;
        uint32_t pc = 0;
        uint32_t next_pc = 0;
        int opcode = 0;
        int funct3 = 0;
        int funct7 = 0;
        int rs1 = 0;
        int rs2 = 0;
        int rd = 0;
        int rs1_val = 0;
        int rs2_val = 0;
        int imm = 0;
        ControlSignals ctrl{};
    };

    struct EX_MEM
    {
        bool valid = false;
        uint32_t pc = 0;
        uint32_t next_pc = 0;
        int alu_result = 0;
        bool alu_zero = false;
        uint32_t branch_target = 0;
        bool take_branch = false;
        int rs2_val = 0;
        int rd = 0;
        ControlSignals ctrl{};
    };

    struct MEM_WB
    {
        bool valid = false;
        uint32_t next_pc = 0;
        int mem_read_data = 0;
        int alu_result = 0;
        int rd = 0;
        ControlSignals ctrl{};
    };

    // --- Pipeline Registers ---
    IF_ID if_id{}, next_if_id{};
    ID_EX id_ex{}, next_id_ex{};
    EX_MEM ex_mem{}, next_ex_mem{};
    MEM_WB mem_wb{}, next_mem_wb{};

    // --- Internal Helpers ---
    void d_mem_init(uint32_t addr, uint32_t data)
    {
        data_memory[addr / 4] = data;
    }

    void ControlUnit(int opcode, int funct3, int funct7, ControlSignals &ctrl)
    {
        // Reset controls first
        ctrl = ControlSignals{};

        switch (opcode)
        {
        case 3: // lw
            ctrl.reg_write = true;
            ctrl.alu_src = true;
            ctrl.mem_to_reg = true;
            ctrl.mem_read = true;
            break;
        case 19: // I-type
            ctrl.reg_write = true;
            ctrl.alu_src = true;
            ctrl.alu_op = 2;
            break;
        case 35: // sw
            ctrl.alu_src = true;
            ctrl.mem_write = true;
            break;
        case 51: // R-type
            ctrl.reg_write = true;
            ctrl.alu_op = 2;
            break;
        case 99: // branch
            ctrl.branch = true;
            ctrl.alu_op = 1;
            break;
        case 111: // jal
            ctrl.reg_write = true;
            ctrl.jump = true;
            break;
        case 103: // jalr
            ctrl.reg_write = true;
            ctrl.jump = true;
            ctrl.alu_src = true;
            ctrl.alu_op = 2;
            break;
        default:
            break;
        }

        switch (ctrl.alu_op)
        {
        case 0:
            ctrl.alu_ctrl = 2;
            break; // ADD (Loads / Stores / Jumps)
        case 1:
            ctrl.alu_ctrl = 6;
            break; // SUB (Branches)
        case 2:
            if (opcode == 19)
            {
                if (funct3 == 0)
                    ctrl.alu_ctrl = 2; // addi
                if (funct3 == 7)
                    ctrl.alu_ctrl = 0; // andi
                if (funct3 == 6)
                    ctrl.alu_ctrl = 1; // ori
            }
            else if (opcode == 51)
            {
                if (funct3 == 0 && funct7 == 0)
                    ctrl.alu_ctrl = 2; // add
                if (funct3 == 0 && funct7 == 32)
                    ctrl.alu_ctrl = 6; // sub
                if (funct3 == 7 && funct7 == 0)
                    ctrl.alu_ctrl = 0; // and
                if (funct3 == 6 && funct7 == 0)
                    ctrl.alu_ctrl = 1; // or
            }
            break;
        }
    }

    // --- Pipeline Stages ---
    void Fetch()
    {
        if (stall_pipeline)
            return;

        unsigned int target_idx = pc / 4;
        next_if_id = IF_ID{}; // Clear next state

        if (target_idx < instruction_memory.size())
        {
            next_if_id.char_instr = instruction_memory[target_idx];
            // Parse binary string to uint32_t
            next_if_id.int_instr = static_cast<uint32_t>(std::stoul(next_if_id.char_instr, nullptr, 2));
            next_if_id.pc = pc;
            next_if_id.next_pc = pc + 4;
            next_if_id.valid = true;

            pc += 4; // Speculative PC update
        }
    }

    void Decode()
    {
        next_id_ex = ID_EX{};
        next_id_ex.valid = if_id.valid;
        if (!if_id.valid)
            return;

        uint32_t int_instr = if_id.int_instr;

        next_id_ex.opcode = int_instr & 0x7F;
        next_id_ex.rd = (int_instr >> 7) & 0x1F;
        next_id_ex.funct3 = (int_instr >> 12) & 0x07;
        next_id_ex.rs1 = (int_instr >> 15) & 0x1F;
        next_id_ex.rs2 = (int_instr >> 20) & 0x1F;
        next_id_ex.funct7 = (int_instr >> 25) & 0x7F;

        next_id_ex.rs1_val = register_file[next_id_ex.rs1];
        next_id_ex.rs2_val = register_file[next_id_ex.rs2];
        next_id_ex.pc = if_id.pc;
        next_id_ex.next_pc = if_id.next_pc;

        int opcode = next_id_ex.opcode;
        int imm = 0;

        bool stall = false;
        if (id_ex.ctrl.mem_read && id_ex.rd != 0 &&
            (id_ex.rd == next_id_ex.rs1 || id_ex.rd == next_id_ex.rs2)) {
            stall = true;
        }
        if (stall) {
            next_id_ex = ID_EX{};
            stall_pipeline = true;
            return;
        }

        stall_pipeline = false;

        if (opcode == 0x33 || opcode == 0x3B)
        { // R-Type
            imm = 0;
            }
            else if (opcode == 0x67 || opcode == 0x03 || opcode == 0x13)
            { // I-Type
                imm = static_cast<int32_t>(int_instr) >> 20;
            }
            else if (opcode == 0x23)
            { // S-Type
                imm = ((static_cast<int32_t>(int_instr & 0xFE000000) >> 20) | ((int_instr >> 7) & 0x1F));
            }
            else if (opcode == 0x63)
            { // B-Type
                imm = ((static_cast<int32_t>(int_instr & 0x80000000) >> 19) |
                       ((int_instr & 0x00000080) << 4) |
                       ((int_instr & 0x7E000000) >> 20) |
                       ((int_instr & 0x00000F00) >> 7));
            }
            else if (opcode == 0x17 || opcode == 0x37)
            { // U-Type
                imm = int_instr & 0xFFFFF000;
            }
            else if (opcode == 0x6F)
            { // J-Type
                imm = ((static_cast<int32_t>(int_instr & 0x80000000) >> 11) |
                       (int_instr & 0x000FF000) |
                       ((int_instr & 0x00100000) >> 9) |
                       ((int_instr & 0x7FE00000) >> 20));
            }

        next_id_ex.imm = imm;
        ControlUnit(opcode, next_id_ex.funct3, next_id_ex.funct7, next_id_ex.ctrl);
    }

    void Execute()
    {
        next_ex_mem = EX_MEM{};
        next_ex_mem.valid = id_ex.valid;
        if (!id_ex.valid)
            return;

        int forwardA = 0;
        int forwardB = 0;

        if (ex_mem.ctrl.reg_write && ex_mem.rd != 0)
        {
            if (ex_mem.rd == id_ex.rs1)
                forwardA = 2;
            if (ex_mem.rd == id_ex.rs2)
                forwardB = 2;
        }

        if (mem_wb.ctrl.reg_write && mem_wb.rd != 0)
        {
            if (mem_wb.rd == id_ex.rs1 && forwardA != 2)
                forwardA = 1;
            if (mem_wb.rd == id_ex.rs2 && forwardB != 2)
                forwardB = 1;
        }

        int alu_input_A = id_ex.rs1_val;
        if (forwardA == 2)
            alu_input_A = ex_mem.alu_result;
        else if (forwardA == 1)
            alu_input_A = write_data; // Helper function to get what WB is about to write

        int forwarded_rs2_val = id_ex.rs2_val;
        if (forwardB == 2)
            forwarded_rs2_val = ex_mem.alu_result;
        else if (forwardB == 1)
            forwarded_rs2_val = write_data;

        // The second ALU operand is either the forwarded Rs2 OR the immediate value
        int alu_input_B = id_ex.ctrl.alu_src ? id_ex.imm : forwarded_rs2_val;

        switch (id_ex.ctrl.alu_ctrl)
        {
        case 0:
            next_ex_mem.alu_result = alu_input_A & alu_input_B;
            break;
        case 1:
            next_ex_mem.alu_result = alu_input_A | alu_input_B;
            break;
        case 2:
            next_ex_mem.alu_result = alu_input_A + alu_input_B;
            break;
        case 6:
            next_ex_mem.alu_result = alu_input_A - alu_input_B;
            break;
        case 7:
            next_ex_mem.alu_result = (alu_input_A < alu_input_B) ? 1 : 0;
            break;
        default:
            next_ex_mem.alu_result = 0;
            break;
        }

        next_ex_mem.rs2_val = forwarded_rs2_val;
    
        next_ex_mem.alu_zero = (next_ex_mem.alu_result == 0);

        bool take_branch = false;
        if (id_ex.ctrl.branch)
        {
            if (id_ex.funct3 == 0x0 && next_ex_mem.alu_zero)
                take_branch = true; // beq
            if (id_ex.funct3 == 0x1 && !next_ex_mem.alu_zero)
                take_branch = true; // bne
        }

        if (id_ex.ctrl.jump || take_branch)
        {
            next_ex_mem.take_branch = true;
            if (id_ex.ctrl.jump && id_ex.opcode == 103)
            {
                next_ex_mem.branch_target = (id_ex.rs1_val + id_ex.imm) & ~1; // jalr
            }
            else
            {
                next_ex_mem.branch_target = id_ex.pc + id_ex.imm; // jal and branches
            }
        }

        // Pass along needed data
        next_ex_mem.rs2_val = id_ex.rs2_val;
        next_ex_mem.rd = id_ex.rd;
        next_ex_mem.ctrl = id_ex.ctrl;
        next_ex_mem.pc = id_ex.pc;
        next_ex_mem.next_pc = id_ex.next_pc;
    }

    void Mem()
    {
        next_mem_wb = MEM_WB{};
        next_mem_wb.valid = ex_mem.valid;
        if (!ex_mem.valid)
            return;

        int idx = ex_mem.alu_result / 4;

        if (ex_mem.ctrl.mem_write)
        {
            data_memory[idx] = ex_mem.rs2_val;
            std::cout << "total_clock_cycles " << clock_cycle
                      << " :\nmemory 0x" << std::hex << ex_mem.alu_result
                      << " is modified to 0x" << ex_mem.rs2_val << std::dec << "\n";
        }
        if (ex_mem.ctrl.mem_read)
        {
            next_mem_wb.mem_read_data = data_memory[idx];
        }

        // Handle PC updates for branches/jumps in memory stage
        if (ex_mem.take_branch)
        {
            pc = ex_mem.branch_target;
            std::cout << "pc is modified to 0x" << std::hex << pc << std::dec << " (Branch/Jump Taken)\n";

            next_if_id = IF_ID{};
            next_id_ex = ID_EX{};
        }

        next_mem_wb.alu_result = ex_mem.alu_result;
        next_mem_wb.rd = ex_mem.rd;
        next_mem_wb.ctrl = ex_mem.ctrl;
        next_mem_wb.next_pc = ex_mem.next_pc;
    }

    void Writeback()
    {
        if (!mem_wb.valid)
            return;

        if (mem_wb.ctrl.reg_write && mem_wb.rd != 0)
        {
            if (mem_wb.ctrl.mem_to_reg)
                write_data = mem_wb.mem_read_data;
            else if (mem_wb.ctrl.jump)
                write_data = mem_wb.next_pc;
            else
                write_data = mem_wb.alu_result;

            register_file[mem_wb.rd] = write_data;

            std::cout << "total_clock_cycles " << clock_cycle
                      << " : x" << mem_wb.rd << " is modified to 0x"
                      << std::hex << register_file[mem_wb.rd] << std::dec << "\n";
        }
    }

    void UpdateRegisters()
    {
        if_id = next_if_id;
        id_ex = next_id_ex;
        ex_mem = next_ex_mem;
        mem_wb = next_mem_wb;
    }

    void PrintPipelineTrace()
    {
        std::cout << "==========================================\n";
        std::cout << " Clock Cycle: " << clock_cycle << "\n";
        std::cout << "==========================================\n";

        // 1. FETCH STAGE (Looks at what Fetch put into next_if_id)
        std::cout << "[FETCH]     ";
        if (stall_pipeline)
        {
            std::cout << "STALLED (PC held at 0x" << std::hex << pc << std::dec << ")\n";
        }
        else if (next_if_id.valid)
        {
            std::cout << "Fetched PC: 0x" << std::hex << next_if_id.pc << std::dec
                      << " (Instr: 0x" << std::hex << next_if_id.int_instr << std::dec << ")\n";
        }
        else
        {
            std::cout << "NOP (Bubble)\n";
        }

        // 2. DECODE STAGE (Looks at what Decode put into next_id_ex)
        std::cout << "[DECODE]    ";
        if (next_id_ex.valid)
        {
            std::cout << "Opcode: " << next_id_ex.opcode
                      << " | Rs1: x" << next_id_ex.rs1 << " (Val: " << next_id_ex.rs1_val << ")"
                      << " | Rs2: x" << next_id_ex.rs2 << " (Val: " << next_id_ex.rs2_val << ")"
                      << " | Rd: x" << next_id_ex.rd
                      << " | Imm: " << next_id_ex.imm << "\n";
        }
        else
        {
            std::cout << "NOP (Bubble)\n";
        }

        // 3. EXECUTE STAGE (Looks at what Execute put into next_ex_mem)
        std::cout << "[EXECUTE]   ";
        if (next_ex_mem.valid)
        {
            std::cout << "ALU Result: 0x" << std::hex << next_ex_mem.alu_result << std::dec
                      << " | Zero: " << (next_ex_mem.alu_zero ? "T" : "F")
                      << " | Branch Target: 0x" << std::hex << next_ex_mem.branch_target << std::dec << "\n";
        }
        else
        {
            std::cout << "NOP (Bubble)\n";
        }

        // 4. MEMORY STAGE (Looks at what Mem put into next_mem_wb)
        std::cout << "[MEMORY]    ";
        if (next_mem_wb.valid)
        {
            if (next_mem_wb.ctrl.mem_write)
                std::cout << "Writing to Mem... ";
            else if (next_mem_wb.ctrl.mem_read)
                std::cout << "Reading from Mem... (Read Data: " << next_mem_wb.mem_read_data << ")\n";
            else
                std::cout << "No Mem Access\n";
        }
        else
        {
            std::cout << "NOP (Bubble)\n";
        }

        // 5. WRITEBACK STAGE (Looks at what WB just wrote to the register file)
        std::cout << "[WRITEBACK] ";
        if (mem_wb.valid)
        {
            if (mem_wb.ctrl.reg_write && mem_wb.rd != 0)
            {
                std::cout << "Wrote Data: " << write_data << " to Register x" << mem_wb.rd << "\n";
            }
            else
            {
                std::cout << "No Register Write\n";
            }
        }
        else
        {
            std::cout << "NOP (Bubble)\n";
        }

        std::cout << "\n";
    }

public:
    RiscvPipeline()
    {
        // Initialize Base State
        register_file[1] = 0x20;
        register_file[2] = 0x5;
        register_file[10] = 0x70;
        register_file[11] = 0x4;

        d_mem_init(0x70, 0x5);
        d_mem_init(0x74, 0x10);
    }

    bool LoadProgram(const std::string &filename)
    {
        std::ifstream file(filename);
        if (!file.is_open())
        {
            std::cerr << "Error opening instruction file: " << filename << "\n";
            return false;
        }

        std::string line;
        while (std::getline(file, line))
        {
            // Remove potential carriage returns for cross-platform safety
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (!line.empty())
            {
                instruction_memory.push_back(line);
            }
        }
        return true;
    }

    void Run()
    {
        while (true)
        {
            Writeback();
            Mem();
            Execute();
            Decode();
            Fetch();

            if (!next_if_id.valid && !id_ex.valid && !ex_mem.valid && !mem_wb.valid)
            {
                break; // Pipeline drained
            }

            PrintPipelineTrace();

            clock_cycle++;
            UpdateRegisters();
        }

        std::cout << "program terminated\n";
        std::cout << "total execution time is " << clock_cycle << " cycles\n";
    }
};

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <instruction_file>\n";
        return 1;
    }

    RiscvPipeline cpu;

    if (!cpu.LoadProgram(argv[1]))
    {
        return 1;
    }

    cpu.Run();

    return 0;
}