# Comment this out if you don't have Tcl/Tk on your system

#GUIMODE=-DHAS_GUI

# Modify the following line so that gcc can find the libtcl.so and
# libtk.so libraries on your system. You may need to use the -L option
# to tell gcc which directory to look in. Comment this out if you
# don't have Tcl/Tk.

#TKLIBS=-L/usr/lib -ltk -ltcl

# Modify the following line so that gcc can find the tcl.h and tk.h
# header files on your system. Comment this out if you don't have
# Tcl/Tk.

#TKINC=-isystem /usr/include/tcl8.5

##################################################
# You shouldn't need to modify anything below here
##################################################

# Use this rule (make all) to build the Y86-64 tools. The variables you've
# assigned to GUIMODE, TKLIBS, and TKINC will override the values that
# are currently assigned in seq/Makefile and pipe/Makefile.
all:
	(cd misc; make all)
#	(cd pipe; make all GUIMODE=$(GUIMODE) TKLIBS="$(TKLIBS)" TKINC="$(TKINC)")
	(cd seq; make all GUIMODE=$(GUIMODE) TKLIBS="$(TKLIBS)" TKINC="$(TKINC)")
	(cd y86-code; make all)

handin: FORCE
	@make clean > /dev/null
	@rm -rf handin
	@mkdir handin
	@cp misc/isacore.c handin 
	@cp seq/seq-full.hcl handin
	@cp seq/ssimcore.c handin
	@(cd handin; tar cvzf pa4.tar.gz isacore.c seq-full.hcl ssimcore.c ) > /dev/null
	@echo "Submit handin/pa4.tar.gz file to the sys.snu.ac.kr server"

clean:
	rm -rf handin
	rm -f *~ core
	(cd misc; make clean)
	(cd seq; make clean)
	(cd y86-code; make clean)

FORCE: ;
