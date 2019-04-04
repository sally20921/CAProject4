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

#define MAXBUF 1024

#ifdef HAS_GUI
#include <tk.h>
#endif /* HAS_GUI */

#define MAXARGS 128
#define MAXBUF 1024
#define TKARGS 3

/***************
 * Begin Globals
 ***************/

/* Simulator name defined and initialized by the compiled HCL file */
/* according to the -n argument supplied to hcl2c */
extern char simname[];

/* SEQ=0, SEQ+=1. Modified by HCL main() */
int plusmode = 0; 

/* Parameters modifed by the command line */
int gui_mode = FALSE;    /* Run in GUI mode instead of TTY mode? (-g) */
char *object_filename;   /* The input object file name. */
FILE *object_file;       /* Input file handle */
bool_t verbosity = 2;    /* Verbosity level [TTY only] (-v) */ 
#ifdef SNU
word_t instr_limit = 1000000; /* Instruction limit [TTY only] (-l) */
#else
word_t instr_limit = 10000; /* Instruction limit [TTY only] (-l) */
#endif
bool_t do_check = FALSE; /* Test with YIS? [TTY only] (-t) */

#ifdef SNU
int snu_mode = FALSE;	/* Print output for automatic grading server */
#endif

/************* 
 * End Globals 
 *************/


/***************************
 * Begin function prototypes 
 ***************************/

static void usage(char *name);           /* Print helpful usage message */
static void run_tty_sim();               /* Run simulator in TTY mode */

#ifdef HAS_GUI
void addAppCommands(Tcl_Interp *interp); /* Add application-dependent commands */
#endif /* HAS_GUI */

/*************************
 * End function prototypes
 *************************/


/*******************************************************************
 * Part 1: This part is the initial entry point that handles general
 * initialization. It parses the command line and does any necessary
 * setup to run in either TTY or GUI mode, and then starts the
 * simulation.
 *******************************************************************/

/* 
 * sim_main - main simulator routine. This function is called from the
 * main() routine in the HCL file.
 */
int sim_main(int argc, char **argv)
{
    int i;
    int c;
    char *myargv[MAXARGS];

    
    /* Parse the command line arguments */
#ifdef SNU
    while ((c = getopt(argc, argv, "htgsl:v:")) != -1) {
#else
    while ((c = getopt(argc, argv, "htgl:v:")) != -1) {
#endif
	switch(c) {
	case 'h':
	    usage(argv[0]);
	    break;
	case 'l':
	    instr_limit = atoll(optarg);
	    break;
	case 'v':
	    verbosity = atoi(optarg);
	    if (verbosity < 0 || verbosity > 2) {
		printf("Invalid verbosity %d\n", verbosity);
		usage(argv[0]);
	    }
	    break;
	case 't':
	    do_check = TRUE;
	    break;
	case 'g':
	    gui_mode = TRUE;
	    break;
#ifdef SNU
	case 's':
		snu_mode = TRUE;
		break;
#endif
	default:
	    printf("Invalid option '%c'\n", c);
	    usage(argv[0]);
	    break;
	}
    }
#ifdef SNU
	if (snu_mode)
	{
		gui_mode = FALSE;
		verbosity = 0;
	}
#endif




    /* Do we have too many arguments? */
    if (optind < argc - 1) {
	printf("Too many command line arguments:");
	for (i = optind; i < argc; i++)
	    printf(" %s", argv[i]);
	printf("\n");
	usage(argv[0]);
    }


    /* The single unflagged argument should be the object file name */
    object_filename = NULL;
    object_file = NULL;
    if (optind < argc) {
	object_filename = argv[optind];
	object_file = fopen(object_filename, "r");
	if (!object_file) {
	    fprintf(stderr, "Couldn't open object file %s\n", object_filename);
	    exit(1);
	}
    }


    /* Run the simulator in GUI mode (-g flag) */
    if (gui_mode) {

#ifndef HAS_GUI
	printf("To run in GUI mode, you must recompile with the HAS_GUI constant defined.\n");
	exit(1);
#endif /* HAS_GUI */

	/* In GUI mode, we must specify the object file on command line */ 
	if (!object_file) {
	    printf("Missing object file argument in GUI mode\n");
	    usage(argv[0]);
	}

	/* Build the command line for the GUI simulator */
	for (i = 0; i < TKARGS; i++) {
	    if ((myargv[i] = malloc(MAXBUF*sizeof(char))) == NULL) {
		perror("malloc error");
		exit(1);
	    }
	}
	strcpy(myargv[0], argv[0]);

#if 0
	printf("argv[0]=%s\n", argv[0]);
	{
	    char buf[1000]; 
	    getcwd(buf, 1000);
	    printf("cwd=%s\n", buf);
	}
#endif

	if (plusmode == 0) /* SEQ */
	    strcpy(myargv[1], "seq.tcl");
	else
	    strcpy(myargv[1], "seq+.tcl");
	strcpy(myargv[2], object_filename);
	myargv[3] = NULL;

	/* Start the GUI simulator */
#ifdef HAS_GUI
	Tk_Main(TKARGS, myargv, Tcl_AppInit);
#endif /* HAS_GUI */
	exit(0);
    }

    /* Otherwise, run the simulator in TTY mode (no -g flag) */
    run_tty_sim();

    exit(0);
}

/* 
 * run_tty_sim - Run the simulator in TTY mode
 */
static void run_tty_sim() 
{
    word_t icount = 0;
    status = STAT_AOK;
    cc_t result_cc = 0;
    word_t byte_cnt = 0;
    mem_t mem0, reg0;
    state_ptr isa_state = NULL;


    /* In TTY mode, the default object file comes from stdin */
    if (!object_file) {
	object_file = stdin;
    }

    /* Initializations */
    if (verbosity >= 2)
	sim_set_dumpfile(stdout);
    sim_init();

#ifndef SNU
    /* Emit simulator name */
    printf("%s\n", simname);
#endif

    byte_cnt = load_mem(mem, object_file, 1);
    if (byte_cnt == 0) {
	fprintf(stderr, "No lines of code found\n");
	exit(1);
    } else if (verbosity >= 2) {
	printf("%lld bytes of code read\n", byte_cnt);
    }
    fclose(object_file);
    if (do_check) {
	isa_state = new_state(0);
	free_mem(isa_state->r);
	free_mem(isa_state->m);
	isa_state->m = copy_mem(mem);
	isa_state->r = copy_mem(reg);
	isa_state->cc = cc;
    }

    mem0 = copy_mem(mem);
    reg0 = copy_mem(reg);
    

    icount = sim_run(instr_limit, &status, &result_cc);
    if (verbosity > 0) {
	printf("%lld instructions executed\n", icount);
	printf("Status = %s\n", stat_name(status));
	printf("Condition Codes: %s\n", cc_name(result_cc));
	printf("Changed Register State:\n");
	diff_reg(reg0, reg, stdout);
	printf("Changed Memory State:\n");
#ifdef SNU
	diff_mem(mem0, mem, stdout, (word_t) 0);
#else
	diff_mem(mem0, mem, stdout);
#endif
    }
#ifdef SNU
	if (snu_mode)
	{
		FILE *fp;
		if ((fp = fopen("memory.out", "w")) == NULL)
		{
			printf("Cannot write memory dump file\n");
			exit(1);
		}
		fprintf(fp, "Changed Memory State:\n");
		diff_mem(mem0, mem, fp, (word_t) 0x1000);
		fclose(fp);
		printf("%lld instructions executed\n", icount);
	}
#endif

    if (do_check) {
	byte_t e = STAT_AOK;
	int step;
	bool_t match = TRUE;

	for (step = 0; step < instr_limit && e == STAT_AOK; step++) {
	    e = step_state(isa_state, stdout);
	}

	if (diff_reg(isa_state->r, reg, NULL)) {
	    match = FALSE;
	    if (verbosity > 0) {
		printf("ISA Register != Pipeline Register File\n");
		diff_reg(isa_state->r, reg, stdout);
	    }
	}
#ifdef SNU
	if (diff_mem(isa_state->m, mem, NULL, (word_t) 0)) {
#else
	if (diff_mem(isa_state->m, mem, NULL)) {
#endif
	    match = FALSE;
	    if (verbosity > 0) {
		printf("ISA Memory != Pipeline Memory\n");
#ifdef SNU
		diff_mem(isa_state->m, mem, stdout, (word_t) 0);
#else
		diff_mem(isa_state->m, mem, stdout);
#endif
	    }
	}
	if (isa_state->cc != result_cc) {
	    match = FALSE;
	    if (verbosity > 0) {
		printf("ISA Cond. Codes (%s) != Pipeline Cond. Codes (%s)\n",
		       cc_name(isa_state->cc), cc_name(result_cc));
	    }
	}
	if (match) {
	    printf("ISA Check Succeeds\n");
	} else {
	    printf("ISA Check Fails\n");
	}
    }
}



/*
 * usage - print helpful diagnostic information
 */
static void usage(char *name)
{
    printf("Usage: %s [-htg] [-l m] [-v n] file.yo\n", name);
    printf("file.yo required in GUI mode, optional in TTY mode (default stdin)\n");
    printf("   -h     Print this message\n");
    printf("   -g     Run in GUI mode instead of TTY mode (default TTY)\n");  
    printf("   -l m   Set instruction limit to m [TTY mode only] (default %lld)\n", instr_limit);
    printf("   -v n   Set verbosity level to 0 <= n <= 2 [TTY mode only] (default %d)\n", verbosity);
    printf("   -t     Test result against ISA simulator (yis) [TTY mode only]\n");
#ifdef SNU
	printf("   -s     Print output for automatic grading server\n");
#endif
    exit(0);
}


#ifdef HAS_GUI

/* Create string in hex/oct/binary format with leading zeros */
/* bpd denotes bits per digit  Should be in range 1-4,
   bpw denotes bits per word.*/
void wstring(uword_t x, int bpd, int bpw, char *str)
{
    int digit;
    uword_t mask = ((uword_t) 1 << bpd) - 1;
    for (digit = (bpw-1)/bpd; digit >= 0; digit--) {
	uword_t val = (x >> (digit * bpd)) & mask;
	*str++ = digits[val];
    }
    *str = '\0';
}

/* used for formatting instructions */
static char status_msg[128];

/* SEQ+ */
static char *format_prev()
{
    char istring[17];
    char mstring[17];
    char pstring[17];
    wstring(prev_valc, 4, 64, istring);
    wstring(prev_valm, 4, 64, mstring);
    wstring(prev_valp, 4, 64, pstring);
    sprintf(status_msg, "%c %s %s %s %s",
	    prev_bcond ? 'Y' : 'N',
	    iname(HPACK(prev_icode, prev_ifun)),
	    istring, mstring, pstring);

    return status_msg;
}

static char *format_pc()
{
    char pstring[17];
    wstring(pc, 4, 64, pstring);
    sprintf(status_msg, "%s", pstring);
    return status_msg;
}

static char *format_f()
{
    char valcstring[17];
    char valpstring[17];
    wstring(valc, 4, 64, valcstring);
    wstring(valp, 4, 64, valpstring);
    sprintf(status_msg, "%s %s %s %s %s", 
	    iname(HPACK(icode, ifun)),
	    reg_name(ra),
	    reg_name(rb),
	    valcstring,
	    valpstring);
    return status_msg;
}

static char *format_d()
{
    char valastring[17];
    char valbstring[17];
    wstring(vala, 4, 64, valastring);
    wstring(valb, 4, 64, valbstring);
    sprintf(status_msg, "%s %s %s %s %s %s",
	    valastring,
	    valbstring,
	    reg_name(destE),
	    reg_name(destM),
	    reg_name(srcA),
	    reg_name(srcB));

    return status_msg;
}

static char *format_e()
{
    char valestring[17];
    wstring(vale, 4, 64, valestring);
    sprintf(status_msg, "%c %s",
	    bcond ? 'Y' : 'N',
	    valestring);
    return status_msg;
}

static char *format_m()
{
    char valmstring[17];
    wstring(valm, 4, 64, valmstring);
    sprintf(status_msg, "%s", valmstring);
    return status_msg;
}

static char *format_npc()
{
    char npcstring[17];
    wstring(pc_in, 4, 64, npcstring);
    sprintf(status_msg, "%s", npcstring);
    return status_msg;
}
#endif /* HAS_GUI */

/* Report system state */
#ifdef SNU
void sim_report() {
#else
static void sim_report() {
#endif

#ifdef HAS_GUI
    if (gui_mode) {
	report_pc(pc);
	if (plusmode) {
	    report_state("PREV", format_prev());
	    report_state("PC", format_pc());
	} else {
	    report_state("OPC", format_pc());
	}
	report_state("F", format_f());
	report_state("D", format_d());
	report_state("E", format_e());
	report_state("M", format_m());
	if (!plusmode) {
	    report_state("NPC", format_npc());
	}
	show_cc(cc);
    }
#endif /* HAS_GUI */

}

/* If dumpfile set nonNULL, lots of status info printed out */
void sim_set_dumpfile(FILE *df)
{
    dumpfile = df;
}

/*
 * sim_log dumps a formatted string to the dumpfile, if it exists
 * accepts variable argument list
 */
void sim_log( const char *format, ... ) {
    if (dumpfile) {
	va_list arg;
	va_start( arg, format );
	vfprintf( dumpfile, format, arg );
	va_end( arg );
    }
}


/*************************************************************
 * Part 3: This part contains simulation control for the TK
 * simulator. 
 *************************************************************/

#ifdef HAS_GUI

/**********************
 * Begin Part 3 globals	
 **********************/

/* Hack for SunOS */
extern int matherr();
int *tclDummyMathPtr = (int *) matherr;

static char tcl_msg[256];

/* Keep track of the TCL Interpreter */
static Tcl_Interp *sim_interp = NULL;

static mem_t post_load_mem;

/**********************
 * End Part 3 globals	
 **********************/


/* function prototypes */
int simResetCmd(ClientData clientData, Tcl_Interp *interp,
		int argc, char *argv[]);
int simLoadCodeCmd(ClientData clientData, Tcl_Interp *interp,
		   int argc, char *argv[]);
int simLoadDataCmd(ClientData clientData, Tcl_Interp *interp,
		   int argc, char *argv[]);
int simRunCmd(ClientData clientData, Tcl_Interp *interp,
	      int argc, char *argv[]);
void addAppCommands(Tcl_Interp *interp);

/******************************************************************************
 *	tcl command definitions
 ******************************************************************************/

/* Implement command versions of the simulation functions */
int simResetCmd(ClientData clientData, Tcl_Interp *interp,
		int argc, char *argv[])
{
    sim_interp = interp;
    if (argc != 1) {
	interp->result = "No arguments allowed";
	return TCL_ERROR;
    }
    sim_reset();
    if (post_load_mem) {
	free_mem(mem);
	mem = copy_mem(post_load_mem);
    }
    interp->result = stat_name(STAT_AOK);
    return TCL_OK;
}

int simLoadCodeCmd(ClientData clientData, Tcl_Interp *interp,
		   int argc, char *argv[])
{
    FILE *object_file;
    word_t code_count;
    sim_interp = interp;
    if (argc != 2) {
	interp->result = "One argument required";
	return TCL_ERROR;
    }
    object_file = fopen(argv[1], "r");
    if (!object_file) {
	sprintf(tcl_msg, "Couldn't open code file '%s'", argv[1]);
	interp->result = tcl_msg;
	return TCL_ERROR;
    }
    sim_reset();
    code_count = load_mem(mem, object_file, 0);
    post_load_mem = copy_mem(mem);
    sprintf(tcl_msg, "%lld", code_count);
    interp->result = tcl_msg;
    fclose(object_file);
    return TCL_OK;
}

int simLoadDataCmd(ClientData clientData, Tcl_Interp *interp,
		   int argc, char *argv[])
{
    FILE *data_file;
    word_t word_count = 0;
    interp->result = "Not implemented";
    return TCL_ERROR;


    sim_interp = interp;
    if (argc != 2) {
	interp->result = "One argument required";
	return TCL_ERROR;
    }
    data_file = fopen(argv[1], "r");
    if (!data_file) {
	sprintf(tcl_msg, "Couldn't open data file '%s'", argv[1]);
	interp->result = tcl_msg;
	return TCL_ERROR;
    }
    sprintf(tcl_msg, "%lld", word_count);
    interp->result = tcl_msg;
    fclose(data_file);
    return TCL_OK;
}


int simRunCmd(ClientData clientData, Tcl_Interp *interp,
	      int argc, char *argv[])
{
    word_t step_limit = 1;
    byte_t run_status;
    cc_t cc;
    sim_interp = interp;
    if (argc > 2) {
	interp->result = "At most one argument allowed";
	return TCL_ERROR;
    }
    if (argc >= 2 &&
	(sscanf(argv[1], "%lld", &step_limit) != 1 ||
	 step_limit < 0)) {
	sprintf(tcl_msg, "Cannot run for '%s' cycles!", argv[1]);
	interp->result = tcl_msg;
	return TCL_ERROR;
    }
    sim_run(step_limit, &run_status, &cc);
    interp->result = stat_name(run_status);
    return TCL_OK;
}

/******************************************************************************
 *	registering the commands with tcl
 ******************************************************************************/

void addAppCommands(Tcl_Interp *interp)
{
    sim_interp = interp;
    Tcl_CreateCommand(interp, "simReset", (Tcl_CmdProc *) simResetCmd,
		      (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateCommand(interp, "simCode", (Tcl_CmdProc *) simLoadCodeCmd,
		      (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateCommand(interp, "simData", (Tcl_CmdProc *) simLoadDataCmd,
		      (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateCommand(interp, "simRun", (Tcl_CmdProc *) simRunCmd,
		      (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
} 

/******************************************************************************
 *	tcl functionality called from within C
 ******************************************************************************/

/* Provide mechanism for simulator to update register display */
void signal_register_update(reg_id_t r, word_t val) {
    int code;
    sprintf(tcl_msg, "setReg %d %lld 1", (int) r, (word_t) val);
    code = Tcl_Eval(sim_interp, tcl_msg);
    if (code != TCL_OK) {
	fprintf(stderr, "Failed to signal register set\n");
	fprintf(stderr, "Error Message was '%s'\n", sim_interp->result);
    }
}

/* Provide mechanism for simulator to generate memory display */
void create_memory_display() {
    int code;
    sprintf(tcl_msg, "createMem %lld %lld", minAddr, memCnt);
    code = Tcl_Eval(sim_interp, tcl_msg);
    if (code != TCL_OK) {
	fprintf(stderr, "Command '%s' failed\n", tcl_msg);
	fprintf(stderr, "Error Message was '%s'\n", sim_interp->result);
    } else {
	word_t i;
	for (i = 0; i < memCnt && code == TCL_OK; i+=8) {
	    word_t addr = minAddr+i;
	    word_t val;
	    if (!get_word_val(mem, addr, &val)) {
		fprintf(stderr, "Out of bounds memory display\n");
		return;
	    }
	    sprintf(tcl_msg, "setMem %lld %lld", addr, val);
	    code = Tcl_Eval(sim_interp, tcl_msg);
	}
	if (code != TCL_OK) {
	    fprintf(stderr, "Couldn't set memory value\n");
	    fprintf(stderr, "Error Message was '%s'\n", sim_interp->result);
	}
    }
}

/* Provide mechanism for simulator to update memory value */
void set_memory(word_t addr, word_t val) {
    int code;
    word_t nminAddr = minAddr;
    word_t nmemCnt = memCnt;

    /* First see if we need to expand memory range */
    if (memCnt == 0) {
	nminAddr = addr;
	nmemCnt = 8;
    } else if (addr < minAddr) {
	nminAddr = addr;
	nmemCnt = minAddr + memCnt - addr;
    } else if (addr >= minAddr+memCnt) {
	nmemCnt = addr-minAddr+8;
    }
    /* Now make sure nminAddr & nmemCnt are multiples of 16 */
    nmemCnt = ((nminAddr & 0xF) + nmemCnt + 0xF) & ~0xF;
    nminAddr = nminAddr & ~0xF;

    if (nminAddr != minAddr || nmemCnt != memCnt) {
	minAddr = nminAddr;
	memCnt = nmemCnt;
	create_memory_display();
    } else {
	sprintf(tcl_msg, "setMem %lld %lld", addr, val);
	code = Tcl_Eval(sim_interp, tcl_msg);
	if (code != TCL_OK) {
	    fprintf(stderr, "Couldn't set memory value 0x%llx to 0x%llx\n",
		    addr, val);
	    fprintf(stderr, "Error Message was '%s'\n", sim_interp->result);
	}
    }
}

/* Provide mechanism for simulator to update condition code display */
void show_cc(cc_t cc)
{
    int code;
    sprintf(tcl_msg, "setCC %d %d %d",
	    GET_ZF(cc), GET_SF(cc), GET_OF(cc));
    code = Tcl_Eval(sim_interp, tcl_msg);
    if (code != TCL_OK) {
	fprintf(stderr, "Failed to display condition codes\n");
	fprintf(stderr, "Error Message was '%s'\n", sim_interp->result);
    }
}

/* Provide mechanism for simulator to clear register display */
void signal_register_clear() {
    int code;
    code = Tcl_Eval(sim_interp, "clearReg");
    if (code != TCL_OK) {
	fprintf(stderr, "Failed to signal register clear\n");
	fprintf(stderr, "Error Message was '%s'\n", sim_interp->result);
    }
}

/* Provide mechanism for simulator to report instructions as they are 
   read in
*/

void report_line(word_t line_no, word_t addr, char *hex, char *text) {
    int code;
    sprintf(tcl_msg, "addCodeLine %lld %lld {%s} {%s}", line_no, addr, hex, text);
    code = Tcl_Eval(sim_interp, tcl_msg);
    if (code != TCL_OK) {
	fprintf(stderr, "Failed to report code line 0x%llx\n", addr);
	fprintf(stderr, "Error Message was '%s'\n", sim_interp->result);
    }
}


/* Provide mechanism for simulator to report which instruction
   is being executed */
void report_pc(word_t pc)
{
    int t_status;
    char addr[18];
    char code[20];
    Tcl_DString cmd;
    Tcl_DStringInit(&cmd);
    Tcl_DStringAppend(&cmd, "simLabel ", -1);
    Tcl_DStringStartSublist(&cmd);
    sprintf(addr, "%llu", pc);
    Tcl_DStringAppendElement(&cmd, addr);

    Tcl_DStringEndSublist(&cmd);
    Tcl_DStringStartSublist(&cmd);
    sprintf(code, "%s","*");
    Tcl_DStringAppend(&cmd, code, -1);
    Tcl_DStringEndSublist(&cmd);
    t_status = Tcl_Eval(sim_interp, Tcl_DStringValue(&cmd));
    if (t_status != TCL_OK) {
	fprintf(stderr, "Failed to report code '%s'\n", code);
	fprintf(stderr, "Error Message was '%s'\n", sim_interp->result);
    }
}

/* Report single line of stage state */
void report_state(char *id, char *txt)
{
    int t_status;
    sprintf(tcl_msg, "updateStage %s {%s}", id, txt);
    t_status = Tcl_Eval(sim_interp, tcl_msg);
    if (t_status != TCL_OK) {
	fprintf(stderr, "Failed to report processor status\n");
	fprintf(stderr, "\tStage %s, status '%s'\n",
		id, txt);
	fprintf(stderr, "\tError Message was '%s'\n", sim_interp->result);
    }
}

/*
 * Tcl_AppInit - Called by TCL to perform application-specific initialization.
 */
int Tcl_AppInit(Tcl_Interp *interp)
{
    /* Tell TCL about the name of the simulator so it can  */
    /* use it as the title of the main window */
    Tcl_SetVar(interp, "simname", simname, TCL_GLOBAL_ONLY);

    if (Tcl_Init(interp) == TCL_ERROR)
	return TCL_ERROR;
    if (Tk_Init(interp) == TCL_ERROR)
	return TCL_ERROR;
    Tcl_StaticPackage(interp, "Tk", Tk_Init, Tk_SafeInit);

    /* Call procedure to add new commands */
    addAppCommands(interp);

    /*
     * Specify a user-specific startup file to invoke if the application
     * is run interactively.  Typically the startup file is "~/.apprc"
     * where "app" is the name of the application.  If this line is deleted
     * then no user-specific startup file will be run under any conditions.
     */
    Tcl_SetVar(interp, "tcl_rcFileName", "~/.wishrc", TCL_GLOBAL_ONLY);
    return TCL_OK;

}

 
#endif /* HAS_GUI */
