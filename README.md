# IPTV360 - homebrew Xbox 360 (libxenon)

Gerenciador de playlists M3U + **player MPEG-1/MP2** (marco 1 de TV ao vivo).
Gera `xenon.elf` (ELF32), que o XeLL Reloaded carrega do pendrive.

## Teste de desempenho de video
Copie `xenon.elf`, `playlist.m3u`, `test_352.mpg` e `test_640.mpg` para a raiz do pendrive (FAT32).
No app: **X** = teste 352x288, **Start** = teste 640x480. B/Back/Logo interrompe.
No fim aparece o resultado: FPS, velocidade (x1.00 = tempo real) e falhas de audio.

## Fluxo de TV ao vivo (proximos marcos)
PC/Raspberry (FFmpeg) converte o canal para MPEG-1 + MP2 e o Xbox decodifica com pl_mpeg (MIT).

## Estrutura
- source/player_core.cpp  - decodificacao (pl_mpeg), fila de audio, metricas (testado no PC)
- source/player_xenon.cpp - framebuffer tiled, xenon_sound, controle
- source/tile.h           - enderecamento do framebuffer do Xenos (testado contra o libxenon)
- source/m3u.cpp          - parser de playlists
- tests/                  - testes que rodam no PC (CI)
