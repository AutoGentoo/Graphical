%top {
#include <stdlib.h>
#include <string.h>
#include "language.h"
}

%option prefix="depend"

%union {
    char* identifier;
    Atom* atom_type;
    Dependency* depend_type;
    RequiredUse* use_type;

    struct {
        char* target;
        use_operator_t operator;
    } depend_expr_select;

    struct {
        char* name;
        char* sub_name;
        atom_slot_t slot_opts;
    } slot;

    AtomFlag* atom_flags;
}

%token '-'
%token '<'
%token '>'
%token '='
%token '!'
%token '['
%token ']'
%token '?'
%token '('
%token ')'
%token '|'
%token '^'
%token ','
%token '~'
%token <identifier> IDENTIFIER
%token <identifier> REPOSITORY
%token <atom_type> ATOM
%token END_OF_FILE
%token <atom_flags> ATOM_FLAGS
%token <slot> SLOT
%token <depend_expr_select> USESELECT

%type <atom_type> atom_version
%type <atom_type> atom_block
%type <atom_type> atom_slot
%type <atom_type> atom_repo
%type <atom_type> atom
%type <depend_type> depend_expr
%type <depend_type> depend_expr_single
%type <depend_type> program_depend
%start<depend_type> program_depend

//%destructor { if ($$.target) free($$.target); } <depend_expr_select>
//%destructor { Py_DECREF($$); } <atom_type>;
//%destructor { Py_DECREF($$); } <atom_flags>;
//%destructor { free($$.name); if($$.sub_name) free($$.sub_name); } <slot>;
//%destructor { free($$); } <identifier>;

%bottom {
Atom* atom_parse(void* buffers, const char* input)
{
    Dependency* out = depend_parse(buffers, input);

    if (!out)
    {
        return NULL;
    }

    Atom* atom_out = out->atom;
    Py_INCREF(atom_out);
    Py_DECREF(out);

    return atom_out;
}
}

==

"[\n]"                  {}
"[ \t\r\\]+"            {/* skip */}
"{repo}"                  {yyval->identifier = strdup(yytext + 2); return REPOSITORY;}
"{slot}"                  {
                            char* ref = strdup(yytext);
                            int len = strlen(ref);
                            yyval->slot.slot_opts = ATOM_SLOT_IGNORE;
                            if (ref[len - 1] == '*') {
                                yyval->slot.slot_opts = ATOM_SLOT_IGNORE;
                                ref[--len] = 0;
                            }
                            else if (ref[len - 1] == '=') {
                                yyval->slot.slot_opts = ATOM_SLOT_REBUILD;
                                ref[--len] = 0;
                            }

                            char* name_splt = strchr(ref + 1, '/');
                            if (name_splt)
                                *name_splt = 0;
                            if (ref[1] == 0) {
                                yyval->slot.name = NULL;
                                yyval->slot.sub_name = NULL;
                                free(ref);
                                return SLOT;
                            }
                            yyval->slot.name = strdup(ref + 1);
                            if (name_splt)
                                yyval->slot.sub_name = strdup(name_splt + 1);
                            else
                                yyval->slot.sub_name = NULL;

                            free(ref);
                            return SLOT;
                        }
"{atomflags}"           {
                            char* textref = strdup(yytext);
                            AtomFlag* start = NULL;
                            AtomFlag* last = NULL;
                            AtomFlag* current;

                            for (char* tok = strtok(textref + 1, " ,]"); tok; tok = strtok(NULL, " ,]"))
                            {
                                size_t len = strlen(tok);
                                if (!len)
                                    continue;
                                atom_use_t option;

                                if (*tok == '!' && tok[len - 1] == '?')
                                {
                                    tok[len - 1] = 0;
                                    tok++;
                                    option = ATOM_USE_DISABLE_IF_OFF;
                                }
                                else if (*tok == '!' && tok[len - 1] == '=')
                                {
                                    tok[len - 1] = 0;
                                    tok++;
                                    option = ATOM_USE_OPPOSITE;
                                }
                                else if (tok[len - 1] == '?')
                                {
                                    tok[len - 1] = 0;
                                    option = ATOM_USE_ENABLE_IF_ON;
                                }
                                else if (tok[len - 1] == '=')
                                {
                                    tok[len - 1] = 0;
                                    option = ATOM_USE_EQUAL;
                                }
                                else if (*tok == '-')
                                {
                                    tok++;
                                    option = ATOM_USE_DISABLE;
                                }
                                else
                                    option = ATOM_USE_ENABLE;

                                current = atomflag_build(tok);
                                current->option = option;

                                if (!last)
                                    last = current;
                                else
                                {
                                    last->next = current;
                                    last = current;
                                }

                                if (!start)
                                    start = current;
                            }

                            yyval->atom_flags = start;
                            free(textref);
                            return ATOM_FLAGS;
                        }
"{use_select}"          {
                            char* ref = strdup(yytext);

                            if (*ref == '!')
                            {
                                yyval->depend_expr_select.operator = USE_OP_DISABLE;
                                ref++;
                            }
                            else
                            {
                                yyval->depend_expr_select.operator = USE_OP_ENABLE;
                            }

                            size_t len = strlen(ref);
                            ref[len - 1] = 0;
                            yyval->depend_expr_select.target = strndup(ref, len - 1);
                            free(ref);
                            return USESELECT;
                        }
"\\?\\?"                    {yyval->depend_expr_select.target = NULL; yyval->depend_expr_select.operator = USE_OP_MOST_ONE; return USESELECT;}
"\\|\\|"                    {yyval->depend_expr_select.target = NULL; yyval->depend_expr_select.operator = USE_OP_LEAST_ONE; return USESELECT;}
"\\^\\^"                    {yyval->depend_expr_select.target = NULL; yyval->depend_expr_select.operator = USE_OP_EXACT_ONE; return USESELECT;}
"-"                     {return '-';}
"<"                     {return '<';}
">"                     {return '>';}
"="                     {return '=';}
"!"                     {return '!';}
"[\\[]"                  {return '[';}
"[\\]]"                  {return ']';}
"[\\?]"                  {return '?';}
"[\\(]"                  {return '(';}
"[\\)]"                  {return ')';}
"[\\^]"                  {return '^';}
"[\\|]"                  {return '|';}
"[\\,]"                  {return ',';}
"[\\~]"                  {return '~';}
"{atom}"                {
                            yyval->atom_type = (Atom*)PyAtom_new(&PyAtomType, NULL, NULL);
                            atom_init(yyval->atom_type, yytext);
                            return ATOM;
                        }

"[\\*]"                  {return '*';}

==

%%

program_depend : depend_expr                {$$ = (void*)$1;}
               |                            {$$ = NULL;}
               ;

depend_expr  : depend_expr_single               {$$ = $1;}
             | depend_expr_single depend_expr   {$$ = $1; $$->next = $2;}
             ;

depend_expr_single  : atom                          {$$ = dependency_build_atom($1);}
                    | USESELECT '(' depend_expr ')' {$$ = dependency_build_use($1.target, $1.operator, $3); free($1.target);}
                    | '(' depend_expr ')'           {$$ = dependency_build_grouping($2);}
                    ;

atom        : atom_repo ATOM_FLAGS          {$$ = $1; $$->useflags = $2;}
            | atom_repo                     {$$ = $1; $$->useflags = NULL;}
            ;

atom_repo   : atom_slot REPOSITORY  {
                                        $$ = $1;
                                        if ($$->repository) free($$->repository);
                                        $$->repository = $2;
                                    }
            | atom_slot             {$$ = $1;
                                     if ($$->repository) free($$->repository);
                                     $$->repository = NULL;
                                    }
            ;

atom_slot   : atom_block SLOT             {
                                               $$ = $1;
                                               $$->slot = $2.name;
                                               $$->sub_slot = $2.sub_name;
                                               $$->slot_opts = $2.slot_opts;
                                          }
            | atom_block                {$$ = $1; $$->slot = NULL; $$->sub_slot = NULL; $$->slot_opts = ATOM_SLOT_IGNORE;}
            ;

atom_block  :  '!' '!' atom_version     {$$ = $3; $$->blocks = ATOM_BLOCK_HARD;}
            |      '!' atom_version     {$$ = $2; $$->blocks = ATOM_BLOCK_SOFT;}
            |          atom_version     {$$ = $1; $$->blocks = ATOM_BLOCK_NONE;}
            ;

atom_version   : '>' '=' ATOM           {$$ = $3; $$->range = ATOM_VERSION_GE;}
               | '<' '=' ATOM           {$$ = $3; $$->range = ATOM_VERSION_LE;}
               |     '=' ATOM           {$$ = $2; $$->range = ATOM_VERSION_E;}
               |     '>' ATOM           {$$ = $2; $$->range = ATOM_VERSION_G;}
               |     '~' ATOM           {$$ = $2; $$->range = ATOM_VERSION_REV;}
               |     '<' ATOM           {$$ = $2; $$->range = ATOM_VERSION_L;}
               |         ATOM           {$$ = $1; $$->range = ATOM_VERSION_ALL;}
               ;

/*
command_atom   : atom_repo              {$$ = $1;}
               | '>' '=' IDENTIFIER     {$$ = cmdline_atom_new($3); if ($$) $$->range = ATOM_VERSION_GE;}
               | '<' '=' IDENTIFIER     {$$ = cmdline_atom_new($3); if ($$) $$->range = ATOM_VERSION_LE;}
               |     '=' IDENTIFIER     {$$ = cmdline_atom_new($2); if ($$) $$->range = ATOM_VERSION_E;}
               |     '>' IDENTIFIER     {$$ = cmdline_atom_new($2); if ($$) $$->range = ATOM_VERSION_G;}
               |     '~' IDENTIFIER     {$$ = cmdline_atom_new($2); if ($$) $$->range = ATOM_VERSION_REV;}
               |     '<' IDENTIFIER     {$$ = cmdline_atom_new($2); if ($$) $$->range = ATOM_VERSION_L;}
               |         IDENTIFIER     {$$ = cmdline_atom_new($1); if ($$) $$->range = ATOM_VERSION_ALL;}
               ;

command_line   : command_atom ATOM_FLAGS         {$1->useflags = $2; $$ = dependency_build_atom($1);}
               | command_atom                    {$$ = dependency_build_atom($1);}
               | command_line command_line       {$$ = $1; $$->next = $2;}
               ;
*/
%%