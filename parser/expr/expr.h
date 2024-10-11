#ifndef EXPR_H
#define EXPR_H

#include "../../lexer.h"
#include "../parser.h"

parser_node *parse_expr(typed_token **tkns_ptr);
parser_node *parse_terminal(typed_token **tkns_ptr);

typedef struct
{
    int op;
    parser_node *left;
    parser_node *right;
} node_binary_op;

typedef struct
{
    parser_node *cond;
    parser_node *true_val;
    parser_node *false_val;
} node_cond;

#endif
