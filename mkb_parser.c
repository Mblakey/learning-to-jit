

#include "mkb_parser.h"


#define FL_BLK_SIZE 128

/* freelists */
mkb_ast_node *ast_node_fl = NULL; 
mkb_ast_edge *ast_edge_fl = NULL; 

/* hash symbol table */
strhash_entry *var_tbl[HT_SIZE]; 


const char* var_type_to_string(enum mkb_var_type t) {
  switch (t) {
    case STRING: return "STRING"; 
    case INTEGER: return "INTEGER"; 
    case FLOAT: return "FLOAT"; 
    case NONE: return "NONE"; 
  }
}


mkb_ast_node* mkb_ast_node_alloc(enum mkb_ast_node_kind type) 
{
  if (!ast_node_fl) {
    ast_node_fl = malloc(FL_BLK_SIZE * sizeof(mkb_ast_node)); 
    for (unsigned int i=0; i<FL_BLK_SIZE-1; i++) 
      ast_node_fl[i].fl_nxt = &ast_node_fl[i+1]; 
    ast_node_fl[FL_BLK_SIZE-1].fl_nxt = NULL;  
  }
  
  mkb_ast_node *node = ast_node_fl; 
  ast_node_fl = node->fl_nxt; 

  memset(node, 0, sizeof(mkb_ast_node)); 
  node->kind = type; 
  return node; 
}

const char* mkb_ast_node_type(mkb_ast_node *restrict node) {
  if (!node)
    return "NULL_PTR"; 

  switch (node->kind) {
    case UNK_KIND: return "UNK_KIND"; 
    case OP_KIND: return "OP_KIND"; 
    case DECL_KIND: return "DECL_KIND"; 
    case VAR_KIND: return "VAR_KIND"; 
    case EXPR_KIND: return "EXPR_KIND"; 
    case LOOP_KIND: return "LOOP_KIND"; 
    case FUNC_KIND: return "FUNC_KIND"; 
    case NUM_KIND: return "NUM_KIND"; 
    case STR_KIND: return "STR_KIND"; 
    case STMT_KIND: return "STMT_KIND"; 
    case ROOT_KIND: return "ROOT_KIND"; 
  }
}

mkb_ast_edge* mkb_ast_edge_alloc() 
{
  if (!ast_edge_fl) {
    ast_edge_fl = malloc(FL_BLK_SIZE * sizeof(mkb_ast_edge)); 
    for (unsigned int i=0; i<FL_BLK_SIZE-1; i++) 
      ast_edge_fl[i].nxt = &ast_edge_fl[i+1]; 
    ast_edge_fl[FL_BLK_SIZE-1].nxt = NULL;  
  }
  
  mkb_ast_edge *edge = ast_edge_fl; 
  ast_edge_fl = edge->nxt; 
  
  edge->nxt = NULL; 
  edge->dwn = NULL;
  return edge; 
}


mkb_ast_node* mkb_new_operator(unsigned char ch) 
{
  mkb_ast_node *op_node = mkb_ast_node_alloc(OP_KIND); 
  switch (ch) {
    case '+': op_node->op_code = ADD; return op_node; 
    case '-': op_node->op_code = SUB; return op_node;
    case '*': op_node->op_code = MUL; return op_node;
    case '/': op_node->op_code = DIV; return op_node;
    case '=': op_node->op_code = STO; return op_node;
  }
  return NULL; 
}


bool mkb_append_to_var(mkb_ast_node *restrict node, unsigned char ch) 
{
  if (node->var_len == sizeof(node->var_name))
    return false;
  
  node->var_name[node->var_len++] = ch; 
  return true; 
}


bool mkb_append_to_number(mkb_ast_node *restrict node, unsigned char ch) 
{
  node->num_value *= 10; 
  node->num_value += ch - '0'; 
  return true; 
}


bool mkb_append_to_string(mkb_ast_node *restrict node, unsigned char ch) 
{
  if (node->str_len == sizeof(node->str))
    return false;
  
  node->str[node->str_len++] = ch; 
  return true; 
}


mkb_ast_edge* mkb_add_child(mkb_ast_node *restrict parent, 
                            const mkb_ast_node *restrict child) 
{
  if (parent == child) {
    /* error codes */
    return NULL; 
  }
  
  mkb_ast_edge *prev = parent->children;
  if (!prev) {
    mkb_ast_edge *new_edge = mkb_ast_edge_alloc(); 
    new_edge->dwn = (mkb_ast_node*)child; 
    parent->children = new_edge; 
    return new_edge; 
  }

  while (prev->nxt) {
    prev = prev->nxt;
  }

  mkb_ast_edge *new_edge = mkb_ast_edge_alloc(); 
  new_edge->dwn = (mkb_ast_node*)child; 
  prev->nxt = new_edge; 

  return new_edge; 
}


mkb_ast_node* mkb_loop_stmt(mkb_ast_node *restrict n) {
  return n->children->dwn; 
}


mkb_ast_node* mkb_operator_lft(mkb_ast_node *restrict n) {
  return n->children->dwn; 
}


mkb_ast_node* mkb_operator_rgt(mkb_ast_node *restrict n) {
  return n->children->nxt->dwn; 
}


mkb_ast_node* mkb_parse_stmt(const unsigned char *buffer, 
                             size_t buflen, 
                             unsigned int *pos, 
                             int stmt_depth); 


mkb_ast_node* mkb_recursive_decent(const unsigned char *buffer, 
                                   size_t buflen, 
                                   unsigned int *pos)
{
  mkb_ast_node *lhs = NULL; 
  mkb_ast_node *rhs = NULL; 
  mkb_ast_node *op  = NULL; 
  mkb_ast_node *stmt  = NULL; 
  unsigned int i = *pos; 

/* opening jump state */
lexer_state_unknown:
  for (; i < buflen; i++) {
    unsigned char ch = buffer[i]; 
    if (ch == ' ' || ch == '\t' || ch == '\n')
      continue; 

    if (isalpha(ch))
      goto lexer_state_variable; 
    else if (isdigit(ch))
      goto lexer_state_number; 
    else if (ch == '\"') {
      i++; 
      goto lexer_state_string;
    }
    else {
      fprintf(stderr, "Error: invalid expression syntax - %c\n", ch);
      return NULL; 
    }
  }

  fprintf(stderr, "Error: invalid expression syntax - EOF\n");
  return NULL; 

/* expects the ops here, LHS must be set to come here */
lexer_state_close_op:
  for (; i < buflen; i++) {
    unsigned char ch = buffer[i]; 
    switch (ch) {
      case '+':
      case '-':
      case '/':
      case '*':
      case '=':
        op = mkb_new_operator(ch); 
        
        ++i; 
        rhs = mkb_recursive_decent(buffer, buflen, &i);
        if (!rhs)
          return NULL; 
        mkb_add_child(op, rhs); 
        mkb_add_child(op, lhs); 
        lhs = op; 
      
      /* fall through switch */
      case ';':
        goto lexer_state_return; 

      /* ignore white space */
      case ' ':
      case '\t':
      case '\n':
        break;

      default:
        fprintf(stderr, "Error: expected operator or closure - %c\n", ch); 
        return NULL;
    }
  }

  fprintf(stderr, "Error: expected operator or closure\n"); 
  return NULL; 

lexer_state_variable:
  lhs = mkb_ast_node_alloc(UNK_KIND); 
  for (; i < buflen; i++) {
    unsigned char ch = buffer[i]; 
    if (!isalpha(ch)) {
      if (lhs->var_len == 3 && 
          memcmp(lhs->var_name, "meg", 3)==0 &&
          ch == '(') 
      {
        lhs->kind  = LOOP_KIND; 
        lhs->range = 0; 
        goto lexer_state_loop;
      }

      if (lhs->var_len == 7 && 
          memcmp(lhs->var_name, "rachael", 7)==0 &&
          ch == '(') 
      {
        lhs->var_len = 0; 
        memset(lhs->var_name, 0, sizeof(lhs->var_name)); 
        lhs->kind  = PRINT_KIND; 
        goto lexer_state_print;
      }
      
      if (!strhash_table_store(var_tbl, lhs, lhs->var_name, lhs->var_len)) 
        lhs->kind = VAR_KIND;
      else 
        lhs->kind = DECL_KIND;

      fprintf(stderr, "[mkb] created variable %s\n", lhs->var_name); 
      goto lexer_state_close_op; 
    }
    else if (!mkb_append_to_var(lhs, ch)) {
      fprintf(stderr, "Error: invalid syntax for variable\n"); 
      return NULL; 
    }
  }

  fprintf(stderr, "Error: invalid syntax for variable\n"); 
  return NULL; 

lexer_state_number:
  lhs = mkb_ast_node_alloc(NUM_KIND); 
  for (; i < buflen; i++) {
    unsigned char ch = buffer[i]; 
    if (!isdigit(ch)) {
      fprintf(stderr, "[mkb] created number %llu\n", lhs->num_value);
      goto lexer_state_close_op; 
    }
    else if (!mkb_append_to_number(lhs, ch)) {
      fprintf(stderr, "Error: invalid syntax for number\n"); 
      return NULL; 
    }
  }

  fprintf(stderr, "Error: invalid syntax for number\n"); 
  return NULL;

lexer_state_string:
  lhs = mkb_ast_node_alloc(STR_KIND); 
  for (; i < buflen; i++) {
    unsigned char ch = buffer[i]; 
    if (ch == '\"') {
      fprintf(stderr, "[mkb] created string %s\n", lhs->str);
      i++;
      goto lexer_state_close_op; 
    }
    else if (!mkb_append_to_string(lhs, ch)) {
      fprintf(stderr, "Error: invalid syntax for string\n"); 
      return NULL; 
    }
  }

  fprintf(stderr, "Error: invalid syntax for string\n"); 
  return NULL;

lexer_state_print:
  if (i == buflen || buffer[i++] != '(') {
    fprintf(stderr, "Error: invalid syntax for print\n"); 
    return NULL;
  }

  for (; i < buflen; i++) {
    unsigned char ch = buffer[i]; 
    if (!isalpha(ch)) {
      if (ch != ')') {
        fprintf(stderr, "Error: invalid syntax for print\n"); 
        return NULL;
      }

      if (!strhash_table_load(var_tbl, lhs->var_name, lhs->var_len)) {
        fprintf(stderr, "Error: undefined variable for print - %s\n", lhs->var_name); 
        return NULL; 
      }
      i++; 

      goto lexer_state_close_op; 
    }
    else if (!mkb_append_to_var(lhs, ch)) {
      fprintf(stderr, "Error: invalid syntax for print variable\n"); 
      return NULL; 
    }
  }

  fprintf(stderr, "Error: invalid syntax for print\n"); 
  return NULL; 

lexer_state_loop:
  if (i == buflen || buffer[i++] != '(') {
    fprintf(stderr, "Error: invalid syntax for loop\n"); 
    return NULL;
  }

  for (; i < buflen; i++) {
    unsigned char ch = buffer[i]; 
    if (!isdigit(ch)) {
      if (ch != ')') {
        fprintf(stderr, "Error: invalid syntax for loop\n"); 
        return NULL;
      }
      break;
    }
    lhs->range *= 10;
    lhs->range += ch - '0'; 
  }
  i++;

  for (; i < buflen; i++) {
    unsigned char ch = buffer[i]; 
    if (ch == ' ' || ch == '\t' || ch == '\n')
      continue; 
    else if (ch != '{') {
      fprintf(stderr, "Error: invalid syntax for loop\n"); 
      return NULL;
    }
    else {
      i++;
      break;
    }
  }

  stmt = mkb_parse_stmt(buffer, buflen, &i, 1);
  if (!stmt) {
    fprintf(stderr, "Error: invalid syntax for loop\n"); 
    return NULL;
  }
  mkb_add_child(lhs, stmt); 


lexer_state_return:
  if (!lhs) {
    fprintf(stderr, "Error: returning empty statement\n");
    return NULL; 
  } 
  
  *pos = i; 
  return lhs; 
}


// x = 2 + 2 + 2; 
// ADD -> 2 -> ADD -> 2 -> 2

mkb_ast_node* mkb_parse_stmt(const unsigned char *buffer, 
                             size_t buflen, 
                             unsigned int *pos, 
                             int stmt_depth) 
{
  mkb_ast_node *stmt = mkb_ast_node_alloc(STMT_KIND); 

  for (unsigned int i = *pos; i < buflen; i++) {
    unsigned char ch = buffer[i]; 
    if (ch == ' ' || ch == '\t' || ch == '\n')
      continue; 

    if (ch == '}') {
      if (stmt_depth == 0) {
        fprintf(stderr, "Error: invalid statment closure\n");
        return NULL; 
      }
      *pos = ++i; 
      return stmt; 
    }
    
    mkb_ast_node *expr = mkb_recursive_decent(buffer, buflen, &i); 
    if (!expr) 
      return NULL; 

    mkb_add_child(stmt, expr); 
    
    if (buffer[i] != ';') {
      fprintf(stderr, "Error: invalid expression end\n");
      return NULL; 
    }

    fprintf(stderr, "[mkb] created expression\n");  

  }
  return stmt; 
}


mkb_ast_node* mkb_parse(const unsigned char *buffer, size_t buflen) 
{
  mkb_ast_node *root = mkb_ast_node_alloc(ROOT_KIND); 
  unsigned int pos = 0; 
  mkb_ast_node *stmt = mkb_parse_stmt(buffer, buflen, &pos, 0); 
  if (!stmt)
    return NULL;
  mkb_add_child(root, stmt); 
  return root;
}



enum mkb_var_type mkb_assign_types(struct mkb_ast_node *n)
{
  mkb_ast_node *lhs;
  mkb_ast_node *rhs;

  enum mkb_var_type ltype;
  enum mkb_var_type rtype;
  
  switch (n->kind) {
    case ROOT_KIND: 
    case STMT_KIND: 
      for (mkb_ast_edge *e=n->children; e; e = e->nxt)
        mkb_assign_types(e->dwn); 
      break; 

    case EXPR_KIND: 
    case LOOP_KIND:
        mkb_assign_types(n->children->dwn); 
        break; 
    
    case OP_KIND:
        lhs = n->children->dwn;
        rhs = n->children->nxt->dwn;

        switch (n->op_code) {
          case ADD: 
          case SUB: 
          case MUL: 
          case DIV: 
            ltype = mkb_assign_types(lhs);
            rtype = mkb_assign_types(rhs);

            if (ltype != rtype) {
              return ltype > rtype ? ltype : rtype; 
            }
            return ltype; 

          case STO: 
            rhs->var_type = mkb_assign_types(lhs);
            break;
        }
        break;

    case NUM_KIND: return INTEGER; 
    case STR_KIND: return STRING; 
  }

  return NONE;
}


