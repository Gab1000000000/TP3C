/* gc.c --- Gestionnaire mémoire.  */

#include "gc.h"
#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#define HEAPSIZE 33554432


/*TYPE DECLARATIONS*/
typedef char byte;
typedef unsigned int position;

/*GLOBAL HEAP*/
static byte pool[HEAPSIZE]; 

/*GLOBAL INDEX OF FREE POSITION*/
unsigned int freep = 0;

/*FORWARD FUNCTION DECLARATIONS*/
void memMove(byte array[],
             unsigned int init, 
             unsigned int final,
             unsigned int size);


/*-------------------------------BEGIN-PAGE-SYSTEM----------------------------------
 * DESCRIPTION
 * 
 * This is the page system. It's somewhat a virtual memory system.
 * 
 * It has a linked list of pages, all ordered by allocation (we always
 * allocate at the end, O(1) since we keep a reference to the last page in memory).
 * 
 * It has globally accessible first element and uses NULL as sentinel.
 * 
 * Each page has a last byte (the right) which is allocated to
 * represent the mark bit (byte in our case). Since a mark byte is added, 
 * we don't really give a damn about the size and don't require it to be a multiple of 2...
 * 
 * If during garbage collection, the last byte of the page isn't marked, the
 * page is destroyed and the next slides left on the last known free position.
 *
 * 
 * Operations complexity (only the ones used in here) 
 * ADD -> O(1), we keep a pointer to the last page
 * DEFRAG -> O(n), this is a combination of walking through the list and
 * moving the memory left with memMove (which allows overlay of structures).
 * 
 * DEFRAG is called only in the special case that the memory left on the right
 * is too small to accomodate the next structure.
 * 
 * Although this system doesn't have suitable alignment, from a practical standpoint
 * it has the advantage of no fragmentation (we can fill the block completely and 
 * it is guaranteed to provide almost as much space as advertised (we add 1 mark byte
 * per object, so the more objects, the more marks...).
 * 
 * This system will be inefficient when a large quantity of small objects are used
 * because we keep unique page for each object.
 * 
 * COMPOSITION
 * The system is composed of the following functions:
 * high level
 *      -addPage(size): adds new memory location
 *      -defrag(): goes through the pages and, delete unmarked ones, and does the defragmentation.
 * 
 * mid level
 *      -MV(newLeft, PAGE, pool[]): slides the page on the leftmost available spot
 * 
 * low level
 *      -memMove(newLeft, OldLeft, Size): exactly like memmove but on byte array
 *      -memSet(Left, Size, Char) : exactly like memset but on byte array
 */

struct page 
{
    unsigned int left;
    unsigned int right;
    unsigned int size;
    struct GCobject * obj;
    struct page* next;
};
typedef struct page page;


page ANCHOR = {0, 0, 0, NULL ,NULL};
page* FIRSTPAGE = &ANCHOR;
page* LASTPAGE =  &ANCHOR;



/*SHIFT PAGE LEFT IN MEMORY*/
void MV(unsigned int newLeftPosition, 
               page* PAGE, byte pool[])
{
    /*only shift left, since we defrag*/
    assert (newLeftPosition < (PAGE->left));
    
    unsigned int oldPosition = (PAGE->left);
    int offset = (PAGE->left) - newLeftPosition;
    
    (PAGE->left) = newLeftPosition;
    (PAGE->right) -= offset;
    (PAGE->obj) -= offset;
    memMove(pool, oldPosition, newLeftPosition,(PAGE->size));
    
}

/*GET FREE AVAILABLE MEMORY
 * meaningful only after defrag, otherwise
 * dead objects are conserved 
 */
unsigned int AVAILABLEMEM()
{
    return(HEAPSIZE - freep);
}



/*-----------------------------------END-PAGE-SYSTEM-----------------------------------*/






/*ADD PAGE TO PAGE SYSTEM
 * by definition, it is added at the end, so it becomes the lastpage
 */
struct GCobject** addPage(unsigned int size)
{
    
    /* allocate a new page */
    page* newPage = (page*) malloc(sizeof(page));
    
    /* the location will be at the end of the system 
       and we allocate +1 for the markbyte */
    (newPage->left) = freep;
    (newPage->right) = (freep+size+1);
    (newPage->size) = (size+1);
    (newPage->next) = NULL;
    (newPage->obj) = (struct GCobject*) &pool[freep];
    
    /*(int*) &array[position];*/
    /* update the LASTPAGE */
    (LASTPAGE->next) = newPage;
    LASTPAGE = newPage;
    freep = (newPage->right)+1;
    pool[(newPage->left) + (newPage->size)] = 'U';
    return &(newPage->obj);
}



    
/*DEFRAG*/
/* to see if the byte is marked we introduce symbols 
 * that are located at the last byte of the structure.
 * They are reached through pointer arithmetic by
 * the page system and the marking system...
 * 'U' -> unmarked
 * 'M' -> marked
 * clustefuck quite possible... careful
 * Also need to call updatePointers after to insure completion
 */
void defrag()
{
    /* We start at zero and increment */
    freep = 0;
    
    /* We ignore the first page, its just there*/
    page* tmp_before = FIRSTPAGE;
    page* tmp = (FIRSTPAGE->next);
    int finished = 0;
    
    if(tmp==NULL)
    {
        finished = 1;
    }
    
    while(finished == 0)
    {
        /* get the mark byte */
        int index = (tmp->right);
        
        /* if the byte is marked, we unmark it and slide leftmost */
        if(pool[index] == 'M')
        {
            pool[index] ='U';
            if(freep != (tmp->left))
            {
                MV(freep, tmp, pool);
            }
            freep = ((tmp->right)+1);
            tmp_before = tmp;
            tmp = tmp->next;
        }
        
        /* if the byte is unmarked, we delete the structure */
        else if(pool[index] == 'U')
        {
            /* connect after to before */
            page* tmp_next = (tmp->next);
            (tmp_before->next) = tmp_next;
            
            /* free the node and move on */
            free(tmp);
            tmp = tmp_next;
        }

        
        if(tmp ==NULL)
        {
            finished = 1;
        }
        
    }
    
    /* reassign LASTPAGE to the last */
    if(tmp == NULL)
    {
        LASTPAGE = tmp_before;
    }
    else
    {
        LASTPAGE = tmp;
    }
    
}





/*MEMORY FUNCTIONS-----------------------------------------------------------*/

 /*MEMCPY FOR BYTE ARRAY*/
void memMove(byte array[],       //array to manipulate
             unsigned int init,  //initial position
             unsigned int final, //final position
             unsigned int size)  //size of the mem chunk
{
    byte buffer[size];
    int i = 0;
    for(i; i<size; i++)
    {
        int j = i+init;
        buffer[i] = array[j];
        //array[j]='\0';
    }
    i=0;
    for(i; i<size; i++)
    {
        int j = i+final;
        array[j] = buffer[i];
    }
}

/*MEMSET FOR BYTE ARRAY*/
void memSet(byte array[],
            byte value,
            unsigned int position,
            unsigned int size)
{
    int i = 0;
    for(i; i<size; i++)
    {
        array[i+position] = value;
    }
}
    



/* --------------------------BEGIN-PRINT-FUNCTIONS------------------------------------ */
/* PRINT A SINGLE MEMORY PAGE */
void printPage(page* p)
{
    printf("PAGE           size             %u\n", (p->size));
    printf("               left position    %u\n", (p->left));
    printf("               right position   %u\n", (p->right));
    printf("               array location   %p\n", &pool[(p->left)]);
    printf("               pointer location %p\n", (p->obj));
    printf("               mark byte        %c\n", pool[(p->right)]);
    if((p->next)==NULL) 
    {
        printf("               TERMINAL\n");
    }
    printf("\n");
}
/* PRINT ALL REACHABLE MEMORY PAGES */
void printAllPages()
{
    page* tmp = FIRSTPAGE;
    int i = 1;
    while(i==1)
    {
        printPage(tmp);
        if((tmp->next) != NULL)
        {
            tmp = (tmp->next);
        }
        else
        {
            i=0;
        }
    }
}

/* -----------------------------END-PRINT-FUNCTIONS------------------------------------ */


    
    
    
    
    
    
    
    
    
    
    
/*------------------------------------------------------------------*/

struct GCroot ROOTS = {NULL, NULL};




struct GCobject** gc_malloc (struct GCclass *c)
{  
   /* Get the memory size */
   unsigned int memSize = (unsigned int) (c->size);
   
   if(HEAPSIZE<memSize)
   {
        printf("NO MEM LEFT, IMMINENT SEGFAULT :)\n");
        return NULL;
   }
   
   /* on first pass, if there isn't enough mem, we defrag */
   if (AVAILABLEMEM() < memSize)
   {
       defrag();
   }
   /* if there still isn't enough memory, we allocate fuckall */
   if (AVAILABLEMEM()< memSize)
   {
       printf("NO MEM LEFT, IMMINENT SEGFAULT :)\n");
       return NULL;
   }
   
   /* if it gets there, we allocate */
   /* (int*) &array[position]; */
   return addPage(memSize);
}




/* --------------------------BEGIN-STATS------------------------------------ */

/*RETURNS STATUS OF MEM SYSTEM*/
struct GCstats gc_stats (void)
{
    /* We proceed through the pages */
    int count = -1;
    int usedSize = 0;
    page* tmp = FIRSTPAGE;
    int i = 1;
    while(i==1)
    {
        count++;
        usedSize += (tmp->size);
        if((tmp->next)==NULL)
        {
            i=0;
        }
        else
        {
            tmp = (tmp->next);
        }
    }
    
    struct GCstats r = {count, usedSize, ((HEAPSIZE) - usedSize)}; 
    return r;
}

/*PRETTY PRINT MEMORY STATUS*/
void printStats()
{
    struct GCstats stats = gc_stats();
    printf("\n");
    /* 15 char spacing */
    printf("               objects          %d\n", stats.count);
    printf("HEAP STATUS    used memory      %d\n", stats.used);
    printf("               available memory %d\n", stats.free);
    printf("\n");
    
}
/* ----------------------------END-STATS------------------------------------ */




static bool marked (struct GCobject *o)
{
    /* So we know that the pointer is inside an array of bytes */

    if((byte) ((size_t)o->class+1) == 'M')
    {
        return true;
    }
    else if((byte) ((size_t)o->class+1) == 'U')
    {
        return false;
    }
    else
    {
        printf("Bug with pointer arithmetic on mark byte. Returned false.\n");
        return false;
    }
}

void gc_mark (struct GCobject *o)
{
   /* ¡!¡ À REMPLIR !¡! */
}

/*
struct GCobject {
   struct GCclass *class;
};

struct GCclass {
   int size;
   void (*mark) (struct GCobject *o);
};
*/

int *intMalloc(char array[], int position)
{
    return (int*) &array[position];
}
    
    

void gc_protect (struct GCroot *r)
{
   assert (r->next == NULL); /* Pour notre usage, pas celui du mutateur!  */
  
}

void gc_unprotect (struct GCroot *r)
{
   /* ¡!¡ À REMPLIR !¡! */
}

int garbage_collect (void)
{
   /* ¡!¡ À REMPLIR !¡! */
}




struct ListInt {
   struct GCclass *class;
   int n;
   struct ListInt *next;
};

void test1(void)
{
    addPage(250);
    addPage(1000);
//     pool[251] = 'M';
    printStats();
    printAllPages();
    
    printf("\n------------------------------------------------------------------\n");
    defrag();
    printAllPages();
    
    printf("\n------------------------------------------------------------------\n");
    printPage(LASTPAGE);
    addPage(499999);
    addPage(4999999);
    
    printf("\n------------------------------------------------------------------\n");
    printAllPages();
    printf("\n------------------------------------------------------------------\n");
    printStats();
    printf("free position is now %d\n", freep);
}





void main(void)
{
    test1();
   
 
}