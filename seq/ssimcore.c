/***********************************************************************
 *
 * ssim.c - Sequential Y86-64 simulator
 * 
 * Copyright (c) 2002, 2015. Bryant and D. O'Hallaron, All rights reserved.
 * May not be used, modified, or copied without permission.
 ***********************************************************************/ 

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include "isa.h"
#include "sim.h"


/*********************************************************
 * Part 2: This part contains the core simulator routines.
 *********************************************************/

/**********************
 * Begin Part 2 Globals
 **********************/

/*
 * Variables related to hardware units in the processor
 */
mem_t mem;  /* Instruction and data memory */
word_t minAddr = 0;
word_t memCnt = 0;

/* Other processor state */
mem_t reg;               /* Register file */
cc_t cc = DEFAULT_CC;    /* Condition code register */
cc_t cc_in = DEFAULT_CC; /* Input to condition code register */

/* 
 * SEQ+: Results computed by previous instruction.
 * Used to compute PC in current instruction 
 */
byte_t prev_icode = I_NOP;
byte_t prev_ifun = 0;
word_t prev_valc = 0;
word_t prev_valm = 0;
word_t prev_valp = 0;
bool_t prev_bcond = FALSE;

byte_t prev_icode_in = I_NOP;
byte_t prev_ifun_in = 0;
word_t prev_valc_in = 0;
word_t prev_valm_in = 0;
word_t prev_valp_in = 0;
bool_t prev_bcond_in = FALSE;


/* Program Counter */
word_t pc = 0; /* Program counter value */
word_t pc_in = 0;/* Input to program counter */

/* Intermediate values */
byte_t imem_icode = I_NOP;
byte_t imem_ifun = F_NONE;
byte_t icode = I_NOP;
word_t ifun = 0;
byte_t instr = HPACK(I_NOP, F_NONE);
word_t ra = REG_NONE;
word_t rb = REG_NONE;
word_t valc = 0;
word_t valp = 0;
bool_t imem_error;
bool_t instr_valid;

word_t srcA = REG_NONE;
word_t srcB = REG_NONE;
word_t destE = REG_NONE;
word_t destM = REG_NONE;
word_t vala = 0;
word_t valb = 0;
word_t vale = 0;

bool_t bcond = FALSE;
bool_t cond = FALSE;
word_t valm = 0;
bool_t dmem_error;

bool_t mem_write = FALSE;
word_t mem_addr = 0;
word_t mem_data = 0;
#ifdef SNU
bool_t mem_byte = FALSE;
#endif
byte_t status = STAT_AOK;


/* Values computed by control logic */
word_t gen_pc();  /* SEQ+ */
word_t gen_icode();
word_t gen_ifun();
word_t gen_need_regids();
word_t gen_need_valC();
word_t gen_instr_valid();
word_t gen_srcA();
word_t gen_srcB();
word_t gen_dstE();
word_t gen_dstM();
word_t gen_aluA();
word_t gen_aluB();
word_t gen_alufun();
word_t gen_set_cc();
word_t gen_mem_addr();
word_t gen_mem_data();
word_t gen_mem_read();
word_t gen_mem_write();
#ifdef SNU
word_t gen_mem_byte();
#endif
word_t gen_Stat();
word_t gen_new_pc();

/* Log file */
FILE *dumpfile = NULL;

#ifdef HAS_GUI
/* Representations of digits */
static char digits[16] =
    {'0', '1', '2', '3', '4', '5', '6', '7',
     '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
#endif /* HAS_GUI */

/********************
 * End Part 2 Globals
 ********************/

static int initialized = 0;
void sim_init()
{

    /* Create memory and register files */
    initialized = 1;
    mem = init_mem(MEM_SIZE);
    reg = init_reg();
    sim_reset();
    clear_mem(mem);
}

void sim_reset()
{
    if (!initialized)
	sim_init();
    clear_mem(reg);
    minAddr = 0;
    memCnt = 0;

#ifdef HAS_GUI
    if (gui_mode) {
	signal_register_clear();
	create_memory_display();
	sim_report();
    }
#endif

    if (plusmode) {
	prev_icode = prev_icode_in = I_NOP;
	prev_ifun = prev_ifun_in = 0;
	prev_valc = prev_valc_in = 0;
	prev_valm = prev_valm_in = 0;
	prev_valp = prev_valp_in = 0;
	prev_bcond = prev_bcond_in = FALSE;
	pc = 0;
    } else {
	pc_in = 0;
    }
    cc = DEFAULT_CC;
    cc_in = DEFAULT_CC;
    destE = REG_NONE;
    destM = REG_NONE;
    mem_write = FALSE;
    mem_addr = 0;
    mem_data = 0;

    /* Reset intermediate values to clear display */
    icode = I_NOP;
    ifun = 0;
    instr = HPACK(I_NOP, F_NONE);
    ra = REG_NONE;
    rb = REG_NONE;
    valc = 0;
    valp = 0;

    srcA = REG_NONE;
    srcB = REG_NONE;
    destE = REG_NONE;
    destM = REG_NONE;
    vala = 0;
    valb = 0;
    vale = 0;

    cond = FALSE;
    bcond = FALSE;
    valm = 0;

    sim_report();
}

/* Update the processor state */
static void update_state()
{
    if (plusmode) {
	prev_icode = prev_icode_in;
	prev_ifun  = prev_ifun_in;
	prev_valc  = prev_valc_in;
	prev_valm  = prev_valm_in;
	prev_valp  = prev_valp_in;
	prev_bcond = prev_bcond_in;
    } else {
	pc = pc_in;
    }
    cc = cc_in;
    /* Writeback */
    if (destE != REG_NONE)
	set_reg_val(reg, destE, vale);
    if (destM != REG_NONE)
	set_reg_val(reg, destM, valm);

    if (mem_write) {
      /* Should have already tested this address */
        
        if(gen_mem_byte() == 1){
            set_byte_val(mem, mem_addr, (byte_t) mem_data);
        } else {
            set_word_val(mem, mem_addr, mem_data);
        }
        
	sim_log("Wrote 0x%llx to address 0x%llx\n", mem_data, mem_addr);
#ifdef HAS_GUI
	    if (gui_mode) {
		if (mem_addr % 8 != 0) {
		    /* Just did a misaligned write.
		       Need to display both words */
		    word_t align_addr = mem_addr & ~0x3;
		    word_t val;
		    get_word_val(mem, align_addr, &val);
		    set_memory(align_addr, val);
		    align_addr+=8;
		    get_word_val(mem, align_addr, &val);
		    set_memory(align_addr, val);
		} else {
		    set_memory(mem_addr, mem_data);
		}
	    }
#endif /* HAS_GUI */
    }
}

/* Execute one instruction */
/* Return resulting status */
static byte_t sim_step()
{
    word_t aluA;
    word_t aluB;
    word_t alufun;

    status = STAT_AOK;
    imem_error = dmem_error = FALSE;

    update_state(); /* Update state from last cycle */

    if (plusmode) {
	pc = gen_pc();
    }
    valp = pc;
    instr = HPACK(I_NOP, F_NONE);
    imem_error = !get_byte_val(mem, valp, &instr);
    if (imem_error) {
	sim_log("Couldn't fetch at address 0x%llx\n", valp);
    }
    imem_icode = HI4(instr);
    imem_ifun = LO4(instr);
    icode = gen_icode();
    ifun  = gen_ifun();
    instr_valid = gen_instr_valid();
    valp++;
    if (gen_need_regids()) {
	byte_t regids;
	if (get_byte_val(mem, valp, &regids)) {
	    ra = GET_RA(regids);
	    rb = GET_RB(regids);
	} else {
	    ra = REG_NONE;
	    rb = REG_NONE;
	    status = STAT_ADR;
	    sim_log("Couldn't fetch at address 0x%llx\n", valp);
	}
	valp++;
    } else {
	ra = REG_NONE;
	rb = REG_NONE;
    }

    if (gen_need_valC()) {
	if (get_word_val(mem, valp, &valc)) {
	} else {
	    valc = 0;
	    status = STAT_ADR;
	    sim_log("Couldn't fetch at address 0x%llx\n", valp);
	}
	valp+=8;
    } else {
	valc = 0;
    }
    sim_log("IF: Fetched %s at 0x%llx.  ra=%s, rb=%s, valC = 0x%llx\n",
	    iname(HPACK(icode,ifun)), pc, reg_name(ra), reg_name(rb), valc);

    if (status == STAT_AOK && icode == I_HALT) {
	status = STAT_HLT;
    }
    
    srcA = gen_srcA();
    if (srcA != REG_NONE) {
	vala = get_reg_val(reg, srcA);
    } else {
	vala = 0;
    }
    
    srcB = gen_srcB();
    if (srcB != REG_NONE) {
	valb = get_reg_val(reg, srcB);
    } else {
	valb = 0;
    }

    cond = cond_holds(cc, ifun);

    destE = gen_dstE();
    destM = gen_dstM();

    aluA = gen_aluA();
    aluB = gen_aluB();
    alufun = gen_alufun();
    vale = compute_alu(alufun, aluA, aluB);
    cc_in = cc;
    if (gen_set_cc())
	cc_in = compute_cc(alufun, aluA, aluB);

    bcond =  cond && (icode == I_JMP);

    mem_addr = gen_mem_addr();
    mem_data = gen_mem_data();


    if (gen_mem_read()) {
        if (gen_mem_byte() == 1) {
            dmem_error = dmem_error || !get_byte_val(mem, mem_addr, &valm);
        }
        else {
            dmem_error = dmem_error || !get_word_val(mem, mem_addr, &valm);
        }
      if (dmem_error) {
	sim_log("Couldn't read at address 0x%llx\n", mem_addr);
      }
    } else
      valm = 0;

    mem_write = gen_mem_write();
    if (mem_write) {
      /* Do a test read of the data memory to make sure address is OK */
      word_t junk;
      dmem_error = dmem_error || !get_word_val(mem, mem_addr, &junk);
      
    }

    status = gen_Stat();

    if (plusmode) {
	prev_icode_in = icode;
	prev_ifun_in = ifun;
	prev_valc_in = valc;
	prev_valm_in = valm;
	prev_valp_in = valp;
	prev_bcond_in = bcond;
    } else {
	/* Update PC */
	pc_in = gen_new_pc();
    } 
    sim_report();
    return status;
}


/*
  Run processor until one of following occurs:
  - An error status is encountered in WB.
  - max_instr instructions have completed through WB

  Return number of instructions executed.
  if statusp nonnull, then will be set to status of final instruction
  if ccp nonnull, then will be set to condition codes of final instruction
*/
word_t sim_run(word_t max_instr, byte_t *statusp, cc_t *ccp)
{
    word_t icount = 0;
    byte_t run_status = STAT_AOK;
    while (icount < max_instr) {
	run_status = sim_step();
	icount++;
	if (run_status != STAT_AOK)
	    break;
    }
    if (statusp)
	*statusp = run_status;
    if (ccp)
	*ccp = cc;
    return icount;
}

