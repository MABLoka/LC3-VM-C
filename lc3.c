#include <stdio.h>
#include <stdint.h>
#include <signal.h>

#include <Windows.h>
#include <conio.h>  // _kbhit

//Memory Storage
#define MEMORY_MAX (1 << 16) //Maximum amount of memory location in the LC-3. 65536

int DEBUG = 0; //Activate to enter debugging mode

int *breakpoints; //Store the locaiton of breakpoints
int num_breakpoints = 0; //Indicates the number of break points

uint16_t memory[MEMORY_MAX]; //Used to simulatate the memory storage of the machine

//Registers
enum{
    R_R0 = 0,
    R_R1,
    R_R2,
    R_R3,
    R_R4,
    R_R5,
    R_R6,
    R_R7,
    R_PC, // Program Counter
    R_COND,
    R_COUNT //number of registers
    };

//Register Storage

uint16_t reg[R_COUNT]; //Used to simulatate the registers of the machine

//Instruction Set

enum{
    OP_BR = 0, // branch
    OP_ADD,    // add 
    OP_LD,     // load
    OP_ST,     // store
    OP_JSR,    // jump register
    OP_AND,    // bitwise and
    OP_LDR,    // load register
    OP_STR,    // store register
    OP_RTI,    // unused
    OP_NOT,    // bitwise not
    OP_LDI,    // load indirect
    OP_STI,    // store indirect
    OP_JMP,    // jump 
    OP_RES,    // reserved (unused)
    OP_LEA,    // load effective address
    OP_TRAP    // execute trap
};

// Condition Flags

enum{
    FL_POS = 1 << 0, // Positive
    FL_ZRO = 1 << 1, // Zero
    FL_NEG = 1 << 2, // Negative
};

// Trap Code

enum
{
    TRAP_GETC = 0x20,  // get character from keyboard, not echoed onto the terminal
    TRAP_OUT = 0x21,   // output a character
    TRAP_PUTS = 0x22,  // output a word string
    TRAP_IN = 0x23,    // get character from keyboard, echoed onto the terminal
    TRAP_PUTSP = 0x24, // output a byte string
    TRAP_HALT = 0x25   // halt the program
};

// Memory Mapped Registers
enum
{
    MR_KBSR = 0xFE00, /* keyboard status */
    MR_KBDR = 0xFE02  /* keyboard data */
};



//Input Buffering Windows

HANDLE hStdin = INVALID_HANDLE_VALUE;
DWORD fdwMode, fdwOldMode;

void disable_input_buffering()
{
    hStdin = GetStdHandle(STD_INPUT_HANDLE);
    GetConsoleMode(hStdin, &fdwOldMode); /* save old mode */
    fdwMode = fdwOldMode
            ^ ENABLE_ECHO_INPUT  /* no input echo */
            ^ ENABLE_LINE_INPUT; /* return when one or
                                    more characters are available */
    SetConsoleMode(hStdin, fdwMode); /* set new mode */
    FlushConsoleInputBuffer(hStdin); /* clear buffer */
}

void restore_input_buffering()
{
    SetConsoleMode(hStdin, fdwOldMode);
}

uint16_t check_key()
{
    return WaitForSingleObject(hStdin, 1000) == WAIT_OBJECT_0 && _kbhit();
}

// Handle Interrupt

void handle_interrupt(int signal)
{
    restore_input_buffering();
    printf("\n");
    exit(-2);
}



// Sign Extend
// To extend values with <16 bits. 0's will be filled if the numeber is pos and 1's will be filled for neg
uint16_t sign_extend(uint16_t x, int bit_count)
{
    if ((x >> (bit_count - 1)) & 1) {
        x |= (0xFFFF << bit_count);
    }
    return x;
}


// Swap
uint16_t swap16(uint16_t x)
{
    return (x << 8) | (x >> 8);
}


//Update Falgs
void update_flags(uint16_t r){

    if (reg[r] == 0)
    {
        reg[R_COND] = FL_ZRO;
    }
    else if (reg[r] >> 15) // a 1 in the left-most bit indicates negative 
    {
        reg[R_COND] = FL_NEG;
    }
    else
    {
        reg[R_COND] = FL_POS;
    }
}


// Read Image File

void read_image_file(FILE* file)
{
    
    uint16_t origin;
    fread(&origin, sizeof(origin), 1, file);
    origin = swap16(origin);

    
    uint16_t max_read = MEMORY_MAX - origin;
    uint16_t* p = memory + origin;
    size_t read = fread(p, sizeof(uint16_t), max_read, file);

    
    for(int i = 0; i < read; i++)
    {
        
        *p = swap16(*p);
        if(DEBUG){
            printf("Memory[0x%04X] = 0x%04X\n", origin + i, *p );
        }
        ++p;
        
    }
}



// Read Image
int read_image(const char* image_path)
{
    FILE* file = fopen(image_path, "rb");
    if (!file) { return 0; };
    read_image_file(file);
    fclose(file);
    return 1;
}

// Access Memory

void mem_write(uint16_t address, uint16_t val)
{
    memory[address] = val;
}

uint16_t mem_read(uint16_t address)
{
    if (address == MR_KBSR)
    {
        if (check_key())
        {
            memory[MR_KBSR] = (1 << 15);
            memory[MR_KBDR] = getchar();
        }
        else
        {
            memory[MR_KBSR] = 0;
        }
    }
    return memory[address];
}





int main(int argc, const char* argv[]){
    
    //Load Arguments
    
    if (argc < 2){
        // Confirm usage
        printf("lc3 [image-file1] ...\n");
        exit(2);
    }

    for (int j = 1; j < argc; ++j){
        if(strcmp(argv[j], "debug") == 0){
            DEBUG = 1;
            continue;
        }
        else if (!read_image(argv[j]) && j>1)
        {
            printf("failed to load image: %s\n", argv[j]);
            exit(1);
        }
    }

    // Setup
    signal(SIGINT, handle_interrupt);
    disable_input_buffering();


    reg[R_COND] = FL_ZRO; //set the flag to zero

    int PC_START = 0x3000;
    reg[R_PC] = PC_START; // set the program counter to 0x3000


    if(DEBUG){
        
        printf("Enter the number of breakpoints: ");
        scanf("%d", &num_breakpoints);

        breakpoints = malloc(num_breakpoints * sizeof(int));

        for (int i = 0; i < num_breakpoints; i++) {
            printf("Enter location for breakpoint %d: 0x", i + 1);
            scanf("%04x", &breakpoints[i]);
        }
    }
    int running = 1;
    while(running){

        //Fetch instruction from memory
        uint16_t instr = mem_read(reg[R_PC]++);
        uint16_t op = instr >> 12; //get the op from the instr
        
        if(DEBUG){
            for (int i = 0; i < num_breakpoints; i++) {

                if(reg[R_PC] - 1 == breakpoints[i]){
                    printf("Breakpoint reached.\n");
                    printf("Fetching nex instruction...\nExecuting Instruction:  0x%04X, OP: 0x%01X, PC: 0x%04X\n", instr, op, reg[R_PC] - 1);
                    printf("Press enter 1 to continue: ");
                    char ch = ' ';
                    while(ch != '1') ch =getc(stdin);

                }
                
            }
            
        }
       
        switch (op){
            case OP_ADD:
                {

                    // get the destination register (DR)
                    uint16_t r0 = (instr >> 9) & 0x7;
                    // get the first operand (SR1)
                    uint16_t r1 = (instr >> 6) & 0x7;
                    // Check of we are in immediate mode
                    uint16_t imm_flag = (instr >> 5) & 0x1;

                    if (imm_flag)
                    {
                        uint16_t imm5 = sign_extend(instr & 0x1F, 5);
                        reg[r0] = reg[r1] + imm5;
                    }
                    else
                    {
                        uint16_t r2 = instr & 0x7;
                        reg[r0] = reg[r1] + reg[r2];
                    }

                    update_flags(r0);

                }
                break;
            case OP_AND:
                {

                    uint16_t r0 = (instr >> 9) & 0x7;
                    uint16_t r1 = (instr >> 6) & 0x7;
                    uint16_t imm_flag = (instr >> 5) & 0x1;

                    if (imm_flag)
                    {   
                        uint16_t imm5 = sign_extend(instr & 0x1F, 5);
                        reg[r0] = reg[r1] & imm5;
                    }
                    else
                    {
                        uint16_t r2 = instr & 0x7;
                        reg[r0] = reg[r1] & reg[r2];
                    }
                    update_flags(r0);

                }
                break;
            case OP_NOT:
                {

                    uint16_t r0 = (instr >> 9) & 0x7;
                    uint16_t r1 = (instr >> 6) & 0x7;

                    reg[r0] = ~reg[r1];
                    update_flags(r0);

                }   
                break;
            case OP_BR:
                {

                    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                    uint16_t cond_flag = (instr >> 9) & 0x7;
                    if (cond_flag & reg[R_COND])
                    {
                        reg[R_PC] += pc_offset;
                    }

                }
                break;
            case OP_JMP:
                {

                    // Also handles RET
                    uint16_t r1 = (instr >> 6) & 0x7;
                    reg[R_PC] = reg[r1];

                }
                break;
            case OP_JSR:
                {

                    uint16_t long_flag = (instr >> 11) & 1;
                    reg[R_R7] = reg[R_PC];
                    if (long_flag)
                    {
                        uint16_t long_pc_offset = sign_extend(instr & 0x7FF, 11);
                        reg[R_PC] += long_pc_offset;  /* JSR */
                    }
                    else
                    {
                        uint16_t r1 = (instr >> 6) & 0x7;
                        reg[R_PC] = reg[r1]; /* JSRR */
                    }

                }
                break;
            case OP_LD:
                {
                    uint16_t r0 = (instr >> 9) & 0x7;
                    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                    reg[r0] = mem_read(reg[R_PC] + pc_offset);
                    update_flags(r0);
                }
                break;
            case OP_LDI:
                {

                    uint16_t r0 = (instr >> 9) & 0x7;
                    // PCoffset 9
                    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                    // add pc_offset to the current PC, look at that memory location to get the final address
                    reg[r0] = mem_read(mem_read(reg[R_PC] + pc_offset));
                    update_flags(r0);

                }
                break;
            case OP_LDR:
                {

                    uint16_t r0 = (instr >> 9) & 0x7;
                    uint16_t r1 = (instr >> 6) & 0x7;
                    uint16_t offset = sign_extend(instr & 0x3F, 6);
                    reg[r0] = mem_read(reg[r1] + offset);
                    update_flags(r0);

                }
                break;
            case OP_LEA:
                {
                    uint16_t r0 = (instr >> 9) & 0x7;
                    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                    reg[r0] = reg[R_PC] + pc_offset;
                    update_flags(r0);
                }
                break;
            case OP_ST:
                {
                    uint16_t r0 = (instr >> 9) & 0x7;
                    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                    mem_write(reg[R_PC] + pc_offset, reg[r0]);
                }
                break;
            case OP_STI:
                {
                    uint16_t r0 = (instr >> 9) & 0x7;
                    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                    mem_write(mem_read(reg[R_PC] + pc_offset), reg[r0]);
                }
                break;
            case OP_STR:
                {
                    uint16_t r0 = (instr >> 9) & 0x7;
                    uint16_t r1 = (instr >> 6) & 0x7;
                    uint16_t offset = sign_extend(instr & 0x3F, 6);
                    mem_write(reg[r1] + offset, reg[r0]);
                }
                break;
            case OP_TRAP:

                {
                    reg[R_R7] = reg[R_PC];
                    
                    switch (instr & 0xFF)
                    {
                        case TRAP_GETC:
                            {

                                // read a single ASCII char
                                reg[R_R0] = (uint16_t)getchar();
                                update_flags(R_R0);
                            }
                            break;
                        case TRAP_OUT:
                            {

                                putc((char)reg[R_R0], stdout);
                                fflush(stdout);
                            }
                            break;

                        case TRAP_PUTS:
                            {

                                // one char per word
                                uint16_t* c = memory + reg[R_R0];
                                while (*c)
                                {
                                    putc((char)*c, stdout);
                                    ++c;
                                }
                                fflush(stdout);

                            }   
                        break;
                        case TRAP_IN:
                        {
                            printf("Enter a character: ");
                            char c = getchar();
                            putc(c, stdout);
                            fflush(stdout);
                            reg[R_R0] = (uint16_t)c;
                            update_flags(R_R0);
                        }
                        break;
                        case TRAP_PUTSP:
                        {
                            /* one char per byte (two bytes per word)
                            here we need to swap back to
                            big endian format */
                            uint16_t* c = memory + reg[R_R0];
                            while (*c)
                            {
                                char char1 = (*c) & 0xFF;
                                putc(char1, stdout);
                                char char2 = (*c) >> 8;
                                if (char2) putc(char2, stdout);
                                    ++c;
                            }
                            fflush(stdout);
                        }
                        break;
                        case TRAP_HALT:
                        {
                            puts("HALT");
                            fflush(stdout);
                            running = 0;
                        }
                        break;
                    }
                }
                break;



            case OP_RES:
            case OP_RTI:
            default:
                abort(); // in case of bad opcode
                break;
        }
    }

}
