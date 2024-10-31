#ifndef INCLUDE_H
#define INCLUDE_H

typedef struct include_stack_
{
    char *filename;
    struct include_stack_ *prev;
} include_stack;

void push_include(include_stack **stack, const char *filename);
void pop_include(include_stack **stack);

char *read_file(const char *filename);
char *search_include_file(const char *filename, int is_system, const char *current_file);

#endif
