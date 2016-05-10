CFLAGS	         =


ODIR=obj
SRCDIR=src

include ${PETSC_DIR}/lib/petsc/conf/variables
include ${PETSC_DIR}/lib/petsc/conf/rules

_DEPS = operators.h solver.h operators_p.h
DEPS  = $(patsubst %,$(SRCDIR)/%,$(_DEPS))

_OBJ  = operators.o solver.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))

$(ODIR)/%.o: $(SRCDIR)/%.c $(DEPS)
	${PETSC_COMPILE} -c -o $@ $< $(CFLAGS) ${PETSC_KSP_LIB}

nv_cooling_2state: $(ODIR)/nv_cooling_2state.o $(OBJ)
	-${CLINKER} -o $@ $^ $(CFLAGS) ${PETSC_KSP_LIB}

simple_jc_test: $(ODIR)/simple_jc_test.o $(OBJ)
	-${CLINKER} -o $@ $^ $(CFLAGS) ${PETSC_KSP_LIB}

# .PHONY: clean

# clean:
# 	rm -f $(ODIR)/*.o
