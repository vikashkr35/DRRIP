#include<string>
#include<fstream>
#include "cache_set_drrip.h"
#include "simulator.h"
#include "config.hpp"
#include "log.h"
#include "cache.h"
#define BRRIP_MAX 32
#define Epoch_size 100000
UInt8 brrip_counter=0;
UInt16 policy[30000];
// DRRIP 

CacheSetDRRIP::CacheSetDRRIP(
      String cfgname, core_id_t core_id,
      CacheBase::cache_t cache_type,
      UInt32 associativity, UInt32 blocksize, CacheSetInfoLRU* set_info, UInt8 num_attempts)
   : CacheSet(cache_type, associativity, blocksize)
   , m_rrip_numbits(Sim()->getCfg()->getIntArray(cfgname+"/drrip/bits",core_id))
   , m_rrip_max((1 << m_rrip_numbits) - 1)
   , m_rrip_insert(m_rrip_max - 1)
   , m_num_attempts(num_attempts)
   , m_replacement_pointer(0)
   , m_set_info(set_info)
{
   m_rrip_bits = new UInt8[m_associativity];		
   for (UInt32 i = 0; i < m_associativity; i++){
      m_rrip_bits[i] = m_rrip_insert;
   }

}

CacheSetDRRIP::~CacheSetDRRIP()
{
   delete [] m_rrip_bits;
}

// This function returns victim index
UInt32
CacheSetDRRIP::getReplacementIndex(CacheCntlr *cntlr)
{   
   if(cntlr->isLastLevel())
   {
        UInt64 Num_Acc=cntlr->getNumaccess();
        
              //printf("num_acc:%d\n",Num_Acc);
        if ((Num_Acc-(Epoch_size*m_glob_epoch_ctr)) >= Epoch_size)  // epoch finished??
        {
		      //printf("epoch num:%d___srrip:%d___srriplin:%d",m_glob_epoch_ctr,m_glob_srrip_miss_ctr,m_glob_srriplin_miss_ctr);
        	   printf("num_acc:%d\n",Num_Acc);
            printf("epoch:%d\n",m_glob_epoch_ctr);
            printf("brrip_miss:%d\n",m_glob_brrip_miss_ctr);
            printf("srrip_miss:%d\n",m_glob_srrip_miss_ctr);
        	   m_glob_epoch_ctr++;		// increasing epoch
        	   
        	   if(m_glob_brrip_miss_ctr > m_glob_srrip_miss_ctr && m_glob_policy_flag < 1)
        	   {
        		   m_glob_policy_flag++;	// winner is srrip
        	   }
		      else if(m_glob_brrip_miss_ctr <= m_glob_srrip_miss_ctr && m_glob_policy_flag > 0)
		      {
			      m_glob_policy_flag--;	// winner is brrip
            }
        	
            policy[m_glob_epoch_ctr]=  m_glob_policy_flag;
            

            printf("____winner policy:%d\n",m_glob_policy_flag);
     
            //reseting counters
            m_glob_srrip_miss_ctr = 0;
            m_glob_brrip_miss_ctr = 0;        
        }
   }
   
   int leader = check_leader(m_glob_set_id); //  BRRIP(leader == 0) , BRRIP

    // Part 1

   for (UInt32 i = 0; i < m_associativity; i++)
   {
      if (!m_cache_block_info_array[i]->isValid())
      {
         if (leader == -1){     //follower set
             if(m_glob_policy_flag==0){      //follow BRRIP
                  m_rrip_bits[i] = m_rrip_max;     
         
                  brrip_counter++;                              //every 32nd insert has RRIP long else distant 
                  if (brrip_counter == BRRIP_MAX)
                  brrip_counter = 0;
                  if (brrip_counter == 0)
                  m_rrip_bits[i] = m_rrip_insert;
                  return i;
            }else if(m_glob_policy_flag==1){          //follow SRRIP
                  m_rrip_bits[i] = m_rrip_insert;
                  return i;
                 }
         } 
         else if(leader ==0){   //leader BRRIP
                  m_glob_brrip_miss_ctr++;

                  m_rrip_bits[i] = m_rrip_max;     
         
                  brrip_counter++;                              //every 32nd insert has RRIP long else distant 
                  if (brrip_counter == BRRIP_MAX)
                  brrip_counter = 0;
                  if (brrip_counter == 0)
                  m_rrip_bits[i] = m_rrip_insert;
                  return i; 
         } 
         else if (leader ==1){    //leader SRRIP
                  m_glob_srrip_miss_ctr++;
                  m_rrip_bits[i] = m_rrip_insert;
                  return i;
              }
                
      }
   }   
 
   //Part 2  

   UInt8 attempt = 0;

   for(UInt32 j = 0; j <= m_rrip_max; ++j)
   {
      for (UInt32 i = 0; i < m_associativity; i++)
      {
         if (m_rrip_bits[m_replacement_pointer] >= m_rrip_max)
         {
            // We choose the first non-touched line as the victim (note that we start searching from the replacement pointer position)
            UInt8 index = m_replacement_pointer;

            bool qbs_reject = false;
            if (attempt < m_num_attempts - 1)
            {
               LOG_ASSERT_ERROR(cntlr != NULL, "CacheCntlr == NULL, QBS can only be used when cntlr is passed in");
               qbs_reject = cntlr->isInLowerLevelCache(m_cache_block_info_array[index]);
            }

            if (qbs_reject)
            {
               // Block is contained in lower-level cache, and we have more tries remaining.
               // Move this block to MRU and try again
               m_rrip_bits[index] = 0;
               cntlr->incrementQBSLookupCost();
               ++attempt;
               continue;
            }

            m_replacement_pointer = (m_replacement_pointer + 1) % m_associativity;
            // Prepare way for a new line:
             if (leader == -1){     //follower set
             if(m_glob_policy_flag==0){      //follow BRRIP
                  m_rrip_bits[index] = m_rrip_max;     
         
                  brrip_counter++;                              //every 32nd insert has RRIP long else distant 
                  if (brrip_counter == BRRIP_MAX)
                  brrip_counter = 0;
                  if (brrip_counter == 0)
                  m_rrip_bits[index] = m_rrip_insert;
            }else if(m_glob_policy_flag==1){          //follow SRRIP
                  m_rrip_bits[index] = m_rrip_insert;
                 }
         } 
         else if(leader ==0){   //leader BRRIP
                  m_glob_brrip_miss_ctr++;

                  m_rrip_bits[index] = m_rrip_max;     
         
                  brrip_counter++;                              //every 32nd insert has RRIP long else distant 
                  if (brrip_counter == BRRIP_MAX)
                  brrip_counter = 0;
                  if (brrip_counter == 0)
                  m_rrip_bits[index] = m_rrip_insert;
         } 
         else if (leader ==1){    //leader SRRIP
                  m_glob_srrip_miss_ctr++;
                  m_rrip_bits[index] = m_rrip_insert;
         }



            m_set_info->incrementAttempt(attempt);

            LOG_ASSERT_ERROR(isValidReplacement(index), "DRRIP selected an invalid replacement candidate" );
            return index;
         }

         m_replacement_pointer = (m_replacement_pointer + 1) % m_associativity;
      }

      // Increment all RRIP counters until one hits RRIP_MAX
      for (UInt32 i = 0; i < m_associativity; i++)
      {
         if (m_rrip_bits[i] < m_rrip_max)
         {
            m_rrip_bits[i]++;
         }
      }
   }

   LOG_PRINT_ERROR("Error finding replacement index");
}


void
CacheSetDRRIP::updateReplacementIndex(UInt32 accessed_index)
{
   m_set_info->increment(m_rrip_bits[accessed_index]);

   if (m_rrip_bits[accessed_index] > 0)
	      m_rrip_bits[accessed_index]=0;						   
}

int
CacheSetDRRIP::check_leader(UInt32 set_id)
{
    if ((set_id-(set_id/64))%64==0)   
      return 0;
    else if((set_id+1+(set_id/64))%64==0)
      return 1;
   return -1;
}
