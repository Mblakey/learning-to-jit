
#include "mkb_tests.h"
#include "mkb_parser.h"

#include <x86intrin.h>

#define USE_JIT 0

#if USE_JIT
#include <libtcc.h>
#define JIT_FLAG 0x1u
#endif

char c_buffer[16384] = {0}; 
char local[4096] = {0}; 


#if USE_JIT
strhash_entry *jit_vars[64]; 
unsigned int njit_vars = 0;

static void mkb_jit_parse_context(mkb_ast_node *n, strhash_entry *var_store[])
{
  switch (n->kind) {
    case ROOT_KIND: 
      for (mkb_ast_edge *e=n->children; e; e = e->nxt)
        mkb_jit_parse_context(e->dwn, var_store); 
      return;

    case STMT_KIND: 
        for (mkb_ast_edge *e = n->children; e; e = e->nxt)
          mkb_jit_parse_context(e->dwn, var_store); 
        return;
   
    case OP_KIND:
        mkb_jit_parse_context(mkb_operator_rgt(n), var_store);
        mkb_jit_parse_context(mkb_operator_lft(n), var_store);
        return; 
    
    case LOOP_KIND: 
      mkb_jit_parse_context(mkb_loop_stmt(n), var_store); 
      return; 
    
    /* maintain the order for the context struct */
    case VAR_KIND: {
        strhash_entry *ent = strhash_table_load(var_store, n->var_name, n->var_len); 
        if (ent && !ent->flags) {
          ent->flags |= JIT_FLAG; 
          jit_vars[njit_vars++] = ent;
        }
        return;
    }
  }
  return;
}


static void mkb_jit_emit_C(mkb_ast_node *n, int depth, int loop_cnt)
{
  switch (n->kind) {
    case ROOT_KIND: 
      for (mkb_ast_edge *e=n->children; e; e = e->nxt)
        mkb_jit_emit_C(e->dwn, 0, 0); 
      return;

    case STMT_KIND: 
        for (int i=0; i<depth; i++)
          strcat(c_buffer, " "); 
        strcat(c_buffer, "{\n"); 
        for (mkb_ast_edge *e = n->children; e; e = e->nxt) {
          for (int i=0; i<depth; i++)
            strcat(c_buffer, " "); 
          mkb_jit_emit_C(e->dwn, depth+2, loop_cnt); 
          strcat(c_buffer, ";\n"); 
        }
        for (int i=0; i<depth; i++)
          strcat(c_buffer, " "); 
        strcat(c_buffer, "}"); 
        return;
   
    case OP_KIND:
        mkb_jit_emit_C(mkb_operator_rgt(n), depth, loop_cnt);
        switch (n->op_code) {
          case ADD: strcat(c_buffer, " + "); break;
          case SUB: strcat(c_buffer, " - "); break;
          case MUL: strcat(c_buffer, " * "); break;
          case DIV: strcat(c_buffer, " / "); break;
          case STO: strcat(c_buffer, " = "); break;
        }

        mkb_jit_emit_C(mkb_operator_lft(n), depth, loop_cnt);
        return; 

    case DECL_KIND: 
        switch (n->var_type) {
          case INTEGER: strcat(c_buffer, "int "); break;
          case FLOAT: strcat(c_buffer, "float "); break;
          case STRING: strcat(c_buffer, "const char *"); break;
          case NONE: strcat(c_buffer, "null "); break;
        }
        snprintf(local, sizeof(local), "%s", n->var_name); 
        strcat(c_buffer, local); 
        return;

    case VAR_KIND: 
        snprintf(local, sizeof(local), "*(ctx->%s)", n->var_name); 
        strcat(c_buffer, local); 
        return;

    case PRINT_KIND:
      snprintf(local, sizeof(local), "printf(\"%%llu\\n\", %s)", n->var_name); 
      strcat(c_buffer, local); 
      return; 

    case NUM_KIND: 
        snprintf(local, sizeof(local), "%llu", n->num_value); 
        strcat(c_buffer, local); 
        return;

    case STR_KIND: 
        snprintf(local, sizeof(local), "\"%s\"", n->str); 
        strcat(c_buffer, local); 
        return;

    case LOOP_KIND: {
      unsigned char lchr = 'i' + loop_cnt; 
      snprintf(local, sizeof(local), "for (unsigned int _%c=0; _%c<%d; _%c++)\n", lchr, lchr, n->range, lchr); 
      strcat(c_buffer, local); 
      mkb_jit_emit_C(mkb_loop_stmt(n), depth, loop_cnt+1); 
      return; 
    }
  }

  return;
}
#endif

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
          (uint64_t)strhash_table_load(var_store, top_n->var_name, top_n->var_len)->value
        );
        break; 

      case VAR_KIND:
        vars[nvars++] = (uint64_t)strhash_table_load(var_store, top_n->var_name, top_n->var_len)->value;  
        break;
      
      /* JIT LOGIC */
      case LOOP_KIND: {

#if USE_JIT
        strcat(c_buffer, "#include <stdlib.h>\n");
        strcat(c_buffer, "#include <stdio.h>\n");

        /* parse the variables into determined order */
        mkb_jit_parse_context(top_n, var_store); 
        
        /* write the context struct */
        strcat(c_buffer, "struct jit_context {\n");
        for (unsigned int i=0; i<njit_vars; i++) {
          snprintf(local, sizeof(local), "uint64_t *%s;\n", jit_vars[i]->key);
          strcat(c_buffer, local); 
        }
        strcat(c_buffer, "};\n\n"); 

        /* emit the C from the loop */ 
        strcat(c_buffer, "void jit_func(struct jit_context *ctx) {\n");
        
        mkb_jit_emit_C(top_n, 0, 0); 
        strcat(c_buffer, "\n}\n"); 

        /* debug emitted the C code */
        printf("%s\n", c_buffer); 

        unsigned char *jit_context = (unsigned char*)malloc(sizeof(uint64_t*)*njit_vars);
        uint64_t **jit_ptr = (uint64_t**)jit_context;  
        
        /* write directly into the store memory addresses */
        for (unsigned int i=0; i<njit_vars; i++) {
          jit_ptr[i] = (uint64_t*)(&jit_vars[i]->value); 
          jit_vars[i]->flags = 0x0; 
        }

        TCCState *tcc = tcc_new();

        tcc_set_output_type(tcc, TCC_OUTPUT_MEMORY);
        tcc_compile_string(tcc, c_buffer);
        tcc_relocate(tcc, TCC_RELOCATE_AUTO);

        void (*fn)(void *ctx) = tcc_get_symbol(tcc, "jit_func");
        fn(jit_context);

        tcc_delete(tcc);

        free(jit_context); 
#else
        if (top_n->incr == top_n->range) {
          nstack--;
          top_n->incr = 0;
        }
        else {
          top_n->incr++; 
          stack[nstack++] = (struct rc_mkb_node){ .n=top_e->dwn, .e=top_e->dwn->children };
        }
        continue;
#endif
    }
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


