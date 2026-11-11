#include "../source/m3u.h"
#include <assert.h>
#include <stdio.h>

int main(int argc, char** argv) {
    std::vector<Channel> v;
    assert(m3u_load(argc > 1 ? argv[1] : "tests/sample.m3u", v));
    for (size_t i = 0; i < v.size(); ++i)
        printf("[%zu] name='%s' group='%s' url='%s'\n", i, v[i].name.c_str(),
               v[i].group.c_str(), v[i].url.c_str());
    assert(v.size() == 4);
    assert(v[0].name == "Canal Um");
    assert(v[0].group == "Noticias, Mundo");   // virgula dentro de aspas
    assert(v[1].name == "Canal Dois" && v[1].group.empty());
    assert(v[2].name == "http://exemplo.invalid/3.ts"); // sem EXTINF
    assert(v[3].name == "Sem Grupo");
    puts("OK");
    return 0;
}
