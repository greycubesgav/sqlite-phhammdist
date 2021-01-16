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
        sqlite3_result_error(context, "phhammdist: requires 2 TEXT or 2 INTEGER arguments.", -1);
        return;
    }

    int datatype_arg1 = sqlite3_value_type(argv[0]);
    int datatype_arg2 = sqlite3_value_type(argv[1]);
    
    #ifdef DEBUG
      fprintf( stderr, "datatype_arg1: %d datatype_arg2:%d\n", datatype_arg1, datatype_arg2);
    #endif

    int64_t INT_ph1 = 0;
    int64_t INT_ph2 = 0;

    uint64_t UINT_ph1 = 0;
    uint64_t UINT_ph2 = 0;

    if (datatype_arg1 == SQLITE3_TEXT && datatype_arg2 == SQLITE3_TEXT) {
        const char *phBlob1 = sqlite3_value_blob(argv[0]);
        const char *phBlob2 = sqlite3_value_blob(argv[1]);
        char *ptr = NULL;

        UINT_ph1 = strtoul(phBlob1, &ptr, 10);
        UINT_ph2 = strtoul(phBlob2, &ptr, 10);
    } else if (datatype_arg1 == SQLITE_INTEGER && datatype_arg2 == SQLITE_INTEGER) {
        INT_ph1 = sqlite3_value_int64(argv[0]);
        INT_ph2 = sqlite3_value_int64(argv[1]);

        UINT_ph1 = (uint64_t) INT_ph1;
        UINT_ph2 = (uint64_t) INT_ph2;
        
        #ifdef DEBUG
          fprintf( stderr, "INT_ph1: %lld, INT_ph2: %lld\n", INT_ph1, INT_ph2);
          fprintf( stderr, "ph1: %llu, ph2: %llu\n", UINT_ph1, UINT_ph2);
        #endif
        //ph1 = sqlite3_value_int64(argv[0]);
        //ph2 = sqlite3_value_int64(argv[1]);
    
    } else {
        sqlite3_result_error(context, "phhammdist: both arguments need to be of type TEXT or both arguments need to be of type INTEGER.", -1);
        return;
    }
    #ifdef DEBUG
      fprintf( stderr, "INT_ph1: %lld, INT_ph2: %lld\n", INT_ph1, INT_ph2);
      fprintf( stderr, "UINT_ph1: %llu, UINT_ph2: %llu\n", UINT_ph1, UINT_ph2);    
    #endif

    uint64_t p_ham_distance = ph_hamming_distance(UINT_ph1,UINT_ph1);
    sqlite3_result_int(context, p_ham_distance);
}

int init_sqlite(sqlite3 *db){
    return sqlite3_create_function(db, "phhammdist", 2, SQLITE_ANY, 0, phhammdist, 0, 0);
}

#ifdef _WIN32
__declspec(dllexport)
#endif

int sqlite3_phhammdist_init(sqlite3 *db, char **pzErrMsg, const sqlite3_api_routines *pApi){
  int rc = SQLITE_OK;
  SQLITE_EXTENSION_INIT2(pApi);
  return init_sqlite(db);
}



