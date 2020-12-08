/* ks.c - kscript interpreter, commandline interface
 *
 * @author: Cade Brown <cade@kscript.org>
 */
#include <ks/impl.h>
#include <ks/compiler.h>
#include <ks/getarg.h>


/* Compile and run generically */
static kso do_gen(ks_str fname, ks_str src) {
    /* Turn the input code into a list of tokens */
    ks_tok* toks = NULL;
    ks_ssize_t n_toks = ks_lex(fname, src, &toks);
    if (n_toks < 0) {
        ks_free(toks);
        return NULL;
    }

    /* Parse the tokens into an AST */
    ks_ast prog = ks_parse_prog(fname, src, n_toks, toks);
    ks_free(toks);
    if (!prog) {
        return NULL;
    }

    /* Compile the AST into a bytecode object which can be executed */
    ks_code code = ks_compile(fname, src, prog, NULL);
    KS_DECREF(prog);
    if (!code) {
        return NULL;
    }

    /*

    ks_str t = ks_str_new(-1, "dis");
    kso x = kso_getattr((kso)code, t);
    KS_DECREF(t);
    if (x) {
        kso y = kso_call(x, 0, NULL);
        KS_DECREF(x);
        ks_printf("DIS: \n%S\n", y);
        KS_DECREF(y);
    } else {
        kso_catch_ignore_print();
    }

    */

    /* Execute the program, which should return the value */
    kso res = kso_call((kso)code, 0, NULL);
    KS_DECREF(code);
    if (!res) return NULL;

    return res;
}

/* Do expression with '-e' */
static bool do_e(ks_str src) {
    ks_str fname = ks_fmt("<expr>");

    kso res = do_gen(fname, src);
    KS_DECREF(fname);
    if (!res) return NULL;

    KS_DECREF(res);
    return true;
}


/* Do file */
static bool do_f(ks_str fname) {
    ks_str src = ksio_readall(fname);
    if (!src) return false;

    kso res = do_gen(fname, src);
    KS_DECREF(src);
    if (!res) return false;

    KS_DECREF(res);
    return true;
}


int main(int argc, char** argv) {
    if (!ks_init()) return 1;
    int i;

    /* Initialize 'os.argv' */
    ks_list_clear(ksos_argv);
    for (i = 0; i < argc; ++i) {
        ks_list_pushu(ksos_argv, (kso)ks_str_new(-1, argv[i]));
    }

    ksga_Parser p = ksga_Parser_new("ks", "kscript interpreter, commandline interface", "0.0.1", "Cade Brown <cade@kscript.org>");

    ksga_opt(p, "expr", "Compiles and runs an expression", "-e,--expr", NULL, KSO_NONE);
    ksga_opt(p, "code", "Compiles and runs code", "-c,--code", NULL, KSO_NONE);
    ksga_pos(p, "args", "File to run and arguments given to it", NULL, -1);

    ks_dict args = ksga_parse(p, ksos_argv);
    kso_exit_if_err();

    /* Get arguments */
    kso expr = ks_dict_get_c(args, "expr"), code = ks_dict_get_c(args, "code");
    ks_list newargv = (ks_list)ks_dict_get_c(args, "args");
    kso_exit_if_err();

    /* Reclaim 'os.argv' */
    ks_list_clear(ksos_argv);
    for (i = 0; i < newargv->len; ++i) {
        ks_list_pushu(ksos_argv, newargv->elems[i]);
    }
    if (ksos_argv->len == 0) {
        ks_list_pushu(ksos_argv, (kso)ks_str_new(-1, "-"));
    }

    KS_DECREF(p);
    bool res = false;

    if (expr == KSO_NONE && code == KSO_NONE) {
        /* Run file */
        ks_str fname = (ks_str)ksos_argv->elems[0];
        if (fname->data[0] == '-') {
            /* Interactive session */
            res = ks_inter();
        } else {
            /* Execute file */
            res = do_f(fname);
        }
    } else if (expr != KSO_NONE && code == KSO_NONE) {
        ks_list_insertu(ksos_argv, 0, (kso)ks_str_new(-1, "<expr>"));
        res = do_e((ks_str)expr);
    } else if (expr == KSO_NONE && code != KSO_NONE) {
        ks_list_insertu(ksos_argv, 0, (kso)ks_str_new(-1, "<code>"));
        res = do_e((ks_str)code);
    } else {
        KS_THROW(kst_Error, "Given both '-e' and '-c'");
    }


    KS_DECREF(expr);
    KS_DECREF(code);
    KS_DECREF(newargv);
    KS_DECREF(args);
    kso_exit_if_err();

    return 0;
}
