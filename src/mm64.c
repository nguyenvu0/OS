#include "mm.h"
#include <stdlib.h>
#include <stdio.h>

#ifdef MM64

/* 
 * 5-level paging scheme 
 * PGD -> P4D -> PUD -> PMD -> PT -> Frame
 */

/* 
 * Macros for 64-bit paging
 */
#define PAGING64_PGD_INDEX(x) (((x) >> 48) & 0x1ff)
#define PAGING64_P4D_INDEX(x) (((x) >> 39) & 0x1ff)
#define PAGING64_PUD_INDEX(x) (((x) >> 30) & 0x1ff)
#define PAGING64_PMD_INDEX(x) (((x) >> 21) & 0x1ff)
#define PAGING64_PT_INDEX(x)  (((x) >> 12) & 0x1ff)
#define PAGING64_OFFSET(x)    ((x) & 0xfff)

/*
 * Page table walk function
 * Returns the pointer to the PTE or NULL if not found/allocated
 */
uint64_t *pgtable_walk(struct mm_struct *mm, uint64_t addr) {
    if (mm->pgd == NULL) return NULL;
    
    int pgd_idx = PAGING64_PGD_INDEX(addr);
    if (mm->pgd[pgd_idx] == 0) return NULL; 
    
    uint64_t *p4d = (uint64_t *)mm->pgd[pgd_idx];
    int p4d_idx = PAGING64_P4D_INDEX(addr);
    if (p4d[p4d_idx] == 0) return NULL;
    
    uint64_t *pud = (uint64_t *)p4d[p4d_idx];
    int pud_idx = PAGING64_PUD_INDEX(addr);
    if (pud[pud_idx] == 0) return NULL;
    
    uint64_t *pmd = (uint64_t *)pud[pud_idx];
    int pmd_idx = PAGING64_PMD_INDEX(addr);
    if (pmd[pmd_idx] == 0) return NULL;
    
    uint64_t *pt = (uint64_t *)pmd[pmd_idx];
    int pt_idx = PAGING64_PT_INDEX(addr);
    
    return &pt[pt_idx];
}

/* 
 * Helper to allocate tables if they don't exist
 */
int vmap_page_range_64(struct pcb_t *caller, int addr, int pgnum, 
                       struct framephy_struct *frames, struct vm_rg_struct *ret_rg) 
{
    struct framephy_struct *fpit = frames;
    int pgit = 0;
    uint64_t vaddr = (uint64_t)addr; 
    
    /* Initialize PGD if null */
    if (caller->mm->pgd == NULL) {
        caller->mm->pgd = malloc(512 * sizeof(uint64_t));
        for(int i=0; i<512; i++) caller->mm->pgd[i] = 0;
    }

    for (pgit = 0; pgit < pgnum; pgit++) {
        if (fpit == NULL) return -1;
        
        uint64_t curr_vaddr = vaddr + pgit * PAGING_PAGESZ; 
        
        // Walk and allocate
        int pgd_idx = PAGING64_PGD_INDEX(curr_vaddr);
        
        if (caller->mm->pgd[pgd_idx] == 0) {
            caller->mm->pgd[pgd_idx] = (uint64_t)malloc(512 * sizeof(uint64_t));
             uint64_t *new_p4d = (uint64_t *)caller->mm->pgd[pgd_idx];
             for(int i=0; i<512; i++) new_p4d[i] = 0;
        }
        
        uint64_t *p4d = (uint64_t *)caller->mm->pgd[pgd_idx];
        int p4d_idx = PAGING64_P4D_INDEX(curr_vaddr);
        if (p4d[p4d_idx] == 0) {
            p4d[p4d_idx] = (uint64_t)malloc(512 * sizeof(uint64_t));
            uint64_t *new_pud = (uint64_t *)p4d[p4d_idx];
            for(int i=0; i<512; i++) new_pud[i] = 0;
        }
        
        uint64_t *pud = (uint64_t *)p4d[p4d_idx];
        int pud_idx = PAGING64_PUD_INDEX(curr_vaddr);
        if (pud[pud_idx] == 0) {
            pud[pud_idx] = (uint64_t)malloc(512 * sizeof(uint64_t));
            uint64_t *new_pmd = (uint64_t *)pud[pud_idx];
            for(int i=0; i<512; i++) new_pmd[i] = 0;
        }
        
        uint64_t *pmd = (uint64_t *)pud[pud_idx];
        int pmd_idx = PAGING64_PMD_INDEX(curr_vaddr);
        if (pmd[pmd_idx] == 0) {
            pmd[pmd_idx] = (uint64_t)malloc(512 * sizeof(uint64_t));
            uint64_t *new_pt = (uint64_t *)pmd[pmd_idx];
            for(int i=0; i<512; i++) new_pt[i] = 0;
        }
        
        uint64_t *pt = (uint64_t *)pmd[pmd_idx];
        int pt_idx = PAGING64_PT_INDEX(curr_vaddr);
        
        // Set PTE
        // We need to format the PTE similar to 32-bit but 64-bit wide
        // For simplicity, just store FPN for now, or use pte_set_fpn logic adapted
        pt[pt_idx] = fpit->fpn; 
        // Set present bit? 
        // In this simulation, we might just store the FPN directly if the macros expect that.
        // However, let's try to respect the PTE format if possible.
        // The existing macros are 32-bit. We might need 64-bit macros.
        // For now, let's assume the lower 32-bits are compatible.
        
        pte_t pte_val = 0;
        pte_set_fpn(&pte_val, fpit->fpn);
        pt[pt_idx] = pte_val;

        fpit = fpit->fp_next;
    }
    
    return 0;
}

void print_pgtbl64(struct mm_struct *mm, uint64_t start, uint64_t end) {
    printf("print_pgtbl64: %ld - %ld\n", start, end);
    if (mm == NULL || mm->pgd == NULL) return;

    // Iterate through all possible pages in range
    // This is inefficient for sparse tables, but good for debugging small ranges
    // Better: Walk the tree
    
    // Simple recursive walk or just print what we have
    // Let's just print valid entries
    
    for(int i=0; i<512; i++) {
        if(mm->pgd[i] != 0) {
            uint64_t *p4d = (uint64_t *)mm->pgd[i];
            for(int j=0; j<512; j++) {
                if(p4d[j] != 0) {
                    uint64_t *pud = (uint64_t *)p4d[j];
                    for(int k=0; k<512; k++) {
                        if(pud[k] != 0) {
                            uint64_t *pmd = (uint64_t *)pud[k];
                            for(int l=0; l<512; l++) {
                                if(pmd[l] != 0) {
                                    uint64_t *pt = (uint64_t *)pmd[l];
                                    for(int m=0; m<512; m++) {
                                        if(pt[m] != 0) {
                                            printf("PGD[%d] P4D[%d] PUD[%d] PMD[%d] PT[%d] -> Frame: %ld\n",
                                                i, j, k, l, m, PAGING_PTE_FPN(pt[m]));
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

#endif
