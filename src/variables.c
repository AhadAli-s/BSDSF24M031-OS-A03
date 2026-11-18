#include "shell.h"
#include <ctype.h>

typedef struct var {
    char* name;
    char* value;
    struct var* next;
} var_t;

static var_t* var_head = NULL;

// A valid assignment looks like NAME=value where NAME starts with a letter
// or underscore and continues with letters, digits or underscores
int is_assignment(const char* tok) {
    if (!(isalpha((unsigned char)tok[0]) || tok[0] == '_')) {
        return 0;
    }
    int i = 1;
    while (isalnum((unsigned char)tok[i]) || tok[i] == '_') {
        i++;
    }
    return tok[i] == '=';
}

void var_set(const char* name, const char* value) {
    var_t* last = NULL;

    for (var_t* v = var_head; v != NULL; v = v->next) {
        if (strcmp(v->name, name) == 0) {
            free(v->value);
            v->value = strdup(value);
            return;
        }
        last = v;
    }

    var_t* nv = malloc(sizeof(var_t));
    nv->name = strdup(name);
    nv->value = strdup(value);
    nv->next = NULL;
    if (last == NULL) {
        var_head = nv;
    } else {
        last->next = nv;
    }
}

const char* var_get(const char* name) {
    for (var_t* v = var_head; v != NULL; v = v->next) {
        if (strcmp(v->name, name) == 0) {
            return v->value;
        }
    }
    return NULL;
}

void var_print_all(void) {
    for (var_t* v = var_head; v != NULL; v = v->next) {
        printf("%s=%s\n", v->name, v->value);
    }
}

void var_free_all(void) {
    var_t* v = var_head;
    while (v != NULL) {
        var_t* next = v->next;
        free(v->name);
        free(v->value);
        free(v);
        v = next;
    }
    var_head = NULL;
}
