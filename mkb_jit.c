

#include <stdlib.h>
#include <stdio.h>

#include "mkb_tests.h"
#include "mkb_parser.h"

#include <libtcc.h>

char c_buffer[16384] = {0}; 
char local[4096] = {0}; 

#if 0
void jit_compile_and_run(JitContext *ctx, const char *emitted_src) {
  TCCState *tcc = tcc_new();

  // Output to memory - no temp files, no disk
  tcc_set_output_type(tcc, TCC_OUTPUT_MEMORY);

  // // Point tcc at the header search path we found in the Makefile
  // tcc_add_include_path(tcc, LIBTCC_INCLUDE_PATH);

  if (tcc_compile_string(tcc, emitted_src) == -1) {
      fprintf(stderr, "JIT compilation failed\n");
      tcc_delete(tcc);
      return;
  }

  // Relocate into an in-process executable region
  // TCC handles the mmap, permissions, everything
  if (tcc_relocate(tcc, TCC_RELOCATE_AUTO) == -1) {
      fprintf(stderr, "JIT relocation failed\n");
      tcc_delete(tcc);
      return;
  }

  // Pull the function pointer directly out of the compiled image
  void (*jit_body)(JitContext *) = tcc_get_symbol(tcc, "jit_body");

  if (jit_body)
    jit_body(ctx);

  // TCC owns the memory region - delete frees it
  tcc_delete(tcc);
}
#endif


static void mkb_emit_C(mkb_ast_node *n, int depth, int loop_cnt)
{
  switch (n->kind) {
    case ROOT_KIND: 
      for (mkb_ast_edge *e=n->children; e; e = e->nxt)
        mkb_emit_C(e->dwn, 0, 0); 
      return;

    case STMT_KIND: 
        for (int i=0; i<depth; i++)
          strcat(c_buffer, " "); 
        strcat(c_buffer, "{\n"); 
        for (mkb_ast_edge *e = n->children; e; e = e->nxt) {
          for (int i=0; i<depth; i++)
            strcat(c_buffer, " "); 
          mkb_emit_C(e->dwn, depth+2, loop_cnt); 
          strcat(c_buffer, ";\n"); 
        }
        for (int i=0; i<depth; i++)
          strcat(c_buffer, " "); 
        strcat(c_buffer, "}"); 
        return;
   
    case OP_KIND:
        mkb_emit_C(mkb_operator_rgt(n), depth, loop_cnt);
        switch (n->op_code) {
          case ADD: strcat(c_buffer, " + "); break;
          case SUB: strcat(c_buffer, " - "); break;
          case MUL: strcat(c_buffer, " * "); break;
          case DIV: strcat(c_buffer, " / "); break;
          case STO: strcat(c_buffer, " = "); break;
        }

        mkb_emit_C(mkb_operator_lft(n), depth, loop_cnt);
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
        snprintf(local, sizeof(local), "%s", n->var_name); 
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
      mkb_emit_C(mkb_loop_stmt(n), depth, loop_cnt+1); 
      return; 
    }
  }

  return;
}


int main() {
  mkb_ast_node *ast_root = mkb_parse((unsigned char*)test_case, strlen(test_case)); 
  if (!ast_root) 
    return 1; 
  
  mkb_assign_types(ast_root); 

  strcat(c_buffer, "#include <stdlib.h>\n");
  strcat(c_buffer, "#include <stdio.h>\n");
  strcat(c_buffer, "void jit_func(void *ctx)\n");
  mkb_emit_C(ast_root, 0, 0); 

  printf("%s\n", c_buffer); 

  TCCState *tcc = tcc_new();

  tcc_set_output_type(tcc, TCC_OUTPUT_MEMORY);
  tcc_compile_string(tcc, c_buffer);
  tcc_relocate(tcc, TCC_RELOCATE_AUTO);

  void (*fn)(void *ctx) = tcc_get_symbol(tcc, "jit_func");
  fn(NULL);

  tcc_delete(tcc);

  return 0; 
}


