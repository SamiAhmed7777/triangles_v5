
#ifndef TRIANGLES_ONIONSEED_H
#define TRIANGLES_ONIONSEED_H

// Hardcoded onion seed nodes for initial peer discovery.
// Also fetched dynamically via https://seeds.cryptographic-triangles.org/seeds.txt
static const char *strMainNetOnionSeed[][1] = {
    {"jbpfhe7zw3qm67wy3j2ayysp3mnrjobopthnko3b3sgahqtecblwqmid.onion"},
    {"uddaxjbo3lh2zskg7w6gwln4ty5cel7q4c5jbx7fdtv6zf2j47gdlyad.onion"},
    {"el5sirhhleecuctpeeprelzubpqmoqivvra3rzlwbjttinxa4fq3wnid.onion"},
    {"sj5dhybnlp3v4y5niyc5unrnd6s43lyx5ibup7rolyosjbi2u2hsbvyd.onion"},
    {"i3kr5meha7se4ns3wss3h7v46m6uksfzv4wrohdqxpj6n35wyo2bvlid.onion"},
    {NULL}
};

static const char *strTestNetOnionSeed[][1] = {
    {NULL}
};

#endif
