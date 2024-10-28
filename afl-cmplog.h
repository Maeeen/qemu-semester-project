#pragma once

#include "types.h"
#include "afl.h"
#include "disas.h"

// ± the same copy as https://github.com/AFLplusplus/qemuafl/blob/d40bcd896521e5a1f0c9939d020fe6291dbdd370/qemuafl/imported/cmplog.h#L41

#define CMPLOG_ENV "___AFL_EINS_ZWEI_POLIZEI___"
#define CMPLOG_SHM_ENV "__AFL_CMPLOG_SHM_ID"

#define CMP_MAP_W 65536
#define CMP_MAP_H 32
#define CMP_MAP_RTN_H (CMP_MAP_H / 2)


#define SHAPE_BYTES(x) (x + 1)

#define CMP_TYPE_INS 0
#define CMP_TYPE_RTN 1

// TODO: ideally include AFLplusplus
struct cmp_operands {
  u64 v0;
  u64 v0_128;
  u64 v0_256_0;  // u256 is unsupported by any compiler for now, so future use
  u64 v0_256_1;
  u64 v1;
  u64 v1_128;
  u64 v1_256_0;
  u64 v1_256_1;
  u8  unused[8];
} __attribute__((packed));

struct cmp_header {  // 16 bit = 2 bytes
  unsigned hits : 6;       // up to 63 entries, we have CMP_MAP_H = 32
  unsigned shape : 5;      // 31+1 bytes max
  unsigned type : 1;       // 2: cmp, rtn
  unsigned attribute : 4;  // 16 for arithmetic comparison types
} __attribute__((packed));


struct cmp_map {
  struct cmp_header   headers[CMP_MAP_W];
  struct cmp_operands log[CMP_MAP_W][CMP_MAP_H];
};

struct cmpfn_operands {
  u8 v0[32];
  u8 v0_len;
  u8 v1[32];
  u8 v1_len;
} __attribute__((packed));

typedef struct cmp_operands cmp_map_list[CMP_MAP_H];

int cmplog_init();
void cmplog_log(size_t location, u64 v0, u64 v1, u64 effective_bits);
int cmplog_shmem(void** mem, size_t* size);

struct cmplog_cb_data {
  struct disas_insn_operands ops;
  char mem_accesses; // the mem accesses that have been done
  u64 location;
  u64 v0_mem;
  u64 v1_mem;
};