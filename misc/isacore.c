#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "isa.h"


bool_t get_byte_val(mem_t m, word_t pos, byte_t *dest)
{
    if (pos < 0 || pos >= m->len)
	return FALSE;
    *dest = m->contents[pos];
    return TRUE;
}

bool_t get_word_val(mem_t m, word_t pos, word_t *dest)
{
    int i;
    word_t val;
    if (pos < 0 || pos + 8 > m->len)
	return FALSE;
    val = 0;
    for (i = 0; i < 8; i++) {
	word_t b =  m->contents[pos+i] & 0xFF;
	val = val | (b <<(8*i));
    }
    *dest = val;
    return TRUE;
}

bool_t set_byte_val(mem_t m, word_t pos, byte_t val)
{
    if (pos < 0 || pos >= m->len)
	return FALSE;
    m->contents[pos] = val;
    return TRUE;
}

bool_t set_word_val(mem_t m, word_t pos, word_t val)
{
    int i;
    if (pos < 0 || pos + 8 > m->len)
	return FALSE;
    for (i = 0; i < 8; i++) {
	m->contents[pos+i] = (byte_t) val & 0xFF;
	val >>= 8;
    }
    return TRUE;
}


word_t compute_alu(alu_t op, word_t argA, word_t argB)
{
    word_t val;
    switch(op) {
    case A_ADD:
	val = argA+argB;
	break;
    case A_SUB:
	val = argB-argA;
	break;
    case A_AND:
	val = argA&argB;
	break;
    case A_XOR:
	val = argA^argB;
	break;
    case A_MUL:
    val = argA * argB ;
    break;
    case A_DIV:
    if (argA == 0) {
        val =  0x8000000000000000;
    }
    else {
        val = argB/argA;
    }
    break;
    default:
	val = 0;
    }
    return val;
}

cc_t compute_cc(alu_t op, word_t argA, word_t argB)
{
    word_t val = compute_alu(op, argA, argB);
    bool_t zero = (val == 0);
    bool_t sign = ((word_t)val < 0);
    bool_t ovf;
    switch(op) {
    case A_ADD:
        ovf = (((word_t) argA < 0) == ((word_t) argB < 0)) &&
  	       (((word_t) val < 0) != ((word_t) argA < 0));
	break;
    case A_SUB:
        ovf = (((word_t) argA > 0) == ((word_t) argB < 0)) &&
	       (((word_t) val < 0) != ((word_t) argB < 0));
	break;
    case A_AND:
    case A_XOR:
	ovf = FALSE;
	break;
    case A_MUL:
        ovf = (argA != val/argB);
        break;
    case A_DIV:
    if (argA == 0) {
        ovf = TRUE;
        break;
    }
    
    default:
	ovf = FALSE;
    }
    return PACK_CC(zero,sign,ovf);
    
}


/* Branch logic */
bool_t cond_holds(cc_t cc, cond_t bcond) {
    bool_t zf = GET_ZF(cc);
    bool_t sf = GET_SF(cc);
    bool_t of = GET_OF(cc);
    bool_t jump = FALSE;
    
    switch(bcond) {
    case C_YES:
	jump = TRUE;
	break;
    case C_LE:
	jump = (sf^of)|zf;
	break;
    case C_L:
	jump = sf^of;
	break;
    case C_E:
	jump = zf;
	break;
    case C_NE:
	jump = zf^1;
	break;
    case C_GE:
	jump = sf^of^1;
	break;
    case C_G:
	jump = (sf^of^1)&(zf^1);
	break;
    default:
	jump = FALSE;
	break;
    }
    return jump;
}


/* Execute single instruction.  Return status. */
stat_t step_state(state_ptr s, FILE *error_file)
{
    word_t argA, argB;
    byte_t byte0 = 0;
    byte_t byte1 = 0;
    itype_t hi0;
    alu_t  lo0;
    reg_id_t hi1 = REG_NONE;
    reg_id_t lo1 = REG_NONE;
    bool_t ok1 = TRUE;
    word_t cval = 0;
    word_t okc = TRUE;
    word_t val, dval;
    bool_t need_regids;
    bool_t need_imm;
    word_t ftpc = s->pc;  /* Fall-through PC */

    if (!get_byte_val(s->m, ftpc, &byte0)) {
	if (error_file)
	    fprintf(error_file,
		    "PC = 0x%llx, Invalid instruction address\n", s->pc);
	return STAT_ADR;
    }
    ftpc++;

    hi0 = HI4(byte0);
    lo0 = LO4(byte0);

    need_regids =
	(hi0 == I_RRMOVQ || hi0 == I_ALU || hi0 == I_PUSHQ ||
	 hi0 == I_POPQ || hi0 == I_IRMOVQ || hi0 == I_RMMOVQ ||
	 hi0 == I_MRMOVQ || hi0 == I_IADDQ);

    if (need_regids) {
	ok1 = get_byte_val(s->m, ftpc, &byte1);
	ftpc++;
	hi1 = HI4(byte1);
	lo1 = LO4(byte1);
    }

    need_imm =
	(hi0 == I_IRMOVQ || hi0 == I_RMMOVQ || hi0 == I_MRMOVQ ||
	 hi0 == I_JMP || hi0 == I_CALL || hi0 == I_IADDQ);

    if (need_imm) {
	okc = get_word_val(s->m, ftpc, &cval);
	ftpc += 8;
    }

    switch (hi0) {
    case I_NOP:
	s->pc = ftpc;
	break;
    case I_HALT:
	return STAT_HLT;
	break;
    case I_RRMOVQ:  /* Both unconditional and conditional moves */
	if (!ok1) {
	    if (error_file)
		fprintf(error_file,
			"PC = 0x%llx, Invalid instruction address\n", s->pc);
	    return STAT_ADR;
	}
	if (!reg_valid(hi1)) {
	    if (error_file)
		fprintf(error_file,
			"PC = 0x%llx, Invalid register ID 0x%.1x\n",
			s->pc, hi1);
	    return STAT_INS;
	}
	if (!reg_valid(lo1)) {
	    if (error_file)
		fprintf(error_file,
			"PC = 0x%llx, Invalid register ID 0x%.1x\n",
			s->pc, lo1);
	    return STAT_INS;
	}
	val = get_reg_val(s->r, hi1);
	if (cond_holds(s->cc, lo0))
	  set_reg_val(s->r, lo1, val);
	s->pc = ftpc;
	break;
    case I_IRMOVQ:
	if (!ok1) {
	    if (error_file)
		fprintf(error_file,
			"PC = 0x%llx, Invalid instruction address\n", s->pc);
	    return STAT_ADR;
	}
	if (!okc) {
	    if (error_file)
		fprintf(error_file,
			"PC = 0x%llx, Invalid instruction address",
			s->pc);
	    return STAT_INS;
	}
	if (!reg_valid(lo1)) {
	    if (error_file)
		fprintf(error_file,
			"PC = 0x%llx, Invalid register ID 0x%.1x\n",
			s->pc, lo1);
	    return STAT_INS;
	}
	set_reg_val(s->r, lo1, cval);
	s->pc = ftpc;
	break;
    case I_RMMOVQ:
	if (!ok1) {
	    if (error_file)
		fprintf(error_file,
			"PC = 0x%llx, Invalid instruction address\n", s->pc);
	    return STAT_ADR;
	}
	if (!okc) {
	    if (error_file)
		fprintf(error_file,
			"PC = 0x%llx, Invalid instruction address\n", s->pc);
	    return STAT_INS;
	}
	if (!reg_valid(hi1)) {
	    if (error_file)
		fprintf(error_file,
			"PC = 0x%llx, Invalid register ID 0x%.1x\n",
			s->pc, hi1);
	    return STAT_INS;
	}
	if (reg_valid(lo1)) 
	    cval += get_reg_val(s->r, lo1);
	val = get_reg_val(s->r, hi1);
	if (!set_word_val(s->m, cval, val)) {
	    if (error_file)
		fprintf(error_file,
			"PC = 0x%llx, Invalid data address 0x%llx\n",
			s->pc, cval);
	    return STAT_ADR;
	}
	s->pc = ftpc;
	break;
    case I_MRMOVQ:
	if (!ok1) {
	    if (error_file)
		fprintf(error_file,
			"PC = 0x%llx, Invalid instruction address\n", s->pc);
	    return STAT_ADR;
	}
	if (!okc) {
	    if (error_file)
		fprintf(error_file,
			"PC = 0x%llx, Invalid instruction addres\n", s->pc);
	    return STAT_INS;
	}
	if (!reg_valid(hi1)) {
	    if (error_file)
		fprintf(error_file,
			"PC = 0x%llx, Invalid register ID 0x%.1x\n",
			s->pc, hi1);
	    return STAT_INS;
	}
	if (reg_valid(lo1)) 
	    cval += get_reg_val(s->r, lo1);
	if (!get_word_val(s->m, cval, &val))
	    return STAT_ADR;
	set_reg_val(s->r, hi1, val);
	s->pc = ftpc;
	break;
    case I_ALU:
	if (!ok1) {
	    if (error_file)
		fprintf(error_file,
			"PC = 0x%llx, Invalid instruction address\n", s->pc);
	    return STAT_ADR;
	}
	argA = get_reg_val(s->r, hi1);
	argB = get_reg_val(s->r, lo1);
	val = compute_alu(lo0, argA, argB);
	set_reg_val(s->r, lo1, val);
	s->cc = compute_cc(lo0, argA, argB);
	s->pc = ftpc;
	break;
    case I_JMP:
	if (!ok1) {
	    if (error_file)
		fprintf(error_file,
			"PC = 0x%llx, Invalid instruction address\n", s->pc);
	    return STAT_ADR;
	}
	if (!okc) {
	    if (error_file)
		fprintf(error_file,
			"PC = 0x%llx, Invalid instruction address\n", s->pc);
	    return STAT_ADR;
	}
	if (cond_holds(s->cc, lo0))
	    s->pc = cval;
	else
	    s->pc = ftpc;
	break;
    case I_CALL:
	if (!ok1) {
	    if (error_file)
		fprintf(error_file,
			"PC = 0x%llx, Invalid instruction address\n", s->pc);
	    return STAT_ADR;
	}
	if (!okc) {
	    if (error_file)
		fprintf(error_file,
			"PC = 0x%llx, Invalid instruction address\n", s->pc);
	    return STAT_ADR;
	}
	val = get_reg_val(s->r, REG_RSP) - 8;
	set_reg_val(s->r, REG_RSP, val);
	if (!set_word_val(s->m, val, ftpc)) {
	    if (error_file)
		fprintf(error_file,
			"PC = 0x%llx, Invalid stack address 0x%llx\n", s->pc, val);
	    return STAT_ADR;
	}
	s->pc = cval;
	break;
    case I_RET:
	/* Return Instruction.  Pop address from stack */
	dval = get_reg_val(s->r, REG_RSP);
	if (!get_word_val(s->m, dval, &val)) {
	    if (error_file)
		fprintf(error_file,
			"PC = 0x%llx, Invalid stack address 0x%llx\n",
			s->pc, dval);
	    return STAT_ADR;
	}
	set_reg_val(s->r, REG_RSP, dval + 8);
	s->pc = val;
	break;
    case I_PUSHQ:
	if (!ok1) {
	    if (error_file)
		fprintf(error_file,
			"PC = 0x%llx, Invalid instruction address\n", s->pc);
	    return STAT_ADR;
	}
	if (!reg_valid(hi1)) {
	    if (error_file)
		fprintf(error_file,
			"PC = 0x%llx, Invalid register ID 0x%.1x\n", s->pc, hi1);
	    return STAT_INS;
	}
	val = get_reg_val(s->r, hi1);
	dval = get_reg_val(s->r, REG_RSP) - 8;
	set_reg_val(s->r, REG_RSP, dval);
	if  (!set_word_val(s->m, dval, val)) {
	    if (error_file)
		fprintf(error_file,
			"PC = 0x%llx, Invalid stack address 0x%llx\n", s->pc, dval);
	    return STAT_ADR;
	}
	s->pc = ftpc;
	break;
    case I_POPQ:
	if (!ok1) {
	    if (error_file)
		fprintf(error_file,
			"PC = 0x%llx, Invalid instruction address\n", s->pc);
	    return STAT_ADR;
	}
	if (!reg_valid(hi1)) {
	    if (error_file)
		fprintf(error_file,
			"PC = 0x%llx, Invalid register ID 0x%.1x\n", s->pc, hi1);
	    return STAT_INS;
	}
	dval = get_reg_val(s->r, REG_RSP);
	set_reg_val(s->r, REG_RSP, dval+8);
	if (!get_word_val(s->m, dval, &val)) {
	    if (error_file)
		fprintf(error_file,
			"PC = 0x%llx, Invalid stack address 0x%llx\n",
			s->pc, dval);
	    return STAT_ADR;
	}
	set_reg_val(s->r, hi1, val);
	s->pc = ftpc;
	break;
    case I_IADDQ:
	if (!ok1) {
	    if (error_file)
		fprintf(error_file,
			"PC = 0x%llx, Invalid instruction address\n", s->pc);
	    return STAT_ADR;
	}
	if (!okc) {
	    if (error_file)
		fprintf(error_file,
			"PC = 0x%llx, Invalid instruction address",
			s->pc);
	    return STAT_INS;
	}
	if (!reg_valid(lo1)) {
	    if (error_file)
		fprintf(error_file,
			"PC = 0x%llx, Invalid register ID 0x%.1x\n",
			s->pc, lo1);
	    return STAT_INS;
	}
	argB = get_reg_val(s->r, lo1);
	val = argB + cval;
	set_reg_val(s->r, lo1, val);
	s->cc = compute_cc(A_ADD, cval, argB);
	s->pc = ftpc;
	break;
    default:
	if (error_file)
	    fprintf(error_file,
		    "PC = 0x%llx, Invalid instruction %.2x\n", s->pc, byte0);
	return STAT_INS;
    }
    return STAT_AOK;
}
