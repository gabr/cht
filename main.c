#if 0
./build.sh && ./a.out
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
 *  Item i = { "New element", 123 };
 *  Item items[LEN]={}; // remember to initialize your collection to zero
 *  RobinHT_Set(items, LEN, sizeof(Item), &i);
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
 *  Item i;
 *  #define ht_set(...) i=(Item){__VA_ARGS__}; RobinHT_Set(items, LEN, sizeof(Item), &i)
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
 * NOTE: The key cannot be NULL as that represent empty slot in the table.
 *
 * BONUS: Notice that you can use the RobinHT as a Hash Set:
 *
 *  char *hs[LEN]={};
 *  #define hs_set(key) RobinHT_Set(hs, LEN, sizeof(char*), &key)
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

#define __RobinHT_Dist(x) (i<x?(buflen+i-x):(i-x))
#define __RobinHT_Key(i)  (*((char **)(i)))

int RobinHT_Hash(char* key){
    static const int rol=5;
    unsigned int hash = 0x55555555;
    while (*key){
        hash^=*key++; /* this below is rol(hash,5) */
        hash =(hash<<rol)|(hash>>(sizeof(hash)-rol));
    }
    return hash;
}

void RobinHT_Set(void* buf, size_t buflen, size_t itemsiz, void* itemdata){
    char *key,*dstkey,*items,*item,*dst,*swap,swapbuf[itemsiz];
    int itemhash,dsthash;
    size_t i,scanned,itemi,dsti;
    assert(buf);
    assert(buflen>0);
    assert(buflen<(size_t)-1);
    assert(itemsiz>0);
    assert(itemsiz<=buflen);
    /* calculate hash and find place for the item */
    item=itemdata;
    key=__RobinHT_Key(item);
    assert(key);
    swap=swapbuf,items=buf;
    itemhash=RobinHT_Hash(key);
    itemi=itemhash%buflen;
    for(scanned=0,i=itemi;;i=(i+1)%buflen){
        dst=items+(i*itemsiz);
        dstkey=__RobinHT_Key(dst);
        /* slot is empty or has the same key */
        if(!dstkey||(*key==*dstkey&&!strcmp(key+1,dstkey+1))){
            /* Compare first character before even calling the strcmp().
               This yelds performance improvements in most cases. */
            memcpy(dst,item,itemsiz);
            return;
        }
        /* different key - decide which item to move out */
        dsthash=RobinHT_Hash(dstkey);
        dsti=dsthash%buflen;
        /* if our item is further from its supposed place then it
         * stays here and we move out the "richer" item */
        if(__RobinHT_Dist(dsti)<__RobinHT_Dist(itemi)){
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

size_t __RobinHT_Find(void* buf, size_t buflen, size_t itemsiz, char* key){
    char *dst,*dstkey,*items;
    int itemhash,dsthash;
    size_t i,itemi,dsti,scanned;
    assert(buf);
    assert(buflen>0);
    assert(buflen<(size_t)-1);
    assert(itemsiz>0);
    assert(itemsiz<=buflen);
    assert(key);
    items=buf;
    itemhash=RobinHT_Hash(key);
    itemi=itemhash%buflen;
    for(scanned=0,i=itemi;;i=(i+1)%buflen){
        dst=items+(i*itemsiz);
        dstkey=__RobinHT_Key(dst);
        if(!dstkey) return -1; /* empty slot */
        if(*dstkey==*key&&!strcmp(dstkey+1,key+1)) return i;
        dsthash=RobinHT_Hash(dstkey);
        dsti=dsthash%buflen;
        /* we should have found our item by now as the current dst one
           has lover distance to its original index then our item which
           will never happen */
        if(__RobinHT_Dist(dsti)<__RobinHT_Dist(itemi)) return -1;
        if(++scanned>=buflen) return -1; /* not found */
    }
    assert(0 && "should never reach here");
    return -1;
}

/* returns 1 if item was found, 0 otherwise */
int RobinHT_Get(void* buf, size_t buflen, size_t itemsiz, char* key, void* itembuf){
    size_t i=__RobinHT_Find(buf,buflen,itemsiz,key);
    if(i==(size_t)-1) return 0;
    if(itembuf) memcpy(itembuf,((char*)buf)+(i*itemsiz),itemsiz);
    return 1;
}

/* returns 1 if item was found and removed, 0 otherwise */
int RobinHT_Rem(void* buf, size_t buflen, size_t itemsiz, char* key){
    char *items;
    size_t i,dsti;
    int dsthash;
    i=__RobinHT_Find(buf,buflen,itemsiz,key);
    if(i==(size_t)-1) return 0;
    items=buf;
    /* clear found item and move next ones back */
    bzero((void*)(items+(i*itemsiz)), sizeof(char*));
    for(i=(i+1)%buflen;__RobinHT_Key(items+(i*itemsiz));i=(i+1)%buflen){
        dsthash=RobinHT_Hash(items+(i*itemsiz));
        dsti=dsthash%buflen;
        if(i-dsti==0) break;
        memcpy(items+((i-1)%buflen*itemsiz),items+(i*itemsiz),itemsiz);
    }
    return 1;
}

int main(void) {
    typedef struct { char* key; int val; } Item;
    #define LEN 997
    int found;
    Item i={ "New element", 123 };
    Item items[LEN]={0};
    RobinHT_Set(items, LEN, sizeof(Item), &i);
    found = RobinHT_Get(items, LEN, sizeof(Item), "New element", &i);
    assert(found);
    if (found) {
      assert(RobinHT_Rem(items, LEN, sizeof(Item), "New element"));
    }
    found = RobinHT_Get(items, LEN, sizeof(Item), "New element", &i);
    assert(!found);
}
