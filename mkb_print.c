

#include "mkb_tests.h"
#include "mkb_parser.h"


static void print_node(mkb_ast_node *n) {
    switch (n->kind) {
        case VAR_KIND: 
          printf("VAR(%s)", n->var_name); 
          break; 

        case DECL_KIND: 
          printf("DECL(%s)", n->var_name); 
          break; 

        case PRINT_KIND: 
          printf("PRINT(%s)", n->var_name); 
          break; 

        case NUM_KIND: 
          printf("NUM(%llu)", n->num_value); 
          break; 

        case LOOP_KIND: 
          printf("LOOP(0..%d)", n->range); 
          break; 

        case OP_KIND:    
          switch (n->op_code) {
            case ADD: printf("ADD"); break; 
            case SUB: printf("SUB"); break; 
            case MUL: printf("MUL"); break; 
            case DIV: printf("DIV"); break; 
            case STO: printf("STO"); break; 
          }
          break; 
    
      case EXPR_KIND: printf("EXPR"); break; 
      case STMT_KIND: printf("STMT"); break; 
      case ROOT_KIND: printf("AST"); break; 
    }
}


static void print_ast(mkb_ast_node *n, const char *prefix, bool is_left) {
    if (!n) 
      return;

    printf("%s", prefix);
    printf("%s", is_left ? "├── " : "└── ");

    print_node(n);
    printf("\n"); 

    // build the prefix for children
    char new_prefix[256];
    snprintf(new_prefix, sizeof(new_prefix), "%s%s",
             prefix,
             is_left ? "│   " : "    ");  // indent under branch or space
    
    if (n->kind == STMT_KIND)
      for (mkb_ast_edge *e = n->children; e; e = e->nxt) 
        print_ast(e->dwn, new_prefix, e->nxt != NULL);
    else if (n->kind == OP_KIND) {
      print_ast(mkb_operator_lft(n), new_prefix, true);
      print_ast(mkb_operator_rgt(n), new_prefix, false);
    }
    else if (n->kind == LOOP_KIND) {
      print_ast(mkb_loop_stmt(n), new_prefix, false);
    }
}


void dump_ast(mkb_ast_node *root) {
  print_node(root);
  printf("\n"); 

  char prefix[256] = "";

  for (mkb_ast_edge *e = root->children; e; e = e->nxt) 
    print_ast(e->dwn, prefix, e->nxt != NULL);
}


int main() {
  mkb_ast_node *ast_root = mkb_parse((unsigned char*)test_case, strlen(test_case)); 
  if (!ast_root) 
    return 1; 
  
  dump_ast(ast_root); 
  return 0; 
}
