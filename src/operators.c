#include "kron_p.h" //Includes petscmat.h and operators_p.h
#include "quac_p.h" 
#include "operators.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

/* TODO? : 
 * - put wrappers into quac.h
 * - variable number of arguments to add_to_ham and add_to_lin
 * - add_to_ham_mult4 for coupling between two vec subsystems
 */


static int              op_initialized = 0;
/* Declare private, library variables. Externed in operators_p.h */
int op_finalized;
Mat full_A;
long total_levels;
int num_subsystems;
operator subsystem_list[MAX_SUB];

/*
 * create_op creates a basic set of operators, namely the creation, annihilation, and
 * number operator. 
 * Inputs:
 *        int number_of_levels: number of levels for this basic set
 * Outputs:
 *       operator *new_op: lowering op (op), raising op (op->dag), and number op (op->n)
 */

void create_op(int number_of_levels,operator *new_op) {
  operator temp = NULL;

  _check_initialized_op();

  /* First make the annihilation operator */
  temp             = malloc(sizeof(struct operator));
  temp->n_before   = total_levels;
  temp->my_levels  = number_of_levels;
  temp->my_op_type = LOWER;
  /* Since this is a basic operator, not a vec, set positions to -1 */
  temp->position   = -1;
  *new_op          = temp;

  temp             = malloc(sizeof(struct operator));
  temp->n_before   = total_levels;
  temp->my_levels  = number_of_levels;
  temp->my_op_type = RAISE;
  /* Since this is a basic operator, not a vec, set positions to -1 */
  temp->position   = -1;
  (*new_op)->dag   = temp;

  temp             = malloc(sizeof(struct operator));
  temp->n_before   = total_levels;
  temp->my_levels  = number_of_levels;
  temp->my_op_type = NUMBER;
  /* Since this is a basic operator, not a vec, set positions to -1 */
  temp->position   = -1;

  (*new_op)->n     = temp;

  /* Increase total_levels */
  total_levels = total_levels * number_of_levels;

  /* Add to list */
  subsystem_list[num_subsystems] = (*new_op);
  num_subsystems++;
  return;
}

/*
 * create_op creates a basic set of operators, namely the creation, annihilation, and
 * number operator. 
 * Inputs:
 *        int number_of_levels: number of levels for this basic set
 * Outputs:
 *       operator *new_op: lowering op (op), raising op (op->dag), and number op (op->n)
 */

void create_vec(int number_of_levels,vec_op *new_vec) {
  operator temp = NULL;
  int i;
  _check_initialized_op();

  (*new_vec) = malloc(number_of_levels*(sizeof(struct operator*)));
  for (i=0;i<number_of_levels;i++){
    temp             = malloc(sizeof(struct operator));
    temp->n_before   = total_levels;
    temp->my_levels  = number_of_levels;
    temp->my_op_type = VEC;
    /* This is a VEC operator; set its position */
    temp->position   = i;
    (*new_vec)[i]       = temp;
  }

  /* Increase total_levels */
  total_levels = total_levels * number_of_levels;

  /* 
   * We store just the first VEC in the subsystem list, since it has
   * enough information to define all others
   */
  subsystem_list[num_subsystems] = (*new_vec)[0];
  num_subsystems++;
  return;

}

/*
 * add_to_ham adds a*op(handle1) to the hamiltonian
 * Inputs:
 *        double a:    scalar to multiply op(handle1)
 *        operator op: operator to add
 * Outputs:
 *        none
 */
void add_to_ham(double a,operator op){
  PetscScalar    mat_scalar;

  
  _check_initialized_A();

  /*
   * Construct the dense Hamiltonian only on the master node
   * extra_before and extra_after are 1, letting the code know to
   * do the dense, operator space H.
   */
  /* if (nid==0) { */
  /*   mat_scalar = a; */
  /*   _add_to_PETSc_kron(mat_scalar,op->n_before,op->my_levels,op->my_op_type,op->position,1,1); */
  /* } */

  /*
   * Add -i * (I cross H) to the superoperator matrix, A
   * Since this is an additional I before, we simply
   * pass total_levels as extra_before
   * We pass the -a*PETSC_i to get the sign and imaginary part correct.
   */


  mat_scalar = -a*PETSC_i;
  _add_to_PETSc_kron(mat_scalar,op->n_before,op->my_levels,
                     op->my_op_type,op->position,total_levels,1);

  /*
   * Add i * (H cross I) to the superoperator matrix, A
   * Since this is an additional I after, we simply
   * pass total_levels as extra_after.
   * We pass a*PETSC_i to get the imaginary part correct.
   */

  mat_scalar = a*PETSC_i;
  _add_to_PETSc_kron(mat_scalar,op->n_before,op->my_levels,
                     op->my_op_type,op->position,1,total_levels);
  return;
}



/*
 * add_to_ham_mult2 adds a*op(handle1)*op(handle2) to the hamiltonian
 * Inputs:
 *        double a:     scalar to multiply op(handle1)
 *        operator op1: the first operator
 *        operator op2: the second operator
 * Outputs:
 *        none
 */
void add_to_ham_mult2(double a,operator op1,operator op2){
  PetscScalar mat_scalar;
  int         multiply_vec,n_after;
  _check_initialized_A();
  multiply_vec = _check_op_type2(op1,op2);
  
  /*
   * Add -i * (I cross H) to the superoperator matrix, A
   * Since this is an additional I before, we simply
   * pass total_levels as extra_before
   * We pass the -a*PETSC_i to get the sign and imaginary part correct.
   */

  mat_scalar = -a*PETSC_i;
  if (multiply_vec){
    /* 
     * We are multiplying two vec ops. This will only be one value (of 1.0) in the 
     * subspace, at location op1->position, op2->position.
     */
    n_after    = total_levels/(op1->my_levels*op1->n_before);
    _add_to_PETSc_kron_ij(mat_scalar,op1->position,op2->position,op1->n_before*total_levels,
                          n_after,op1->my_levels);
  } else {
    /* We are multiplying two normal ops and have to do a little more work. */
    _add_to_PETSc_kron_comb(mat_scalar,op1->n_before,op1->my_levels,op1->my_op_type,op1->position,
                            op2->n_before,op2->my_levels,op2->my_op_type,op2->position,
                            total_levels,1,1);
  }
  /*
   * Add i * (H cross I) to the superoperator matrix, A
   * Since this is an additional I after, we simply
   * pass total_levels as extra_after.
   * We pass a*PETSC_i to get the imaginary part correct.
   */
  mat_scalar = a*PETSC_i;
  if (multiply_vec){
    /* 
     * We are multiplying two vec ops. This will only be one value (of 1.0) in the 
     * subspace, at location op1->position, op2->position.
     */
    n_after    = total_levels/(op1->my_levels*op1->n_before);
    _add_to_PETSc_kron_ij(mat_scalar,op1->position,op2->position,op1->n_before,
                          n_after*total_levels,op1->my_levels);
  } else {
    /* We are multiplying two normal ops and have to do a little more work. */
    _add_to_PETSc_kron_comb(mat_scalar,op1->n_before,op1->my_levels,op1->my_op_type,op1->position,
                            op2->n_before,op2->my_levels,op2->my_op_type,op2->position,
                            1,1,total_levels);
  }

  return;
}


/*
 * add_to_ham_mult3 adds a*op1*op2*op3 to the hamiltonian
 * currently assumes either (op1,op2) or (op2,op3) is a pair 
 * of vector operators
 * Inputs:
 *        double a:     scalar to multiply op(handle1)
 *        operator op1: the first operator
 *        operator op2: the second operator
 *        operator op3: the second operator
 * Outputs:
 *        none
 */

void add_to_ham_mult3(double a,operator op1,operator op2,operator op3){
  PetscScalar mat_scalar;
  int         first_pair;
  _check_initialized_A();
  first_pair = _check_op_type3(op1,op2,op3);

  /*
   * Add -i * (I cross H) to the superoperator matrix, A
   * Since this is an additional I before, we simply
   * pass total_levels as extra_before
   * We pass the -a*PETSC_i to get the sign and imaginary part correct.
   */
  mat_scalar  = -a*PETSC_i;
  if (first_pair) {
    /* The first pair is the vec pair and op3 is the normal op*/  
    _add_to_PETSc_kron_comb_vec(mat_scalar,op3->n_before,op3->my_levels,
                                op3->my_op_type,op1->n_before,op1->my_levels,
                                op1->position,op2->position,total_levels,1,1);

  } else {
    /* The last pair is the vec pair and op1 is the normal op*/  
    _add_to_PETSc_kron_comb_vec(mat_scalar,op1->n_before,op1->my_levels,
                                op1->my_op_type,op2->n_before,op2->my_levels,
                                op2->position,op3->position,total_levels,1,1);

  }
  /*
   * Add i * (H cross I) to the superoperator matrix, A
   * Since this is an additional I after, we simply
   * pass total_levels as extra_after.
   * We pass a*PETSC_i to get the imaginary part correct.
   */
  mat_scalar  = a*PETSC_i;
  if (first_pair) {
    /* The first pair is the vec pair and op3 is the normal op*/  
    _add_to_PETSc_kron_comb_vec(mat_scalar,op3->n_before,op3->my_levels,
                                op3->my_op_type,op1->n_before,op1->my_levels,
                                op1->position,op2->position,1,1,total_levels);

  } else {
    /* The last pair is the vec pair and op1 is the normal op*/  
    _add_to_PETSc_kron_comb_vec(mat_scalar,op1->n_before,op1->my_levels,
                                op1->my_op_type,op2->n_before,op2->my_levels,
                                op2->position,op3->position,1,1,total_levels);

  }

  return;
}

/*
 * add_lin adds a Lindblad L(C) term to the system of equations, where
 * L(C)p = C p C^t - 1/2 (C^t C p + p C^t C)
 * Or, in superoperator space
 * Lp    = C cross C - 1/2(C^t C cross I + I cross C^t C) p
 *
 * Inputs:
 *        double a:    scalar to multiply L term (note: Full term, not sqrt())
 *        operator op: op to make L(C) of
 * Outputs:
 *        none
 */

void add_lin(double a,operator op){
  PetscScalar    mat_scalar;

  _check_initialized_A();

  /*
   * Add (I cross C^t C) to the superoperator matrix, A
   * Which is (I_total cross I_before cross C^t C cross I_after)
   * Since this is an additional I_total before, we simply
   * set extra_before to total_levels
   */
  mat_scalar = -0.5*a;
  _add_to_PETSc_kron_lin(mat_scalar,op->n_before,op->my_levels,op->my_op_type,
                         op->position,total_levels,1);
  /*
   * Add (C^t C cross I) to the superoperator matrix, A
   * Which is (I_before cross C^t C cross I_after cross I_total)
   * Since this is an additional I_total after, we simply
   * set extra_after to total_levels
   */
  _add_to_PETSc_kron_lin(mat_scalar,op->n_before,op->my_levels,op->my_op_type,
                         op->position,1,total_levels);

  /*
   * Add (C' cross C') to the superoperator matrix, A, where C' is the full space
   * representation of C. Let I_b = I_before and I_a = I_after
   * This simplifies to (I_b cross C cross I_a cross I_b cross C cross I_a)
   * or (I_b cross C cross I_ab cross C cross I_a)
   * This is just like add_to_ham_comb, with n_between = n_after*n_before
   */
  mat_scalar = a;
  _add_to_PETSc_kron_lin_comb(mat_scalar,op->n_before,op->my_levels,op->my_op_type,
                              op->position);

  return;
}


/*
 * add_lin_vec adds a Lindblad L(C=|op1><op2|) term to the system of equations, where
 * L(|1><2|)p = |1><2| p |2><1| - 1/2 (|2><2| p + p |2><2|)
 * Or, in superoperator space
 * Lp    = |1><2| cross |1><2| - 1/2(|2><2| cross I + I cross |2><2|) p
 *
 * where C is the outer product of two VECs
 * Inputs:
 *        double a:     scalar to multiply L term (note: Full term, not sqrt())
 *        operator op1: VEC 1
 *        operator op2: VEC 2
 * Outputs:
 *        none
 */

void add_lin_vec(double a,operator op1,operator op2){
  PetscScalar mat_scalar;
  int         k3,i1,j1,i2,j2,i_comb,j_comb,comb_levels;
  int         multiply_vec,n_after;   
  
  _check_initialized_A();
  multiply_vec =  _check_op_type2(op1,op2);

  if (!multiply_vec){
    if (nid==0) {
      printf("ERROR! Lindblad of two basic ops not supported.");
      exit(0);
    }
  }

  /*
   * Add (I cross C^t C)  = (I cross |2><2| ) to the superoperator matrix, A
   * Which is (I_total cross I_before cross C^t C cross I_after)
   * Since this is an additional I_total before, we simply
   * set extra_before to total_levels
   *
   * Since this is an outer product of VEC ops, its i,j is just op2->position,op2->position,
   * and its val is 1.0
   */
  n_after    = total_levels/(op1->my_levels*op1->n_before);
  mat_scalar = -0.5*a;

  _add_to_PETSc_kron_ij(mat_scalar,op2->position,op2->position,op2->n_before*total_levels,
                        n_after,op2->my_levels);
  /*
   * Add (C^t C cross I) = (|2><2| cross I) to the superoperator matrix, A
   * Which is (I_before cross C^t C cross I_after cross I_total)
   * Since this is an additional I_total after, we simply
   * set extra_after to total_levels
   */
  _add_to_PETSc_kron_ij(mat_scalar,op2->position,op2->position,op2->n_before,
                        n_after*total_levels,op2->my_levels);

  /*
   * Add (C' cross C') to the superoperator matrix, A, where C' is the full space
   * representation of C. Let I_b = I_before and I_a = I_after
   * This simplifies to (I_b cross C cross I_a cross I_b cross C cross I_a)
   * or (I_b cross C cross I_ab cross C cross I_a)
   * This is just like add_to_ham_comb, with n_between = n_after*n_before
   */
  comb_levels = op1->my_levels*op1->my_levels*op1->n_before*n_after;
  mat_scalar = a;
  for (k3=0;k3<op2->n_before*n_after;k3++){
    /*
     * Since this is an outer product of VEC ops, there is only 
     * one entry in C (and it is 1.0); we need not loop over anything.
     */
    i1   = op1->position;
    j1   = op2->position;
    i2   = i1 + k3*op1->my_levels;
    j2   = j1 + k3*op1->my_levels;

    /* 
     * Using the standard Kronecker product formula for 
     * A and I cross B, we calculate
     * the i,j pair for handle1 cross I cross handle2.
     * Through we do not use it here, we note that the new
     * matrix is also diagonal.
     * We need my_levels*n_before*n_after because we are taking
     * C cross (Ia cross Ib cross C), so the the size of the second operator
     * is my_levels*n_before*n_after
     */
    i_comb = op1->my_levels*op1->n_before*n_after*i1 + i2;
    j_comb = op1->my_levels*op1->n_before*n_after*j1 + j2;
    

    _add_to_PETSc_kron_ij(mat_scalar,i_comb,j_comb,op1->n_before,
                          n_after,comb_levels);
    
  }
  return;
}

/*
 * _check_initialized_op checks if petsc was initialized and sets up variables
 * for op creation. It also errors if there are too many subsystems or
 * if add_to_ham or add_to_lin was called.
 */

void _check_initialized_op(){
  /* Check to make sure petsc was initialize */
  if (!petsc_initialized){ 
    if (nid==0){
      printf("ERROR! You need to call QuaC_initialize before creating\n");
      printf("       any operators!\n");
      exit(0);
    }
  }

  /* Set up counters on first call */
  if (!op_initialized){
    op_finalized   = 0;
    total_levels   = 1;
    op_initialized = 1;
  }
    
  if (num_subsystems+1>MAX_SUB&&nid==0){
    if (nid==0){
      printf("ERROR! Too many systems for this MAX_SUB\n");
      exit(0);
    }
  }

  if (op_finalized){
    if (nid==0){
      printf("ERROR! You cannot add more operators after\n");
      printf("       calling add_to_ham or add_to_lin!\n");
      exit(0);
    }
  }

}

/*
 * _check_op_type2 checks to make sure the two ops can be
 * multiplied in a meaningful way. |s> a^\dagger doesn't make sense,
 * for instance
 * Inputs:
 *       operator op1
 *       operator op2
 * Return:
 *       0 if normal op * normal op
 *       1 if vec op * vec op
 */

int _check_op_type2(operator op1,operator op2){
  int return_value;
  /* Check if we are trying to multiply a vec and nonvec operator - should not happen */
  if (op1->my_op_type==VEC&&op2->my_op_type!=VEC){
    if (nid==0){
      printf("ERROR! Multiplying a VEC_OP and a regular OP does not make sense!\n");
      exit(0);
    }
  }
  /* Check if we are trying to multiply a vec and nonvec operator - should not happen */
  if (op2->my_op_type==VEC&&op1->my_op_type!=VEC){
    if (nid==0){
      printf("ERROR! Multiplying a VEC_OP and a regular OP does not make sense!\n");
      exit(0);
    }
  }

  /* Return 1 if we are multiplying two vec ops */
  if (op1->my_op_type==VEC&&op2->my_op_type==VEC){
    /* Check to make sure the two VEC are within the same subspace */
    if (op1->n_before!=op2->n_before) {
      if (nid==0){
        printf("ERROR! Multiplying two VEC_OPs from different subspaces does not make sense!\n");
        exit(0);
      }
    }
    return_value = 1;
  }

  /* Return 0 if we are multiplying two normal ops */
  if (op1->my_op_type!=VEC&&op2->my_op_type!=VEC){
    return_value = 0;
  }
  
  return return_value;
}


/*
 * _check_op_type3 checks to make sure the three ops can be
 * multiplied in a meaningful way. |s> a^\dagger a doesn't make sense,
 * for instance
 * Inputs:
 *       operator op1
 *       operator op2
 *       operator op3
 * Return:
 *       1 if the first pair is the vec pair
 *       0 if the second pair is the vec pair
 */
int _check_op_type3(operator op1,operator op2,operator op3){
  int return_value;

  /* Check to make sure the VEC op location makes sense */
  if (op1->my_op_type==VEC&&op2->my_op_type==VEC&&op3->my_op_type==VEC){
    if (nid==0){
      printf("ERROR! Multiplying three VEC_OPs does not make sense!\n");
      exit(0);
    }
  }

  if (op1->my_op_type!=VEC&&op2->my_op_type!=VEC&&op3->my_op_type==VEC){
    if (nid==0){
      printf("ERROR! Multiplying one VEC_OP and two normal ops does not make sense!\n");
      exit(0);
    }
  }

  if (op1->my_op_type==VEC&&op2->my_op_type!=VEC&&op3->my_op_type!=VEC){
    if (nid==0){
      printf("ERROR! Multiplying one VEC_OP and two normal ops does not make sense!\n");
      exit(0);
    }
  }

  if (op1->my_op_type!=VEC&&op2->my_op_type==VEC&&op3->my_op_type!=VEC){
    if (nid==0){
      printf("ERROR! Multiplying one VEC_OP and two normal ops does not make sense!\n");
      exit(0);
    }
  }

  if (op1->my_op_type==VEC&&op2->my_op_type!=VEC&&op3->my_op_type==VEC){
    if (nid==0){
      printf("ERROR! Multiplying VEC*OP*VEC does not make sense!\n");
      exit(0);
    }
  }

  if (op1->my_op_type!=VEC&&op2->my_op_type!=VEC&&op3->my_op_type!=VEC){
    if (nid==0){
      printf("ERROR! Multiplying OP*OP*OP currently not supported.\n");
      exit(0);
    }
  }

  /* Check to make sure the two VEC are in the same subsystems */
  if (op1->my_op_type==VEC&&op2->my_op_type==VEC){
    if (op1->n_before!=op2->n_before){
      if (nid==0){
        printf("ERROR! Multiplying two VEC from different subspaces does not make sense.\n");
        exit(0);
      }
    }
    return_value = 1;
  }
  
  /* Check to make sure the two VEC subsystems are the same */
  if (op2->my_op_type==VEC&&op3->my_op_type==VEC){
    if (op2->n_before!=op3->n_before){
      if (nid==0){
        printf("ERROR! Multiplying two VEC from different subspaces does not make sense.\n");
        exit(0);
      }
    }
    return_value = 0;
  }

  return return_value;
}

/*
 * _check_initialized_A checks to make sure petsc was initialized,
 * some ops were created, and, on first call, sets up the 
 * data structures for the matrices.
 */

void _check_initialized_A(){
  int            i,j;
  long           dim;
  PetscErrorCode ierr;

  /* Check to make sure petsc was initialize */
  if (!petsc_initialized){ 
    if (nid==0){
      printf("ERROR! You need to call QuaC_initialize before creating\n");
      printf("       any operators!");
      exit(0);
    }
  }
  /* Check to make sure some operators were created */
  if (!op_initialized){
    if (nid==0){
      printf("ERROR! You need to create operators before you add anything to\n");
      printf("       the Hamiltonian or Lindblad!\n");
      exit(0);
    }
  }
  
  if (!op_finalized){
    op_finalized = 1;
    /* Allocate space for (dense) Hamiltonian matrix in operator space
     * (for printing and debugging purposes)
     */
    if (nid==0) {
      printf("Operators created. Total Hilbert space size: %d\n",total_levels);
    }
    dim = total_levels*total_levels;
    /* Setup petsc matrix */
    //FIXME - do something better than 5*total_levels!!
    ierr = MatCreateAIJ(PETSC_COMM_WORLD,PETSC_DECIDE,PETSC_DECIDE,dim,dim,
                        5*total_levels,NULL,5*total_levels,NULL,&full_A);CHKERRQ(ierr);
    ierr = MatSetFromOptions(full_A);CHKERRQ(ierr);
    ierr = MatSetUp(full_A);CHKERRQ(ierr);
  }

  return;
}
