#ifndef CACHE_SET_BRRIP_H
#define CACHE_SET_BRRIP_H

#include "cache_set.h"
#include "cache_set_lru.h"

class CacheSetBRRIP : public CacheSet
{
   public:
      CacheSetBRRIP(String cfgname, core_id_t core_id,
            CacheBase::cache_t cache_type,
            UInt32 associativity, UInt32 blocksize, CacheSetInfoLRU* set_info, UInt8 num_attempts);
      ~CacheSetBRRIP();

      UInt32 getReplacementIndex(CacheCntlr *cntlr);
      void updateReplacementIndex(UInt32 accessed_index);

   private:
      const UInt8 m_rrip_numbits;
      const UInt8 m_rrip_max;
      const UInt8 m_rrip_insert;
      const UInt8 m_num_attempts;
      UInt8* m_rrip_bits;
      UInt8  m_replacement_pointer;
      CacheSetInfoLRU* m_set_info;
};

#endif /* CACHE_SET_H */
