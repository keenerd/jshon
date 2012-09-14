#define _GNU_SOURCE
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/param.h>
#include <unistd.h>
#include <jansson.h>
#include <errno.h>

// MIT licensed, (c) 2011 Kyle Keen <keenerd@gmail.com>

/*
    build with gcc -o jshon jshon.c -ljansson

    stdin is always json
    stdout is always json (except for -u, -t, -l, -k)

    -P -> detect and ignore JSONP wrapper, if present
    -S -> sort keys when writing objects
    -Q -> quiet, suppress stderr
    -V -> enable slower/safer pass-by-value
    -C -> continue through errors
    -F path -> read from file instead of stdin
    -I -> change file in place, requires -F

    -t(ype) -> str, object, list, number, bool, null
    -l(ength) -> only works on str, dict, list
    -k(eys) -> only works on dict
    -e(xtract) index -> only works on dict, list
    -s(tring) value -> adds json escapes
    -n(onstring) value -> creates true/false/null/array/object/int/float
    -u(nstring) -> removes json escapes, display value
    -p(op) -> pop/undo the last manipulation
    -d(elete) index -> remove an element from an object or array
    -i(nsert) index -> opposite of extract, merges json up the stack
                       objects will overwrite, arrays will insert
                       arrays can take negative numbers or 'append'
    -a(cross) -> iterate across the current dict or list

    --version -> returns an arbitrary number, exits

    Multiple commands can be chained.
    Entire json is loaded into memory.
    -e/-a copies and stores on a stack with -V.
    Could use up a lot of memory, usually does not.
    (For now we don't have to worry about circular refs,
    but adding 'swap' breaks that proof.)

    Consider a golf mode with shortcuts for -e -a -u -p -l
    -g 'results.*.Name.!.^.Version.!.^.Description.!'
    -g 'data.children.*.data.url.!'
    -g 'c.d.!.^.e.!'
    (! on object/array does -l)
    If you have keys with .!^* in them, use the normal options.
    Implementing this is going to be a pain.
    Maybe overwrite the original argv data?
    Maybe two nested parse loops?

    -L(abel)
    add jsonpipe/style/prefix/labels\t to pretty-printed json

    color?
    loadf for stdin?
*/

#define JSHONVER 20120914

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

#if (defined (__SVR4) && defined (__sun))
#include <stdarg.h>

int asprintf(char **ret, const char *format, ...)
{
    va_list ap;
    fprintf(stderr, "%s\n", "in the asprintf");

    *ret = NULL;  /* Ensure value can be passed to free() */

    va_start(ap, format);
    int count = vsnprintf(NULL, 0, format, ap);
    va_end(ap);

    if (count >= 0)
    {
        char* buffer = malloc(count + 1);
        if (buffer == NULL)
            {return -1;}

        va_start(ap, format);
        count = vsnprintf(buffer, count + 1, format, ap);
        va_end(ap);

        if (count < 0)
        {
            free(buffer);
            return count;
        }
        *ret = buffer;
    }

    return count;
}
#endif

int dumps_flags = JSON_INDENT(1);
int by_value = 0;
int in_place = 0;
char* file_path = "";

// for error reporting
int quiet = 0;
int crash = 1;
char** g_argv;

// stack depth is limited by maxargs
// if you need more depth, use a SAX parser
#define STACKDEPTH 128

json_t* stack[STACKDEPTH];
json_t** stackpointer = stack;

void err(char* message)
// also see arg_err() and json_err() below
{
    if (!quiet)
        {fprintf(stderr, "%s\n", message);}
    if (crash)
        {exit(1);}
}

void hard_err(char* message)
{
    err(message);
    exit(1);
}

void arg_err(char* message)
{
    char* temp;
    int i;
    i = asprintf(&temp, message, optind-1, g_argv[optind-1]);
    if (i == -1)
        {hard_err("internal error: out of memory");}
    err(temp);
}

void PUSH(json_t* json)
{
    if (stackpointer >= &stack[STACKDEPTH])
        {hard_err("internal error: stack overflow");}
    if (json == NULL)
    {
        arg_err("parse error: bad json on arg %i, \"%s\"");
        json = json_null();
    }
    *stackpointer++ = json;
}

json_t** stack_safe_peek()
{
    if (stackpointer < &stack[1])
    {
        err("internal error: stack underflow");
        PUSH(json_null());
    }
    return stackpointer - 1;
}

// can not use two macros on the same line
#define POP        *((stackpointer = stack_safe_peek()))
#define PEEK       *(stack_safe_peek())

json_t* maybe_deep(json_t* json)
{
    if (by_value)
        {return json_deep_copy(json);}
    return json;
}

typedef struct
{
    void*    itr;  // object iterator
    json_t** stk;  // stack reentry
    uint     lin;  // array iterator
    int      opt;  // optind reentry
    int      fin;  // finished iteration
} mapping;

mapping mapstack[STACKDEPTH];
mapping* mapstackpointer = mapstack;

mapping* map_safe_peek()
{
    if (mapstackpointer < &mapstack[1])
        {hard_err("internal error: mapstack underflow");}
    return mapstackpointer - 1;
}

void MAPPUSH()
{
    if (mapstackpointer >= &mapstack[STACKDEPTH])
        {hard_err("internal error: mapstack overflow");}
    mapstackpointer++;
    map_safe_peek()->stk = stack_safe_peek();
    map_safe_peek()->opt = optind;
    map_safe_peek()->fin = 0;
    switch (json_typeof(PEEK))
    {
        case JSON_OBJECT:
            map_safe_peek()->itr = json_object_iter(PEEK);
            break;
        case JSON_ARRAY:
            map_safe_peek()->lin = 0;
            break;
        default:
            err("parse error: type not mappable");
    }
}

void MAPNEXT()
{
    stackpointer = map_safe_peek()->stk + 1;
    optind = map_safe_peek()->opt;
    switch (json_typeof(*(map_safe_peek()->stk)))
    {
        case JSON_OBJECT:
            json_object_iter_key(map_safe_peek()->itr);
            PUSH(maybe_deep(json_object_iter_value(map_safe_peek()->itr)));
            map_safe_peek()->itr = json_object_iter_next(*(map_safe_peek()->stk), map_safe_peek()->itr);
            if (!map_safe_peek()->itr)
                {map_safe_peek()->fin = 1;}
            break;
        case JSON_ARRAY:
            PUSH(maybe_deep(json_array_get(*(map_safe_peek()->stk), map_safe_peek()->lin)));
            map_safe_peek()->lin++;
            if (map_safe_peek()->lin >= json_array_size(*(map_safe_peek()->stk)))
                {map_safe_peek()->fin = 1;}
            break;
        default:
            err("parse error: type not mappable");
            map_safe_peek()->fin = 1;
    }
}

void MAPPOP()
{
    stackpointer = map_safe_peek()->stk;
    optind = map_safe_peek()->opt;
    mapstackpointer = map_safe_peek();
}

// can not use two macros on the same line
#define MAPPEEK       *(map_safe_peek())
#define MAPEMPTY      (mapstackpointer == mapstack)

char* read_stream(FILE* fp)
// http://stackoverflow.com/questions/2496668/
{
    char buffer[BUFSIZ];
    size_t contentSize = 1; // includes NULL
    char* content = malloc(sizeof(char) * BUFSIZ);
    if(content == NULL)
        {return "";}
    content[0] = '\0';
    while(fgets(buffer, BUFSIZ, fp))
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

    if(ferror(fp))
    {
        free(content);
        return "";
    }
    return content;
}

char* read_stdin(void)
{
    if (isatty(fileno(stdin)))
        {return "";}
    return read_stream(stdin);
}

char* read_file(char* path)
{
    FILE* fp;
    char* content;
    fp = fopen(path, "r");
    content = read_stream(fp);
    fclose(fp);
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

    char* first = in;
    char* last = in + strlen(in) - 1;

    // skip over whitespace and semicolons at the end
    while (first < last && (JSON_WHITE(*last) || *last == ';'))
        {--last;}

    // count closing brackets at the end, still skipping whitespace
    int brackets = 0;
    while (first < last && (JSON_WHITE(*last) || *last == ')'))
    {
        if (*last == ')')
            {++brackets;}
        --last;
    }

    // no closing brackets? it's not jsonp
    if (brackets == 0)
        {return in;}

    // skip leading whitespace
    while (first < last && JSON_WHITE(*first))
        {++first;}

    // skip leading identifier if present
    while (first < last && JSON_IDENTIFIER(*first))
        {++first;}

    // skip over forward brackets and whitespace, counting down the opening brackets
    // against the closing brackets we've already done
    while (first < last && (JSON_WHITE(*first) || *first == '('))
    {
        if (*first == '(')
            {--brackets;}
        ++first;
    }

    // at this point we have a valid jsonp wrapper, provided that the number of opening
    // and closing brackets matched, and provided the two pointers didn't meet in
    // the middle (leaving no room for any actual JSON)
    if (brackets != 0 || !(first < last))
        {return in;}

    // count lines and columns skipped over
    *rows_skipped = *cols_skipped = 0;
    while (in < first) 
    {
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



char* smart_dumps(json_t* json)
// json_dumps is broken on simple types
{
    char* temp;
    char* temp2;
    json_t* j2;
    int i;
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
            i = asprintf(&temp2, "%.*s", (signed)strlen(temp)-2, &temp[1]);
            if (i == -1)
                {hard_err("internal error: out of memory");}
            return temp2;
        case JSON_INTEGER:
            i = asprintf(&temp, "%ld", (long)json_integer_value(json));
            if (i == -1)
                {hard_err("internal error: out of memory");}
            return temp;
        case JSON_REAL:
            i = asprintf(&temp, "%f", json_real_value(json));
            if (i == -1)
                {hard_err("internal error: out of memory");}
            return temp;
        case JSON_TRUE:
            return "true";
        case JSON_FALSE:
            return "false";
        case JSON_NULL:
            return "null";
        default:
            err("internal error: unknown type");
            return "null";
    }
}

/*char* pretty_dumps(json_t* json)
// underscore-style colorizing
// needs a more or less rewrite of dumps()
{
    int depth = 0;
    // loop over everything
    // needs a stack
    // number, orange
    // string, green
    // null, bold white
    // string, purple?
}*/

json_t* smart_loads(char* j_string)
// json_loads is broken on simple types
{
    json_t* json;
    json_error_t error;
    char *temp;
    int i;
    i = asprintf(&temp, "[%s]", j_string);
    if (i == -1)
        {hard_err("internal error: out of memory");}
    json = compat_json_loads(temp, &error);
    if (!json)
        {return json_string(j_string);}
    return json_array_get(json, 0);
}

char* pretty_type(json_t* json)
{
    if (json == NULL)
        {err("internal error: null pointer"); return "NULL";}
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
            err("internal error: unknown type");
            return "NULL";
    }
}

void json_err(char* message, json_t* json)
{
    char* temp;
    int i;
    i = asprintf(&temp, "parse error: type '%s' %s (arg %i)", pretty_type(json), message, optind-1);
    if (i == -1)
        {hard_err("internal error: out of memory");}
    err(temp);
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
            json_err("has no length", json);
            return 0;
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
        {json_err("has no keys", json); return;}
    if (!((keys = malloc(sizeof(char*) * json_object_size(json)))))
        {hard_err("internal error: out of memory");}

    iter = json_object_iter(json);
    n = 0;
    while (iter)
    {
        keys[n++] = json_object_iter_key(iter);
        iter = json_object_iter_next(json, iter);
    }

    if (dumps_flags & JSON_SORT_KEYS)
        {qsort(keys, n, sizeof(char*), compare_strcmp);}

    for (i = 0; i < n; ++i)
        {printf("%s\n", keys[i]);}

    free(keys);
}

json_t* nonstring(char* arg)
{
    json_t* temp;
    char* endptr;
    if (!strcmp(arg, "null") || !strcmp(arg, "n"))
        {return json_null();}
    if (!strcmp(arg, "true") || !strcmp(arg, "t"))
        {return json_true();}
    if (!strcmp(arg, "false") || !strcmp(arg, "f"))
        {return json_false();}
    if (!strcmp(arg, "array") || !strcmp(arg, "[]"))
        {return json_array();}
    if (!strcmp(arg, "object") || !strcmp(arg, "{}"))
        {return json_object();}
    errno = 0;
    temp = json_integer(strtol(arg, &endptr, 10));
    if (!errno && *endptr=='\0')
        {return temp;}
    errno = 0;
    temp = json_real(strtod(arg, &endptr));
    if (!errno && *endptr=='\0')
        {return temp;}
    arg_err("parse error: illegal nonstring on arg %i, \"%s\"");
    return json_null();
}

const char* unstring(json_t* json)
{
    switch (json_typeof(json))
    {
        case JSON_STRING:
            return json_string_value(json);
        case JSON_INTEGER:
        case JSON_REAL:
        case JSON_TRUE:
        case JSON_FALSE:
        case JSON_NULL:
            return smart_dumps(json);
        case JSON_OBJECT:
        case JSON_ARRAY:
        default:
            json_err("is not simple/printable", json);
            return "";
    }
}

int estrtol(char* key)
// strtol with more error handling
{
    int i;
    char* endptr;
    errno = 0;
    i = strtol(key, &endptr, 10);
    if (errno || *endptr!='\0')
    {
        arg_err("parse error: illegal index on arg %i, \"%s\"");
        //return json_null();
        i = 0;
    }
    return i;
}

json_t* extract(json_t* json, char* key)
{
    int i, s;
    json_t* temp;
    switch (json_typeof(json))
    {
        case JSON_OBJECT:
            temp = json_object_get(json, key);
            if (temp == NULL)
                {break;}
            return temp;
        case JSON_ARRAY:
            s = json_array_size(json);
            if (s == 0)
                {break;}
            i = estrtol(key);
            return json_array_get(json, i % s);
        case JSON_STRING:
        case JSON_INTEGER:
        case JSON_REAL:
        case JSON_TRUE:
        case JSON_FALSE:
        case JSON_NULL:
        default:
            break;
    }
    json_err("has no elements to extract", json);
    return json_null();
}

json_t* delete(json_t* json, char* key)
// no error checking
{
    int i, s;
    switch (json_typeof(json))
    {
        case JSON_OBJECT:
            json_object_del(json, key);
            return json;
        case JSON_ARRAY:
            s = json_array_size(json);
            if (s == 0)
                {return json;}
            i = estrtol(key);
            json_array_remove(json, i % s);
            return json;
        case JSON_STRING:
        case JSON_INTEGER:
        case JSON_REAL:
        case JSON_TRUE:
        case JSON_FALSE:
        case JSON_NULL:
        default:
            json_err("cannot lose elements", json);
            return json;
    }
}

json_t* update_native(json_t* json, char* key, json_t* j_value)
// no error checking
{
    int i, s;
    switch (json_typeof(json))
    {
        case JSON_OBJECT:
            json_object_set(json, key, j_value);
            return json;
        case JSON_ARRAY:
            if (!strcmp(key, "append"))
            {
                json_array_append(json, j_value);
                return json;
            }
            // otherwise, insert
            i = estrtol(key);
            s = json_array_size(json);
            if (s == 0)
                {i = 0;}
            else
                {i = i % s;}
            json_array_insert(json, i, j_value);
            return json;
        case JSON_STRING:
        case JSON_INTEGER:
        case JSON_REAL:
        case JSON_TRUE:
        case JSON_FALSE:
        case JSON_NULL:
        default:
            json_err("cannot gain elements", json);
            return json;
    }
}

json_t* update(json_t* json, char* key, char* j_string)
{
    return update_native(json, key, smart_loads(j_string));
}

void debug_stack(int optchar)
{
    json_t** j;
    printf("BEGIN STACK DUMP %c\n", optchar);
    for (j=stack; j<stackpointer; j++)
        {printf("%s\n", smart_dumps(*j));}
}

void debug_map()
{
    mapping* m;
    printf("BEGIN MAP DUMP\n");
    for (m=mapstack; m<mapstackpointer; m++)
        {printf("%s\n", smart_dumps(*(m->stk)));}
}

int main (int argc, char *argv[])
#define ALL_OPTIONS "PSQVCItlkupaF:e:s:n:d:i:"
{
    char* content = "";
    char* arg1 = "";
    FILE* fp;
    json_t* json = NULL;
    json_t* jval = NULL;
    json_error_t error;
    int output = 1;  // flag if json should be printed
    int optchar;
    int jsonp = 0;   // flag if we should tolerate JSONP wrapping
    int jsonp_rows = 0, jsonp_cols = 0;   // rows+cols skipped over by JSONP prologue
    g_argv = argv;

    // todo: get more jsonp stuff out of main

    // avoiding getopt_long for now because the BSD version is a pain
    if (argc == 2 && strncmp(argv[1], "--version", 9) == 0)
        {printf("%i\n", JSHONVER); exit(0);}

    // non-manipulation options
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
            case 'Q':
                quiet = 1;
                break;
            case 'V':
                by_value = 1;
                break;
            case 'C':
                crash = 0;
                break;
            case 'I':
                in_place = 1;
                break;
            case 'F':
                file_path = (char*) strdup(optarg);
                break;
            case 't':
            case 'l':
            case 'k':
            case 'u':
            case 'p':
            case 'e':
            case 's':
            case 'n':
            case 'd':
            case 'i':
            case 'a':
                break;
            default:
                if (!quiet)
                    {fprintf(stderr, "Valid: -[P|S|Q|V|C|I] [-F path] -[t|l|k|u|p|a] -[s|n] value -[e|i|d] index\n");}
                if (crash)
                    {exit(2);}
                break;
        }
    }
    optind = 1;
#ifdef BSD
    optreset = 1;
#endif

    if (in_place && strlen(file_path)==0)
        {err("warning: in-place editing (-I) requires -F");}

    if (!strcmp(file_path, "-"))
        {content = read_stdin();}
    else if (strlen(file_path) > 0)
        {content = read_file(file_path);}
    else
        {content = read_stdin();}
    if (!content[0] && !quiet)
        {fprintf(stderr, "warning: nothing to read\n");}

    if (jsonp)
        {content = remove_jsonp_callback(content, &jsonp_rows, &jsonp_cols);}

    if (content[0])
        {json = compat_json_loads(content, &error);}

    if (!json && content[0])
    {
        const char *jsonp_status = "";
        if (jsonp)
            {jsonp_status = (jsonp_rows||jsonp_cols) ? "(jsonp detected) " : "(jsonp not detected) ";}

#if JANSSON_MAJOR_VERSION < 2
        if (!quiet)
            {fprintf(stderr, "json %sread error: line %0d: %s\n",
                 jsonp_status, error.line + jsonp_rows, error.text);}
#else
        if (!quiet)
            {fprintf(stderr, "json %sread error: line %0d column %0d: %s\n",
                jsonp_status, error.line + jsonp_rows, error.column + jsonp_cols, error.text);}
#endif
        exit(1);
    }

    if (json)
        {PUSH(json);}

    do
    {
        if (! MAPEMPTY)
        {
            while (map_safe_peek()->fin)
            {
                MAPPOP();
                if (MAPEMPTY)
                    {exit(0);}
                if (map_safe_peek()->fin)
                    {MAPNEXT();}
            }
            MAPNEXT();
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
                case 'p':  // pop stack
                    json = POP;
                    if (by_value)
                        {json_decref(json);}
                    output = 1;
                    break;
                case 's':  // load string
                    arg1 = (char*) strdup(optarg);
                    PUSH(json_string(arg1));
                    output = 1;
                    break;
                case 'n':  // load nonstring
                    arg1 = (char*) strdup(optarg);
                    PUSH(nonstring(arg1));
                    output = 1;
                    break;
                case 'e':  // extract
                    arg1 = (char*) strdup(optarg);
                    json = PEEK;
                    PUSH(extract(maybe_deep(json), arg1));
                    output = 1;
                    break;
                case 'd':  // delete
                    arg1 = (char*) strdup(optarg);
                    json = POP;
                    PUSH(delete(json, arg1));
                    output = 1;
                    break;
                case 'i':  // insert
                    arg1 = (char*) strdup(optarg);
                    jval = POP;
                    json = POP;
                    PUSH(update_native(json, arg1, jval));
                    output = 1;
                    break;
                case 'a':  // across
                    // something about -a is not mappable?
                    MAPPUSH();
                    MAPNEXT();
                    output = 0;
                    break;
                case 'P':  // not manipulations
                case 'S': 
                case 'Q':
                case 'V':
                case 'C':
                case 'I':
                case 'F':
                    break;
                default:
                    if (crash)
                        {exit(2);}
                    break;
            }
        }
        if (!in_place && output && stackpointer != stack)
            {printf("%s\n", smart_dumps(PEEK));}
    } while (! MAPEMPTY);

    if (in_place && strlen(file_path) > 0)
    {
        fp = fopen(file_path, "w");
        fprintf(fp, "%s\n", smart_dumps(stack[0]));
        fclose(fp);
    }
}


