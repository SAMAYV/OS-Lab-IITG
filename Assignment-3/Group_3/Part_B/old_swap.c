#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "swap.h"
#include "stat.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"

#define PAGES_LOW 100
#define PAGES_HIGH 1000

// Type will always be 0, but other types may also be added
struct swap_info_struct swap_info[1];
struct spinlock swaplock;

// Forward declarations
swp_entry_t get_swap_page(void);
int scan_swap_map(struct swap_info_struct*);
void lru_list_initialize();
void lru_cache_del(pte_t, uint);

#define SWAPFILE_CLUSTER 16
//#define SWAPFILE_CLUSTER 3

void kswapinit()
{
	unsigned int swapmap_bytes_needed = SWAPFILE_PAGES * sizeof(unsigned short);
	cprintf("kernel: Initializing swap info\n");

	memset(&swap_info[0],0,sizeof(struct swap_info_struct));
	swap_info[0].pages = swap_info[0].max = SWAPFILE_PAGES;
	swap_info[0].max--;
	swap_info[0].swap_map_pages = 1 + (swapmap_bytes_needed / PGSIZE);
	swap_info[0].flags = SWP_WRITEOK;
	swap_info[0].highest_bit = SWAPFILE_PAGES - 1;
	swap_info[0].cluster_nr = SWAPFILE_CLUSTER;

	initlock(&swaplock,"swaplock");
	initlock(&swap_info[0].sdev_lock,"sdev_lock");

	cprintf("kernel: swapmap bytes needed: %d\n",swapmap_bytes_needed);
	cprintf("kernel: swapmap pages needed: %d\n",swap_info[0].swap_map_pages);

	for (int x = 0; x < swap_info[0].swap_map_pages; x++)
	{
		// We assume here that kalloc'ed pages will happen in a sequence from high to low. This method should be executed early enough
		// in xv6 initialization, so this should happen every time. Hence we can do a strightforward implementation.
		
		// Example scenario:
		// 4 pages needed for the swap map total, so 4 passes thru the loop
		// 1st page allocated at 0x804FF000
		// 2nd page allocated at 0x804FE000
		// 3rd page allocated at 0x804FD000
		// 4th page allocated at 0x804FC000. Swap map pointer set to 0x804FC000 since this is the last page.
		
		// In this case we have the range for the swap map allocated from 0x804FC000 to 0x804FFFFF for a total of 16k

		char *new_kalloc_page = kalloc();
		memset(new_kalloc_page,0,PGSIZE);
		cprintf("kernel: Allocating page for swap map at address 0x%p\n",new_kalloc_page);
		
		if (x == swap_info[0].swap_map_pages - 1)
		{
			// Allocate & zero out this section of the map
			swap_info[0].swap_map = (unsigned short*)new_kalloc_page;
			cprintf("kernel: Swap map pointer set to address 0x%p\n",new_kalloc_page);
		}
	}

	lru_list_initialize();
	cprintf("kernel: Done initializing swap info\n");
}

void print_swap_map()
{
	cprintf("Swap map(lb==%d, hb==%d):\n",swap_info[0].lowest_bit,swap_info[0].highest_bit);

	for (int x = 0; x < SWAPFILE_PAGES; x++)
		cprintf("%d ",swap_info[0].swap_map[x]);

	cprintf("\n");
}

unsigned int swap_page_total_count() {
	return SWAPFILE_PAGES;
}

unsigned int swap_page_count() {
  return swap_info[0].pages;
}

inline unsigned int swap_refcount(unsigned long offset)
{
	if (offset > SWAPFILE_PAGES)
		panic("swap_refcount");
	return swap_info[0].swap_map[offset];
}

int swap_duplicate(swp_entry_t entry)
{
	unsigned int offset = SWP_OFFSET(entry);

	if (offset > SWAPFILE_PAGES)
		panic("swap_duplicate");
	
	swap_list_lock();
	++swap_info[0].swap_map[offset];	
	swap_list_unlock();
	return swap_info[0].swap_map[offset];
}

int swap_entry_free(struct swap_info_struct *p, unsigned long offset)
{
	int count = p->swap_map[offset];

	if (count < SWAP_MAP_MAX) {
		count--;
		p->swap_map[offset] = count;
		if (!count) {
			if (offset < p->lowest_bit)
				p->lowest_bit = offset;
			if (offset > p->highest_bit)
				p->highest_bit = offset;
			p->pages++;
		}
	}
	return count;
}

int swap_free(swp_entry_t entry)
{
	int retval = 0;
	struct swap_info_struct * p = &swap_info[0];

	swap_list_lock();
	retval = swap_entry_free(p, SWP_OFFSET(entry));
	swap_list_unlock();

	return retval;
}

int swap_free_nolocks(swp_entry_t entry) {
	return swap_entry_free(&swap_info[0], SWP_OFFSET(entry));
}

// Frees all swap pages
void free_swap_pages(struct proc *currproc)
{
  swap_list_lock();
  for (int index1 = 0; index1 < NPDENTRIES; index1++)
  {
    pde_t *pde = &(currproc->pgdir[index1]);

    if (*pde & PTE_P && index1 < 512)
    {
      pde_t *pgtab = (pte_t*)P2V(PTE_ADDR(*pde));

      for (int index2 = 11; index2 < NPTENTRIES; index2++)
      {
        if (!(pgtab[index2] & PTE_P) && (pgtab[index2] & PTE_U))
        {
			// Check if this page is swapped out
			swp_entry_t this_entry = pte_to_swp_entry(pgtab[index2]);
			uint offset = SWP_OFFSET(this_entry);

			if (offset < SWAPFILE_PAGES && swap_info[0].swap_map[offset] != 0)
			{
				cprintf("process [%s] exiting. freeing slot entry %d. New refcount==%d\n",currproc->name,offset,swap_free_nolocks(this_entry));
			}
        }
      }
    }
  }
  swap_list_unlock();
}

int swap_out(pte_t *mapped_victim_pte, unsigned int offset)
{
	struct swap_info_struct *p = &swap_info[0];
	int file_offset = offset + 1, retval = -1;
	uint old_offset;
	char *kernel_addr = P2V(PTE_ADDR(*mapped_victim_pte));

	if(p->swap_file == NULL)
		return -1;

	old_offset = p->swap_file->off;

	// Quick and dirty hack for now. Need a lock-protected state variable later
	myproc()->pages_swapped_out++;

	// Write contents to swapfile
	p->swap_file->off = (unsigned int)(file_offset * PGSIZE);
  	retval = filewrite(p->swap_file,kernel_addr,PGSIZE);
  	p->swap_file->off = old_offset;

	return retval;
}

// My own method. Swap a page into main memory from the specified slot
int swap_in(void *page_addr, unsigned int offset)
{
	struct swap_info_struct *p = &swap_info[0];
	int file_offset = offset + 1, retval = -1;
	uint old_offset;

	if (p->swap_file == NULL)
	// SWAPFILE pointer not set yet with ksetswapfileptr() system call
		return -1;

	old_offset = p->swap_file->off;

	// Quick and dirty hack for now. Need a lock-protected state variable later
	myproc()->pages_swapped_out--;

	// Read contents from swapfile
	p->swap_file->off = (unsigned int)(file_offset * PGSIZE);
	retval = fileread(p->swap_file,page_addr,PGSIZE);
	p->swap_file->off = old_offset;

	return retval;
}

swp_entry_t get_swap_page()
{
	struct swap_info_struct * p;
	unsigned long offset;
	swp_entry_t entry;
	int type;

	entry.val = 0;	// Out of memory 
	swap_list_lock();
	type = 0;
	if (type < 0)
		goto out;
	if (swap_info[0].pages <= 0)
		goto out;

	while (1) {
		p = &swap_info[type];
		if ((p->flags & SWP_WRITEOK) == SWP_WRITEOK) {
			swap_device_lock(p);

			//cprintf("before scan_swap_map\n");
			offset = scan_swap_map(p);
			//cprintf("after scan_swap_map. offset==%d\n",offset);
			
			swap_device_unlock(p);
			if (offset >= 0) {
				entry = SWP_ENTRY(type,offset);
				goto out;
			}
		}
	}
out:
	swap_list_unlock();
	return entry;
}

inline void ksetswapfileptr(struct file *f)
{
  cprintf("kernel: Swap file pointer set! Address of file pointer: 0x%p\n",f);
  swap_info[0].swap_file = filedup(f);
}

inline int scan_swap_map(struct swap_info_struct *si)
{
	unsigned long offset;
	/* We try to cluster swap pages by allocating them sequentially in swap. Once we've allocated SWAPFILE_CLUSTER pages this way, however, we resort to
	 * first-free allocation, starting a new cluster. This prevents us from scattering swap pages all over the entire
	 * swap partition, so that we reduce overall disk seek times between swap pages. */
	if (si->cluster_nr) {
		while (si->cluster_next <= si->highest_bit) {
			offset = si->cluster_next++;
			if (si->swap_map[offset])
				continue;
			si->cluster_nr--;
			goto got_page;
		}
	}
	si->cluster_nr = SWAPFILE_CLUSTER;

	/* try to find an empty (even not aligned) cluster. */
	offset = si->lowest_bit;
 check_next_cluster:
	if (offset+SWAPFILE_CLUSTER-1 <= si->highest_bit)
	{
		int nr;
		for (nr = offset; nr < offset+SWAPFILE_CLUSTER; nr++)
			if (si->swap_map[nr])
			{
				offset = nr+1;
				goto check_next_cluster;
			}
		goto got_page;
	}

	/* No luck, so now go finegrined as usual. -Andrea */
	for (offset = si->lowest_bit; offset <= si->highest_bit ; offset++) {
		if (si->swap_map[offset])
			continue;
		si->lowest_bit = offset+1;
	got_page:
		if (offset == si->lowest_bit)
			si->lowest_bit++;
		if (offset == si->highest_bit)
			si->highest_bit--;
		if (si->lowest_bit > si->highest_bit) {
			si->lowest_bit = si->max;
			si->highest_bit = 0;
		}
		si->swap_map[offset] = 1;
		si->pages--;
		si->cluster_next = offset+1;
		return offset;
	}
	si->lowest_bit = si->max;
	si->highest_bit = 0;
	return 0;
}

// LRU section
struct lru_list_entry {
	pte_t addr;
	struct lru_list_entry *next;
};

struct lru_list_struct {
	struct lru_list_entry *active_list;
	struct lru_list_entry *inactive_list;
	struct spinlock lru_lock;
	unsigned long nr_active_pages;
	unsigned long nr_inactive_pages;	
};

// Note that we can have a large number of entries in the LRU list, which may overflow the kernel stack.
// As a result, we need a subsystem to manage the doling out of LRU entries to be used, which we will dub the LRU bank

// Needed to figure out how many pages to allocate for LRU list bank
// Assume 12 bytes for header (prev & next pointers and count)
#define LRU_HEADER_SIZE 12
#define LRU_ENTRIES_PER_PAGE ((PGSIZE - LRU_HEADER_SIZE) / sizeof(struct lru_list_entry))

struct lru_bank_page {
	// Contains blocks of LRU entries to be given out when needed
	struct lru_bank_page *prev;
	struct lru_bank_page *next;
	unsigned int used;

	struct lru_list_entry slots[LRU_ENTRIES_PER_PAGE];
};

// Main container for LRU lists
struct lru_list_struct lru_list;
#define lru_list_lock()     acquire(&lru_list.lru_lock);
#define lru_list_unlock()   release(&lru_list.lru_lock);

struct lru_bank_page *lru_bank = NULL;

void lru_list_initialize()
{
	cprintf("kernel: Initializing LRU list container.\n");

	lru_bank = (struct lru_bank_page*)kalloc();

	if (!lru_bank)
		panic("Unable to allocate LRU bank!");
	
	memset(lru_bank,0,sizeof(struct lru_bank_page));

	cprintf("kernel: First page of LRU entry bank created at 0x%p\n", lru_bank);

	cprintf("kernel: Initializing LRU active & inactive lists\n", lru_bank);
	lru_list.active_list = NULL;
	lru_list.inactive_list = NULL;
	lru_list.nr_active_pages = 0;
	lru_list.nr_inactive_pages = 0;
	initlock(&lru_list.lru_lock,"lru_lock");

	cprintf("kernel: LRU entries per page: %d\n", LRU_ENTRIES_PER_PAGE);
}

struct lru_list_entry *lru_bank_get_new()
// Returns a fresh LRU entry to be used
{
	struct lru_list_entry *retval = NULL;
	struct lru_bank_page *lb_curr = lru_bank;
	struct lru_bank_page *lb_last = lb_curr;

	while (lb_curr)
	{
		uint exit = 0;

		if (lb_curr->used < LRU_ENTRIES_PER_PAGE)
		{
			// Take an entry from the current page
			for (int x = 0; x < LRU_ENTRIES_PER_PAGE; x++)
			{
				if (lb_curr->slots[x].addr == 0)
				// Found free entry
				{
					retval = &lb_curr->slots[x];
					lb_curr->used++;
					exit = 1;
					break;
				}
			}
		}

		if (exit > 0)
			break;

		// No free entries found. Try next page
		lb_last = lb_curr;
		lb_curr = lb_curr->next;
	}

	if (retval == NULL)
	// All LRU entries in all currently allocated bank pages are in use. Create a new bank page
	{
		if (lb_last == NULL)
			panic("lru_bank_get_new(): lb_last != NULL assertion failed");

		// Create new bank page
		lb_curr = lb_last->next = (struct lru_bank_page*)kalloc();
		lb_curr->prev = lb_last;
		memset(lb_curr,0,PGSIZE);

		// Assign new LRU item
		retval = &lb_curr->slots[0];
		lb_curr->used++;
	}

	//cprintf("kernel: Got new LRU entry at address 0x%p\n",retval);

	retval->addr = 0;
	retval->next = NULL;
	return retval;
}

// Finds the LRU bank page of the associated entry. Should never return NULL
struct lru_bank_page *lru_bank_find_page(struct lru_list_entry *entry)
{
	struct lru_bank_page *retval = NULL;
	struct lru_bank_page *currpg = lru_bank;

	while (currpg && retval == NULL)
	{
		if ((uint)entry >= (uint)currpg && (uint)entry < (uint)currpg + PGSIZE)
			retval = currpg;

		currpg = currpg->next;
	}

	return retval;
}

// Removes target entry and releases it back into the LRU bank. Does not change the next field
void lru_bank_release(struct lru_list_entry *target)
{
	//cprintf("kernel: lru_bank_page() finding LRU entry with target==0x%p,target->addr==0x%p\n",target,target->addr);
	struct lru_bank_page *currpg = lru_bank_find_page(target);
	
	if (currpg == NULL)
		panic("lru_bank_release()");
	
	// Blank out this LRU entry and free it for use
	target->addr = 0;
	currpg->used--;
}

// Add a cold page to the front of the inactive_list. Will be moved to active_list with a call to mark_page_accessed()
// if the page is known to be hot, such as when a page is faulted in (pageHot > 0).
void lru_cache_add(pde_t addr, int pageHot)
{
	cprintf("kernel: Adding %s pte at kernel address 0x%p to LRU cache\n",(pageHot == 0 ? "cold" : "hot"),addr);
	lru_list_lock();

	struct lru_list_entry *new_entry = lru_bank_get_new();

	//cprintf("kernel: lru_cache_add(): new_entry==0x%p\n",new_entry);

	new_entry->addr = addr;
	if (pageHot <= 0)
	{
		// Add cold page to front of inactive list
		new_entry->next = lru_list.inactive_list;
		lru_list.inactive_list = new_entry;

		lru_list.nr_inactive_pages++;
	}
	else {
		// Since we know the page is hot, put it in the front of the active list right now
		new_entry->next = lru_list.active_list;
		lru_list.active_list = new_entry;
		lru_list.nr_active_pages++;
		// *pte |= PTE_A; <----- Add accessed bit (reference purposes)
	}

	// Clear accessed bit (although this is pointless if the page is hot and this is called from trap.c as a page fault)
	pte_t *pte = (pte_t*)addr;
	*pte &= ~PTE_A;
	lru_list_unlock();
}

// Removes a page from the LRU lists by calling either del_page_from_active_list()
// or del_page_from_inactive_list(), whichever is appropriate.
// The parameter rangeSize deletes all addresses between (addr) and (addr + rangeSize). Set rangeSize to 0 to specify a single address
void lru_cache_del(pte_t addr, uint rangeSize)
{
	lru_list_lock();
	struct lru_list_entry *curr = NULL;
	struct lru_list_entry *prev = NULL;

	// Search active list first
	curr = lru_list.active_list;
	prev = curr;

	while (curr)
	{
		if (curr->addr >= addr && curr->addr <= ((uint)addr + rangeSize))
		{
			// Remove the entry
			cprintf("kernel: Removing LRU entry for PTE at 0x%p from active list\n",*curr);
			lru_bank_release(curr);
			lru_list.nr_active_pages--;
			
			if (lru_list.active_list == curr)
				lru_list.active_list = curr->next;
			else
				prev->next = curr->next;

			if (rangeSize == 0) {
				lru_list_unlock();
				return;
			}
		}
		prev = curr;
		curr = curr->next;
	}

	// Now search inactive list
	curr = lru_list.inactive_list;
	prev = curr;

	while (curr)
	{
		if (curr->addr >= addr && curr->addr <= ((uint)addr + rangeSize))
		{
			// Remove the entry
			cprintf("kernel: Removing LRU entry for PTE at 0x%p from inactive list\n",*curr);
			lru_bank_release(curr);
			lru_list.nr_inactive_pages--;
			
			if (lru_list.inactive_list == curr)
				lru_list.inactive_list = curr->next;
			else
				prev->next = curr->next;

			if (rangeSize == 0)
			{
				lru_list_unlock();
				return;
			}
		}
		prev = curr;
		curr = curr->next;
	}

	lru_list_unlock();
	return;
}

// Removes a page from the inactive_list and places it on active_list. 
// It is very rarely called directly as the caller has to know the page is on inactive_list. mark_page_accessed() should be used instead
void activate_page(pte_t addr)
{
	pte_t *pte = (pte_t*)addr;
	// Clear accessed bit
	*pte &= ~PTE_A;
	lru_cache_del(addr,0);
	lru_cache_add(addr,1);
}

// Mark that the page has been accessed. Implementation more complex in linux but here we just call activate_page()
void mark_page_accessed(pte_t addr)
{
	activate_page(addr);
}

// Method to list all pages in the LRU cache. Used for demonstration/debugging purposes
void print_lru()
{
	struct lru_list_entry *entry = lru_list.active_list;
	unsigned int index = 0;
	cprintf("*** LRU table ***\n");
	cprintf("*** Active list ***\n");

	while (entry)
	{
		pte_t *pte = (pte_t*)entry->addr;
		cprintf("Entry #%d: PTE addr==0x%p, accessed bit: %d\n",index,entry->addr,(*pte & PTE_A));
		index++;
		entry = entry->next;
	}

	cprintf("*** Inactive list ***\n");
	entry = lru_list.inactive_list;
	index = 0;

	while (entry)
	{
		pte_t *pte = (pte_t*)entry->addr;
		cprintf("Entry #%d: PTE addr==0x%p, accessed bit: %d\n",index,entry->addr,(*pte & PTE_A));
		index++;
		entry = entry->next;
	}	
	cprintf("*** End of LRU table ***\n");
}

// Simplified version of the refill_inactive() method on linux. Here, we do everything in this method.
// Our goal is to make the active list comprise 2/3 of the total, and the inactive list the remaining 1/3
void refill_inactive()
{
	// nr_pages is the # of pages we want to swap out
	unsigned long nr_pages = 1;
	unsigned long ratio = 0;
	unsigned long index = 0;
	unsigned long total_lru_pages = lru_list.nr_active_pages + lru_list.nr_inactive_pages;
	unsigned long active_target_count;
	struct lru_list_entry *entry = lru_list.active_list;

	// Get # of pages to move
	// Per the docs, the active needs needs to be 2/3 the size of the inactive list
	// Hence, the active list is 2/5 the total and the inactive 3/5 of the total
	active_target_count = 2 * total_lru_pages;
	active_target_count /= 5;
	ratio = lru_list.nr_active_pages - active_target_count;

	if (ratio < 0)
		ratio = 0;

	cprintf("kernel: refill_inactive() found: atc==%d, nr_pages==%d, active==%d, inactive==%d, ratio==%d \n",active_target_count, nr_pages,lru_list.nr_active_pages,lru_list.nr_inactive_pages,ratio);
	cprintf("kernel: refill_inactive() moving %d pages from active to inactive list\n",ratio);

	// Move the pages
	while (entry != NULL)
	{
		// Check for pages which have been accessed
		pte_t addr = entry->addr;
		pte_t *pte = (pte_t*)entry->addr;
		struct lru_list_entry *nextentry = entry->next;
		uint is_active = (*pte & PTE_A);

		// We're either going to move the page to the front of the active list or to the inactive list
		// In either case, we'll delete the one we have now

		if (!(is_active) && ratio > 0)
		{
			// Accessed bit is not set, so page isn't hot anymore. Move to inactive list
			cprintf("kernel: moving active LRU entry to inactive list\n");
			// Clear accessed bit
			*pte &= ~PTE_A;		
			lru_cache_del(addr,0);
			lru_cache_add(addr,0);
			// Add accessed bit
			//*pte |= PTE_A;
			ratio--;
		}
		else if (is_active)
		{
			// Page is still hot, clear accessed bit & move to front of active list
			cprintf("kernel: moving hot LRU entry to front of active list\n");
			mark_page_accessed(entry->addr);
		}
		entry = nextentry;
		index++;
	}
}

// Place any inactive pages accessed back into the active list
void refill_active()
{
	struct lru_list_entry *entry = lru_list.inactive_list;

	while (entry != NULL)
	{
		// Check for pages which have been accessed
		pte_t *pte = (pte_t*)entry->addr;
		struct lru_list_entry *nextentry = entry->next;
		// We're either going to move the page to the front of the active list or to the inactive list
		// In either case, we'll delete the one we have now

		// Accessed bit is set, so page is now hot. Move to active list
		if (*pte & PTE_A)
		{
			cprintf("kernel: moving hot LRU entry to front of active list\n");
			mark_page_accessed(entry->addr);
		}
		entry = nextentry;
	}
}

// Rotates the active and inactive LRU lists loosely according to the simplified LRQ2 algorithm
void lru_rotate_lists()
{
	refill_inactive();
	refill_active();
}

void lru_remove_proc_pages(struct proc *currproc)
{
  // Standard page directory crawl
  for (int index1 = 0; index1 < NPDENTRIES; index1++)
  {
    pde_t *pde = &(currproc->pgdir[index1]);

    if (*pde & PTE_P && index1 < 512)
    {
      pde_t *pgtab = (pte_t*)P2V(PTE_ADDR(*pde));

      cprintf("kernel: Removing LRU entries between 0x%p and 0x%p.\n",pgtab,(uint)pgtab+PGSIZE);
	  lru_cache_del((uint)pgtab,PGSIZE);
	  
    }
  }
}

// Returns the address of a page directory entry for the next victim page
unsigned int *get_victim_page()
{

	struct lru_list_entry *curr = NULL;
	struct lru_list_entry *victim_entry = NULL;
	pte_t *pte = NULL;

	// Try this # of times to rotate the list and find a victim. 
	int attempts = 2;

	for (int index = 0; index < attempts; index++)
	{
		// Give us some fresh inactives
		lru_rotate_lists();
		curr = lru_list.inactive_list;
		while (curr)
		// This loop gets the last inactive entry that hasn't been accessed
		{
			pte = (pte_t*)curr->addr;
			if (!(*pte & PTE_A) && (*pte & PTE_P))
				// Page not accessed & is still present in memory. Make this the current victim
				victim_entry = curr;
			curr = curr->next;
		}

		if(victim_entry != NULL)
			break;
	}
	
	if (victim_entry == NULL)
		panic("No LRU inactive pages present in physical memory");
	else {
		pte = (pte_t*)victim_entry->addr;
		return (unsigned int*)victim_entry->addr;
	}
  	return 0;
}
