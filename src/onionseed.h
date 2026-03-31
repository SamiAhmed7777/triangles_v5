#ifndef TRIANGLES_ONIONSEED_H
#define TRIANGLES_ONIONSEED_H

// hidden service seeds
// v5 hard fork: v2 onion seeds removed (Tor v2 deprecated Oct 2021)
// v3 onion seeds - bootstrap nodes deployed 2026-03-30
static const char *strMainNetOnionSeed[][1] = {
    // Main nodes
    {"gxvrhv3qitnc6kobrhsrse46bmcfitnybapor3or3oczzuxn6hfzxyid.onion"},   // DNS2
    {"futmtrvh6j34t7s6yjdxfia6iwuyfzwh4k5eqfof5kfhoqk3xmi3qoqd.onion"},   // Original seed
    {"i6tk7soznftvoibtskwlezviskiererhjndpsmrff4kaxw7jnd5izfqd.onion"},     // DNS3
    // Docker seed nodes
    {"sgn3pus7ssrwbhidvsu5dav3titilzz7mplclvslfuisduy3v7boqiid.onion"},     // seed-1
    {"hjveej4qjjm2npaxshse7qnrgbqsdyu67io6erdw5ihdil377552xsqd.onion"},     // seed-2
    {"krqbt7oyizmjp72oo6oiu6rcf7trxz5rpynjfx6whgavtpf4tkjaajyd.onion"},     // seed-3
    {"pjhodbrj3swqyj33ezkhyr4mz4f5x4xcueim4mwlybjlragmzruwheid.onion"},     // seed-4
    {NULL}
};

static const char *strTestNetOnionSeed[][1] = {
    {NULL}
};

#endif
