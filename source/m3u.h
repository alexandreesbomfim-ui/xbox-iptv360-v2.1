// Parser M3U portavel (sem dependencias do Xbox 360) - testavel no PC.
#ifndef IPTV360_M3U_H
#define IPTV360_M3U_H

#include <stddef.h>
#include <string>
#include <vector>

struct Channel {
    std::string name;
    std::string group;
    std::string url;
};

// Le um arquivo M3U/M3U8 e acrescenta os canais em 'out'.
// Retorna false se o arquivo nao puder ser aberto.
bool m3u_load(const char* path, std::vector<Channel>& out, size_t maxChannels = 20000);

// Interpreta uma linha #EXTINF (exposto para testes).
void m3u_parse_extinf(const std::string& line, Channel& ch);

#endif
