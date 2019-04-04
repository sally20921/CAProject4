#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "isa.h"


/* Are we running in GUI mode? */
extern int gui_mode;

/* Bytes Per Line = Block size of memory */
#define BPL 32

struct {
    char *name;
    int id;
} reg_table[REG_ERR+1] = 
{
    {"%rax",   REG_RAX},
    {"%rcx",   REG_RCX},
    {"%rdx",   REG_RDX},
    {"%rbx",   REG_RBX},
    {"%rsp",   REG_RSP},
    {"%rbp",   REG_RBP},
    {"%rsi",   REG_RSI},
    {"%rdi",   REG_RDI},
    {"%r8",   REG_R8},
    {"%r9",   REG_R9},
    {"%r10",   REG_R10},
    {"%r11",   REG_R11},
    {"%r12",   REG_R12},
    {"%r13",   REG_R13},
    {"%r14",   REG_R14},
    {"----",   REG_NONE},
    {"----",   REG_ERR}
};


reg_id_t find_register(char *name)
{
    int i;
    for (i = 0; i < REG_NONE; i++)
	if (!strcmp(name, reg_table[i].name))
	    return reg_table[i].id;
    return REG_ERR;
}

char *reg_name(reg_id_t id)
{
    if (id >= 0 && id < REG_NONE)
	return reg_table[id].name;
    else
	return reg_table[REG_NONE].name;
}

/* Is the given register ID a valid program register? */
int reg_valid(reg_id_t id)
{
  return id >= 0 && id < REG_NONE && reg_table[id].id == id;
}

instr_t instruction_set[] = 
{
    {"nop",    HPACK(I_NOP, F_NONE), 1, NO_ARG, 0, 0, NO_ARG, 0, 0 },
    {"halt",   HPACK(I_HALT, F_NONE), 1, NO_ARG, 0, 0, NO_ARG, 0, 0 },
    {"rrmovq", HPACK(I_RRMOVQ, F_NONE), 2, R_ARG, 1, 1, R_ARG, 1, 0 },
    /* Conditional move instructions are variants of RRMOVQ */
    {"cmovle", HPACK(I_RRMOVQ, C_LE), 2, R_ARG, 1, 1, R_ARG, 1, 0 },
    {"cmovl", HPACK(I_RRMOVQ, C_L), 2, R_ARG, 1, 1, R_ARG, 1, 0 },
    {"cmove", HPACK(I_RRMOVQ, C_E), 2, R_ARG, 1, 1, R_ARG, 1, 0 },
    {"cmovne", HPACK(I_RRMOVQ, C_NE), 2, R_ARG, 1, 1, R_ARG, 1, 0 },
    {"cmovge", HPACK(I_RRMOVQ, C_GE), 2, R_ARG, 1, 1, R_ARG, 1, 0 },
    {"cmovg", HPACK(I_RRMOVQ, C_G), 2, R_ARG, 1, 1, R_ARG, 1, 0 },
    /* arg1hi indicates number of bytes */
    {"irmovq", HPACK(I_IRMOVQ, F_NONE), 10, I_ARG, 2, 8, R_ARG, 1, 0 },
    {"rmmovq", HPACK(I_RMMOVQ, F_NONE), 10, R_ARG, 1, 1, M_ARG, 1, 0 },
    {"rmmovb", HPACK(I_RMMOVQ, M_BYTE), 10, R_ARG, 1, 1, M_ARG, 1, 0 },
    {"mrmovq", HPACK(I_MRMOVQ, F_NONE), 10, M_ARG, 1, 0, R_ARG, 1, 1 },
    {"mrmovb", HPACK(I_MRMOVQ, M_BYTE), 10, M_ARG, 1, 0, R_ARG, 1, 1 },
    {"addq",   HPACK(I_ALU, A_ADD), 2, R_ARG, 1, 1, R_ARG, 1, 0 },
    {"subq",   HPACK(I_ALU, A_SUB), 2, R_ARG, 1, 1, R_ARG, 1, 0 },
    {"andq",   HPACK(I_ALU, A_AND), 2, R_ARG, 1, 1, R_ARG, 1, 0 },
    {"xorq",   HPACK(I_ALU, A_XOR), 2, R_ARG, 1, 1, R_ARG, 1, 0 },
    {"mulq",   HPACK(I_ALU, A_MUL), 2, R_ARG, 1, 1, R_ARG, 1, 0 },
    {"divq",   HPACK(I_ALU, A_DIV), 2, R_ARG, 1, 1, R_ARG, 1, 0 },
    /* arg1hi indicates number of bytes */
    {"jmp",    HPACK(I_JMP, C_YES), 9, I_ARG, 1, 8, NO_ARG, 0, 0 },
    {"jle",    HPACK(I_JMP, C_LE), 9, I_ARG, 1, 8, NO_ARG, 0, 0 },
    {"jl",     HPACK(I_JMP, C_L), 9, I_ARG, 1, 8, NO_ARG, 0, 0 },
    {"je",     HPACK(I_JMP, C_E), 9, I_ARG, 1, 8, NO_ARG, 0, 0 },
    {"jne",    HPACK(I_JMP, C_NE), 9, I_ARG, 1, 8, NO_ARG, 0, 0 },
    {"jge",    HPACK(I_JMP, C_GE), 9, I_ARG, 1, 8, NO_ARG, 0, 0 },
    {"jg",     HPACK(I_JMP, C_G), 9, I_ARG, 1, 8, NO_ARG, 0, 0 },
    {"call",   HPACK(I_CALL, F_NONE),    9, I_ARG, 1, 8, NO_ARG, 0, 0 },
    {"ret",    HPACK(I_RET, F_NONE), 1, NO_ARG, 0, 0, NO_ARG, 0, 0 },
    {"pushq",  HPACK(I_PUSHQ, F_NONE) , 2, R_ARG, 1, 1, NO_ARG, 0, 0 },
    {"popq",   HPACK(I_POPQ, F_NONE) ,  2, R_ARG, 1, 1, NO_ARG, 0, 0 },
    {"iaddq",  HPACK(I_IADDQ, F_NONE), 10, I_ARG, 2, 8, R_ARG, 1, 0 },
    /* this is just a hack to make the I_POP2 code have an associated name */
    {"pop2",   HPACK(I_POP2, F_NONE) , 0, NO_ARG, 0, 0, NO_ARG, 0, 0 },

    /* For allocation instructions, arg1hi indicates number of bytes */
    {".byte",  0x00, 1, I_ARG, 0, 1, NO_ARG, 0, 0 },
    {".word",  0x00, 2, I_ARG, 0, 2, NO_ARG, 0, 0 },
    {".long",  0x00, 4, I_ARG, 0, 4, NO_ARG, 0, 0 },
    {".quad",  0x00, 8, I_ARG, 0, 8, NO_ARG, 0, 0 },
    {NULL,     0   , 0, NO_ARG, 0, 0, NO_ARG, 0, 0 }
};

instr_t invalid_instr =
    {"XXX",     0   , 0, NO_ARG, 0, 0, NO_ARG, 0, 0 };

instr_ptr find_instr(char *name)
{
    int i;
    for (i = 0; instruction_set[i].name; i++)
	if (strcmp(instruction_set[i].name,name) == 0)
	    return &instruction_set[i];
    return NULL;
}

/* Return name of instruction given its encoding */
char *iname(int instr) {
    int i;
    for (i = 0; instruction_set[i].name; i++) {
	if (instr == instruction_set[i].code)
	    return instruction_set[i].name;
    }
    return "<bad>";
}


instr_ptr bad_instr()
{
    return &invalid_instr;
}


mem_t init_mem(int len)
{

    mem_t result = (mem_t) malloc(sizeof(mem_rec));
    len = ((len+BPL-1)/BPL)*BPL;
    result->len = len;
    result->contents = (byte_t *) calloc(len, 1);
    return result;
}

void clear_mem(mem_t m)
{
    memset(m->contents, 0, m->len);
}

void free_mem(mem_t m)
{
    free((void *) m->contents);
    free((void *) m);
}

mem_t copy_mem(mem_t oldm)
{
    mem_t newm = init_mem(oldm->len);
    memcpy(newm->contents, oldm->contents, oldm->len);
    return newm;
}

#ifdef SNU
bool_t diff_mem(mem_t oldm, mem_t newm, FILE *outfile, word_t start_addr)
#else
bool_t diff_mem(mem_t oldm, mem_t newm, FILE *outfile)
#endif
{
    word_t pos;
    int len = oldm->len;
    bool_t diff = FALSE;
    if (newm->len < len)
	len = newm->len;
#ifdef SNU
    for (pos = start_addr; (!diff || outfile) && pos < len; pos += 8) {
#else
    for (pos = 0; (!diff || outfile) && pos < len; pos += 8) {
#endif
        word_t ov = 0;  word_t nv = 0;
	get_word_val(oldm, pos, &ov);
	get_word_val(newm, pos, &nv);
	if (nv != ov) {
	    diff = TRUE;
	    if (outfile)
		fprintf(outfile, "0x%.4llx:\t0x%.16llx\t0x%.16llx\n", pos, ov, nv);
	}
    }
    return diff;
}

int hex2dig(char c)
{
    if (isdigit((int)c))
	return c - '0';
    if (isupper((int)c))
	return c - 'A' + 10;
    else
	return c - 'a' + 10;
}

#define LINELEN 4096
int load_mem(mem_t m, FILE *infile, int report_error)
{
    /* Read contents of .yo file */
    char buf[LINELEN];
    char c, ch, cl;
    int byte_cnt = 0;
    int lineno = 0;
    word_t bytepos = 0;
#ifdef HAS_GUI
    int empty_line = 1;
    int addr = 0;
    char hexcode[21];
    /* For display */
    int line_no = 0;
    char line[LINELEN];
    int index = 0;
#endif /* HAS_GUI */   
    while (fgets(buf, LINELEN, infile)) {
	int cpos = 0;
#ifdef HAS_GUI
	empty_line = 1;
#endif
	lineno++;
	/* Skip white space */
	while (isspace((int)buf[cpos]))
	    cpos++;

	if (buf[cpos] != '0' ||
	    (buf[cpos+1] != 'x' && buf[cpos+1] != 'X'))
	    continue; /* Skip this line */      
	cpos+=2;

	/* Get address */
	bytepos = 0;
	while (isxdigit((int)(c=buf[cpos]))) {
	    cpos++;
	    bytepos = bytepos*16 + hex2dig(c);
	}

	while (isspace((int)buf[cpos]))
	    cpos++;

	if (buf[cpos++] != ':') {
	    if (report_error) {
		fprintf(stderr, "Error reading file. Expected colon\n");
		fprintf(stderr, "Line %d:%s\n", lineno, buf);
		fprintf(stderr,
			"Reading '%c' at position %d\n", buf[cpos], cpos);
	    }
	    return 0;
	}

#ifdef HAS_GUI
	addr = bytepos;
	index = 0;
#endif

	while (isspace((int)buf[cpos]))
	    cpos++;

	/* Get code */
	while (isxdigit((int)(ch=buf[cpos++])) && 
	       isxdigit((int)(cl=buf[cpos++]))) {
	    byte_t byte = 0;
	    if (bytepos >= m->len) {
		if (report_error) {
		    fprintf(stderr,
			    "Error reading file. Invalid address. 0x%llx\n",
			    bytepos);
		    fprintf(stderr, "Line %d:%s\n", lineno, buf);
		}
		return 0;
	    }
	    byte = hex2dig(ch)*16+hex2dig(cl);
	    m->contents[bytepos++] = byte;
	    byte_cnt++;
#ifdef HAS_GUI
	    empty_line = 0;
	    hexcode[index++] = ch;
	    hexcode[index++] = cl;
#endif
	}
#ifdef HAS_GUI
	/* Fill rest of hexcode with blanks.
	   Needs to be 2x longest instruction */
	for (; index < 20; index++)
	    hexcode[index] = ' ';
	hexcode[index] = '\0';

	if (gui_mode) {
	    /* Now get the rest of the line */
	    while (isspace((int)buf[cpos]))
		cpos++;
	    cpos++; /* Skip over '|' */
	    
	    index = 0;
	    while ((c = buf[cpos++]) != '\0' && c != '\n') {
		line[index++] = c;
	    }
	    line[index] = '\0';
	    if (!empty_line)
		report_line(line_no++, addr, hexcode, line);
	}
#endif /* HAS_GUI */ 
    }
    return byte_cnt;
}

void dump_memory(FILE *outfile, mem_t m, word_t pos, int len)
{
    int i, j;
    while (pos % BPL) {
	pos --;
	len ++;
    }

    len = ((len+BPL-1)/BPL)*BPL;

    if (pos+len > m->len)
	len = m->len-pos;

    for (i = 0; i < len; i+=BPL) {
	word_t val = 0;
	fprintf(outfile, "0x%.4llx:", pos+i);
	for (j = 0; j < BPL; j+= 8) {
	    get_word_val(m, pos+i+j, &val);
	    fprintf(outfile, " %.16llx", val);
	}
    }
}

mem_t init_reg()
{
    return init_mem(128);
}

void free_reg(mem_t r)
{
    free_mem(r);
}

mem_t copy_reg(mem_t oldr)
{
    return copy_mem(oldr);
}

bool_t diff_reg(mem_t oldr, mem_t newr, FILE *outfile)
{
    word_t pos;
    int len = oldr->len;
    bool_t diff = FALSE;
    if (newr->len < len)
	len = newr->len;
    for (pos = 0; (!diff || outfile) && pos < len; pos += 8) {
        word_t ov = 0;
        word_t nv = 0;
	get_word_val(oldr, pos, &ov);
	get_word_val(newr, pos, &nv);
	if (nv != ov) {
	    diff = TRUE;
	    if (outfile)
		fprintf(outfile, "%s:\t0x%.16llx\t0x%.16llx\n",
			reg_table[pos/8].name, ov, nv);
	}
    }
    return diff;
}

word_t get_reg_val(mem_t r, reg_id_t id)
{
    word_t val = 0;
    if (id >= REG_NONE)
	return 0;
    get_word_val(r,id*8, &val);
    return val;
}

void set_reg_val(mem_t r, reg_id_t id, word_t val)
{
    if (id < REG_NONE) {
	set_word_val(r,id*8,val);
#ifdef HAS_GUI
	if (gui_mode) {
	    signal_register_update(id, val);
	}
#endif /* HAS_GUI */
    }
}
     
void dump_reg(FILE *outfile, mem_t r) {
    reg_id_t id;
    for (id = 0; reg_valid(id); id++) {
	fprintf(outfile, "   %s  ", reg_table[id].name);
    }
    fprintf(outfile, "\n");
    for (id = 0; reg_valid(id); id++) {
	word_t val = 0;
	get_word_val(r, id*8, &val);
	fprintf(outfile, " %llx", val);
    }
    fprintf(outfile, "\n");
}

struct {
    char symbol;
    int id;
} alu_table[A_NONE+1] = 
{
    {'+',   A_ADD},
    {'-',   A_SUB},
    {'&',   A_AND},
    {'^',   A_XOR},
	{'*',   A_MUL},
	{'/', 	A_DIV},
    {'?',   A_NONE}
};

char op_name(alu_t op)
{
    if (op < A_NONE)
	return alu_table[op].symbol;
    else
	return alu_table[A_NONE].symbol;
}

char *cc_names[8] = {
    "Z=0 S=0 O=0",
    "Z=0 S=0 O=1",
    "Z=0 S=1 O=0",
    "Z=0 S=1 O=1",
    "Z=1 S=0 O=0",
    "Z=1 S=0 O=1",
    "Z=1 S=1 O=0",
    "Z=1 S=1 O=1"};

char *cc_name(cc_t c)
{
    int ci = c;
    if (ci < 0 || ci > 7)
	return "???????????";
    else
	return cc_names[c];
}

/* Status types */

char *stat_names[] = { "BUB", "AOK", "HLT", "ADR", "INS", "PIP" };

char *stat_name(stat_t e)
{
    if (e < 0 || e > STAT_PIP)
	return "Invalid Status";
    return stat_names[e];
}

/**************** Implementation of ISA model ************************/

state_ptr new_state(int memlen)
{
    state_ptr result = (state_ptr) malloc(sizeof(state_rec));
    result->pc = 0;
    result->r = init_reg();
    result->m = init_mem(memlen);
    result->cc = DEFAULT_CC;
    return result;
}

void free_state(state_ptr s)
{
    free_reg(s->r);
    free_mem(s->m);
    free((void *) s);
}

state_ptr copy_state(state_ptr s) {
    state_ptr result = (state_ptr) malloc(sizeof(state_rec));
    result->pc = s->pc;
    result->r = copy_reg(s->r);
    result->m = copy_mem(s->m);
    result->cc = s->cc;
    return result;
}

bool_t diff_state(state_ptr olds, state_ptr news, FILE *outfile) {
    bool_t diff = FALSE;

    if (olds->pc != news->pc) {
	diff = TRUE;
	if (outfile) {
	    fprintf(outfile, "pc:\t0x%.16llx\t0x%.16llx\n", olds->pc, news->pc);
	}
    }
    if (olds->cc != news->cc) {
	diff = TRUE;
	if (outfile) {
	    fprintf(outfile, "cc:\t%s\t%s\n", cc_name(olds->cc), cc_name(news->cc));
	}
    }
    if (diff_reg(olds->r, news->r, outfile))
	diff = TRUE;
#ifdef SNU
    if (diff_mem(olds->m, news->m, outfile, (word_t) 0))
#else
    if (diff_mem(olds->m, news->m, outfile))
#endif
	diff = TRUE;
    return diff;
}

