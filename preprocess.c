#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include "lexer.h"
#include "linked_list.h"
#include "include.h"
#include "preprocess.h"
#include "codegen/codegen.h"

typedef struct
{
    char *id;
    typed_token *replace;
} define;

typed_token *clone(typed_token *tkn)
{
    typed_token *ret = (typed_token *)malloc(sizeof(typed_token));
    memcpy((void *)ret, (void *)tkn, sizeof(typed_token));
    ret->next = NULL;
    if (tkn->data)
    {
        // Clone string data
        if (tkn->type_id == TKN_ID || tkn->type_id == TKN_LIT_STR)
        {
            ret->data = strdup((char *)tkn->data);
        }
        else
        {
            // For other types, just copy the data pointer
            ret->data = tkn->data;
        }
    }
    return ret;
}

define *find_def(linked_list *defs, char *id)
{
    list_node *curr = defs->first;
    while (curr)
    {
        define *def = (define *)curr->value;
        if (strcmp(def->id, id) == 0)
        {
            return def;
        }
        curr = curr->next;
    }
    return NULL;
}

int is_header_file(const char *filename)
{
    const char *ext = strrchr(filename, '.');
    return ext && strcmp(ext, ".h") == 0;
}

char *get_source_filename(const char *header_filename)
{
    size_t len = strlen(header_filename);
    char *source_filename = malloc(len + 1);
    strcpy(source_filename, header_filename);
    source_filename[len - 1] = 'c';
    return source_filename;
}

typed_token *preprocess_include(typed_token **tkns_ptr, include_stack **inc_stack)
{
    typed_token *tkn = *tkns_ptr;
    tkn = tkn->next; // Skip #include

    if (!tkn || (tkn->type_id != TKN_LIT_STR && tkn->type_id != TKN_LT))
    {
        fprintf(stderr, "Invalid #include syntax\n");
        return NULL;
    }

    char *filename = NULL;
    int is_system = 0;

    if (tkn->type_id == TKN_LIT_STR)
    {
        filename = strdup((char *)tkn->data);
        tkn = tkn->next;
    }
    else
    {
        // Handle system header files
        is_system = 1;
        tkn = tkn->next;

        char buffer[1024] = {0};
        int pos = 0;

        while (tkn && tkn->type_id != TKN_GT)
        {
            if (tkn->type_id == TKN_ID || tkn->type_id == TKN_DOT)
            {
                int len = strlen((char *)tkn->data);
                if (pos + len >= sizeof(buffer) - 1)
                {
                    fprintf(stderr, "Include filename too long\n");
                    return NULL;
                }
                if (tkn->type_id == TKN_DOT)
                {
                    buffer[pos] = '.';
                    pos++;
                }
                else
                {
                    strcat(buffer, (char *)tkn->data);
                    pos += len;
                }
            }
            tkn = tkn->next;
        }

        if (!tkn)
        {
            fprintf(stderr, "Unterminated system include\n");
            return NULL;
        }

        filename = strdup(buffer);
        tkn = tkn->next;
    }

    // Get current file from include stack
    const char *current_file = (*inc_stack) ? (*inc_stack)->filename : NULL;

    char *full_path = search_include_file(filename, is_system, current_file);
    if (!full_path)
    {
        fprintf(stderr, "Include file not found: %s\n", filename);
        free(filename);
        return NULL;
    }

    typed_token *result = NULL;

    if (is_header_file(filename))
    {
        include_stack *curr = *inc_stack;
        while (curr)
        {
            if (strcmp(curr->filename, full_path) == 0)
            {
                fprintf(stderr, "Circular include detected: %s\n", filename);
                free(filename);
                free(full_path);
                return NULL;
            }
            curr = curr->prev;
        }

        char *content = read_file(full_path);
        if (!content)
        {
            free(filename);
            free(full_path);
            return NULL;
        }

        push_include(inc_stack, full_path);
        typed_token *header_tokens = tokenize(content);
        result = preprocess(header_tokens, full_path);
        pop_include(inc_stack);
        free(content);

        // Look for corresponding .c file
        char *source_filename = get_source_filename(filename);
        char *source_path = search_include_file(source_filename, is_system, current_file);

        if (source_path)
        {
            char *source_content = read_file(source_path);
            if (source_content)
            {
                push_include(inc_stack, source_path);
                typed_token *source_tokens = tokenize(source_content);
                typed_token *processed_source = preprocess(source_tokens, source_path);
                pop_include(inc_stack);

                if (processed_source)
                {
                    typed_token *last = result;
                    while (last && last->next)
                        last = last->next;
                    if (last)
                        last->next = processed_source;
                    else
                        result = processed_source;
                }
                free(source_content);
            }
            free(source_path);
        }
        free(source_filename);
    }
    else
    {
        char *content = read_file(full_path);
        if (!content)
        {
            free(filename);
            free(full_path);
            return NULL;
        }

        push_include(inc_stack, full_path);
        typed_token *included_tokens = tokenize(content);
        result = preprocess(included_tokens, full_path);
        pop_include(inc_stack);
        free(content);
    }

    free(filename);
    free(full_path);

    *tkns_ptr = tkn;
    return result;
}

typed_token *preprocess(typed_token *tkns, const char *filename)
{
    linked_list defines = new_linked_list();
    include_stack *inc_stack = NULL;

    if (filename)
    {
        push_include(&inc_stack, filename);
    }

    typed_token *first = NULL;
    typed_token *last = NULL;

    typed_token *tkn = tkns;
    while (tkn)
    {
        if (tkn->type_id == TKN_MACRO_DEFINE)
        {
            tkn = tkn->next;
            if (tkn->type_id == TKN_ID)
            {
                char *name = cc_asprintf("%s", (char *)tkn->data);
                tkn = tkn->next;
                define *def = (define *)malloc(sizeof(define));
                def->id = name;
                def->replace = clone(tkn);
                add_to_list(&defines, (void *)def);
                tkn = tkn->next;
                continue;
            }
        }

        if (tkn->type_id == TKN_MACRO_INCLUDE)
        {
            typed_token *included = preprocess_include(&tkn, &inc_stack);
            if (included)
            {
                // Link included tokens into the stream
                if (!first)
                {
                    first = included;
                    last = included;
                    while (last->next)
                    {
                        last = last->next;
                    }
                }
                else
                {
                    last->next = included;
                    while (last->next)
                    {
                        last = last->next;
                    }
                }
                continue;
            }
        }

        if (tkn->type_id == TKN_ID)
        {
            define *def = find_def(&defines, (char *)tkn->data);
            if (def)
            {
                if (!first)
                {
                    first = clone(def->replace);
                    last = first;
                }
                else
                {
                    last->next = clone(def->replace);
                    last = last->next;
                }

                tkn = tkn->next;
                continue;
            }
        }

        if (!first)
        {
            first = clone(tkn);
            last = first;
        }
        else
        {
            last->next = clone(tkn);
            last = last->next;
        }

        tkn = tkn->next;
    }
    return first;
}