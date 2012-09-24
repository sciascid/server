/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#include "includes.h"
#include "test.h"
#include "cachetable-test.h"

bool v1_written;
uint64_t val1;
bool v2_written;
uint64_t val2;
uint64_t val3;
bool check_me;

static void
flush (CACHEFILE f __attribute__((__unused__)),
       int UU(fd),
       CACHEKEY k  __attribute__((__unused__)),
       void *v     __attribute__((__unused__)),
       void** UU(dd),
       void *e     __attribute__((__unused__)),
       PAIR_ATTR s      __attribute__((__unused__)),
       PAIR_ATTR* new_size      __attribute__((__unused__)),
       bool w      __attribute__((__unused__)),
       bool keep   __attribute__((__unused__)),
       bool c      __attribute__((__unused__)),
        bool UU(is_clone)
       ) {
    /* Do nothing */
    if (verbose) { printf("FLUSH: %d\n", (int)k.b); }
    //usleep (5*1024*1024);
    if(check_me) {
        assert(c);
        assert(keep);
        assert(w);
        if (v == &val1) {
            v1_written = true;
        }
        else if (v == &val2) {
            v2_written = true;
        }
        else {
            assert(false);
        }
    }
}

static int
fetch (CACHEFILE f        __attribute__((__unused__)),
       PAIR UU(p),
       int UU(fd),
       CACHEKEY k         __attribute__((__unused__)),
       uint32_t fullhash __attribute__((__unused__)),
       void **value       __attribute__((__unused__)),
       void** UU(dd),
       PAIR_ATTR *sizep        __attribute__((__unused__)),
       int  *dirtyp,
       void *extraargs    __attribute__((__unused__))
       ) {
  *dirtyp = 0;
  *value = extraargs;
  *sizep = make_pair_attr(8);
  return 0;
}

static void get_key_and_fullhash(CACHEKEY* cachekey, uint32_t* fullhash, void* extra) {
    assert(extra == NULL);
    cachekey->b = 3;
    *fullhash = 3;
}


static void
cachetable_test (bool write_first, bool write_second, bool start_checkpoint) {
    const int test_limit = 12;
    int r;
    CACHETABLE ct;
    toku_cachetable_create(&ct, test_limit, ZERO_LSN, NULL_LOGGER);
    char fname1[] = __SRCFILE__ "test1.dat";
    unlink(fname1);
    CACHEFILE f1;
    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);
    create_dummy_functions(f1);//toku_cachefile_set_userdata(ft, NULL, NULL, 

    void* v1;
    void* v2;
    long s1;
    long s2;
    CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);
    wc.flush_callback = flush;
    r = toku_cachetable_get_and_pin(f1, make_blocknum(1), 1, &v1, &s1, wc, fetch, def_pf_req_callback, def_pf_callback, true, &val1);
    assert(r==0);
    r = toku_cachetable_get_and_pin(f1, make_blocknum(2), 2, &v2, &s2, wc, fetch, def_pf_req_callback, def_pf_callback, true, &val2);
    assert(r==0);
    
    CACHEFILE dependent_cfs[2];
    dependent_cfs[0] = f1;
    dependent_cfs[1] = f1;
    CACHEKEY dependent_keys[2];
    dependent_keys[0] = make_blocknum(1);
    dependent_keys[1] = make_blocknum(2);
    uint32_t dependent_fullhash[2];
    dependent_fullhash[0] = 1;
    dependent_fullhash[1] = 2;
    // now we set the dirty state of these two.
    enum cachetable_dirty cd[2];
    cd[0] = write_first ? CACHETABLE_DIRTY : CACHETABLE_CLEAN;
    cd[1] = write_second ? CACHETABLE_DIRTY : CACHETABLE_CLEAN;
    CHECKPOINTER cp = toku_cachetable_get_checkpointer(ct);
    if (start_checkpoint) {
        //
        // should mark the v1 and v2 as pending
        //
        toku_cachetable_begin_checkpoint(cp, NULL);
    }
    //
    // This call should cause a flush for both
    //
    check_me = true;
    v1_written = false;
    v2_written = false;

    CACHEKEY put_key;
    uint32_t put_fullhash;
    toku_cachetable_put_with_dep_pairs(
        f1,
        get_key_and_fullhash,
        &val3,
        make_pair_attr(8),
        wc,
        NULL,
        2, //num_dependent_pairs
        dependent_cfs,
        dependent_keys,
        dependent_fullhash,
        cd,
        &put_key,
        &put_fullhash,
        put_callback_nop
        );
    assert(put_key.b == 3);
    assert(put_fullhash == 3);

    if (start_checkpoint) {
        assert(v1_written == write_first);
        assert(v2_written == write_second);
    }
    
    check_me = false;
    r = toku_test_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_CLEAN, make_pair_attr(8));
    r = toku_test_cachetable_unpin(f1, make_blocknum(2), 2, CACHETABLE_CLEAN, make_pair_attr(8));
    r = toku_test_cachetable_unpin(f1, make_blocknum(3), 3, CACHETABLE_CLEAN, make_pair_attr(8));

    if (start_checkpoint) {
        toku_cachetable_end_checkpoint(
            cp, 
            NULL, 
            NULL,
            NULL
            );
    }

    toku_cachetable_verify(ct);
    toku_cachefile_close(&f1, false, ZERO_LSN);
    toku_cachetable_close(&ct);


}

int
test_main(int argc, const char *argv[]) {
  default_parse_args(argc, argv);
  cachetable_test(false,false,true);
  cachetable_test(false,true,true);
  cachetable_test(true,false,true);
  cachetable_test(true,true,true);
  cachetable_test(false,false,false);
  cachetable_test(false,true,false);
  cachetable_test(true,false,false);
  cachetable_test(true,true,false);
  return 0;
}
