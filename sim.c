#include "rv32i.h"

int main(int argc, char** argv) {
        
/* The following code initializes values in some registers
   and in some parts of the main memory. It then reads 32-bit
   instructions from a text file and executes the corresponding
   operations.
*/

    rf[1]  = 0x20; 
    rf[2]  = 0x5;  
    rf[10] = 0x70; 
    rf[11] = 0x4;  

    d_mem_init( 0x70, 0x5 );
    d_mem_init( 0x74, 0x10 );


    FILE *program = fopen( argv[1], "r");
    if (program == NULL) {
        perror("Error opening instruction file");
        return 1;
    }


    while (1) {
        //fetch instruction until none left
        if (!Fetch(program, char_instr, sizeof(char_instr))) {
            break;
        }

        //decode instruction
        Decode();

        ControlUnit(opcode, funct3, funct7);

        Execute(alu_ctrl);  

        Mem(alu_result, rs2_val, mem_read, mem_write);

        Writeback();

        // Output Print Formatting
        if (reg_write == 1 && rd != 0) {
            printf("total_clock_cycles %lu : x%d is modified to 0x%x ", clock_cycle, rd, rf[rd]);
        } 
        else if (mem_write == 1) {
            printf("total_clock_cycles %lu :\nmemory 0x%x is modified to 0x%x ", clock_cycle, alu_result, rs2_val);
        } 
        else { 
            printf("total_clock_cycles %lu : ", clock_cycle);
        }

        // UPDATE PC
        if (jump || use_branch_target) {
            pc = branch_target;
        } else {
            pc = pc + 4;
        }
        
        printf("pc is modified to 0x%x\n", pc);
    }

    fclose(program);
    printf("program terminated\n");
    printf("total execution time is %lu cycles\n", clock_cycle);
    return 0;
}
