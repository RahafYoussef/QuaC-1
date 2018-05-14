#include <petsc.h>
#include "quantum_gates.h"

void projectq_qasm_read(char[],PetscInt*,circuit*);
void projectq_vqe_get_expectation(char[],Vec,PetscScalar*);
void _projectq_qasm_add_gate(char*,circuit*,PetscReal);
void projectq_vqe_get_expectation_encoded(char[],Vec,PetscScalar*,PetscInt,...);
