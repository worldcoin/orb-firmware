/**
  ******************************************************************************
  * @file    kms_mem.c
  * @author  MCD Application Team
  * @brief   This file contains utilities for the manipulation of memory
  *          of Key Management Services (KMS)
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2019 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file in
  * the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "kms.h"                /* PKCS11 definitions */
#if defined(KMS_ENABLED)
#include "kms_init.h"           /* KMS session services */
#include "kms_mem.h"            /* KMS memory utilities */
#include "kms_low_level.h"      /* KMS reporting APIs */
#if defined(KMS_MEM_USE_HEAP_ALLOCATOR)
#include <stdlib.h>
#endif /* KMS_MEM_USE_HEAP_ALLOCATOR */

#if defined(KMS_MEM_USE_POOL_ALLOCATOR)
/* Pool definition inclusions ----------------------------------------------- */

/**
  * @brief Pool header
  */
typedef struct
{
  uint32_t    canaries[2];          /*!< Canaries to check corruption */
  uint32_t    size;                 /*!< Pool size */
  uint32_t    used;                 /*!< Pool in use flag */
#if defined(KMS_MEM_DEBUGGING)
  uint32_t    session;              /*!< Session that requested allocation */
  uint32_t    caller;               /*!< Caller address */
  uint32_t    reqSize;              /*!< Requested size */
  uint32_t    reserved;             /*!< empty */
#endif /* KMS_MEM_DEBUGGING */
} kms_mem_pool_header_t;

/**
  * @brief Pool normalized header
  * @note  Contains only canary part of the header
  */
static const uint32_t normalizedHeader[2] = {0x54762fd6UL,
                                             0x6aeef1d2UL
                                            };

/**
  * @brief Pool footer
  */
typedef struct
{
  uint32_t    canaries[4];      /*!< Canaries to check corruption */
} kms_mem_pool_footer_t;

/**
  * @brief Pool normalized header
  * @note  Contains only canary part of the footer
  */
static const kms_mem_pool_footer_t normalizedFooter = {{
    0x6aeef1d2UL,
    0x8ae1c029UL,
    0xdced746eUL,
    0x5411254fUL
  }
};


/*
 * First inclusion to declare structure
 */
#define KMS_MEM_DECLARE_POOL_START() \
  typedef struct                     \
  {

/*
 * Each pool element is constituated as follow:
 * - a header (see kms_mem_pool_header_t)
 * - the memory pool build on a uint8_t buffer
 * - an alignment segment whose size is between 1 and 4 depending on the pool length value
 * - a footer  (see kms_mem_pool_footer_t)
 */
#define KMS_MEM_DECLARE_POOL_ENTRY(ID,SIZE)                       \
  kms_mem_pool_header_t head_##ID;                                \
  uint8_t               pool_##ID[SIZE];                          \
  uint8_t               align_##ID[4UL - ((SIZE)&0x3UL)];         \
  kms_mem_pool_footer_t foot_##ID;


#define KMS_MEM_DECLARE_POOL_END() \
  } kms_mem_pool_t;

#include "kms_mem_pool_def.h"

#undef KMS_MEM_DECLARE_POOL_START
#undef KMS_MEM_DECLARE_POOL_ENTRY
#undef KMS_MEM_DECLARE_POOL_END

/*
 * Second inclusion to instantiate structure, init is done at runtime
 */
#define KMS_MEM_DECLARE_POOL_START() \
  kms_mem_pool_t kms_mem_pool __attribute__((aligned(4)));

#define KMS_MEM_DECLARE_POOL_ENTRY(ID,SIZE)   ;

#define KMS_MEM_DECLARE_POOL_END()            ;

#include "kms_mem_pool_def.h"

#undef KMS_MEM_DECLARE_POOL_START
#undef KMS_MEM_DECLARE_POOL_ENTRY
#undef KMS_MEM_DECLARE_POOL_END

/*
 * Third inclusion to instantiate pool pointers
 */
#define KMS_MEM_DECLARE_POOL_START()                   \
  kms_mem_pool_header_t * const kms_mem_pool_tab[] = {

#define KMS_MEM_DECLARE_POOL_ENTRY(ID,SIZE) \
  &(kms_mem_pool.head_##ID),

#define KMS_MEM_DECLARE_POOL_END() \
  };

#include "kms_mem_pool_def.h"

#undef KMS_MEM_DECLARE_POOL_START
#undef KMS_MEM_DECLARE_POOL_ENTRY
#undef KMS_MEM_DECLARE_POOL_END

#endif /* KMS_MEM_USE_POOL_ALLOCATOR */

/** @addtogroup Key_Management_Services Key Management Services (KMS)
  * @{
  */

/** @addtogroup KMS_MEM Memory utilities
  * @{
  */

/* Private types -------------------------------------------------------------*/
/** @addtogroup KMS_MEM_Private_Types Private Types
  * @{
  */
#if defined(KMS_MEM_USE_HEAP_ALLOCATOR) && defined(KMS_MEM_DEBUGGING)
/**
  * @brief  HEAP allocator descriptor structure
  */
typedef struct
{
  CK_SESSION_HANDLE   session;  /*!< Session that requested allocation */
  size_t              size;     /*!< Size allocated */
  uint32_t            caller;   /*!< Caller address */
  void               *pMem;     /*!< Allocated memory pointer */
} kms_mem_heap_alloc_t;

#if !defined(KMS_MEM_MAX_ALLOCATION)
#define KMS_MEM_MAX_ALLOCATION (10U)    /*!< Max number of HEAP allocation that are stored */
#endif /* KMS_MEM_MAX_ALLOCATION */
/**
  * @brief  HEAP allocator management structure
  */
typedef struct
{
  kms_mem_heap_alloc_t  pool[KMS_MEM_MAX_ALLOCATION];   /*!< Allocations table */
  uint32_t              allocs;                         /*!< Allocations counter */
} kms_mem_mgt_t;
#endif /* KMS_MEM_USE_HEAP_ALLOCATOR && KMS_MEM_DEBUGGING */
/**
  * @}
  */
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/** @addtogroup KMS_MEM_Private_Variables Private Variables
  * @{
  */
#if defined(KMS_MEM_USE_HEAP_ALLOCATOR) && defined(KMS_MEM_DEBUGGING)
kms_mem_mgt_t kms_mem_heap_manager;     /*!< HEAP allocator manager variable */
#endif /* KMS_MEM_USE_HEAP_ALLOCATOR && KMS_MEM_DEBUGGING */
/**
  * @}
  */

/* Private function prototypes -----------------------------------------------*/
#if defined(KMS_MEM_USE_POOL_ALLOCATOR)
static void mempool_init(kms_mem_pool_header_t *pHead, uint32_t size);
#endif /* KMS_MEM_USE_POOL_ALLOCATOR */
/* Private function ----------------------------------------------------------*/
#if defined(KMS_MEM_USE_POOL_ALLOCATOR)
/** @addtogroup KMS_MEM_Private_Functions Private Functions
  * @{
  */
/**
  * @brief  Memory pool initialization
  * @retval None
  */
static void mempool_init(kms_mem_pool_header_t *pHead, uint32_t size)
{
  uint8_t *ptr;
  kms_mem_pool_footer_t *p_foot;

  ptr = (uint8_t *)(((uint32_t)pHead) + sizeof(kms_mem_pool_header_t));
  p_foot = (kms_mem_pool_footer_t *)(((uint32_t)pHead) + sizeof(kms_mem_pool_header_t) + size + (4UL - (size & 0x3UL)));
  /* Initialize header with canaries and controls */
  for (uint32_t i = 0; i < (sizeof(pHead->canaries) / sizeof(pHead->canaries[0])); i++)
  {
    pHead->canaries[i] = normalizedHeader[i];
  }
  pHead->size = size;
  pHead->used = 0;
#if defined(KMS_MEM_DEBUGGING)
  pHead->session = KMS_SESSION_ID_INVALID;
  pHead->caller = 0;
  pHead->reqSize = 0;
  pHead->reserved = 0;
#endif /* KMS_MEM_DEBUGGING */
  (void)memset(ptr, 0, size);   /* Initialize pool buffer contents to 0 */
  /* Initialize footer with canaries */
  for (uint32_t i = 0; i < (sizeof(p_foot->canaries) / sizeof(p_foot->canaries[0])); i++)
  {
    p_foot->canaries[i] = normalizedFooter.canaries[i];
  }
}
/**
  * @}
  */
#endif /* KMS_MEM_USE_POOL_ALLOCATOR */

/* Exported variables --------------------------------------------------------*/
/* Exported functions --------------------------------------------------------*/

/** @addtogroup KMS_MEM_Exported_Functions Exported Functions
  * @{
  */

/**
  * @brief  Initialize memory management structure
  * @retval None
  */
#if !defined(KMS_MEM_USE_CUSTOM_ALLOCATOR)
void KMS_MemInit(void)
{
#if defined(KMS_MEM_USE_HEAP_ALLOCATOR) && defined(KMS_MEM_DEBUGGING)
  for (uint32_t i = 0; i < KMS_MEM_MAX_ALLOCATION; i++)
  {
    kms_mem_heap_manager.pool[i].session = KMS_SESSION_ID_INVALID;
    kms_mem_heap_manager.pool[i].size = 0;
    kms_mem_heap_manager.pool[i].caller = 0;
    kms_mem_heap_manager.pool[i].pMem = NULL_PTR;
  }
  kms_mem_heap_manager.allocs = 0;
#endif /* KMS_MEM_USE_HEAP_ALLOCATOR && KMS_MEM_DEBUGGING */
#if defined(KMS_MEM_USE_POOL_ALLOCATOR)

#define KMS_MEM_DECLARE_POOL_START()      ;
#define KMS_MEM_DECLARE_POOL_ENTRY(ID,SIZE)    \
  mempool_init(&(kms_mem_pool.head_##ID), SIZE);
#define KMS_MEM_DECLARE_POOL_END()        ;
#include "kms_mem_pool_def.h"
#undef KMS_MEM_DECLARE_POOL_START
#undef KMS_MEM_DECLARE_POOL_ENTRY
#undef KMS_MEM_DECLARE_POOL_END

#endif /* KMS_MEM_USE_POOL_ALLOCATOR */
#if defined(KMS_MEM_LOGGING)
  KMS_LL_ReportMemInit();
#endif /* KMS_MEM_LOGGING */
}
#endif /* KMS_MEM_USE_CUSTOM_ALLOCATOR */

/**
  * @brief  Allocate memory
  * @param  Session Session requesting the memory
  * @param  Size Size of the memory to allocate
  * @retval Allocated pointer if successful to allocate, NULL_PTR if failed
  */
#if !defined(KMS_MEM_USE_CUSTOM_ALLOCATOR)
CK_VOID_PTR KMS_Alloc(CK_SESSION_HANDLE Session, size_t Size)
{
  void *ptr = NULL_PTR;
#if defined(KMS_MEM_DEBUGGING)
  uint32_t LR = KMS_LL_GetLR();
#else /* KMS_MEM_DEBUGGING */
  (void)Session;
#endif /* KMS_MEM_DEBUGGING */

#if defined(KMS_MEM_USE_HEAP_ALLOCATOR)
  ptr = malloc(Size);
#if defined(KMS_MEM_DEBUGGING)
  if (ptr != NULL_PTR)
  {
    for (uint32_t i = 0; i < KMS_MEM_MAX_ALLOCATION; i++)
    {
      if (kms_mem_heap_manager.pool[i].pMem == NULL_PTR)
      {
        kms_mem_heap_manager.pool[i].session = Session;
        kms_mem_heap_manager.pool[i].size = Size;
        kms_mem_heap_manager.pool[i].caller = LR;
        kms_mem_heap_manager.pool[i].pMem = ptr;
        kms_mem_heap_manager.allocs++;
        break;
      }
    }
  }
#endif /* KMS_MEM_DEBUGGING */
#endif /* KMS_MEM_USE_HEAP_ALLOCATOR */
#if defined(KMS_MEM_USE_POOL_ALLOCATOR)
  for (uint32_t i = 0; (i < (sizeof(kms_mem_pool_tab) / sizeof(kms_mem_pool_header_t *))) && (ptr == NULL_PTR); i++)
  {
    if ((kms_mem_pool_tab[i]->used == 0UL) && (kms_mem_pool_tab[i]->size >= Size))
    {
      kms_mem_pool_tab[i]->used = 1;
#if defined(KMS_MEM_DEBUGGING)
      kms_mem_pool_tab[i]->session = Session;
      kms_mem_pool_tab[i]->caller = LR;
      kms_mem_pool_tab[i]->reqSize = Size;
#endif /* KMS_MEM_DEBUGGING */
      ptr = (void *)(uint32_t *)((uint32_t)(kms_mem_pool_tab[i]) + sizeof(kms_mem_pool_header_t));
    }
  }
#endif /* KMS_MEM_USE_POOL_ALLOCATOR */
  if (ptr == NULL_PTR)
  {
    KMS_LL_ReportError(KMS_LL_ERROR_MEM_ALLOC_FAILURE);
  }
#if defined(KMS_MEM_LOGGING)
  else
  {
    KMS_LL_ReportMemAlloc(Size, ptr);
  }
#endif /* KMS_MEM_LOGGING */
  return ptr;
}
#endif /* KMS_MEM_USE_CUSTOM_ALLOCATOR */

/**
  * @brief  Free memory
  * @param  Session Session freeing the memory
  * @param  Ptr     Pointer to the memory to free
  * @retval None
  */
#if !defined(KMS_MEM_USE_CUSTOM_ALLOCATOR)
void KMS_Free(CK_SESSION_HANDLE Session, CK_VOID_PTR Ptr)
{
  uint32_t i;

  (void)Session;

  if (Ptr == NULL_PTR)
  {
    KMS_LL_ReportError(KMS_LL_ERROR_MEM_FREE_NULL_PTR);
  }
  else
  {
#if defined(KMS_MEM_LOGGING)
    KMS_LL_ReportMemFree(Ptr);
#endif /* KMS_MEM_LOGGING */
#if defined(KMS_MEM_USE_HEAP_ALLOCATOR)
#if defined(KMS_MEM_DEBUGGING)
    for (i = 0; i < KMS_MEM_MAX_ALLOCATION; i++)
    {
      if (kms_mem_heap_manager.pool[i].pMem == Ptr)
      {
#if defined(KMS_MEM_CLEANING)
        (void)memset(Ptr, 0, kms_mem_heap_manager.pool[i].size);
#endif /* KMS_MEM_CLEANING */
        kms_mem_heap_manager.pool[i].session = KMS_SESSION_ID_INVALID;
        kms_mem_heap_manager.pool[i].caller = 0;
        kms_mem_heap_manager.pool[i].size = 0;
        kms_mem_heap_manager.pool[i].pMem = NULL_PTR;
        kms_mem_heap_manager.allocs--;
        break;
      }
    }
    if (i >= KMS_MEM_MAX_ALLOCATION)
    {
      KMS_LL_ReportError(KMS_LL_ERROR_MEM_FREE_UNKNOWN);
    }
#endif /* KMS_MEM_DEBUGGING */
    free(Ptr);
#endif /* KMS_MEM_USE_HEAP_ALLOCATOR */
#if defined(KMS_MEM_USE_POOL_ALLOCATOR)
    kms_mem_pool_header_t *phead;
    kms_mem_pool_footer_t *pfoot;
    uint32_t tmp_ptr = (uint32_t)Ptr;

    phead = (kms_mem_pool_header_t *)(tmp_ptr - sizeof(kms_mem_pool_header_t));
    pfoot = (kms_mem_pool_footer_t *)(tmp_ptr + phead->size + 4UL - (phead->size & 0x3UL));
    /* Check canaries */
    if (memcmp((void *)phead, &normalizedHeader, sizeof(normalizedHeader)) != 0)
    {
      KMS_LL_ReportError(KMS_LL_ERROR_MEM_FREE_CANARY);
    }
    if (memcmp((void *)pfoot, &normalizedFooter, sizeof(normalizedFooter)) != 0)
    {
      KMS_LL_ReportError(KMS_LL_ERROR_MEM_FREE_CANARY);
    }
    for (i = 0; i < (sizeof(kms_mem_pool_tab) / sizeof(kms_mem_pool_header_t *)); i++)
    {
      if (phead == kms_mem_pool_tab[i])
      {
        phead->used = 0;
#if defined(KMS_MEM_DEBUGGING)
        phead->session = KMS_SESSION_ID_INVALID;
        phead->caller = 0;
        phead->reqSize = 0;
#endif /* KMS_MEM_DEBUGGING */
#if defined(KMS_MEM_CLEANING)
        (void)memset(Ptr, 0, phead->size);
#endif /* KMS_MEM_CLEANING */
        break;
      }
    }
    if (i >= (sizeof(kms_mem_pool_tab) / sizeof(kms_mem_pool_header_t *)))
    {
      KMS_LL_ReportError(KMS_LL_ERROR_MEM_FREE_UNKNOWN);
    }
#endif /* KMS_MEM_USE_POOL_ALLOCATOR */
  } /* (Ptr == NULL_PTR) */
}
#endif /* KMS_MEM_USE_CUSTOM_ALLOCATOR */

/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */

#endif /* KMS_ENABLED */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
