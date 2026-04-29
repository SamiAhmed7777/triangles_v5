#ifndef TRIANGLES_ONIONSEED_H
#define TRIANGLES_ONIONSEED_H

// Hardcoded onion seed nodes for initial peer discovery.
// Also fetched dynamically via https://seeds.cryptographic-triangles.org/seeds.txt
static const char *strMainNetOnionSeed[][1] = {
    // DNS2 - primary bootstrap server (194.233.88.206)
    {"gxvrhv3qitnc6kobrhsrse46bmcfitnybapor3or3oczzuxn6hfzxyid.onion"},
    // DNS3 - canonical chain reference (74.208.167.19)
    {"i6tk7soznftvoibtskwlezviskiererhjndpsmrff4kaxw7jnd5izfqd.onion"},
    // Hetzner Helsinki - ARM64 staking node (46.62.249.20)
    {"nawqqoazk2hhaglygulpeg6kh7hsgnvi2fursdvpvkantu4ojj26taid.onion"},
    // Contabo seed 1 (173.212.201.200)
    {"vmepp7plxngv4qpyngbgtb6njwnmlwy4api64xnwkhaf6fm3qlqtpfad.onion"},
    // Contabo seed 2
    {"nsldmfujkiwsfha42ajp5zx7gz3ekwdk4nvowdpf56mayuxnzshuykqd.onion"},
    // Contabo seed 3
    {"on4noksywc7b6cdbbxsp535l7j4cugunvlyz3iyhf6sfcg2qzaoy3eqd.onion"},
    // Contabo seed 4
    {"3uyzltm5cy7xzunncp3d7ariw75erabdnj4l3cxwvsxb6h4orc7eiqad.onion"},
    {NULL}
};

static const char *strTestNetOnionSeed[][1] = {
    {NULL}
};

#endif
