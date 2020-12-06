/* vm.c - kscript virtual machine
 *
 * The implementation is an AST visiter, which visits node in a depth-first traversal, where expressions
 *   yield values on the implicit stack (i.e. when being ran they will be the last items on the thread's program
 *   stack). So, it turns an AST into a postfix machine code which can be efficiently computed
 * 
 * Constant expressions just push their value on the stack, and their parents know that they will be the last on the stack.
 * 
 * When the bytecode for nested expressions executes, it should result in a net gain of a single object being left on the stack,
 *   no matter how complicated. Therefore, binary operators can simply compile their children and then assume the top two items
 *   on the stack are what is left over
 * 
 * 
 * 
 * 
 * @author: Cade Brown <cade@kscript.org>
 */
#include <ks/impl.h>
#include <ks/compiler.h>

/* VM - Virtual Machine
 *
 * A Virtual Machine (or VM for short) is an abstracted machine which can understand computations
 *   described in some language (in our case, bytecode, or BC/bc).
 * 
 * The full specification of the kscript bytecode format is defined in `compiler.h`, with the operations
 *   being `KSB_*` enumeration values. Some take just the op code (1 byte), and other take the op code
 *   and a signed integer value (5 bytes total, 1 byte opcode + 4 byte argument)
 * 
 * For actually implementing it, we have bytes in an array that are tightly packed. Then, we start at
 *   position 0, and iterate through, moving along and 'consuming' the bytecode as we go. However, some
 *   instructions may cause a jump backwards or forwards, so it is not always linear in that regard
 *   (and a bytecode with 100 bytes may execute thousands of isntructions). But the code is stored linear in memory,
 *   which is more efficient than an AST traversal, for example
 * 
 * The method used in the control loop is either a switch/case (default), or a computed goto (^0). The former
 *   is less error prone, allows for (some) error checking for malformed bytecode, but is not as (theoretically)
 *   fast as using computed goto. I intend to (once this code is a bit more mature) benchmark the results of
 *   each method and see which is faster
 * 
 * 
 * Possible optimizations:
 *   - Include list operations in this file, so to inline and optimize for specific cases
 * 
 * 
 * References:
 *   ^0: https://eli.thegreenplace.net/2012/07/12/computed-goto-for-efficient-dispatch-tables
 * 
 * @author: Cade Brown <cade@kscript.org>
 */


/** Utilities **/

/* Temporary unlock and relock the GIL so other threads can use it 
 * TODO: implement smarter switching
 */
#define VM_ALLOW_GIL() do { \
    KS_GIL_UNLOCK(); \
    KS_GIL_LOCK(); \
} while(0)

/* Dispatch/Execution (VMD==Virtual Machine Dispatch) */

/* Starts the VMD */
#define VMD_START while (true) switch (*pc)

/* Catches unknown instruction */
#define VMD_CATCH_REST default: fprintf(stderr, "[VM]: Unknown instruction encountered in <bytecode @ %p>: %i (offset: %i)\n", bc, *pc, (int)(pc - bc->bc->data)); assert(false); break;

/* Consume the next instruction 
 * TODO: switch based on instructions or time
 */
#define VMD_NEXT() VM_ALLOW_GIL(); continue;

/* Declare code for a given operator */
#define VMD_OP(_op) case _op: pc += sizeof(ksb);

/* Declare code for a given operator, which takes an argument */
#define VMD_OPA(_op) case _op: arg = ((ksba*)pc)->arg; pc += sizeof(ksba);

/* End the section for an operator */
#define VMD_OP_END  VMD_NEXT(); break;


/* Execute on the current thread and return the result returned, or NULL if
 *   an exception was thrown.
 * 
 * This method does not add anything to the thread's stack frames (that should be
 *   done in the caller, for example in 'kso_call_ext()')
 */
kso _ks_exec(ks_code bc) {
    /* Thread we are executing on */
    ksos_thread th = ksos_thread_get();
    assert(th && th->frames->len > 0);

    /* Frame being executed on */
    ksos_frame frame = (ksos_frame)th->frames->elems[th->frames->len - 1];

    /* Program counter (instruction pointer) */
    #define pc (frame->pc)
    pc = bc->bc->data;

    /* Program stack (value stack) */
    ks_list stk = th->stk;
    int ssl = stk->len;

    /* Get a value from the value cache/constant array */
    #define VC(_idx) (bc->vc->elems[_idx])

    /* Temporaries */
    ks_str name;
    kso L, R, V;

    /* Argument, if the instruction gave one */
    int arg;

    /* Return result */
    kso res = NULL;

    /* Arguments for functions (they need local storage in case something modifies the main stack) */
    int n_args = 0;
    kso* args = NULL;

    /* Allocate and ensure arguments */
    #define ENSURE_ARGS(_n_args) do { \
        n_args = (_n_args); \
        args = ks_zrealloc(args, sizeof(*args), n_args); \
    } while (0)

    /* Pop off the last '_n' arguments from the stack */
    #define ARGS_FROM_STK(_num) do { \
        int _n = (_num); \
        assert(stk->len >= _n); \
        ENSURE_ARGS(_n); \
        int _i; \
        stk->len -= _n; /* Shift off, and copy arguments from end (absorbing references) */\
        for (_i = 0; _i < _n; ++_i) { \
            args[_i] = stk->elems[stk->len + _i]; \
        } \
    } while (0)


    /* 'DECREF' arguments */
    #define DECREF_ARGS(_num) do { \
        int _i, _n = (_num); \
        for (_i = 0; _i < _n; ++_i) { \
            KS_DECREF(args[_i]); \
        } \
    } while (0)

    VMD_START {
        VMD_OP(KSB_NOOP)
        VMD_OP_END

        VMD_OPA(KSB_PUSH)
            ks_list_push(stk, VC(arg));
        VMD_OP_END
        
        VMD_OP(KSB_POPU)
            KS_DECREF(stk->elems[--stk->len]);
        VMD_OP_END
        
        VMD_OPA(KSB_LOAD)
            ks_str name = (ks_str)VC(arg);
            assert(name->type == kst_str);
            V = ks_dict_get_h(ksg_globals, (kso)name, name->v_hash);
            if (!V) goto thrown;
            ks_list_push(stk, V);
            KS_DECREF(V);
        VMD_OP_END

        
        VMD_OPA(KSB_CALL)
            assert(arg >= 1);
            ARGS_FROM_STK(arg);
            V = kso_call(args[0], n_args - 1, args + 1);
            DECREF_ARGS(arg);
            if (!V) goto thrown;
            ks_list_pushu(stk, V);

        VMD_OP_END

        
        VMD_OP(KSB_RET)
            res = ks_list_pop(stk);
            goto done;
        VMD_OP_END

        /* Template for binary operators */
        #define T_BOP(_b, _name) VMD_OP(_b) \
            R = ks_list_pop(stk); \
            L = ks_list_pop(stk); \
            V = ks_bop_##_name(L, R); \
            KS_DECREF(L); KS_DECREF(R); \
            if (!V) goto thrown; \
            ks_list_pushu(stk, V); \
        VMD_OP_END
        
        /* Binary operators */
        T_BOP(KSB_BOP_ADD, add)
        T_BOP(KSB_BOP_SUB, sub)
        T_BOP(KSB_BOP_MUL, mul)
        T_BOP(KSB_BOP_DIV, div)
        T_BOP(KSB_BOP_FLOORDIV, floordiv)
        T_BOP(KSB_BOP_MOD, mod)
        T_BOP(KSB_BOP_POW, pow)
        T_BOP(KSB_BOP_IOR, binior)
        T_BOP(KSB_BOP_AND, binand)
        T_BOP(KSB_BOP_XOR, binxor)
        T_BOP(KSB_BOP_LSH, lsh)
        T_BOP(KSB_BOP_RSH, rsh)
        T_BOP(KSB_BOP_LT, lt)
        T_BOP(KSB_BOP_LE, le)
        T_BOP(KSB_BOP_GT, gt)
        T_BOP(KSB_BOP_GE, ge)

        /* Template for unary operators */
        #define T_UOP(_b, _name) VMD_OP(_b) \
            L = ks_list_pop(stk); \
            V = ks_uop_##_name(L); \
            KS_DECREF(L); \
            if (!V) goto thrown; \
            ks_list_pushu(stk, V); \
        VMD_OP_END

        T_UOP(KSB_UOP_POS, pos)
        T_UOP(KSB_UOP_NEG, neg)
        T_UOP(KSB_UOP_SQIG, sqig)


        /* Error on unknown */
        VMD_CATCH_REST
    }


    thrown:;
    /* Exception was thrown */

    done:;
    /* Clean up and return */

    /* Remove extre values */
    while (stk->len > ssl) {
        KS_DECREF(stk->elems[--stk->len]);
    }

    /* Free temporary arguments */
    ks_free(args);


    return res;

    #undef stk
}


