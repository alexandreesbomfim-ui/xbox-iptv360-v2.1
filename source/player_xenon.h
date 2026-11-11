#ifndef IPTV360_PLAYER_XENON_H
#define IPTV360_PLAYER_XENON_H
#include "player.h"

// Reproduz um .mpg (MPEG-1 + MP2) no Xbox 360: video no framebuffer, audio pelo xenon_sound.
// B / Back / Logo interrompem. Retorna false se nao conseguiu abrir o arquivo.
bool player_play_xenon(const char* path, PlayerStats& st);

#endif
