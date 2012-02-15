#ifndef PACK_H
#define PACK_H

/* Init/free internal compression structures. */
void pack_init();
void pack_free();

ulong pack_pack(char *dst_buf, ulong buf_len,
                char *src_buf, ulong src_len);
ulong pack_unpack(char *dst_buf, ulong buf_len,
                  char *src_buf, ulong src_len);
    
/* ulong pack_compress */

#endif
