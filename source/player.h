// Player MPEG-1 (video) + MP2 (audio) baseado em pl_mpeg.
// O "core" nao depende do Xbox: a plataforma e injetada (PlayerPlatform),
// o que permite testar a logica no PC.
#ifndef IPTV360_PLAYER_H
#define IPTV360_PLAYER_H

#include <stdint.h>

class PlayerPlatform {
public:
    virtual ~PlayerPlatform() {}
    virtual uint64_t now_ms() = 0;                                  // relogio monotonico
    virtual void present(const uint32_t* bgra, int w, int h) = 0;   // mostra um quadro (bytes B,G,R,x)
    virtual int  audio_free_bytes() = 0;                            // espaco livre no buffer de audio
    virtual int  audio_unplayed_bytes() = 0;                        // dados enviados e ainda nao tocados
    virtual void audio_submit(const void* data, int bytes) = 0;     // PCM s16 little-endian, estereo, 48 kHz
    virtual void idle() = 0;                                        // pequena pausa (~1 ms)
    virtual bool quit_requested() = 0;
    virtual void status(const char* text) = 0;                      // linha de estatisticas (1x por segundo)
};

struct PlayerStats {
    int    width, height;
    int    samplerate;        // 0 = sem audio
    int    frames_decoded;
    int    frames_shown;
    double real_seconds;      // tempo real decorrido
    double video_seconds;     // tempo de video decodificado
    double decode_fps;        // quadros decodificados por segundo reais
    double speed;             // video_seconds / real_seconds (>= ~1.0 = tempo real)
    int    audio_underruns;   // vezes que o audio ficou sem dados
};

// Reproduz 'path'. max_seconds > 0 limita a duracao (testes).
// Retorna false se o arquivo nao abrir ou nao tiver video MPEG-1.
bool player_run(const char* path, PlayerPlatform& plat, PlayerStats& st, double max_seconds = 0.0);

#endif
