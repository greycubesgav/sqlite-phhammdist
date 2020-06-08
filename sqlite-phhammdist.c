/* compile osx 
 * gcc -bundle -fPIC -O3 -o popcount.dylib popcount.c
 * */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sqlite3ext.h>

SQLITE_EXTENSION_INIT1

int ph_hamming_distance(const uint64_t hash1,const uint64_t hash2){
    uint64_t x = hash1^hash2;
    const uint64_t m1  = 0x5555555555555555ULL;
    const uint64_t m2  = 0x3333333333333333ULL;
    const uint64_t h01 = 0x0101010101010101ULL;
    const uint64_t m4  = 0x0f0f0f0f0f0f0f0fULL;
    x -= (x >> 1) & m1;
    x = (x & m2) + ((x >> 2) & m2);
    x = (x + (x >> 4)) & m4;
    return (x * h01)>>56;
}

static void phhammdist(sqlite3_context *context, int argc, sqlite3_value **argv){
   
    if (argc != 2) {
        sqlite3_result_error(context, "phhammdist: requires 2 TEXT arguments", -1);
        return;
    }

    int datatype_arg1 = sqlite3_value_type(argv[0]);
    int datatype_arg2 = sqlite3_value_type(argv[1]);
    
    //fprintf( stderr, "datatype_arg1: %d datatype_arg2:%d\n", datatype_arg1, datatype_arg2);

    if ((datatype_arg1 != SQLITE3_TEXT || datatype_arg2 != SQLITE3_TEXT)) {
        sqlite3_result_error(context, "ph_hamming_distance: both arguments need to be of type STRING", -1);
        return;
    }

    const char *phBlob1 = sqlite3_value_blob(argv[0]);
    const char *phBlob2 = sqlite3_value_blob(argv[1]);
    char *ptr = NULL;

    uint64_t ph1 = strtoul(phBlob1, &ptr, 10);
    uint64_t ph2 = strtoul(phBlob2, &ptr, 10);
    
    //fprintf( stderr, "ph1: %llu, ph2: %llu\n", ph1, ph2);

    uint64_t p_ham_distance = ph_hamming_distance(ph1,ph2);
    sqlite3_result_int(context, p_ham_distance);
}

int init_sqlite(sqlite3 *db){
    return sqlite3_create_function(db, "phhammdist", -1, SQLITE_ANY, 0, phhammdist, 0, 0);
}

#ifdef _WIN32
__declspec(dllexport)
#endif

int sqlite3_phhammdist_init(sqlite3 *db, char **pzErrMsg, const sqlite3_api_routines *pApi){
  int rc = SQLITE_OK;
  SQLITE_EXTENSION_INIT2(pApi);
  return init_sqlite(db);
}



