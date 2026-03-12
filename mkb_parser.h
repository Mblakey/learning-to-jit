
#ifndef MKB_PARSER_H
#define MKB_PARSER_H

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#include "strhash.h"

enum mkb_ast_node_kind {
  UNK_KIND,
  OP_KIND,
  DECL_KIND,
  VAR_KIND,
  EXPR_KIND,
  LOOP_KIND,
  FUNC_KIND,
  PRINT_KIND,
  NUM_KIND,
  STR_KIND,
  STMT_KIND,
  ROOT_KIND,
}; 


enum mkb_var_type {
  NONE,
  INTEGER,
  FLOAT,
  STRING,
}; 


enum mkb_op_type {
  ADD,
  SUB,
  MUL,
  DIV,
  STO,
}; 


typedef struct mkb_ast_edge {
  struct mkb_ast_node *dwn; 
  struct mkb_ast_edge *nxt; 
} mkb_ast_edge; 


/* tagged union AST node */
typedef struct mkb_ast_node {
  enum mkb_ast_node_kind kind; 
  mkb_ast_edge *children;  
  union {
    struct { /* operator */
      enum mkb_op_type op_code; 
    };
    struct { /* variable */
      unsigned char var_name[8]; 
      enum mkb_var_type var_type;
      unsigned char var_len;
    }; 
    struct { /* number */
      uint64_t num_value;
    }; 
    struct { /* string */
      unsigned char str[8];
      unsigned char str_len;
    }; 
    struct { /* loop */
      unsigned int range;
      unsigned int incr;
    };
    struct { /* freelist return */
      struct mkb_ast_node *fl_nxt;
    };
  }; 
} mkb_ast_node; 


mkb_ast_node* mkb_loop_stmt(mkb_ast_node *restrict);
mkb_ast_node* mkb_operator_lft(mkb_ast_node *restrict); 
mkb_ast_node* mkb_operator_rgt(mkb_ast_node *restrict); 



const char* mkb_ast_node_type(mkb_ast_node *restrict);

/* C file contains globals for internal state */
mkb_ast_node* mkb_parse(const unsigned char*, size_t); 
enum mkb_var_type mkb_assign_types(struct mkb_ast_node*); 

#endif
