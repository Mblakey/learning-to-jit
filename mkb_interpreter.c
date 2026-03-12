
#include "mkb_tests.h"
#include "mkb_parser.h"

#include <x86intrin.h>

static void mkb_interpreter(mkb_ast_node *restrict root)
{
  /* hash symbol table */
  strhash_entry *var_store[HT_SIZE] = { NULL }; 
  
  struct rc_mkb_node {
    mkb_ast_node *n;
    mkb_ast_edge *e;
  }; 

  unsigned int nvars = 0;
  uint64_t vars[256];
  
  unsigned int nstack = 0;
  struct rc_mkb_node stack[256];
  stack[nstack++] = (struct rc_mkb_node){ .n=root, .e=root->children };

  while (nstack > 0) {
    struct rc_mkb_node *stack_top = &stack[nstack-1];
    mkb_ast_node *top_n = stack_top->n;
    mkb_ast_edge *top_e = stack_top->e;

    if (top_n->kind != LOOP_KIND) {
      if (top_e) {
        stack[nstack++] = (struct rc_mkb_node){ .n=top_e->dwn, .e=top_e->dwn->children };

        if (top_n->kind == OP_KIND && top_n->op_code == STO)
          stack_top->e = NULL; // we dont eval RHS of sto
        else
          stack_top->e = top_e->nxt;
        continue;
      }
    }
    
    switch (top_n->kind) {
      case OP_KIND:
        if (top_n->op_code == STO) {
          uint64_t store = vars[--nvars];
          struct mkb_ast_node *rhs = mkb_operator_rgt(top_n);
          strhash_table_store(var_store, (void*)store, rhs->var_name, rhs->var_len); 
        }
        else {
          uint64_t lhs_store = vars[--nvars];
          uint64_t rhs_store = vars[--nvars];

          switch (top_n->op_code) {
            case ADD: vars[nvars++] = lhs_store + rhs_store; break;
            case SUB: vars[nvars++] = lhs_store - rhs_store; break;
            case MUL: vars[nvars++] = lhs_store * rhs_store; break;
            case DIV: vars[nvars++] = lhs_store / rhs_store; break;
            default: break;
          }
        }
        break;

      case NUM_KIND:
        vars[nvars++] = top_n->num_value; 
        break; 

      case PRINT_KIND:
        printf("you have rachaeled \"%s\" and it is %llu\n",
          top_n->var_name,
          (uint64_t)strhash_table_load(var_store, top_n->var_name, top_n->var_len)
        );
        break; 

      case VAR_KIND:
        vars[nvars++] = (uint64_t)strhash_table_load(var_store, top_n->var_name, top_n->var_len);  
        break;

      case LOOP_KIND:
        if (top_n->incr == top_n->range) {
          nstack--;
          top_n->incr = 0;
        }
        else {
          top_n->incr++; 
          stack[nstack++] = (struct rc_mkb_node){ .n=top_e->dwn, .e=top_e->dwn->children };
        }
        continue;
    }
    nstack--; 
  }

  for (unsigned int i = 0; i < HT_SIZE; i++) {
    strhash_entry *slot = var_store[i];
    while (slot) {
      fprintf(stderr, "%s : %llu\n", slot->key, (unsigned long long)slot->value); 
      slot = slot->next;
    }
  }

  strhash_table_clear(var_store);
}


int main() {
  mkb_ast_node *ast_root = mkb_parse((unsigned char*)test_case, strlen(test_case)); 
  if (!ast_root) 
    return 1; 

  
  uint64_t beg = __rdtsc(); 

  mkb_interpreter(ast_root);   

  uint64_t end = __rdtsc(); 
  fprintf(stderr, "%llu instructions\n", end-beg); 
  return 0; 
}


