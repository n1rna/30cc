#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <libgen.h>

#include "include.h"

#define MAX_PATH 4096

void push_include(include_stack **stack, const char *filename)
{
    include_stack *new_include = malloc(sizeof(include_stack));
    new_include->filename = strdup(filename);
    new_include->prev = *stack;
    *stack = new_include;
}

void pop_include(include_stack **stack)
{
    if (*stack)
    {
        include_stack *tmp = *stack;
        *stack = (*stack)->prev;
        free(tmp->filename);
        free(tmp);
    }
}

char *read_file(const char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (!fp)
    {
        fprintf(stderr, "Error opening include file: %s\n", filename);
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *content = malloc(size + 1);
    fread(content, 1, size, fp);
    content[size] = '\0';

    fclose(fp);
    return content;
}

char *search_include_file(const char *filename, int is_system, const char *current_file)
{
    char path[MAX_PATH];
    char working_dir[MAX_PATH];

    // Get the directory of the current source file
    if (current_file)
    {
        strncpy(working_dir, current_file, MAX_PATH - 1);
        char *dir = dirname(working_dir); // Get directory part

        // Try current source file's directory
        snprintf(path, sizeof(path), "%s/%s", dir, filename);
        if (access(path, F_OK) == 0)
        {
            return strdup(path);
        }
    }

    // Try current working directory
    if (access(filename, F_OK) == 0)
    {
        return strdup(filename);
    }

    // For system includes or if not found in local directories
    if (is_system)
    { // Also check system paths for quoted includes as fallback
        const char *include_paths[] = {
            "/usr/include/",
            "/usr/local/include/",
            NULL};

        for (int i = 0; include_paths[i]; i++)
        {
            snprintf(path, sizeof(path), "%s%s", include_paths[i], filename);
            if (access(path, F_OK) == 0)
            {
                return strdup(path);
            }
        }
    }

    return NULL;
}