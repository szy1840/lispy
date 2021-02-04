#include "mpc.h"
#define LASSERT(args,cond,fmt,...) \
    if(!(cond)) {\
        lval* err=lval_err(fmt,##__VA_ARGS__);\
        lval_del(args);\
        return err;\
    }

#define LASSERT_NUM(func,args,num) \
    LASSERT(args,args->count==num, \
    "Function '%s' passed incorrect number of arguments. "\
    "Got %d, Expected %d.",func,args->count,num)

#define LASSERT_TYPE(func,args,index,expect) \
    LASSERT(args,args->cell[index]->type==expect,\
    "Function '%s' passed incorrect type for argument %d."\
    "Got %s, Expected %s.",\
    func,index,ltype_name(args->cell[index]->type),ltype_name(expect))

#define LASSERT_NOT_EMPTY(func,args,index)\
    LASSERT(args,args->cell[index]->count!=0,\
    "Function '%s' passed {} for argument %d.",func,index);

/* ; ? need ? */
#ifdef _WIN32

static char buffer[2048];
/* fake readline function */
char *readline(char *prompt)
{
    fputs(prompt, stdout);
    fgets(buffer, 2048, stdin);
    char *cpy = malloc(strlen(buffer) + 1);
    strcpy(cpy, buffer);
    cpy[strlen(cpy) - 1] = '\0';
    return cpy;
}
/* fake add_history function */
void add_history(char *unused) {}
#else
#include <editline/readline.h>
#include <editline/history.h>
#endif

/* forward declarations(to resolve cyclic types) */
struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;
typedef lval*(*lbuiltin)(lenv*,lval*);
enum {LVAL_ERR,LVAL_NUM,LVAL_SYM,
    LVAL_FUN,LVAL_SEXPR,LVAL_QEXPR};
struct lval{
    int type;
    //basic
    long num;
    char* err;
    char* sym;
    //function
    lbuiltin builtin;
    lenv* env;
    lval* formals;
    lval* body;
    //expression
    int count;
    struct lval** cell;
};
struct lenv{
    lenv* par;
    int count;
    char** syms;
    lval** vals;
};
/* prototypes */
void lval_print(lval* v);
lval* lval_eval(lenv* e,lval* v);
lval* builtin(lenv* e,lval* a, char* func);
void lenv_del(lenv* e);
lval* lval_copy(lval* v);
lval* builtin(lenv* e,lval* a, char* func);
lval* lval_call(lenv* e,lval* f,lval* a);
lenv* lenv_copy(lenv* e);
lval* builtin_eval(lenv* e,lval* a);
lval* builtin_list(lenv* e,lval* a);

/* return name */
char* ltype_name(int t){
    switch(t){
        case LVAL_FUN: return "Function";
        case LVAL_NUM: return "Number";
        case LVAL_ERR: return "Error";
        case LVAL_SYM: return "Symbol";
        case LVAL_SEXPR: return "S-Expression";
        case LVAL_QEXPR: return "Q-Expression";
        default: return "Unknown";
    }
}
/* construct lenv */
lenv* lenv_new(){
    lenv* e=malloc(sizeof(lenv));
    e->par=NULL;
    e->count=0;
    e->syms=NULL;
    e->vals=NULL;
    return e;
}
/* construct a pointer to a new number type lval */
lval* lval_num(long x){
    lval* v=malloc(sizeof(lval));
    v->type=LVAL_NUM;
    v->num=x;
    return v;
}
/* construct a pointer to a new error type lval */
lval* lval_err(char* fmt,...){
    lval* v=malloc(sizeof(lval));
    v->type=LVAL_ERR;

    va_list va;
    /* initialize va with the last named argument */
    va_start(va,fmt);
    v->err=malloc(512);
    vsnprintf(v->err,511,fmt,va);
    v->err=realloc(v->err,strlen(v->err)+1);
    va_end(va);
    return v;
}
/* construct a pointer to a new symbol type lval */
lval* lval_sym(char* s){
    lval* v=malloc(sizeof(lval));
    v->type=LVAL_SYM;
    v->sym=malloc(strlen(s)+1);
    strcpy(v->sym,s);
    return v;
}
/* construct a pointer to a new empty sexpr lval */
lval* lval_sexpr(){
    lval* v=malloc(sizeof(lval));
    v->type=LVAL_SEXPR;
    v->count=0;
    v->cell=NULL;
    return v;
}
/* construct a pointer to a new empty qexpr lval */
lval* lval_qexpr(){
    lval* v=malloc(sizeof(lval));
    v->type=LVAL_QEXPR;
    v->count=0;
    v->cell=NULL;
    return v;
}
/* construct a pointer to a funcPtr lval */
lval* lval_fun(lbuiltin func){
    lval* v=malloc(sizeof(lval));
    v->type=LVAL_FUN;
    v->builtin=func;
    return v;
}
/* construct a user-defined lval */
lval* lval_lambda(lval* formals,lval* body){
    lval* v=malloc(sizeof(lval));
    v->type=LVAL_FUN;
    v->builtin=NULL;
    v->env=lenv_new();
    v->formals=formals;
    v->body=body;
    return v;
}

/* destruct lval */
void lval_del(lval* v){
    switch(v->type){
        case LVAL_NUM: break;
        case LVAL_FUN:
            if(!v->builtin){
                lenv_del(v->env);
                lval_del(v->formals);
                lval_del(v->body);
            }
            break;
        case LVAL_ERR: 
            free(v->err);
            break;
        case LVAL_SYM:
            free(v->sym);
            break;
        case LVAL_QEXPR:
        case LVAL_SEXPR:
            for (int i = 0; i < v->count; i++){
                lval_del(v->cell[i]);
            }
            free(v->cell);
            break;   
    }

    free(v);
}
/* destruct lenv */
void lenv_del(lenv* e){
    for (int i = 0; i < e->count; i++)
    {
        free(e->syms[i]);
        lval_del(e->vals[i]);
    }
    free(e->syms);
    free(e->vals);
    free(e);
}

/* read */
lval* lval_read_num(mpc_ast_t* t){
    errno=0;
    long x=strtol(t->contents,NULL,10);
    return errno!=ERANGE ? lval_num(x) 
    : lval_err("invalid number");
}

lval* lval_add(lval* v,lval* x){
    v->count++;
    v->cell=realloc(v->cell,sizeof(lval*) * (v->count));
    v->cell[v->count-1]=x;
    return v;
}

lval* lval_read(mpc_ast_t* t){
    if(strstr(t->tag,"number")){
        /* numbers need extra check(though seems it's better to
         only give the number to the func to check) */
        return lval_read_num(t);
    }
    if(strstr(t->tag,"symbol")){
        return lval_sym(t->contents);
    }

    /* if root(>) or sexpr then create empty list */
    lval* x=NULL;
    if(strcmp(t->tag,">")==0){
        x=lval_sexpr();
    }
    if(strstr(t->tag,"sexpr")){
        x=lval_sexpr();
    }
    if(strstr(t->tag,"qexpr")){
        x=lval_qexpr();
    }
    /* fill the list with any valid expression contained within */
    for (int i = 0; i < t->children_num; i++)
    {
        if(strcmp(t->children[i]->contents,"(")==0) continue;
        if(strcmp(t->children[i]->contents,")")==0) continue;
        if(strcmp(t->children[i]->contents,"{")==0) continue;
        if(strcmp(t->children[i]->contents,"}")==0) continue;
        if(strcmp(t->children[i]->tag,"regex")==0) continue;
        
        x=lval_add(x,lval_read(t->children[i]));
    }
    
    return x;
}
/* print */
void lval_expr_print(lval* v,char open, char close){
    putchar(open);
    for (int i = 0; i < v->count; i++)
    {
        lval_print(v->cell[i]);
        /* avoid print trailing space if it's the last one */
        if(i!=(v->count-1)) putchar(' ');
    }
    
    putchar(close);
}
void lval_print(lval* v){
    switch(v->type){
        case LVAL_NUM: 
            printf("%li",v->num);
            break;
        case LVAL_FUN:
            if(v->builtin){
                printf("<builtin>");
            }else{
                printf("(\\");
                lval_print(v->formals);
                putchar(' ');
                lval_print(v->body);
                putchar(')');
            }
            break;
        case LVAL_ERR: 
            printf("Error:%s",v->err);
            break;
        case LVAL_SYM:
            printf("%s",v->sym);
            break;
        case LVAL_SEXPR:
            lval_expr_print(v,'(',')');
            break;
        case LVAL_QEXPR:
            lval_expr_print(v,'{','}');
            break;
    }
}
void lval_println(lval* v){
    lval_print(v);
    putchar('\n');
}

/* copy a lval */
lval* lval_copy(lval* v){
    lval* x=malloc(sizeof(lval));
    x->type=v->type;

    switch(v->type){
        case LVAL_FUN:
            if(v->builtin){
                x->builtin=v->builtin;
            }else{
                x->builtin=NULL;
                x->env=lenv_copy(v->env);
                x->formals=lval_copy(v->formals);
                x->body=lval_copy(v->body);
            }
            break;
        case LVAL_NUM:
            x->num=v->num;
            break;
        case LVAL_ERR:
            x->err=malloc(strlen(v->err)+1);
            strcpy(x->err,v->err);
            break;
        case LVAL_SYM:
            x->sym=malloc(strlen(v->sym)+1);
            strcpy(x->sym,v->sym);
            break;
        case LVAL_SEXPR:
        case LVAL_QEXPR:
        /* copy lists by copying each sub-expr */
            x->count=v->count;
            x->cell=malloc(sizeof(lval*)*(x->count));
            for (int i = 0; i < x->count; i++)
            {
                x->cell[i]=lval_copy(v->cell[i]);
            }
            break;
    }
    return x;
}
/* lval_pop takes an element from the given list and pop it,
while lval_take also delete the list and leave the element only */
lval* lval_pop(lval* v, int i){
    /* x gets the content of v->cell[i](though it is an address),
    so realloc won't make the content of x invalid */
    lval* x=v->cell[i];

    /* shift the memory */
    memmove(&v->cell[i],&v->cell[i+1],sizeof(lval*)*(v->count-i-1));
    v->count--;
    v->cell=realloc(v->cell,sizeof(lval*)*(v->count));
    return x;
}
lval* lval_take(lval* v, int i){
    lval* x=lval_pop(v,i);
    lval_del(v);
    return x;
}

/* lenv */
lval* lenv_get(lenv* e,lval* k){
    for (int i = 0; i < e->count; i++)
    {
        if(strcmp(e->syms[i],k->sym)==0){
            return lval_copy(e->vals[i]);
        }
    }
    if(e->par){
        return lenv_get(e->par,k);
    }else{
        return lval_err("Unbound symbol '%s'",k->sym);
    }
}
void lenv_put(lenv* e,lval* k,lval* v){
    for (int i = 0; i < e->count; i++)
    {
        if(strcmp(e->syms[i],k->sym)==0){
            lval_del(e->vals[i]);
            e->vals[i]=lval_copy(v);
            return;
        }
    }
    /* if nothing found in env */
    e->count++;
    e->vals=realloc(e->vals,sizeof(lval*)*(e->count));
    e->syms=realloc(e->syms,sizeof(char*)*(e->count));
    e->vals[e->count-1]=lval_copy(v);
    e->syms[e->count-1]=malloc(strlen(k->sym)+1);
    strcpy(e->syms[e->count-1],k->sym);
}
/* define env globally */
void lenv_def(lenv* e,lval* k,lval* v){
    while(e->par) e=e->par;
    lenv_put(e,k,v);
}
/* copy a lenv */
lenv* lenv_copy(lenv* e){
    lenv* n=malloc(sizeof(lenv));
    n->par=e->par;
    n->count=e->count;
    n->syms=malloc(sizeof(char*)*(n->count));
    n->vals=malloc(sizeof(lval*)*(n->count));
    for(int i=0;i<e->count;i++){
        n->syms[i]=malloc(strlen(e->syms[i])+1);
        strcpy(n->syms[i],e->syms[i]);
        n->vals[i]=lval_copy(e->vals[i]);
    }
    return n;
}

/* evaluation */
lval* lval_eval_sexpr(lenv* e,lval* v){
    for (int i = 0; i < v->count; i++)
    {
        v->cell[i]=lval_eval(e,v->cell[i]);
    }
    /* error checking */
    for (int i = 0; i < v->count; i++)
    {
        if(v->cell[i]->type==LVAL_ERR){
            return lval_take(v,i);
        }
    }
    /* empty expression */
    if(v->count==0) return v;
    /* single expression */
    if(v->count==1) return lval_take(v,0);

    /* ensure the first element is a function */
    lval* f=lval_pop(v,0);
    if(f->type!=LVAL_FUN){
        lval* err=lval_err(
            "S-Expression starts with incorrect type. "
            "Got %s, Expected %s.",ltype_name(f->type),
            ltype_name(LVAL_FUN));
            lval_del(f);
            lval_del(v);
            return err;
    }

    lval* result=lval_call(e,f,v);
    lval_del(f);
    return result;
}
lval* lval_eval(lenv* e,lval* v){
    if(v->type==LVAL_SYM){
        lval* x=lenv_get(e,v);
        lval_del(v);
        return x;
    }
    if(v->type==LVAL_SEXPR){
        return lval_eval_sexpr(e,v);
    }

    return v;
}
lval* lval_call(lenv* e,lval* f,lval* a){
    if(f->builtin) return f->builtin(e,a);

    int given=a->count;
    int total=f->formals->count;
    while(a->count){
        if(f->formals->count==0){
            lval_del(a);
            return lval_err("Function passed too many"
            " arguments. Got %d,Expected %d.",given,total);
        }
        lval* sym=lval_pop(f->formals,0);
        if(strcmp(sym->sym,"&")==0){
            /* ensure & is followed by another symbol */
            if(f->formals->count!=1){
                lval_del(a);
                return lval_err("Function format invalid. "
                "Symbol '&' not followed by single symbol.");
            }
            /* Next formal should be bound to remaining arguments */
            lval* nsym=lval_pop(f->formals,0);
            lenv_put(f->env,nsym,builtin_list(e,a));
            lval_del(sym);
            lval_del(nsym);
            break;
        }

        lval* val=lval_pop(a,0);
        lenv_put(f->env,sym,val);
        lval_del(sym);
        lval_del(val);
    }
    lval_del(a);

    if(f->formals->count>0 && strcmp(f->formals->cell[0]->sym,"&")==0){
        if(f->formals->count!=2){
            return lval_err("Function format invalid. "
            "Symbol '&' not followed by single symbol.");
        }
        lval_del(lval_pop(f->formals,0));
        lval* sym=lval_pop(f->formals,0);
        lval* val=lval_qexpr();
        lenv_put(f->env,sym,val);
        lval_del(sym);
        lval_del(val);
    }
 ;
    if(f->formals->count==0){
        f->env->par=e;
        return builtin_eval(f->env,lval_add(lval_sexpr(),
        lval_copy(f->body)));
    }else{
        return lval_copy(f);
    }

    lval_del(a);

    f->env->par=e;
    return builtin_eval(f->env,
    lval_add(lval_sexpr(),lval_copy(f->body)));
}

/* builtin functions */
lval* builtin_op(lenv* e,lval* a,char* op){
    for (int i = 0; i < a->count; i++){
        LASSERT_TYPE(op,a,i,LVAL_NUM);
    }
    
    lval* x=lval_pop(a,0);
    /*  it's zero because we used pop to trim the first elem(symbol) in the eval func*/
    if((strcmp(op,"-")==0) && a->count==0) x->num=-(x->num);

    while(a->count>0){
        lval* y=lval_pop(a,0);

        if(strcmp(op,"+")==0) x->num += y->num;
        if(strcmp(op,"-")==0) x->num -= y->num;
        if(strcmp(op,"*")==0) x->num *= y->num;
        if(strcmp(op,"/")==0) {
            if(y->num==0){
                lval_del(x);
                lval_del(y);
                x=lval_err("Division by zero!");
                break;
            }
            x->num /= y->num;
        }
        lval_del(y);
    }
    lval_del(a);
    /* remember x is popped so we can safely del a */
    return x;
}
lval* builtin_add(lenv* e,lval* a){
    return builtin_op(e,a,"+");
}
lval* builtin_sub(lenv* e,lval* a){
    return builtin_op(e,a,"-");
}
lval* builtin_mul(lenv* e,lval* a){
    return builtin_op(e,a,"*");
}
lval* builtin_div(lenv* e,lval* a){
    return builtin_op(e,a,"/");
}

lval* builtin_head(lenv* e,lval* a){
    LASSERT_NUM("head",a,1);
    LASSERT_TYPE("head",a,0,LVAL_QEXPR);
    LASSERT_NOT_EMPTY("head",a,0);

    lval* v=lval_take(a,0);
    while(v->count > 1){
        lval_del(lval_pop(v,1));
    }
    return v;
}
lval* builtin_tail(lenv* e,lval* a){
    LASSERT_NUM("tail",a,1);
    LASSERT_TYPE("tail",a,0,LVAL_QEXPR);
    LASSERT_NOT_EMPTY("tail",a,0);

    lval* v=lval_take(a,0);
    lval_del(lval_pop(v,0));
    return v;
}
lval* builtin_list(lenv* e,lval* a){
    a->type=LVAL_QEXPR;
    return a;
}
lval* builtin_eval(lenv* e,lval* a){
    LASSERT_NUM("eval",a,1);
    LASSERT_TYPE("eval",a,0,LVAL_QEXPR);
    
    lval* x=lval_take(a,0);
    x->type=LVAL_SEXPR;
    return lval_eval(e,x);
}
lval* lval_join(lenv* e,lval* x,lval* y){
    while(y->count){
        x=lval_add(x,lval_pop(y,0));
    }
    lval_del(y);
    return x;
}
lval* builtin_join(lenv* e,lval* a){
    for (int i = 0; i < a->count; i++)
    {
        LASSERT_TYPE("join",a,i,LVAL_QEXPR);
    }
    lval* x=lval_pop(a,0);
    while(a->count){
        x=lval_join(e,x,lval_pop(a,0));
    }
    lval_del(a);
    return x;
}
lval* builtin(lenv* e,lval* a, char* func){
    if(strcmp("list",func)==0) return builtin_list(e,a);
    if(strcmp("head",func)==0) return builtin_head(e,a);
    if(strcmp("tail",func)==0) return builtin_tail(e,a);
    if(strcmp("join",func)==0) return builtin_join(e,a);
    if(strcmp("eval",func)==0) return builtin_eval(e,a);

    if(strstr("+-*/",func)) return builtin_op(e,a,func);

    lval_del(a);
    return lval_err("Unknown Function!");
}

lval* builtin_lambda(lenv* e,lval* a){
    LASSERT_NUM("\\",a,2);
    LASSERT_TYPE("\\",a,0,LVAL_QEXPR);
    LASSERT_TYPE("\\",a,1,LVAL_QEXPR);

    for (int i = 0; i < a->cell[0]->count; i++)
    {
        LASSERT(a,(a->cell[0]->cell[i]->type==LVAL_SYM),
        "Cannot define non-symbol. Got %s, Expected %s",
        ltype_name(a->cell[0]->cell[i]->type),
        ltype_name(LVAL_SYM));
    }
    lval* formals=lval_pop(a,0);
    lval* body=lval_pop(a,0);
    lval_del(a);
    return lval_lambda(formals,body);
}

/* define */
lval* builtin_var(lenv* e,lval* a,char* func){
    LASSERT_TYPE(func,a,0,LVAL_QEXPR);
    lval* syms=a->cell[0];
    for (int i = 0; i < syms->count; i++){
        LASSERT(a,(syms->cell[i]->type==LVAL_SYM),
        "Function '%s' cannot define non-symbol. "
        "Got %s, Expected %s.",func,
        ltype_name(syms->cell[i]->type),
        ltype_name(LVAL_SYM));
    }
    
    LASSERT(a,(syms->count==a->count-1),
    "Function '%s' passed too many arguments for symbols. "
    "Got %d, Expected %d.",func,syms->count,a->count-1);

    for (int i = 0; i < syms->count; i++){
        if(strcmp(func,"def")==0){
            lenv_def(e,syms->cell[i],a->cell[i+1]);
        }
        if(strcmp(func,"=")==0){
            lenv_put(e,syms->cell[i],a->cell[i+1]);
        }
    }
    
    lval_del(a);
    return lval_sexpr();
}
lval* builtin_put(lenv* e,lval* a){
    return builtin_var(e,a,"=");
}
lval* builtin_def(lenv* e,lval* a){
    return builtin_var(e,a,"def");
}


/* register builtins with environment */
void lenv_add_builtin(lenv* e,char* name,lbuiltin func){
    lval* k=lval_sym(name);
    lval* v=lval_fun(func);
    lenv_put(e,k,v);

    lval_del(k);
    lval_del(v);
}
void lenv_add_builtins(lenv* e){
    /* list functions */
    lenv_add_builtin(e,"list",builtin_list);
    lenv_add_builtin(e,"head",builtin_head);
    lenv_add_builtin(e,"tail",builtin_tail);
    lenv_add_builtin(e,"eval",builtin_eval);
    lenv_add_builtin(e,"join",builtin_join);
    /* mathematical functions */
    lenv_add_builtin(e,"+",builtin_add);
    lenv_add_builtin(e,"-",builtin_sub);
    lenv_add_builtin(e,"*",builtin_mul);
    lenv_add_builtin(e,"/",builtin_div);
    /* variable functions */
    lenv_add_builtin(e,"def",builtin_def);
    lenv_add_builtin(e,"=",builtin_put);
    lenv_add_builtin(e,"\\",builtin_lambda);
}

int main(int argc, char **argv)
{
    /* create some parsers */
    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Symbol = mpc_new("symbol");
    mpc_parser_t* Sexpr = mpc_new("sexpr");
    mpc_parser_t* Qexpr = mpc_new("qexpr");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Lispy = mpc_new("lispy");


    mpca_lang(MPCA_LANG_DEFAULT,
    "                                                           \
        number:     /-?[0-9]+/ ;                                \
        symbol:     /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ;           \
        sexpr:      '(' <expr>* ')' ;                           \
        qexpr:      '{' <expr>* '}';                            \
        expr:       <number> | <symbol> | <sexpr> | <qexpr>;    \
        lispy:      /^/ <expr>* /$/ ;                           \
    ",
    Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
    puts("Lispy Version 0.0.1");
    puts("Press Ctrl+c to Exit\n");

    lenv* e=lenv_new();
    lenv_add_builtins(e);
    while(1){
        char *input = readline("lispy> ");
        add_history(input);

        /* attempt to parse the user input */
        mpc_result_t r;
        if(mpc_parse("<stdin>",input,Lispy,&r)){
            lval* x=lval_eval(e,lval_read(r.output));
            lval_println(x);
            lval_del(x);
        }else{
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }
        
        free(input);
    }
    lenv_del(e);
    mpc_cleanup(6,Number,Symbol,Sexpr,Qexpr,Expr,Lispy);

    return 0;
}