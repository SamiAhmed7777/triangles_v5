#ifndef TRIANGLES_TOR_EMBED_HOOKS_H
#define TRIANGLES_TOR_EMBED_HOOKS_H

#ifdef __cplusplus
extern "C" {
#endif

const char* triangles_tor_data_directory();
const char* triangles_onion_service_directory();
int triangles_tor_check_interrupted();
void triangles_tor_set_initialized();
void triangles_tor_wait_initialized();

#ifdef __cplusplus
}
#endif

#endif // TRIANGLES_TOR_EMBED_HOOKS_H
