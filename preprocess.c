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
        printf("-----------filename nosys: %s\n", filename);
        tkn = tkn->next;
    }
    else
    {
        // Handle <header.h>
        is_system = 1;
        tkn = tkn->next;

        // Build the filename from tokens until we hit >
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

    // Search for include file with current file context
    char *full_path = search_include_file(filename, is_system, current_file);

    if (!full_path)
    {
        fprintf(stderr, "Include file not found: %s\n", filename);
        free(filename);
        return NULL;
    }

    // Check for circular includes
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

    // Read and process included file
    char *content = read_file(full_path);
    if (!content)
    {
        free(filename);
        free(full_path);
        return NULL;
    }

    push_include(inc_stack, full_path);
    typed_token *included_tokens = tokenize(content);
    typed_token *processed = preprocess(included_tokens, full_path);
    pop_include(inc_stack);

    free(content);
    free(filename);
    free(full_path);

    *tkns_ptr = tkn;
    return processed;
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

                // Add tkn
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

        // Add tkn
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
