#define _GNU_SOURCE
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/param.h>
#include <unistd.h>
#include <jansson.h>

// MIT licensed, (c) 2011 Kyle Keen <keenerd@gmail.com>

/*
    stdin is always json
    stdout is always json (except for -u, -t, -l, -k)

    -P -> detect and ignore JSONP wrapper, if present
    -S -> sort keys when writing objects

    -t(ype) -> str, object, list, number, bool, null
    -l(ength) -> only works on str, dict, list
    -k(eys) -> only works on dict
    -e(xtract) index -> only works on dict, list
    -s(tring) value -> adds json escapes
    -u(nstring) -> removes json escapes
    -m(modify) index,value -> only works on dict, list
                              index can be append, value can be remove
                              no commas in index
    -i(nsert) index -> opposite of extract, rebuilds json
                       "-e field -i field" returns original structure

    Multiple commands can be chained.
    Entire json is loaded into memory.
    -e copies and stores on a stack.
    Could use up a lot of memory.
    No safety measures anywhere.

*/

// build with gcc -o jshon jshon.c -ljansson


// deal with API incompatibility between jansson 1.x and 2.x
#ifndef JANSSON_MAJOR_VERSION
#  define JANSSON_MAJOR_VERSION (1)
#endif

#if JANSSON_MAJOR_VERSION < 2
#    define compat_json_loads json_loads
#else
static json_t *compat_json_loads(const char *input, json_error_t *error)
{
    return json_loads(input, 0, error);
}
#endif

// stack depth is limited by maxargs
// if you need more depth, use a SAX parser
#define STACKDEPTH 128

json_t* stack[STACKDEPTH];
json_t** stackpointer = &stack[0];

void PUSH(json_t* v)
{
    if (stackpointer >= &stack[STACKDEPTH]) {
        fprintf(stderr, "internal error: stack overflow\n");
        exit(1);
    }
    *stackpointer++ = v;
}

json_t** stack_safe_peek() {
    if (stackpointer < &stack[1]) {
        fprintf(stderr, "internal error: stack underflow\n");
        exit(1);
    }
    return stackpointer - 1;
}

// can not use two macros on the same line
#define POP        *((stackpointer = stack_safe_peek()))
#define PEEK       *(stack_safe_peek())

char* read_stdin(void)
// http://stackoverflow.com/questions/2496668/
{
    char buffer[BUFSIZ];
    size_t contentSize = 1; // includes NULL
    char* content = malloc(sizeof(char) * BUFSIZ);
    if (isatty(fileno(stdin)))
        {return "";}
    if(content == NULL)
        {return "";}
    content[0] = '\0';
    while(fgets(buffer, BUFSIZ, stdin))
    {
        char* old = content;
        contentSize += strlen(buffer);
        content = realloc(content, contentSize);
        if(content == NULL)
        {
            free(old);
            return "";
        }
        strcat(content, buffer);
    }

    if(ferror(stdin))
    {
        free(content);
        return "";
    }
    return content;
}

char* remove_jsonp_callback(char* in, int* rows_skipped, int* cols_skipped)
// this 'removes' jsonp callback code which can surround json, by returning
// a pointer to first byte of real JSON, and overwriting the jsonp stuff at
// the end of the input with a null byte. it also writes out the number of
// lines, and then columns, which were skipped over.
//
// if a legitimate jsonp callback surround is not detected, the original
// input is returned and no other action is taken. this means that JSONP
// syntax errors will be effectively ignored, and will then fail json parsing
//
// this doesn't detect all conceivable JSONP wrappings. a simple function call
// with a reasonable ASCII identifier will work, and that covers 99% of the
// real world
{
    #define JSON_WHITE(x) ((x) == 0x20 || (x) == 0x9 || (x) == 0xA || (x) == 0xD)
    #define JSON_IDENTIFIER(x) (isalnum(x) || (x) == '$' || (x) == '_' || (x) == '.')

    char *first = in;
    char *last = in + strlen(in) - 1;

    // skip over whitespace and semicolons at the end
    while (first < last && (JSON_WHITE(*last) || *last == ';'))
        --last;

    // count closing brackets at the end, still skipping whitespace
    int brackets = 0;
    while (first < last && (JSON_WHITE(*last) || *last == ')'))
    {
        if (*last == ')')
            ++brackets;
        --last;
    }

    // no closing brackets? it's not jsonp
    if (brackets == 0)
        return in;

    // skip leading whitespace
    while (first < last && JSON_WHITE(*first))
        ++first;

    // skip leading identifier if present
    while (first < last && JSON_IDENTIFIER(*first))
        ++first;

    // skip over forward brackets and whitespace, counting down the opening brackets
    // against the closing brackets we've already done
    while (first < last && (JSON_WHITE(*first) || *first == '('))
    {
        if (*first == '(')
            --brackets;
        ++first;
    }

    // at this point we have a valid jsonp wrapper, provided that the number of opening
    // and closing brackets matched, and provided the two pointers didn't meet in
    // the middle (leaving no room for any actual JSON)
    if (brackets != 0 || !(first < last))
        return in;

    // count lines and columns skipped over
    *rows_skipped = *cols_skipped = 0;
    while (in < first) {
        ++*cols_skipped;
        if (*in++ == '\n')
        {
            *cols_skipped = 0;
            ++*rows_skipped;
        }
    }

    // strip off beginning and end
    *(last+1) = '\0';
    return first;
}


int dumps_flags = JSON_INDENT(1);

char* smart_dumps(json_t* json)
// json_dumps is broken on simple types
{
    char* temp;
    char* temp2;
    json_t* j2;
    switch (json_typeof(json))
    {
        case JSON_OBJECT:
            return json_dumps(json, dumps_flags);
        case JSON_ARRAY:
            return json_dumps(json, dumps_flags);
        case JSON_STRING:
            // hack to print escaped string
            // but / is still not escaped
            j2 = json_array();
            json_array_append(j2, json);
            temp = json_dumps(j2, 0);
            asprintf(&temp2, "%.*s", (signed)strlen(temp)-2, &temp[1]);
            return temp2;
        case JSON_INTEGER:
            asprintf(&temp, "%ld", (long)json_integer_value(json));
            return temp;
        case JSON_REAL:
            asprintf(&temp, "%f", json_real_value(json));
            return temp;
        case JSON_TRUE:
            return "true";
        case JSON_FALSE:
            return "false";
        case JSON_NULL:
            return "null";
        default:
            exit(1);
    }
}

json_t* smart_loads(char* j_string)
// json_loads is broken on simple types
{
    json_t* json;
    json_error_t error;
    char *temp;
    asprintf(&temp, "[%s]", j_string);
    json = compat_json_loads(temp, &error);
    if (!json)
        {return json_string(j_string);}
    return json_array_get(json, 0);
}

char* pretty_type(json_t* json)
{
    switch (json_typeof(json))
    {
        case JSON_OBJECT:
            return "object";
        case JSON_ARRAY:
            return "array";
        case JSON_STRING:
            return "string";
        case JSON_INTEGER:
        case JSON_REAL:
            return "number";
        case JSON_TRUE:
        case JSON_FALSE:
            return "bool";
        case JSON_NULL:
            return "null";
        default:
            exit(1);
    }
}

int length(json_t* json)
{
    switch (json_typeof(json))
    {
        case JSON_OBJECT:
            return json_object_size(json);
        case JSON_ARRAY:
            return json_array_size(json);
        case JSON_STRING:
            return strlen(json_string_value(json));
        case JSON_INTEGER:
        case JSON_REAL:
        case JSON_TRUE:
        case JSON_FALSE:
        case JSON_NULL:
        default:
            exit(1);
    }
}

int compare_strcmp(const void *a, const void *b)
{
    const char *sa = ((const char**)a)[0];
    const char *sb = ((const char**)b)[0];
    return strcmp(sa, sb);
}

void keys(json_t* json)
// shoddy, prints directly
{
    void* iter;
    const char** keys;
    size_t i, n;

    if (!json_is_object(json))
        {exit(1);}
    if (!((keys = malloc(sizeof(char*) * json_object_size(json)))))
        {fprintf(stderr, "ERROR: out of memory\n"); exit(1);}

    iter = json_object_iter(json);
    n = 0;
    while (iter)
    {
        keys[n++] = json_object_iter_key(iter);
        iter = json_object_iter_next(json, iter);
    }

    if (dumps_flags & JSON_SORT_KEYS)
        qsort(keys, n, sizeof(char*), compare_strcmp);

    for (i = 0; i < n; ++i)
        printf("%s\n", keys[i]);

    free(keys);
}

const char* unstring(json_t* json)
{
    if (!json_is_string(json))
        {exit(1);}
    return json_string_value(json);
}

json_t* extract(json_t* json, char* key)
{
    int i;
    switch (json_typeof(json))
    {
        case JSON_OBJECT:
            return json_object_get(json, key);
        case JSON_ARRAY:
            i = atoi(key);
            while (i < 0)
                {i += json_array_size(json);}
            while ((unsigned)i >= json_array_size(json))
                {i -= json_array_size(json);}
            return json_array_get(json, i);
        case JSON_STRING:
        case JSON_INTEGER:
        case JSON_REAL:
        case JSON_TRUE:
        case JSON_FALSE:
        case JSON_NULL:
        default:
            exit(1);
    }
}

json_t* delete(json_t* json, char* key)
// no error checking
{
    int i;
    switch (json_typeof(json))
    {
        case JSON_OBJECT:
            json_object_del(json, key);
            return json;
        case JSON_ARRAY:
            i = atoi(key);
            while (i < 0)
                {i += json_array_size(json);}
            while ((unsigned)i >= json_array_size(json))
                {i -= json_array_size(json);}
            json_array_remove(json, i);
            return json;
        case JSON_STRING:
        case JSON_INTEGER:
        case JSON_REAL:
        case JSON_TRUE:
        case JSON_FALSE:
        case JSON_NULL:
        default:
            exit(1);
    }
}

json_t* update(json_t* json, char* key, char* j_string)
// no error checking
{
    int i;
    switch (json_typeof(json))
    {
        case JSON_OBJECT:
            json_object_set(json, key, smart_loads(j_string));
            return json;
        case JSON_ARRAY:
            if (!strcmp(key, "append"))
            {
                json_array_append(json, smart_loads(j_string));
                return json;
            }
            // otherwise, insert
            i = atoi(key);
            while (i < 0)
                {i += json_array_size(json);}
            while ((unsigned)i >= json_array_size(json))
                {i -= json_array_size(json);}
            json_array_insert(json, i, smart_loads(j_string));
            return json;
        case JSON_STRING:
        case JSON_INTEGER:
        case JSON_REAL:
        case JSON_TRUE:
        case JSON_FALSE:
        case JSON_NULL:
        default:
            exit(1);
    }
}

int main (int argc, char *argv[])
{
    char* content = "";
    char* arg1 = "";
    char* arg2 = "";
    char* j_string = "";
    json_t* json;
    int output = 1;  // flag if json should be printed
    int optchar;
    int jsonp = 0;   // flag if we should tolerate JSONP wrapping
    int jsonp_rows = 0, jsonp_cols = 0;   // rows+cols skipped over by JSONP prologue

    // non-manipulation options
#define ALL_OPTIONS "PStlkue:s:m:i:"
    while ((optchar = getopt(argc, argv, ALL_OPTIONS)) != -1)
    {
        switch (optchar)
        {
            case 'P':
                jsonp = 1;
                break;
            case 'S':
                dumps_flags |= JSON_SORT_KEYS;
                break;
            default:
                break;
        }
    }
    optind = 1;
#ifdef BSD
    optreset = 1;
#endif


    content = read_stdin();
    if (!content[0]) {
        fprintf(stderr, "ERROR: json read error: nothing to read on stdin\n");
        exit(1);
    }

    if (jsonp)
        content = remove_jsonp_callback(content, &jsonp_rows, &jsonp_cols);

    json_error_t error;
    PUSH(compat_json_loads(content, &error));
    if (!PEEK) {
        const char *jsonp_status = "";
        if (jsonp)
            jsonp_status = (jsonp_rows||jsonp_cols) ? "(jsonp detected) " : "(jsonp not detected) ";

#if JANSSON_MAJOR_VERSION < 2
        fprintf(stderr, "ERROR: json %sread error, line %0d: %s\n",
            jsonp_status, error.line + jsonp_rows, error.text);
#else
        fprintf(stderr, "ERROR: json %sread error, line %0d column %0d: %s\n",
            jsonp_status, error.line + jsonp_rows, error.column + jsonp_cols, error.text);
#endif
        exit(1);
    }


    while ((optchar = getopt(argc, argv, ALL_OPTIONS)) != -1)
    {
        switch (optchar)
        {
            case 't':  // id type
                printf("%s\n", pretty_type(PEEK));
                output = 0;
                break;
            case 'l':  // length
                printf("%i\n", length(PEEK));
                output = 0;
                break;
            case 'k':  // keys
                keys(PEEK);
                output = 0;
                break;
            case 'u':  // unescape string
                printf("%s\n", unstring(PEEK));
                output = 0;
                break;
            case 's':  // escape string
                arg1 = (char*) strdup(optarg);
                PUSH(json_string(arg1));
                output = 1;
                break;
            case 'm':  // modify
                arg2 = (char*) strdup(optarg);
                arg1 = strsep(&arg2, ",");
                output = 1;
                json = POP;
                if (!strcmp(arg2, "remove"))
                {
                    PUSH(delete(json, arg1));
                    break;
                }
                PUSH(update(json, arg1, arg2));
                break;
            case 'e':  // extract
                arg1 = (char*) strdup(optarg);
                json = PEEK;
                PUSH(extract(json_deep_copy(json), arg1));
                output = 1;
                break;
            case 'i':  // insert
                // pointless string conversion
                // saves writing update_native()
                arg1 = (char*) strdup(optarg);
                j_string = smart_dumps(POP);
                json = POP;
                PUSH(update(json, arg1, j_string));
                output = 1;
                break;
            case 'P':  // not a manipulation
            case 'S': 
                break;
            default:
                printf("Unknown command line option...\n");
                printf("Valid: -P -S -t -l -k -u -e -s -m -i\n");
                exit(0);
                break;
        }
    }
    if (output && stackpointer != stack)
        {printf("%s\n", smart_dumps(POP));}
}

