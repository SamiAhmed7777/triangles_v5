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
    // Docker seed nodes (contabo-de)
    {"uddaxjbo3lh2zskg7w6gwln4ty5cel7q4c5jbx7fdtv6zf2j47gdlyad.onion"},     // seed-1
    {"el5sirhhleecuctpeeprelzubpqmoqivvra3rzlwbjttinxa4fq3wnid.onion"},     // seed-2
    {"sj5dhybnlp3v4y5niyc5unrnd6s43lyx5ibup7rolyosjbi2u2hsbvyd.onion"},     // seed-3
    {"i3kr5meha7se4ns3wss3h7v46m6uksfzv4wrohdqxpj6n35wyo2bvlid.onion"},     // seed-4
    {NULL}
};

static const char *strTestNetOnionSeed[][1] = {
    {NULL}
};

#endif
