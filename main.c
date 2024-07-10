#if 0
#!/usr/bin/env sh
set -e # stop on first error
# doing flags as array allows to comment them out
CFLAGS=(
    "-std=c89"
    "-Wall"
    "-Wextra"
    "-Wshadow"
    "-Wswitch-enum"
   #"-Wmissing-declarations"
    "-Wno-vla" # variable length arrays
    "-Wno-deprecated-declarations"
    "-Wno-misleading-indentation"
    "-pedantic"
    "-ggdb"
)
cc "$0" "${CFLAGS[@]}" -o a.out && ./a.out
exit
#endif

#include <stdio.h>
#include <assert.h>
#include <stdarg.h> /* for va_arg */
#include <string.h> /* for memcpy */

/*
 * The user is responsible for allocating and providing a table for the data.
 * It should be an array of however many maximum amount of items are expected.
 * An assumption is made that the key is string of characters and is the first
 * field in the item structure.
 *
 * Example:
 *                   vvvvv IMPORTANT! start your struct with the key
 *  typedef struct { char* key; int val; } Item;
 *  #define LEN 997
 *  Item i;
 *  Item items[LEN]={}; // remember to initialize your collection to zero
 *  RobinHT_Set(items, LEN, sizeof(Item), (Item){ "New element", 123 });
 *  int found = RobinHT_Get(items, LEN, sizeof(Item), "New element", &i);
 *  if (found) {
 *    RobinHT_Rem(items, LEN, sizeof(Item), "New element");
 *  }
 *
 * If item is not in the table the zeroed out struct will be returned:
 *
 *  if(i.key) { found     }
 *  else      { not found }
 *
 * For convenience and to avoid errors it is advised to wrap that in a macros:
 *
 *  #define ht_set(...) RobinHT_Set(items, LEN, sizeof(Item), (Item){__VA_ARGS__})
 *  #define ht_get(key) RobinHT_Get(items, LEN, sizeof(Item), key, &i), i
 *  #define ht_rem(key) RobinHT_Rem(items, LEN, sizeof(Item), key)
 *  ht_set("New element", 123);
 *  i = ht_get("New element");
 *  if (i.key) {
 *      ht_rem("New element");
 *  }
 *
 * NOTE: Suggested size for the items buffer is a large prime value.
 *       It will improve the distribution of items within the table.
 *       It is also advised for the size to be quite larger then the
 *       maximum expected amount of data to avoid hash collisions.
 *
 * NOTE: The key cannot be empty string as that is used to represent
 *       empty slot in the table.
 *
 * BONUS: Notice that you can use the RobinHT as a Hash Set:
 *
 *  char *hs[LEN]={};
 *  #define hs_set(key) RobinHT_Set(hs, LEN, sizeof(char*), key)
 *  #define hs_has(key) RobinHT_Get(hs, LEN, sizeof(char*), key, NULL)
 *  #define hs_rem(key) RobinHT_Rem(hs, LEN, sizeof(char*), key)
 *
 *  hs_set("One");
 *  hs_set("Two");
 *  if(hs_has("One"))   { true  }
 *  if(hs_has("Two"))   { true  }
 *  if(hs_has("Three")) { false }
 *  hs_rem("Two");
 *  if(hs_has("Two"))   { false }
 */

/* Assumes that the key is char* as first field in the item. */
long RobinHT_Hash(char* item){
    static const int rol=5;
    unsigned long hash = 0x5555555555555555;
    while (*item){
        hash^=*item++; /* this below is rol(hash,5) */
        hash =(hash<<rol)|(hash>>(sizeof(hash)-rol));
    }
    return hash;
}

void RobinHT_Set(char* buf, size_t buflen, size_t itemsiz, ...){
    va_list args;
    char *item,*dst,*swap,itembuf[itemsiz],swapbuf[itemsiz];
    long itemhash,dsthash;
    size_t i,scanned,itemi,dsti;
    assert(buf);
    assert(buflen>0);
    assert(itemsiz>0);
    /* extract item data from variadic arguments */
    va_start(args,itemsiz);
    for(i=0;i<itemsiz;i++) itembuf[i]=va_arg(args,int);
    va_end(args);
    assert(itembuf[0]);
    /* calculate hash and find place for the item */
    swap=swapbuf,item=itembuf;
    itemhash=RobinHT_Hash(item);
    itemi=itemhash%buflen;
    for(scanned=0,i=itemi;;i=(i+1)%buflen){
        dst=buf+(i*itemsiz);
        /* slot is empty or has the same key */
        if(*dst==0||(*item==*dst&&!strcmp(item+1,dst+1))){
            /* Compare first character before even calling the strcmp().
               This yelds performance improvements in most cases. */
            memcpy(dst,item,itemsiz);
            return;
        }
        /* different key - decide which item to move out */
        dsthash=RobinHT_Hash(dst);
        dsti=dsthash%buflen;
        /* if our item is further from its supposed place then it
         * stays here and we move out the "richer" item */
        if(i-dsti<i-itemi){
            memcpy(swap,dst,itemsiz);
            memcpy(dst,item,itemsiz);
            dst=item;
            item=swap;
            swap=dst;
            itemi=dsti;
            itemhash=dsthash;
        }
        assert(++scanned<buflen && "no empty slots in the given buf");
    }
    assert(0 && "should never reach here");
}

size_t __RobinHT_Find(char* buf, size_t buflen, size_t itemsiz, char* key){
    long itemhash;
    char *dst;
    size_t i,scanned;
    assert(buf);
    assert(buflen>0);
    assert(itemsiz>0);
    assert(key);
    itemhash=RobinHT_Hash(key);
    for(scanned=0,i=itemhash%buflen;;i=(i+1)%buflen){
        dst=buf+(i*itemsiz);
        if(*dst&&*dst==*key&&!strcmp(dst+1,key+1)) return i;
        if(++scanned>=buflen) return -1; /* not found */
    }
    assert(0 && "should never reach here");
    return -1;
}

/* returns 1 if item was found, 0 otherwise */
int RobinHT_Get(char* buf, size_t buflen, size_t itemsiz, char* key, char* itembuf){
    size_t i=__RobinHT_Find(buf,buflen,itemsiz,key);
    if(i==(size_t)-1) return 0;
    memcpy(itembuf,buf+(i*itemsiz),itemsiz);
    return 1;
}

/* returns 1 if item was found and removed, 0 otherwise */
int RobinHT_Rem(char* buf, size_t buflen, size_t itemsiz, char* key){
    size_t i = __RobinHT_Find(buf,buflen,itemsiz,key);
    if(i==(size_t)-1) return 0;
    buf[i*itemsiz]=0;
    return 1;
}

int main(void) {
    return 0;
}
