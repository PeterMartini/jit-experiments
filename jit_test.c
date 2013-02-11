
#include <stdio.h>
#include <stdlib.h>
#include <jit/jit.h>

#include <pj_terms.h>
#include <pj_walkers.h>

jit_value_t pj_jit_internal_binop(jit_function_t function, jit_value_t *var_values, int nvars, pj_binop_t *binop);

jit_value_t
pj_jit_internal(jit_function_t function, jit_value_t *var_values, int nvars, pj_term_t *term)
{
  if (term->type == pj_t_variable) {
    pj_var_t *v = (pj_var_t *)term;
    return var_values[v->ivar];
  }
  else if (term->type == pj_t_constant) {
    pj_const_t *c = (pj_const_t *)term;
    if (c->const_type == pj_int_type)
      return jit_value_create_nint_constant(function, jit_type_sys_int, c->value_u.int_value);
    else if (c->const_type == pj_uint_type) /* FIXME no jit_value_create_nuint_constant defined? */
      return jit_value_create_nint_constant(function, jit_type_sys_int, c->value_u.uint_value);
    else if (c->const_type == pj_double_type)
      return jit_value_create_float64_constant(function, jit_type_sys_double, c->value_u.dbl_value);
    else
      abort();
  }
  else if (term->type == pj_t_binop) {
    return pj_jit_internal_binop(function, var_values, nvars, (pj_binop_t *)term);
  }
  else {
    abort();
  }
}

jit_value_t
pj_jit_internal_binop(jit_function_t function, jit_value_t *var_values, int nvars, pj_binop_t *binop)
{
  jit_value_t tmp1, tmp2, rv;
  tmp1 = pj_jit_internal(function, var_values, nvars, binop->op1);
  tmp2 = pj_jit_internal(function, var_values, nvars, binop->op2);
  switch (binop->optype) {
  case pj_binop_add:
    rv = jit_insn_add(function, tmp1, tmp2);
    break;
  case pj_binop_subtract:
    rv = jit_insn_sub(function, tmp1, tmp2);
    break;
  case pj_binop_multiply:
    rv = jit_insn_mul(function, tmp1, tmp2);
    break;
  case pj_binop_divide:
    rv = jit_insn_div(function, tmp1, tmp2);
    break;
  default:
    abort();
  }

  return rv;
}



/* Generates outfun and funtype. funtype indicates the type of all parameters as well
 * as the return value. That's a very serious limitation, but perfectly good enough for
 * now. funtype will be int if all variables and constants are int, otherwise double. */
int
pj_tree_jit(jit_context_t context, pj_term_t *term, jit_function_t *outfun, pj_basic_type *funtype)
{
  unsigned int i;
  jit_function_t function;
  jit_type_t *params;
  jit_type_t signature;

  jit_context_build_start(context);

  /* Get the "function type" which is the type that will be used for the
   * return value as well as all arguments. Double trumps ints. */
  *funtype = pj_tree_determine_funtype(term);

  /* Extract all variable occurrances from the AST */
  pj_var_t **vars;
  unsigned int nvars;
  pj_tree_extract_vars(term, &vars, &nvars);
  printf("Found %i variable occurrances in tree.\n", nvars);

  /* Naive assumption: the maximum ivar is the total number if distinct arguments (-1) */
  unsigned int max_var = 0;
  for (i = 0; i < nvars; ++i) {
    if (max_var < vars[i]->ivar)
      max_var = vars[i]->ivar;
  }
  printf("Found %i distinct variables in tree.\n", 1+max_var);
  nvars = max_var+1;
  free(vars);

  /* Setup libjit func signature */
  params = (jit_type_t *)malloc(nvars*sizeof(jit_type_t));
  for (i = 0; (int)i < nvars; ++i) {
    params[i] = (*funtype == pj_int_type ? jit_type_sys_int : jit_type_sys_double);
  }
  signature = jit_type_create_signature(
    jit_abi_cdecl,
    (*funtype == pj_int_type ? jit_type_sys_int : jit_type_sys_double),
    params,
    nvars,
    1
  );
  function = jit_function_create(context, signature);

  /* Setup libjit values for func params */
  jit_value_t *var_values;
  var_values = (jit_value_t *)malloc(nvars*sizeof(jit_value_t));
  for (i = 0; (int)i < nvars; ++i) {
    var_values[i] = jit_value_get_param(function, i);
  }

  /* Recursively emit instructions for JIT and final return */
  jit_value_t rv = pj_jit_internal(function, var_values, nvars, term);
  jit_insn_return(function, rv);

  /* Make it so! */
  jit_function_compile(function);
  jit_context_build_end(context);

  *outfun = function;
  return 0;
}

int
main(int argc, char **argv)
{
  pj_term_t *t;

  /* initialize tree structure */

  /* This example: (2.2+(v1+v0))*v0 */
  pj_term_t *v0 = 
  t = (pj_term_t *)pj_make_binop(
    pj_binop_multiply,
    (pj_term_t *)pj_make_binop(
      pj_binop_add,
      (pj_term_t *)pj_make_const_dbl(2.2),
      (pj_term_t *)pj_make_binop(
        pj_binop_add,
        (pj_term_t *)pj_make_variable(1, pj_double_type),
        (pj_term_t *)pj_make_variable(0, pj_double_type)
      )
    ),
    (pj_term_t *)pj_make_variable(0, pj_double_type)
  );

  /* This example: 2.3+1 */
  /*
  t = (pj_term_t *)pj_make_binop(
    pj_binop_add,
    (pj_term_t *)pj_make_const_dbl(2.3),
    (pj_term_t *)pj_make_const_int(1)
  );
  */

  /* Just some debug output for the tree */
  pj_dump_tree(t);

  /* Setup JIT compiler */
  jit_context_t context;
  jit_function_t func = NULL;

  context = jit_context_create();

  /* Compile tree to function */
  pj_basic_type funtype;
  if (0 == pj_tree_jit(context, t, &func, &funtype)) {
    printf("JIT succeeded!\n");
  } else {
    printf("JIT failed!\n");
  }

  /* Setup function args */
  double arg1, arg2;
  void *args[2];
  double result;
  arg1 = 2.;
  arg2 = 5.;
  args[0] = &arg1;
  args[1] = &arg2;

  /* Call function (inefficiently) */
  jit_function_apply(func, args, &result);
  printf("foo(%f, %f) = %f\n", (float)arg1, (float)arg2, (float)result);

  void *cl = jit_function_to_closure(func);
  double (*fptr)(double x, double y) = cl;
  //int i;
  //for (i=0;i<1e7;++i){
    result = fptr(arg1, arg2);
  //}
  printf("foo(%f, %f) = %f\n", (float)arg1, (float)arg2, (float)result);

  /* Call function again, with slightly different input */
  arg2 = 3.8;
  jit_function_apply(func, args, &result);
  printf("foo(%f, %f) = %f\n", (float)arg1, (float)arg2, (float)result);

  /* cleanup */
  pj_free_tree(t);
  jit_context_destroy(context);
  return 0;
}

