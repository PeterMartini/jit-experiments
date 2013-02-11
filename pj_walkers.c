#include <stdlib.h>
#include <stdio.h>
#include "pj_walkers.h"

static void
pj_tree_extract_vars_internal(pj_term_t *term, pj_var_t * **vars, unsigned int *nvars)
{
  if (term->type == pj_t_variable)
  {
    /* not efficient, but simple */
    if (vars == NULL)
      *vars = (pj_var_t **)malloc(sizeof(pj_var_t *));
    else
      *vars = (pj_var_t **)realloc(*vars, (*nvars+1) * sizeof(pj_var_t *));
    (*vars)[*nvars] = (pj_var_t *)term;
    (*nvars)++;
  }
  else if (term->type == pj_t_binop)
  {
    pj_tree_extract_vars_internal(((pj_binop_t *)term)->op1, vars, nvars);
    pj_tree_extract_vars_internal(((pj_binop_t *)term)->op2, vars, nvars);
  }
}

void
pj_tree_extract_vars(pj_term_t *term, pj_var_t * **vars, unsigned int *nvars)
{
  *nvars = 0;
  *vars = NULL;
  pj_tree_extract_vars_internal(term, vars, nvars);
}

pj_basic_type
pj_tree_determine_funtype(pj_term_t *term)
{
  if (term->type == pj_t_variable) {
    return ((pj_var_t *)term)->var_type;
  }
  else if (term->type == pj_t_constant) {
    return ((pj_const_t *)term)->const_type;
  }
  else if (term->type == pj_t_binop) {
    pj_basic_type t1, t2;
    t1 = pj_tree_determine_funtype(((pj_binop_t *)term)->op1);
    if (t1 == pj_double_type)
      return pj_double_type; /* double >> int */
    t2 = pj_tree_determine_funtype(((pj_binop_t *)term)->op2);
    if (t2 == pj_double_type)
      return pj_double_type;
    return pj_int_type; /* no uint support yet */
  }
}

