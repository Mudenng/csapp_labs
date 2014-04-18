#include "cachelab.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>

#define NAME_LEN 100
#define BUF_SIZE 100
#define ADDR_BITS 64

#define SET_INDEX(s, b, t, ADDR) (ADDR) >> (b) & ((1 << (s)) - 1)
#define LABEL(s, b, t, ADDR) (ADDR) >> ((b) + (s)) & ((1 << (t)) - 1)

struct cache_line {
        int vaild;
        int life;
        uint64_t label;
};

struct cache_set {
        struct cache_line *lines;
        int num_lines;
};

struct cache {
        struct cache_set *sets;
        int s;  /* Num of set index bits */
        int E;  /* Num of lines per set */
        int b;  /* Num of block bits */
        int t;  /* Num of label bits */
        int hits;
        int misses;
        int evicts;
};

struct cache cache;

enum cstate {
        HIT,
        MISS,
        EVICT,
};

enum cstate load(uint64_t addr) {
        int set_index = SET_INDEX(cache.s, cache.b, cache.t, addr);
        uint64_t label = LABEL(cache.s, cache.b, cache.t, addr);
        int i, flag = 0;
        struct cache_line *line;
        enum cstate rs;
        for (i = 0; i < cache.E; i++) {
                line = &(cache.sets[set_index].lines[i]);
                line->life++;
                if (flag == 0 && label == line->label && line->vaild == 1) {
                        //printf("L %x hit\n", addr);
                        flag = 1;
                        line->life = 0;
                        cache.hits++;
                        rs = HIT;
                }
        }
        if (flag == 0) {
                int select = 0;
                int oldest_age = 0;
                for (i = 0; i < cache.E; i++) {
                        line = &(cache.sets[set_index].lines[i]);
                        if (line->vaild == 0) {
                                select = i;
                                break;
                        }
                        if (line->life > oldest_age) {
                                select = i;
                                oldest_age = line->life;
                        }
                }
                if (i < cache.E) {
                        cache.sets[set_index].lines[select].label = label;
                        cache.sets[set_index].lines[select].vaild = 1;
                        cache.sets[set_index].lines[select].life = 0;
                        //printf("L %x miss\n", addr);
                        cache.misses++;
                        rs = MISS;
                } else {
                        cache.sets[set_index].lines[select].label = label;
                        cache.sets[set_index].lines[select].vaild = 1;
                        cache.sets[set_index].lines[select].life = 0;
                        //printf("L %x miss evict\n", addr);
                        cache.misses++;
                        cache.evicts++;
                        rs = EVICT;
                }
        }
        return rs;
}

enum cstate store(uint64_t addr) {
        int set_index = SET_INDEX(cache.s, cache.b, cache.t, addr);
        uint64_t label = LABEL(cache.s, cache.b, cache.t, addr);
        int i, flag = 0;
        struct cache_line *line;
        enum cstate rs;
        for (i = 0; i < cache.E; i++) {
                line = &(cache.sets[set_index].lines[i]);
                line->life++;
                if (flag == 0 && label == line->label && line->vaild == 1) {
                        //printf("S %x hit\n", addr);
                        flag = 1;
                        line->life = 0;
                        cache.hits++;
                        rs = HIT;
                }
        }
        if (flag == 0) {
                int select = 0;
                int oldest_age = 0;
                for (i = 0; i < cache.E; i++) {
                        line = &(cache.sets[set_index].lines[i]);
                        if (line->vaild == 0) {
                                select = i;
                                break;
                        }
                        if (line->life > oldest_age) {
                                select = i;
                                oldest_age = line->life;
                        }
                }
                if (i < cache.E) {
                        cache.sets[set_index].lines[select].label = label;
                        cache.sets[set_index].lines[select].vaild = 1;
                        cache.sets[set_index].lines[select].life = 0;
                        //prjntf("S %x miss\n", addr);
                        cache.misses++;
                        rs = MISS;
                } else {
                        cache.sets[set_index].lines[select].label = label;
                        cache.sets[set_index].lines[select].vaild = 1;
                        cache.sets[set_index].lines[select].life = 0;
                        //printf("S %x miss evict\n", addr);
                        cache.misses++;
                        cache.evicts++;
                        rs = EVICT;
                }
        }
        return rs;
}

uint64_t xtoi(char *str) {
        uint64_t n = 0;
        char c;
        int i = 0;
        while ((c = str[i++]) != '\0') {
                switch(c) {
                        case 'a':
                                n = n * 16 + 10;
                                break;
                        case 'b':
                                n = n * 16 + 11;
                                break;
                        case 'c':
                                n = n * 16 + 12;
                                break;
                        case 'd':
                                n = n * 16 + 13;
                                break;
                        case 'e':
                                n = n * 16 + 14;
                                break;
                        case 'f':
                                n = n * 16 + 15;
                                break;
                        default:
                                n = n * 16 + (c - '0');
                }
        }
        return n;
}


int main(int argc, char **argv) {
        char filename[NAME_LEN];
        int opt;
        int verbose = 0;

        /* init cache */
        while ((opt = getopt(argc, argv, "s:E:b:t:v")) != -1) {
                switch (opt) {
                        case 's':
                                cache.s = atoi(optarg);
                                break;
                        case 'E':
                                cache.E = atoi(optarg);
                                break;
                        case 'b':
                                cache.b = atoi(optarg);
                                break;
                        case 't':
                                strncpy(filename, optarg, NAME_LEN);
                                break;
                        case 'v':
                                verbose = 1;
                                break;
                        default:
                                exit(-1);
                }
        }
        cache.t = ADDR_BITS - cache.s - cache.b;

        cache.hits = cache.misses = cache.evicts = 0;

        cache.sets = (struct cache_set *) malloc((1 << cache.s) * sizeof(struct cache_set));

        int i;
        for (i = 0; i < (1 << cache.s); i++) {
                cache.sets[i].num_lines = cache.E;
                cache.sets[i].lines = (struct cache_line *) calloc(cache.E, sizeof(struct cache_line));
        }

        FILE *fp = fopen(filename, "r");
        char buf[BUF_SIZE];
        while (fgets(buf, BUF_SIZE, fp) != NULL) {
                if (buf[0] != ' ') {
                        continue;
                }
                char *start = buf;
                char *p[5];
                char *ptr;
                i = 0;
                while ((p[i] = strtok_r(start, " ", &ptr)) != NULL) {
                      i++;
                      start = NULL;
                }
                ptr = strchr(p[1], ',');
                *ptr = '\0';
                uint64_t addr = xtoi(p[1]);
                if (strcmp(p[0], "L") == 0) {
                        int r = load(addr);
                        if (verbose) {
                                if (r == HIT) {
                                        printf("L %x hit\n", addr);
                                } else if (r == MISS) {
                                        printf("L %x miss\n", addr);
                                } else if (r == EVICT) {
                                        printf("L %x miss evict\n", addr);
                                }
                        }
                }
                else if (strcmp(p[0], "S") == 0) {
                        int r = store(addr);
                        if (verbose) {
                                if (r == HIT) {
                                        printf("S %x hit\n", addr);
                                } else if (r == MISS) {
                                        printf("S %x miss\n", addr);
                                } else if (r == EVICT) {
                                        printf("S %x miss evict\n", addr);
                                }
                        }
                }
                else if (strcmp(p[0], "M") == 0) {
                        int r = load(addr);
                        if (verbose) {
                                if (r == HIT) {
                                        printf("M %x hit", addr);
                                } else if (r == MISS) {
                                        printf("M %x miss", addr);
                                } else if (r == EVICT) {
                                        printf("M %x miss evict", addr);
                                }
                        }
                        r = store(addr);
                        if (verbose) {
                                if (r == HIT) {
                                        printf(" hit\n", addr);
                                } else if (r == MISS) {
                                        printf(" miss\n", addr);
                                } else if (r == EVICT) {
                                        printf(" miss evict\n", addr);
                                }
                        }
                }
        }
        
        printSummary(cache.hits, cache.misses, cache.evicts);
        return 0;
}
