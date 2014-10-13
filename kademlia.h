//
//  kademlia.h
//  libjl777
//
//  Created by jl777 on 10/3/14.
//  Copyright (c) 2014 jl777. MIT license.
//

#ifndef libjl777_kademlia_h
#define libjl777_kademlia_h

#define KADEMLIA_ALPHA 7
#define NODESTATS_EXPIRATION 600
#define KADEMLIA_BUCKET_REFRESHTIME 3600
#define KADEMLIA_NUMBUCKETS ((int)(sizeof(K_buckets)/sizeof(K_buckets[0])))
#define KADEMLIA_NUMK ((int)(sizeof(K_buckets[0])/sizeof(K_buckets[0][0])))
struct nodestats *K_buckets[64+1][6];
long Kbucket_updated[KADEMLIA_NUMBUCKETS];
uint64_t Allnodes[10000];
int32_t Numallnodes;
#define MAX_ALLNODES ((int32_t)(sizeof(Allnodes)/sizeof(*Allnodes)))

struct kademlia_store
{
    uint64_t keyhash;
    unsigned char *data;
    uint32_t datalen,laststored,lastaccess;
};
struct kademlia_store K_store[10000];

void add_new_node(uint64_t nxt64bits)
{
    int32_t i;
    if ( Numallnodes > 0 )
    {
        for (i=0; i<MAX_ALLNODES; i++)
        {
            if ( Allnodes[i] == nxt64bits )
                return;
        }
    }
    printf("[%d of %d] ADDNODE.%llu\n",Numallnodes,MAX_ALLNODES,(long long)nxt64bits);
    Allnodes[Numallnodes % MAX_ALLNODES] = nxt64bits;
    Numallnodes++;
}

struct nodestats *get_random_node()
{
    struct nodestats *stats;
    int32_t n;
    if ( Numallnodes > 0 )
    {
        n = (Numallnodes < MAX_ALLNODES) ? Numallnodes : MAX_ALLNODES;
        if ( (stats= get_nodestats(Allnodes[(rand()>>8) % n])) != 0 )
            return(stats);
    }
    return(0);
}

struct kademlia_store *kademlia_getstored(uint64_t keyhash,int32_t storeflag)
{
    int32_t i,oldi = -1;
    uint32_t oldest = 0;
    for (i=0; i<(int)(sizeof(K_store)/sizeof(*K_store)); i++)
    {
        if ( K_store[i].keyhash == 0 || K_store[i].keyhash == keyhash )
        {
            if ( storeflag == 0 && K_store[i].keyhash == 0 )
                return(0);
            printf("kademlia_store[%d] %s %llu\n",i,K_store[i].keyhash == keyhash?"->":"<-",(long long)keyhash);
            K_store[i].keyhash = keyhash;
            K_store[i].lastaccess = (uint32_t)time(NULL);
            return(&K_store[i]);
        }
        if ( oldest == 0 || K_store[i].laststored < oldest )
        {
            oldest = K_store[i].lastaccess;
            oldi = i;
        }
    }
    if ( storeflag != 0 )
    {
        printf("kademlia_store[oldi %d] <- %llu\n",i,(long long)keyhash);
        K_store[oldi].keyhash = keyhash;
        K_store[oldi].laststored = K_store[i].lastaccess = (uint32_t)time(NULL);
        if ( K_store[oldi].data != 0 )
        {
            free(K_store[oldi].data);
            K_store[oldi].data = 0;
            K_store[oldi].datalen = 0;
        }
        return(&K_store[oldi]);
    }
    return(0);
}

int32_t calc_np_dist(struct NXT_acct *np,struct NXT_acct *destnp)
{
    uint64_t a,b;
    a = calc_nxt64bits(np->H.U.NXTaddr);
    b = calc_nxt64bits(destnp->H.U.NXTaddr);
    return(bitweight(a ^ b));
}

int32_t verify_addr(struct sockaddr *addr,char *refipaddr,int32_t refport)
{
    int32_t port;
    char ipaddr[64];
    port = extract_nameport(ipaddr,sizeof(ipaddr),(struct sockaddr_in *)addr);
    if ( strcmp(ipaddr,refipaddr) != 0 )//|| refport != port )
        return(-1);
    return(0);
}

int32_t ismynxtbits(uint64_t destbits)
{
    struct coin_info *cp = get_coin_info("BTCD");
    if ( cp != 0 )
    {
        if ( destbits == cp->pubnxtbits || destbits == cp->srvpubnxtbits )
            return(1);
        else return(0);
    } else return(-1);
}

int32_t ismyipaddr(char *ipaddr)
{
    struct coin_info *cp = get_coin_info("BTCD");
    if ( cp != 0 )
    {
        if ( strcmp("127.0.0.1",ipaddr) == 0 || strcmp(cp->myipaddr,ipaddr) == 0 )
            return(1);
        else return(0);
    } else return(-1);
}

int32_t ismynode(struct sockaddr *addr)
{
    char ipaddr[64];
    int32_t port;
    if ( addr == 0 )
        return(1);
    port = extract_nameport(ipaddr,sizeof(ipaddr),(struct sockaddr_in *)addr);
    return(ismyipaddr(ipaddr));
}

uint32_t addto_hasips(int32_t recalc_flag,struct pserver_info *pserver,uint32_t ipbits)
{
    static int didinit;
    static portable_mutex_t mutex;
    int32_t i;
    uint32_t xorsum = 0;
    if ( didinit == 0 )
    {
        portable_mutex_init(&mutex);
        didinit = 1;
    }
    if ( ipbits == 0 )
        return(0);
    portable_mutex_lock(&mutex);
    if ( pserver->hasips != 0 && pserver->numips > 0 )
    {
        for (i=0; i<pserver->numips; i++)
        {
            //fprintf(stderr,"%x ",ipbits);
            if ( pserver->hasips[i] == ipbits )
            {
                portable_mutex_unlock(&mutex);
                return(0);
            }
        }
    }
    {
        char ipstr[64];
        expand_ipbits(ipstr,ipbits);
        printf("addto_hasips %p num.%d <- %x %s\n",pserver->hasips,pserver->numips,ipbits,ipstr);
    }
    pserver->hasips = realloc(pserver->hasips,sizeof(*pserver->hasips) + (pserver->numips + 1));
    pserver->hasips[pserver->numips] = ipbits;
    pserver->numips++;
    if ( recalc_flag != 0 )
    {
        for (i=0; i<pserver->numips; i++)
            xorsum ^= pserver->hasips[i];
        pserver->xorsum = xorsum;
        pserver->hasnum = pserver->numips;
    }
    portable_mutex_unlock(&mutex);
    return(xorsum);
}

int32_t sort_all_buckets(uint64_t *sortbuf,uint64_t hash)
{
    //uint64_t *sortbuf = 0;
    struct nodestats *stats;
    int32_t i,j,n;
    //long now = time(NULL);
    //sortbuf = calloc(2 * sizeof(*sortbuf),KADEMLIA_NUMBUCKETS * KADEMLIA_NUMK);
    for (i=n=0; i<KADEMLIA_NUMBUCKETS; i++)
    {
        for (j=0; j<KADEMLIA_NUMK; j++)
        {
            if ( (stats= K_buckets[i][j]) == 0 )
                break;
            //if ( stats->numsent > stats->numrecv+10 )//&& (now - stats->lastcontact) > 3600 )
            //    continue;
            sortbuf[n<<1] = bitweight(stats->nxt64bits ^ hash);// + ((stats->gotencrypted == 0) ? 64 : 0);
            sortbuf[(n<<1) + 1] = stats->nxt64bits;
            n++;
        }
    }
    //*nump = n;
    if ( n == 0 )
        return(0);
    else
    {
        //sortbuf = realloc(sortbuf,n * sizeof(uint64_t) * 2);
        sort64s(sortbuf,n,sizeof(*sortbuf)*2);
    }
    return(n);
}

int32_t calc_bestdist(uint64_t keyhash)
{
    int32_t i,j,n,dist,bestdist = 10000;
    struct nodestats *stats;
    if ( kademlia_getstored(keyhash,0) != 0 )
        return(0);
    for (i=n=0; i<KADEMLIA_NUMBUCKETS; i++)
    {
        for (j=0; j<KADEMLIA_NUMK; j++)
        {
            if ( (stats= K_buckets[i][j]) == 0 )
                break;
            dist = bitweight(stats->nxt64bits ^ keyhash);
            if ( dist < bestdist )
                bestdist = dist;
        }
    }
    return(bestdist);
}

uint64_t _send_kademlia_cmd(int32_t encrypted,struct pserver_info *pserver,char *cmdstr,char *NXTACCTSECRET,unsigned char *data,int32_t datalen)
{
    int32_t len = (int32_t)strlen(cmdstr);
    char _tokbuf[4096];
    uint64_t txid;
    len = construct_tokenized_req(_tokbuf,cmdstr,NXTACCTSECRET);
    printf(">>>>>>>> directsend.[%s]\n",_tokbuf);
    txid = directsend_packet(encrypted,pserver,_tokbuf,len,data,datalen);
    return(txid);
}

uint64_t send_kademlia_cmd(uint64_t nxt64bits,struct pserver_info *pserver,char *kadcmd,char *NXTACCTSECRET,char *key,char *datastr)
{
    int32_t encrypted,createdflag,len = 0;
    struct nodestats *stats;
    struct NXT_acct *np;
    unsigned char databuf[32768],*data = 0;
    struct coin_info *cp = get_coin_info("BTCD");
    char pubkeystr[1024],ipaddr[64],cmdstr[2048],verifiedNXTaddr[64],destNXTaddr[64];
    if ( NXTACCTSECRET[0] == 0 || cp == 0 )
    {
        printf("No srvpubaddr or cp.%p NULL\n",cp);
        return(0);
    }
    init_hexbytes_noT(pubkeystr,Global_mp->loopback_pubkey,sizeof(Global_mp->loopback_pubkey));
    verifiedNXTaddr[0] = 0;
    find_NXTacct(verifiedNXTaddr,NXTACCTSECRET);
    if ( pserver == 0 )
    {
        expand_nxt64bits(destNXTaddr,nxt64bits);
        np = get_NXTacct(&createdflag,Global_mp,destNXTaddr);
        expand_ipbits(ipaddr,np->stats.ipbits);
        pserver = get_pserver(0,ipaddr,0,0);
        if ( pserver->nxt64bits == 0 )
            pserver->nxt64bits = nxt64bits;
    } else nxt64bits = pserver->nxt64bits;
    if ( 0 && (pserver->nxt64bits == cp->pubnxtbits || pserver->nxt64bits == cp->srvpubnxtbits) )
    {
        printf("no point to send yourself (%s) dest.%llu pub.%llu srvpub.%llu\n",kadcmd,(long long)pserver->nxt64bits,(long long)cp->pubnxtbits,(long long)cp->srvpubnxtbits);
        return(0);
    }
    encrypted = 1;
    if ( strcmp(kadcmd,"ping") == 0 )
    {
        encrypted = 0;
        stats = get_nodestats(pserver->nxt64bits);
        if ( stats != 0 )
        {
            stats->pingmilli = milliseconds();
            stats->numpings++;
        }
        sprintf(cmdstr,"{\"requestType\":\"%s\",\"NXT\":\"%s\",\"time\":%ld,\"pubkey\":\"%s\",\"ipaddr\":\"%s\"",kadcmd,verifiedNXTaddr,(long)time(NULL),pubkeystr,cp->myipaddr);
    }
    else
    {
        if ( 0 && strcmp(kadcmd,"pong") == 0 )
            encrypted = 0;
        sprintf(cmdstr,"{\"requestType\":\"%s\",\"NXT\":\"%s\",\"time\":%ld,\"pubkey\":\"%s\"",kadcmd,verifiedNXTaddr,(long)time(NULL),pubkeystr);
    }
    if ( key != 0 && key[0] != 0 )
        sprintf(cmdstr+strlen(cmdstr),",\"key\":\"%s\"",key);
    if ( datastr != 0 && datastr[0] != 0 )
    {
        len = (int32_t)strlen(datastr);
        if ( len < 2 || (len & 1) != 0 || is_hexstr(datastr) == 0 )
        {
            if ( datastr[0] == '[' )
                sprintf(cmdstr+strlen(cmdstr),",\"data\":%s",datastr);
            else sprintf(cmdstr+strlen(cmdstr),",\"data\":\"%s\"",datastr);
        }
        else
        {
            len >>= 1;
            sprintf(cmdstr+strlen(cmdstr),",\"data\":%d",len);
            data = databuf;
            decode_hex(data,len,datastr);
        }
    }
    strcat(cmdstr,"}");
    return(_send_kademlia_cmd(encrypted,pserver,cmdstr,NXTACCTSECRET,data,len));
}

void kademlia_update_info(char *destNXTaddr,char *ipaddr,int32_t port,char *pubkeystr,uint32_t lastcontact,int32_t p2pflag)
{
    uint64_t nxt64bits;
    uint32_t ipbits = 0;
    struct pserver_info *pserver;
    struct nodestats *stats = 0;
    if ( destNXTaddr != 0 && destNXTaddr[0] != 0 )
        nxt64bits = calc_nxt64bits(destNXTaddr);
    else nxt64bits = 0;
    if ( port == BTCD_PORT )
    {
        printf("warning: kademlia_update_info port is %d?\n",port);
        port = 0;
    }
    if ( ipaddr != 0 && ipaddr[0] != 0 )
    {
        ipbits = calc_ipbits(ipaddr);
        pserver = get_pserver(0,ipaddr,p2pflag==0?port:0,p2pflag!=0?port:0);
        if ( nxt64bits != 0 && (pserver->nxt64bits == 0 || pserver->nxt64bits != nxt64bits) )
        {
            printf("kademlia_update_info: pserver nxt64bits %llu -> %llu\n",(long long)pserver->nxt64bits,(long long)nxt64bits);
            pserver->nxt64bits = nxt64bits;
        }
    }
    if ( nxt64bits != 0 )
    {
        stats = get_nodestats(nxt64bits);
        if ( stats->nxt64bits == 0 || stats->nxt64bits != nxt64bits )
        {
            printf("kademlia_update_info: nxt64bits %llu -> %llu\n",(long long)stats->nxt64bits,(long long)nxt64bits);
            stats->nxt64bits = nxt64bits;
        }
        if ( ipbits != 0 && (stats->ipbits == 0 || stats->ipbits != ipbits) )
        {
            printf("kademlia_update_info: stats ipbits %u -> %u\n",stats->ipbits,ipbits);
            stats->ipbits = ipbits;
        }
        if ( port != 0 )
        {
            if ( p2pflag != 0 )
            {
                if ( stats->p2pport == 0 || stats->p2pport != port )
                {
                    printf("kademlia_update_info: p2pport %u -> %u\n",stats->p2pport,port);
                    stats->p2pport = port;
                }
            }
            else
            {
                if ( stats->supernet_port == 0 || stats->supernet_port != port )
                {
                    printf("kademlia_update_info: supernet_port %u -> %u\n",stats->supernet_port,port);
                    stats->supernet_port = port;
                }
            }
        }
        if ( pubkeystr != 0 && pubkeystr[0] != 0 && update_pubkey(stats->pubkey,pubkeystr) != 0 && lastcontact != 0 )
            stats->lastcontact = lastcontact;
        /*add_peerinfo(peer);
        expand_nxt64bits(srvNXTaddr,pserver->nxt64bits);
        if ( memcmp(stats->pubkey,zerokey,sizeof(stats->pubkey)) != 0 )
            init_hexbytes_noT(pubkeystr,stats->pubkey,sizeof(stats->pubkey));
        else pubkeystr[0] = 0;
        int32_t set_pubpeerinfo(char *srvNXTaddr,char *srvipaddr,int32_t srvport,struct peerinfo *peer,char *pubBTCD,char *pubkey,uint64_t pubnxtbits,char *pubBTC);
        struct peerinfo *update_peerinfo(int32_t *createdflagp,struct peerinfo *refpeer);
        set_pubpeerinfo(srvNXTaddr,ipaddr,port,&peer,0,pubkeystr,pserver->nxt64bits,0);
        update_peerinfo(&createdflag,&peer);
        fprintf(stderr,"finished updating peer\n");*/
    }
}

char *kademlia_ping(struct sockaddr *prevaddr,char *verifiedNXTaddr,char *NXTACCTSECRET,char *sender,char *pubkey,char *ipaddr,int32_t port,char *destip)
{
    uint64_t txid = 0;
    char retstr[1024];
    if ( ismynode(prevaddr) != 0 ) // user invoked
    {
        if ( destip != 0 && destip[0] != 0 && ismyipaddr(destip) == 0 )
        {
            if ( ismyipaddr(destip) == 0 )
                txid = send_kademlia_cmd(0,get_pserver(0,destip,0,0),"ping",NXTACCTSECRET,0,0);
            sprintf(retstr,"{\"result\":\"kademlia_ping to %s\",\"txid\",\"%llu\"}",destip,(long long)txid);
        }
        else sprintf(retstr,"{\"error\":\"kademlia_ping no destip\"}");
    }
    else // sender ping'ed us
    {
        if ( verify_addr(prevaddr,ipaddr,port) < 0 )
            sprintf(retstr,"{\"error\":\"kademlia_pong from %s doesnt verify (%s/%d)\"}",sender,ipaddr,port);
        else
        {
            txid = send_kademlia_cmd(0,get_pserver(0,ipaddr,0,0),"pong",NXTACCTSECRET,0,0);
            sprintf(retstr,"{\"result\":\"kademlia_pong to (%s/%d)\",\"txid\",\"%llu\"}",ipaddr,port,(long long)txid);
        }
    }
    printf("PING.(%s)\n",retstr);
    return(clonestr(retstr));
}

char *kademlia_pong(struct sockaddr *prevaddr,char *verifiedNXTaddr,char *NXTACCTSECRET,char *sender,char *pubkey,char *ipaddr,uint16_t port)
{
    char retstr[1024];
    struct nodestats *stats;
    stats = get_nodestats(calc_nxt64bits(sender));
    // all the work is already done in update_Kbucket
    if ( stats != 0 )
    {
        stats->pongmilli = milliseconds();
        stats->pingpongsum += (stats->pongmilli - stats->pingmilli);
        stats->numpongs++;
        sprintf(retstr,"{\"result\":\"kademlia_pong from NXT.%s (%s/%d) %.3f millis | numpings.%d numpongs.%d ave %.3f\"}",sender,ipaddr,port,stats->pongmilli-stats->pingmilli,stats->numpings,stats->numpongs,(2*stats->pingpongsum)/(stats->numpings+stats->numpongs+1));
    }
    else sprintf(retstr,"{\"result\":\"kademlia_pong from NXT.%s (%s/%d)\"}",sender,ipaddr,port);
    printf("PONG.(%s)\n",retstr);
    return(clonestr(retstr));
}

struct kademlia_store *do_localstore(uint64_t *txidp,char *key,char *datastr,char *NXTACCTSECRET)
{
    uint64_t keybits;
    int32_t createdflag;
    struct NXT_acct *keynp;
    struct kademlia_store *sp;
    keybits = calc_nxt64bits(key);
    printf("halflen.%ld\n",strlen(datastr)/2);
    sp = kademlia_getstored(keybits,1);
    if ( sp->data != 0 )
        free(sp->data);
    sp->datalen = (int32_t)(strlen(datastr) >> 1);
    sp->data = calloc(1,sp->datalen);
    decode_hex(sp->data,sp->datalen,datastr);
    keynp = get_NXTacct(&createdflag,Global_mp,key);
    *txidp = 0;
    if ( keynp->bestbits != 0 )
    {
        if ( ismynxtbits(keynp->bestdist) == 0 )
            *txidp = send_kademlia_cmd(keynp->bestdist,0,"store",NXTACCTSECRET,key,datastr);
        printf("Bestdist.%d bestbits.%llu\n",keynp->bestdist,(long long)keynp->bestbits);
        keynp->bestbits = 0;
        keynp->bestdist = 0;
    }
    return(sp);
}

char *kademlia_storedata(struct sockaddr *prevaddr,char *verifiedNXTaddr,char *NXTACCTSECRET,char *sender,char *pubkey,char *key,char *datastr)
{
    static unsigned char zerokey[crypto_box_PUBLICKEYBYTES];
    char retstr[32768];
    uint64_t sortbuf[2 * KADEMLIA_NUMBUCKETS * KADEMLIA_NUMK];
    uint64_t keybits,destbits,txid = 0;
    int32_t i,n;
    struct coin_info *cp = get_coin_info("BTCD");
    struct nodestats *stats;
    if ( cp == 0 || key == 0 || key[0] == 0 || datastr == 0 || datastr[0] == 0 )
        return(0);
    keybits = calc_nxt64bits(key);
    memset(sortbuf,0,sizeof(sortbuf));
    n = sort_all_buckets(sortbuf,keybits);
    if ( n != 0 )
    {
        if ( ismynode(prevaddr) != 0 )
        {
            for (i=0; i<n&&i<KADEMLIA_NUMK; i++)
            {
                destbits = sortbuf[(i<<1) + 1];
                if ( ismynxtbits(destbits) == 0 )
                {
                    if ( (stats= get_nodestats(destbits)) != 0 )
                    {
                        if ( memcmp(stats->pubkey,zerokey,sizeof(stats->pubkey)) == 0 )
                            send_kademlia_cmd(destbits,0,"ping",NXTACCTSECRET,0,0);
                    }
                    txid = send_kademlia_cmd(destbits,0,"store",NXTACCTSECRET,key,datastr);
                }
                else do_localstore(&txid,key,datastr,NXTACCTSECRET);
            }
        }
        else do_localstore(&txid,key,datastr,NXTACCTSECRET);
        sprintf(retstr,"{\"result\":\"kademlia_store key.(%s) data.(%s) len.%ld -> txid.%llu\"}",key,datastr,strlen(datastr)/2,(long long)txid);
        //free(sortbuf);
    }
    else
    {
        do_localstore(&txid,key,datastr,NXTACCTSECRET);
        sprintf(retstr,"{\"error\":\"kademlia_store key.(%s) no peers, stored locally\"}",key);
    }
    printf("STORE.(%s)\n",retstr);
    return(clonestr(retstr));
}

char *kademlia_havenode(int32_t valueflag,struct sockaddr *prevaddr,char *verifiedNXTaddr,char *NXTACCTSECRET,char *sender,char *pubkey,char *key,char *value)
{
    char retstr[1024],ipaddr[MAX_JSON_FIELD],destNXTaddr[MAX_JSON_FIELD],pubkeystr[MAX_JSON_FIELD],portstr[MAX_JSON_FIELD],lastcontactstr[MAX_JSON_FIELD];
    int32_t i,n,createdflag,dist;
    uint32_t lastcontact,port;
    uint64_t keyhash,txid = 0;
    cJSON *array,*item;
    struct coin_info *cp = get_coin_info("BTCD");
    struct pserver_info *pserver = 0;
    struct NXT_acct *keynp;
    keyhash = calc_nxt64bits(key);
    if ( key != 0 && key[0] != 0 && value != 0 && value[0] != 0 && (array= cJSON_Parse(value)) != 0 )
    {
        if ( ismynode(prevaddr) == 0 )
        {
            extract_nameport(ipaddr,sizeof(ipaddr),(struct sockaddr_in *)prevaddr);
            pserver = get_pserver(0,ipaddr,0,0);
        } else if ( cp != 0 ) pserver = get_pserver(0,cp->myipaddr,0,0);
        keynp = get_NXTacct(&createdflag,Global_mp,key);
        //printf("parsed value array.%p\n",array);
        if ( is_cJSON_Array(array) != 0 )
        {
            n = cJSON_GetArraySize(array);
            //printf("is arraysize.%d\n",n);
            for (i=0; i<n; i++)
            {
                item = cJSON_GetArrayItem(array,i);
                //printf("item.%p isarray.%d num.%d\n",item,is_cJSON_Array(item),cJSON_GetArraySize(item));
                if ( is_cJSON_Array(item) != 0 && cJSON_GetArraySize(item) == 5 )
                {
                    copy_cJSON(destNXTaddr,cJSON_GetArrayItem(item,0));
                    copy_cJSON(pubkeystr,cJSON_GetArrayItem(item,1));
                    copy_cJSON(ipaddr,cJSON_GetArrayItem(item,2));
                    if ( ipaddr[0] != 0 )
                        addto_hasips(1,pserver,calc_ipbits(ipaddr));
                    copy_cJSON(portstr,cJSON_GetArrayItem(item,3));
                    copy_cJSON(lastcontactstr,cJSON_GetArrayItem(item,4));
                    port = (uint32_t)atol(portstr);
                    lastcontact = (uint32_t)atol(lastcontactstr);
                    //printf("[%s ip.%s %s port.%d lastcontact.%d]\n",destNXTaddr,ipaddr,pubkeystr,port,lastcontact);
                    if ( destNXTaddr[0] != 0 && ipaddr[0] != 0 )
                    {
                        kademlia_update_info(destNXTaddr,ipaddr,port,pubkeystr,lastcontact,0);
                        dist = bitweight(keynp->H.nxt64bits ^ calc_nxt64bits(destNXTaddr));
                        if ( dist < calc_bestdist(keyhash) )
                        {
                            printf("%s new bestdist %d vs %d\n",destNXTaddr,dist,keynp->bestdist);
                            keynp->bestdist = dist;
                            keynp->bestbits = calc_nxt64bits(destNXTaddr);
                            if ( ismynxtbits(keynp->bestbits) == 0 )
                                txid = send_kademlia_cmd(keynp->bestbits,0,valueflag!=0?"findvalue":"findnode",NXTACCTSECRET,key,0);
                        }
                    }
                }
            }
        } else printf("kademlia.(havenode) not array\n");
        free_json(array);
    }
    sprintf(retstr,"{\"result\":\"kademlia_havenode from NXT.%s key.(%s) value.(%s)\"}",sender,key,value);
    //printf("HAVENODE.%d %s\n",valueflag,retstr);
    return(clonestr(retstr));
}

char *kademlia_find(char *cmd,struct sockaddr *prevaddr,char *verifiedNXTaddr,char *NXTACCTSECRET,char *sender,char *pubkey,char *key)
{
    static unsigned char zerokey[crypto_box_PUBLICKEYBYTES];
    char retstr[32768],pubkeystr[256],datastr[32768],numstr[64],ipaddr[64],destNXTaddr[64],*value;
    uint64_t keyhash,senderbits,destbits,txid = 0;
    uint64_t sortbuf[2 * KADEMLIA_NUMBUCKETS * KADEMLIA_NUMK];
    int32_t i,n,createdflag;
    struct NXT_acct *keynp;
    struct NXT_acct *destnp;
    cJSON *array,*item;
    struct kademlia_store *sp;
    struct nodestats *stats;
    printf("kademlia_find.(%s) (%s)\n",cmd,key);
    if ( key != 0 && key[0] != 0 )
    {
        senderbits = calc_nxt64bits(sender);
        keyhash = calc_nxt64bits(key);
        if ( strcmp(cmd,"findvalue") == 0 )
        {
            sp = kademlia_getstored(keyhash,0);
            if ( sp != 0 && sp->data != 0 )
            {
                init_hexbytes_noT(datastr,sp->data,sp->datalen);
                if ( ismynode(prevaddr) == 0 && ismynxtbits(senderbits) == 0 )
                {
                    printf("call store\n");
                    txid = send_kademlia_cmd(senderbits,0,"store",NXTACCTSECRET,key,datastr);
                }
                sprintf(retstr,"{\"data\":\"%s\"}",datastr);
                //printf("FOUND.(%s)\n",retstr);
                return(clonestr(retstr));
            }
        }
        memset(sortbuf,0,sizeof(sortbuf));
        n = sort_all_buckets(sortbuf,keyhash);
        if ( n != 0 )
        {
            if ( ismynode(prevaddr) != 0 ) // user invoked
            {
                keynp = get_NXTacct(&createdflag,Global_mp,key);
                keynp->bestdist = 10000;
                keynp->bestbits = 0;
                for (i=0; i<n&&i<KADEMLIA_ALPHA; i++)
                {
                    destbits = sortbuf[(i<<1) + 1];
                    if ( ismynxtbits(destbits) == 0 )
                    {
                        if ( (stats= get_nodestats(destbits)) != 0 && memcmp(stats->pubkey,zerokey,sizeof(stats->pubkey)) == 0 )
                            send_kademlia_cmd(destbits,0,"ping",NXTACCTSECRET,0,0);
                        printf("call %llu (%s)\n",(long long)destbits,cmd);
                        txid = send_kademlia_cmd(destbits,0,cmd,NXTACCTSECRET,key,0);
                    }
                }
                printf("start %d %s\n",i,cmd);
            }
            else if ( ismynxtbits(senderbits) == 0 ) // need to respond to sender
            {
                array = cJSON_CreateArray();
                for (i=0; i<n&&i<KADEMLIA_NUMK; i++)
                {
                    expand_nxt64bits(destNXTaddr,sortbuf[(i<<1) + 1]);
                    destnp = get_NXTacct(&createdflag,Global_mp,destNXTaddr);
                    stats = &destnp->stats;
                    item = cJSON_CreateArray();
                    cJSON_AddItemToArray(item,cJSON_CreateString(destNXTaddr));
                    if ( memcmp(stats->pubkey,zerokey,sizeof(stats->pubkey)) != 0 )
                    {
                        init_hexbytes_noT(pubkeystr,stats->pubkey,sizeof(stats->pubkey));
                        cJSON_AddItemToArray(item,cJSON_CreateString(pubkeystr));
                    }
                    if ( stats->ipbits != 0 )
                    {
                        expand_ipbits(ipaddr,stats->ipbits);
                        cJSON_AddItemToArray(item,cJSON_CreateString(ipaddr));
                        sprintf(numstr,"%d",stats->supernet_port==0?SUPERNET_PORT:stats->supernet_port);
                        cJSON_AddItemToArray(item,cJSON_CreateString(numstr));
                        sprintf(numstr,"%u",stats->lastcontact);
                        cJSON_AddItemToArray(item,cJSON_CreateString(numstr));
                    }
                    cJSON_AddItemToArray(array,item);
                }
                value = cJSON_Print(array);
                free_json(array);
                stripwhite_ns(value,strlen(value));
                printf("call havenode\n");
                txid = send_kademlia_cmd(senderbits,0,strcmp(cmd,"findnode")==0?"havenode":"havenodeB",NXTACCTSECRET,key,value);
                free(value);
            }
            //free(sortbuf);
        } else printf("kademlia.(%s) no peers\n",cmd);
    }
    sprintf(retstr,"{\"result\":\"kademlia_%s txid.%llu\"}",cmd,(long long)txid);
    printf("FIND.(%s)\n",retstr);
    return(clonestr(retstr));
}

int32_t kademlia_pushstore(uint64_t refbits,uint64_t newbits)
{
    uint64_t txid,keyhash;
    char key[64],datastr[32768];
    struct coin_info *cp = get_coin_info("BTCD");
    int32_t dist,refdist,i,n = 0;
    if ( cp == 0 || ismynxtbits(newbits) != 0 )
        return(0);
    for (i=0; i<(int)(sizeof(K_store)/sizeof(*K_store)); i++)
    {
        if ( (keyhash= K_store[i].keyhash) == 0 )
            break;
        if ( K_store[i].data != 0 && K_store[i].datalen != 0 )
        {
            refdist = bitweight(refbits ^ keyhash);
            dist = bitweight(newbits ^ keyhash);
            if ( dist < refdist )
            {
                init_hexbytes_noT(datastr,K_store[i].data,K_store[i].datalen);
                expand_nxt64bits(key,keyhash);
                txid = send_kademlia_cmd(newbits,0,"store",cp->srvNXTACCTSECRET,key,datastr);
            }
        }
    }
    return(n);
}

void update_Kbucket(int32_t bucketid,struct nodestats *buckets[],int32_t n,struct nodestats *stats)
{
    int32_t j,k,matchflag = -1;
    uint64_t txid;
    char ipaddr[64];
    struct coin_info *cp = get_coin_info("BTCD");
    struct nodestats *eviction;
    if ( stats->ipbits == 0 || stats->nxt64bits == 0 )
        return;
    expand_ipbits(ipaddr,stats->ipbits);
    for (j=n-1; j>=0; j--)
    {
        if ( buckets[j] != 0 && buckets[j]->eviction == stats )
        {
            printf("bucketid.%d: Got pong back from eviction candidate in slot.%d\n",bucketid,j);
            buckets[j] = buckets[j]->eviction;
            break;
        }
    }
    for (j=0; j<n; j++)
    {
        if ( buckets[j] == 0 )
        {
            if ( matchflag < 0 )
            {
                buckets[j] = stats;
                add_new_node(stats->nxt64bits);
                printf("APPEND.%d: bucket[%d] <- %llu %s then call pushstore\n",bucketid,j,(long long)stats->nxt64bits,ipaddr);
                kademlia_pushstore(mynxt64bits(),stats->nxt64bits);
            }
            else if ( j > 0 )
            {
                if ( matchflag != j-1 )
                {
                    for (k=matchflag; k<j-1; k++)
                        buckets[k] = buckets[k+1];
                    buckets[k] = stats;
                    for (k=0; k<j; k++)
                        printf("%llu ",(long long)buckets[k]->nxt64bits);
                    printf("ROTATE.%d: bucket[%d] <- %llu %s | bucketid.%d\n",j-1,matchflag,(long long)stats->nxt64bits,ipaddr,bucketid);
                }
            } else printf("update_Kbucket.%d: impossible case matchflag.%d j.%d\n",bucketid,matchflag,j);
            return;
        }
        else if ( buckets[j] == stats )
            matchflag = j;
    }
    if ( matchflag < 0 ) // stats is new and the bucket is full
    {
        add_new_node(stats->nxt64bits);
        eviction = buckets[0];
        for (k=0; k<n-1; k++)
            buckets[k] = buckets[k+1];
        buckets[n-1] = stats;
        printf("bucket[%d] <- %llu, check for eviction of %llu %s | bucketid.%d\n",n-1,(long long)stats->nxt64bits,(long long)eviction->nxt64bits,ipaddr,bucketid);
        stats->eviction = eviction;
        kademlia_pushstore(mynxt64bits(),stats->nxt64bits);
        if ( cp != 0 && ismynxtbits(eviction->nxt64bits) == 0 )
        {
            txid = send_kademlia_cmd(eviction->nxt64bits,0,"ping",cp->srvNXTACCTSECRET,0,0);
            printf("check for eviction with destip.%u txid.%llu | bucketid.%d\n",eviction->ipbits,(long long)txid,bucketid);
        }
    }
}

void update_Kbuckets(struct nodestats *stats,uint64_t nxt64bits,char *ipaddr,int32_t port,int32_t p2pflag,int32_t lastcontact)
{
    static unsigned char zerokey[crypto_box_PUBLICKEYBYTES];
    struct coin_info *cp = get_coin_info("BTCD");
    uint64_t xorbits;
    int32_t bucketid;
    char pubkeystr[512],NXTaddr[64],*ptr;
    struct pserver_info *pserver;
    if ( stats == 0 )
    {
        printf("update_Kbuckets null stats\n");
        return;
    }
    if ( memcmp(stats->pubkey,zerokey,sizeof(stats->pubkey)) == 0 )
        ptr = 0;
    else
    {
        init_hexbytes_noT(pubkeystr,stats->pubkey,sizeof(stats->pubkey));
        ptr = pubkeystr;
    }
    expand_nxt64bits(NXTaddr,nxt64bits);
    kademlia_update_info(NXTaddr,ipaddr,port,ptr,lastcontact,p2pflag);
    if ( cp != 0 )
    {
        pserver = get_pserver(0,cp->myipaddr,0,0);
        //fprintf(stderr,"call addto_hasips\n");
        addto_hasips(1,pserver,stats->ipbits);
        xorbits = (cp->srvpubnxtbits != 0) ? cp->srvpubnxtbits : cp->pubnxtbits;
        if ( stats->nxt64bits != 0 )
        {
            xorbits ^= stats->nxt64bits;
            bucketid = bitweight(xorbits);
            Kbucket_updated[bucketid] = time(NULL);
            //fprintf(stderr,"call update_Kbucket\n");
            update_Kbucket(bucketid,K_buckets[bucketid],KADEMLIA_NUMK,stats);
        }
    }
}

uint64_t refresh_bucket(struct nodestats *buckets[],long lastcontact,char *NXTACCTSECRET)
{
    uint64_t txid = 0;
    int32_t i,r;
    if ( buckets[0] != 0 && lastcontact >= KADEMLIA_BUCKET_REFRESHTIME )
    {
        for (i=0; i<KADEMLIA_NUMK; i++)
            if ( buckets[i] == 0 )
                break;
        r = ((rand()>>8) % i);
        if ( ismynxtbits(buckets[r]->nxt64bits) == 0 )
            txid = send_kademlia_cmd(buckets[r]->nxt64bits,0,"findnode",NXTACCTSECRET,0,0);
    }
    return(txid);
}

void refresh_buckets(char *NXTACCTSECRET)
{
    static int didinit;
    long now = time(NULL);
    int32_t i,firstbucket,n = 0;
    firstbucket = -1;
    for (i=0; i<KADEMLIA_NUMBUCKETS; i++)
    {
        if ( firstbucket < 0 && K_buckets[i][0] != 0 )
            firstbucket = (i + 1);
        if ( refresh_bucket(K_buckets[i],now - Kbucket_updated[i],NXTACCTSECRET) != 0 )
            n++;
    }
    if ( n != 0 && didinit == 0 && firstbucket < KADEMLIA_NUMBUCKETS-1 )
    {
        printf("initial bucket refresh\n");
        for (i=firstbucket+1; i<KADEMLIA_NUMBUCKETS; i++)
            refresh_bucket(K_buckets[i],KADEMLIA_BUCKET_REFRESHTIME,NXTACCTSECRET);
        didinit = 1;
    }
}

int32_t expire_nodestats(struct nodestats *stats,uint32_t now)
{
    if ( stats->lastcontact != 0 && (now - stats->lastcontact) > NODESTATS_EXPIRATION )
    {
        char ipaddr[64];
        expand_ipbits(ipaddr,stats->ipbits);
        printf("expire_nodestats %s | %u %u %d\n",ipaddr,now,stats->lastcontact,now - stats->lastcontact);
        stats->gotencrypted = stats->modified = 0;
        stats->sentmilli = 0;
        stats->expired = 1;
        return(1);
    }
    return(0);
}

void every_minute(int32_t counter)
{
    static int broadcast_count;
    uint32_t now = (uint32_t)time(NULL);
    int32_t i,n;//,createdflag,len,iter,connected,actionflag;
    char ipaddr[64];//,cmd[4096],packet[4096];//ip_port[64],NXTaddr[64],
    //struct NXT_acct *mynp = 0;
    struct coin_info *cp;
    //struct peerinfo *peer;
    struct nodestats *stats;
    struct pserver_info *pserver;//,*mypserver = 0;
    if ( Finished_init == 0 )
        return;
    now = (uint32_t)time(NULL);
    cp = get_coin_info("BTCD");
    if ( cp == 0 )
        return;
    //printf("<<<<<<<<<<<<< EVERY_MINUTE\n");
    refresh_buckets(cp->srvNXTACCTSECRET);
    if ( broadcast_count == 0 )
    {
        p2p_publishpacket(0,0);
        update_Kbuckets(get_nodestats(cp->srvpubnxtbits),cp->srvpubnxtbits,cp->myipaddr,0,0,0);
    }
    if ( (broadcast_count % 10) == 0 )
    {
        for (i=n=0; i<Num_in_whitelist; i++)
        {
            expand_ipbits(ipaddr,SuperNET_whitelist[i]);
            pserver = get_pserver(0,ipaddr,0,0);
            if ( ismyipaddr(ipaddr) == 0 && ((stats= get_nodestats(pserver->nxt64bits)) == 0 || broadcast_count == 0 || (now - stats->lastcontact) > NODESTATS_EXPIRATION) )
                send_kademlia_cmd(0,pserver,"ping",cp->srvNXTACCTSECRET,0,0), n++;
        }
        printf("PINGED.%d\n",n);
    }
    broadcast_count++;
}

cJSON *gen_pserver_json(struct pserver_info *pserver)
{
    cJSON *array,*json = cJSON_CreateObject();
    int32_t i;
    char ipaddr[64];
    uint32_t *ipaddrs;
    struct nodestats *stats;
    double millis = milliseconds();
    if ( pserver != 0 )
    {
        if ( (ipaddrs= pserver->hasips) != 0 && pserver->numips > 0 )
        {
            array = cJSON_CreateArray();
            for (i=0; i<pserver->numips; i++)
            {
                expand_ipbits(ipaddr,ipaddrs[i]);
                cJSON_AddItemToArray(array,cJSON_CreateString(ipaddr));
            }
            cJSON_AddItemToObject(json,"hasips",array);
        }
        cJSON_AddItemToObject(json,"hasnum",cJSON_CreateNumber(pserver->hasnum));
        cJSON_AddItemToObject(json,"xorsum",cJSON_CreateNumber(pserver->xorsum));
        if ( (stats= get_nodestats(pserver->nxt64bits)) != 0 )
        {
            if ( stats->p2pport != 0 && stats->p2pport != BTCD_PORT )
                cJSON_AddItemToObject(json,"p2p",cJSON_CreateNumber(stats->p2pport));
            if ( stats->supernet_port != 0 && stats->supernet_port != SUPERNET_PORT )
                cJSON_AddItemToObject(json,"port",cJSON_CreateNumber(stats->supernet_port));
            if ( stats->numsent != 0 )
                cJSON_AddItemToObject(json,"sent",cJSON_CreateNumber(stats->numsent));
            if ( stats->sentmilli != 0 )
                cJSON_AddItemToObject(json,"lastsent",cJSON_CreateNumber((millis - stats->sentmilli)/60000.));
            if ( stats->numrecv != 0 )
                cJSON_AddItemToObject(json,"recv",cJSON_CreateNumber(stats->numrecv));
            if ( stats->recvmilli != 0 )
                cJSON_AddItemToObject(json,"lastrecv",cJSON_CreateNumber((millis - stats->recvmilli)/60000.));
            if ( stats->numpings != 0 )
                cJSON_AddItemToObject(json,"pings",cJSON_CreateNumber(stats->numpings));
            if ( stats->numpongs != 0 )
                cJSON_AddItemToObject(json,"pongs",cJSON_CreateNumber(stats->numpongs));
            if ( stats->pingmilli != 0 && stats->pongmilli != 0 )
                cJSON_AddItemToObject(json,"pingtime",cJSON_CreateNumber(stats->pongmilli - stats->pingmilli));
            if ( stats->numpings != 0 && stats->numpongs != 0 && stats->pingpongsum != 0 )
                cJSON_AddItemToObject(json,"avetime",cJSON_CreateNumber((2. * stats->pingpongsum)/(stats->numpings + stats->numpongs)));
        }
    }
    return(json);
}

char *_coins_jsonstr(char *coinsjson,uint64_t coins[4])
{
    int32_t i,n = 0;
    char *str;
    if ( coins == 0 )
        return(0);
    strcpy(coinsjson,",\"coins\":[");
    for (i=0; i<4*64; i++)
        if ( (coins[i>>6] & (1L << (i&63))) != 0 )
        {
            str = coinid_str(i);
            if ( strcmp(str,ILLEGAL_COIN) != 0 )
            {
                if ( n++ != 0 )
                    strcat(coinsjson,", ");
                sprintf(coinsjson+strlen(coinsjson),"\"%s\"",str);
            }
        }
    if ( n == 0 )
        coinsjson[0] = coinsjson[1] = 0;
    else strcat(coinsjson,"]");
    return(coinsjson);
}

cJSON *gen_peerinfo_json(struct nodestats *stats)
{
    char srvipaddr[64],srvnxtaddr[64],numstr[64],hexstr[512],coinsjsonstr[1024];
    cJSON *coins,*json = cJSON_CreateObject();
    struct pserver_info *pserver;
    expand_ipbits(srvipaddr,stats->ipbits);
    expand_nxt64bits(srvnxtaddr,stats->nxt64bits);
    if ( stats->ipbits != 0 )
    {
        //cJSON_AddItemToObject(json,"is_privacyServer",cJSON_CreateNumber(1));
        cJSON_AddItemToObject(json,"srvNXT",cJSON_CreateString(srvnxtaddr));
        cJSON_AddItemToObject(json,"srvipaddr",cJSON_CreateString(srvipaddr));
        cJSON_AddItemToObject(json,"ipbits",cJSON_CreateNumber(stats->ipbits));
        sprintf(numstr,"%d",stats->supernet_port);
        if ( stats->supernet_port != 0 && stats->supernet_port != SUPERNET_PORT )
            cJSON_AddItemToObject(json,"srvport",cJSON_CreateString(numstr));
        sprintf(numstr,"%d",stats->p2pport);
        if ( stats->p2pport != 0 && stats->p2pport != BTCD_PORT )
            cJSON_AddItemToObject(json,"p2pport",cJSON_CreateString(numstr));
        if ( stats->numsent != 0 )
            cJSON_AddItemToObject(json,"sent",cJSON_CreateNumber(stats->numsent));
        if ( stats->numrecv != 0 )
            cJSON_AddItemToObject(json,"recv",cJSON_CreateNumber(stats->numrecv));
        if ( (pserver= get_pserver(0,srvipaddr,0,0)) != 0 )
        {
            //printf("%s pserver.%p\n",srvipaddr,pserver);
            cJSON_AddItemToObject(json,"pserver",gen_pserver_json(pserver));
        }
    }
    else cJSON_AddItemToObject(json,"pubNXT",cJSON_CreateString(srvnxtaddr));
    init_hexbytes_noT(hexstr,stats->pubkey,sizeof(stats->pubkey));
    cJSON_AddItemToObject(json,"pubkey",cJSON_CreateString(hexstr));
    if ( _coins_jsonstr(coinsjsonstr,stats->coins) != 0 )
    {
        coins = cJSON_Parse(coinsjsonstr+9);
        if ( coins != 0 )
            cJSON_AddItemToObject(json,"coins",coins);
        //else printf("warning no coin networks.(%s) probably no peerinfo yet\n",coinsjsonstr);
    }
    return(json);
}

int32_t scan_nodes(uint64_t *newaccts,int32_t max,char *NXTACCTSECRET)
{
    struct coin_info *cp = get_coin_info("BTCD");
    struct pserver_info *pserver,*mypserver;
    uint32_t ipbits,newips[16];
    int32_t i,j,k,m,n,num = 0;
    uint64_t otherbits;
    char ipaddr[64];
    struct nodestats *stats;
    n = (Numallnodes < MAX_ALLNODES) ? Numallnodes : MAX_ALLNODES;
    if ( n > 0 && cp != 0 && cp->myipaddr[0] != 0 )
    {
        mypserver = get_pserver(0,cp->myipaddr,0,0);
        if ( mypserver->hasips != 0 && n != 0 )
        {
            memset(newips,0,sizeof(newips));
            for (i=m=0; i<n; i++)
            {
                if ( (otherbits= Allnodes[i]) != mypserver->nxt64bits )
                {
                    if ( num < max )
                        newaccts[num++] = otherbits;
                    if ( (stats= get_nodestats(otherbits)) != 0 )
                    {
                        expand_ipbits(ipaddr,stats->ipbits);
                        pserver = get_pserver(0,ipaddr,0,0);
                        if ( pserver->hasips != 0 && pserver->numips > 0 )
                        {
                            for (j=0; j<pserver->numips; j++)
                            {
                                ipbits = pserver->hasips[j];
                                for (k=0; k<mypserver->numips; k++)
                                    if ( mypserver->hasips[k] == ipbits )
                                        break;
                                if ( k == mypserver->numips )
                                {
                                    newips[m++] = ipbits;
                                    if ( m >= (int)(sizeof(newips)/sizeof(*newips)) )
                                        break;
                                }
                            }
                        }
                    }
                }
            }
            if ( m > 0 )
            {
                for (i=0; i<m; i++)
                {
                    expand_ipbits(ipaddr,newips[i]);
                    printf("ping new ip.%s\n",ipaddr);
                    send_kademlia_cmd(0,get_pserver(0,ipaddr,0,0),"ping",NXTACCTSECRET,0,0);
                }
            }
        }
    }
    return(num);
}

cJSON *gen_peers_json(struct sockaddr *prevaddr,char *verifiedNXTaddr,char *NXTACCTSECRET,char *sender,int32_t only_privacyServers)
{
    int32_t i,n;
    char pubkeystr[512],key[64],*retstr;
    cJSON *json,*array;
    uint64_t newaccts[16];
    //printf("inside gen_peer_json.%d\n",only_privacyServers);
    json = cJSON_CreateObject();
    array = cJSON_CreateArray();
    n = (Numallnodes < MAX_ALLNODES) ? Numallnodes : MAX_ALLNODES;
    if ( n > 0 )
    {
        for (i=0; i<n; i++)
        {
            if ( Allnodes[i] != 0 )
                cJSON_AddItemToArray(array,gen_peerinfo_json(get_nodestats(Allnodes[i])));
        }
        cJSON_AddItemToObject(json,"peers",array);
        //cJSON_AddItemToObject(json,"only_privacyServers",cJSON_CreateNumber(only_privacyServers));
        cJSON_AddItemToObject(json,"num",cJSON_CreateNumber(n));
        cJSON_AddItemToObject(json,"Numpservers",cJSON_CreateNumber(Numallnodes));
    }
    memset(newaccts,0,sizeof(newaccts));
    n = scan_nodes(newaccts,sizeof(newaccts)/sizeof(*newaccts),NXTACCTSECRET);
    for (i=0; i<n; i++)
    {
        expand_nxt64bits(key,newaccts[i]);
        init_hexbytes(pubkeystr,Global_mp->loopback_pubkey,sizeof(Global_mp->loopback_pubkey));
        retstr = kademlia_find("findnode",prevaddr,verifiedNXTaddr,NXTACCTSECRET,sender,pubkeystr,key);
        if ( retstr != 0 )
            free(retstr);
    }
    return(json);
}


#endif
