
// Credit: Mel Gorman
// https://www.kernel.org/doc/gorman/html/understand/understand014.html
// Section 11.1:  Describing the Swap Area

// Implemented by Zeb Rasco <rascozeb@gmail.com>

// spinlock.c
void            acquire(struct spinlock*);
void            getcallerpcs(void*, uint*);
int             holding(struct spinlock*);
void            initlock(struct spinlock*, char*);
void            release(struct spinlock*);
void            pushcli(void);
void            popcli(void);


#define SWAPFILE_FILENAME "SWAPFILE"
//#define SWAPFILE_PAGES (((8 * 1024 * 1024) / PGSIZE) - 1)
//#define SWAPFILE_PAGES (((1 * 1024 * 1024) / PGSIZE) - 1)
#define SWAPFILE_PAGES 18
#define MAX_SWAP_BADPAGES (PGIZE - 1024 - 512 - 10) / sizeof(long)

/* From Linux 2.4 source code */
#define SWP_USED	1
#define SWP_WRITEOK	3
#define SWAP_CLUSTER_MAX 32
#define SWAP_MAP_MAX	0x7fff
#define SWAP_MAP_BAD	0x8000

// This would contain a prioritized swap order, but will likely be implemented as an array of one
struct swap_list_t {
    int head;                       // First swap area
    int next;                       // Next swap area
};

struct swap_info_struct {

    unsigned int flags;
    //kdev_t swap_device;           // Device corresponding to the partition used for this swap area. Leave as NULL since we'll use a file
    struct spinlock sdev_lock;      // Lock protecting the structure
    struct file *swap_file;         // Pointer to the swapfile
    int fd;                         // Open fd to swapfile
    unsigned int swap_map_pages;    // Number of kalloc()'ed pages used by the swap map
    unsigned short *swap_map;       // This is a large array with one entry for every swap entry, or page sized slot in the area.
                                    // An entry is a reference count of the number of users of this page slot.
                                    // The swap cache counts as one user and every PTE that has been paged out to the slot counts as a user. 
                                    // If it is equal to SWAP_MAP_MAX, the slot is allocated permanently. If equal to SWAP_MAP_BAD, the slot will never be used;
    unsigned int lowest_bit;        // This is the lowest possible free slot available in the swap area and is used to start 
                                    // from when linearly scanning to reduce the search space.
                                    // It is known that there are definitely no free slots below this mark
    unsigned int highest_bit;       // This is the highest possible free slot available in this swap area.
                                    // Similar to lowest_bit, there are definitely no free slots above this mark;
    unsigned int cluster_next;      // This is the offset of the next cluster of blocks to use.
                                    // The swap area tries to have pages allocated in cluster blocks to increase the chance related pages will be stored together;
    unsigned int cluster_nr;        // This the number of pages left to allocate in this cluster
    int prio;                       // Each swap area has a priority. Not needed for this version, since we only use 1

    int pages;                      // Number of usable pages
    unsigned long max;              // Total number of slots in the swap area
    int next;                       // Unused, but would normally contain the index of the next swap area
};

// The first page of any swap area is reserved for the header below
union swap_header {
	struct {
		char reserved[PGSIZE - 10];
		char magic[10];			/* SWAP-SPACE or SWAPSPACE2 */
	} magic;
	struct {
		char		        bootbits[1024];	/* Space for disklabel etc. */
		unsigned int		version;        // This is the version of the swap area layout;
		unsigned int		last_page;      // This is the last usable page in the area;
		unsigned int		nr_badpages;    // The known number of bad pages that exist in the swap area are stored in this field;
		unsigned int        padding[125];   // A disk section is usually about 512 bytes in size. 
                                    // The three fields version, last_page and nr_badpages make up 12 bytes and the padding
                                    // fills up the remaining 500 bytes to cover one sector;
		unsigned int		badpages[1];    // The remainder of the page is used to store the indices of up to MAX_SWAP_BADPAGES number of bad page slots.
                                    // This is used by mkswap, which we don't have, so we probably won't use this
	} info;
};

// TODO: Implement these
// #define PTE_ADDR(pte)   ((uint)(pte) & ~0xFFF)

/* Encode and de-code a swap entry */   
#define pte_to_swp_entry(pte)	((swp_entry_t) { pte })
#define swp_entry_to_pte(x)		((pte_t) { (x).val })

#define SWP_ENTRY(type, offset)		    ((swp_entry_t) { ((type) << 1) | ((offset) << 8) })
#define SWP_OFFSET(swp_entry)           (((swp_entry).val >> 8) & 0x00FFFFFF)
#define SWP_TYPE(swp_entry)             (((swp_entry).val >> 1) & 0x0000003F)

// By having a 24-bit page offset, this allows for swapfiles up to 64GB (assuming 4096 byte PGSIZE, 64GB = 2^24 * PGSIZE)
typedef struct {
    unsigned long val;              // Bit 0 reserved for PTE_P flag (Page present)
                                    // Bits 1-6 are the type (index within the swap_info array), which will always be 0 in this implementation
                                    // But theoretically this would allow 64 seperate swap areas
                                    // Bit 7 is reserved for PAGE_PROTNONE (Page is resident, but not accessible)
                                    // Bits 8-31 are used are to store the offset within the swap_map from the swp_entry_t
} swp_entry_t;

/* From Linux 2.4, changed for xv6 */
extern struct spinlock swaplock;
#define swap_list_lock()	acquire(&swaplock)
#define swap_list_unlock()	release(&swaplock)
#define swap_device_lock(p)	acquire(&p->sdev_lock)
#define swap_device_unlock(p)	release(&p->sdev_lock)