#include "m3u.h"
#include <stdio.h>
#include <string.h>

static void rtrim(std::string& s) {
    while (!s.empty() && (s[s.size() - 1] == '\r' || s[s.size() - 1] == '\n' ||
                          s[s.size() - 1] == ' ' || s[s.size() - 1] == '\t'))
        s.erase(s.size() - 1);
}

static void ltrim(std::string& s) {
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    s.erase(0, i);
}

// Valor de um atributo tipo group-title="Noticias, Mundo"
static std::string attr(const std::string& line, const char* key) {
    std::string k = std::string(key) + "=\"";
    size_t p = line.find(k);
    if (p == std::string::npos) return "";
    p += k.size();
    size_t e = line.find('"', p);
    if (e == std::string::npos) return "";
    return line.substr(p, e - p);
}

void m3u_parse_extinf(const std::string& line, Channel& ch) {
    // O nome vem depois da PRIMEIRA virgula FORA de aspas
    // (atributos como group-title="A, B" podem conter virgulas).
    bool inQuote = false;
    size_t comma = std::string::npos;
    for (size_t i = 0; i < line.size(); ++i) {
        if (line[i] == '"') inQuote = !inQuote;
        else if (line[i] == ',' && !inQuote) { comma = i; break; }
    }
    if (comma != std::string::npos) {
        ch.name = line.substr(comma + 1);
        ltrim(ch.name);
        rtrim(ch.name);
    }
    ch.group = attr(line, "group-title");
    if (ch.name.empty()) {
        std::string n = attr(line, "tvg-name");
        if (!n.empty()) ch.name = n;
    }
}

bool m3u_load(const char* path, std::vector<Channel>& out, size_t maxChannels) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;

    // Linhas de playlist IPTV podem ser longas (tokens em URL)
    static char buf[4096];
    Channel cur;
    bool first = true;

    while (fgets(buf, sizeof(buf), f)) {
        std::string line(buf);
        // linha maior que o buffer: descarta o resto
        if (line.find('\n') == std::string::npos && !feof(f)) {
            int c;
            while ((c = fgetc(f)) != EOF && c != '\n') {}
        }
        if (first) {
            first = false;
            // remove BOM UTF-8
            if (line.size() >= 3 && (unsigned char)line[0] == 0xEF &&
                (unsigned char)line[1] == 0xBB && (unsigned char)line[2] == 0xBF)
                line.erase(0, 3);
        }
        rtrim(line);
        ltrim(line);
        if (line.empty()) continue;

        if (line.compare(0, 8, "#EXTINF:") == 0) {
            cur = Channel();
            m3u_parse_extinf(line, cur);
        } else if (line[0] == '#') {
            continue;  // #EXTM3U, #EXTGRP, comentarios...
        } else {
            cur.url = line;
            if (cur.name.empty()) cur.name = line;
            if (out.size() < maxChannels) out.push_back(cur);
            cur = Channel();
        }
    }
    fclose(f);
    return true;
}
