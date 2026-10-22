# Compilação e limites de reprodutibilidade

## Fontes do núcleo

`third_party/suyu/` contém a árvore de fontes Suyu fixada no commit `347fcf91d95cd39cb5c3db50f1b6d827ad699311` (`mk8-recomp`) e o submódulo Dynarmic no commit `19249e8cbfb6e0d4c062bd7c4a111f55f220c211`. As alterações locais de fontes necessárias ao build estão descritas em [`third_party/suyu/SHIREIA-PATCHES.md`](../third_party/suyu/SHIREIA-PATCHES.md); cabeçalhos, avisos e licenças foram preservados. O frontend Qt do Suyu é desativado porque o Shireia fornece seu próprio frontend.

O candidato público também exclui os payloads de fontes compartilhadas embutidas e recursos de mídia/documentação não usados pelo frontend Shireia, conforme `SHIREIA-PATCHES.md`. O núcleo usa as fontes dos arquivos de firmware instalados pelo usuário e não sintetiza um fallback quando faltam esses arquivos. Isso preserva o caminho de execução com firmware completo, mas o efeito em títulos que dependam de fontes de um firmware incompleto ainda não foi testado.

O núcleo é compilado a partir dessas fontes em `build/suyu-core` antes do frontend. Os archives GNU thin gerados apontam para objetos dentro dessa pasta de build. `cmake/ImportSuyu.cmake` restringe fonte e build às cópias dentro do projeto e rejeita caminhos de checkout ou build externos.

As dependências de código-fonte geridas pelo Suyu estão fixadas em `third_party/suyu/cpmfile.json` por versão e SHA-512. CMake/CPM obtém esses pacotes no primeiro build para `third_party/suyu/.cache/cpm/`, ignorado pelo Git. Pacotes UCRT64 de sistema podem ser usados quando compatíveis. Não são baixados jogos ou conteúdo de usuário.

## Ferramentas e bibliotecas

Requisitos: Windows 10/11 x64; MSYS2 UCRT64 atualizado; GCC/MinGW-w64 x86_64; CMake 3.31 ou posterior; Ninja; Qt 6.5 ou posterior com Base, Quick, QuickControls2, Network e Widgets; SDL2 2.0.14 ou posterior; e bibliotecas UCRT64 de FFmpeg, OpenSSL, zlib, libarchive, libusb, lz4, zstd e Opus. O build também precisa do Windows SDK/headers D3D11 distribuídos com MinGW-w64. CMake/CPM acessa a internet no primeiro build para obter dependências fixadas.

Em um terminal **MSYS2 UCRT64** recém-instalado, atualize os pacotes (`pacman -Syu`, reiniciando o terminal se solicitado) e instale:

```bash
pacman -S --needed \
  mingw-w64-ucrt-x86_64-gcc \
  mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja \
  mingw-w64-ucrt-x86_64-pkgconf \
  mingw-w64-ucrt-x86_64-qt6-base \
  mingw-w64-ucrt-x86_64-qt6-declarative \
  mingw-w64-ucrt-x86_64-icu \
  mingw-w64-ucrt-x86_64-SDL2 \
  mingw-w64-ucrt-x86_64-ffmpeg \
  mingw-w64-ucrt-x86_64-openssl \
  mingw-w64-ucrt-x86_64-zlib \
  mingw-w64-ucrt-x86_64-libarchive \
  mingw-w64-ucrt-x86_64-libusb \
  mingw-w64-ucrt-x86_64-lz4 \
  mingw-w64-ucrt-x86_64-zstd \
  mingw-w64-ucrt-x86_64-opus
```

No PowerShell, passe os caminhos para o prefixo UCRT64. O default é `C:/msys64/ucrt64`; para outra instalação use `-QtPrefix` e `-Toolchain` explicitamente. A validação local usou GCC 16.2.0, CMake 4.4.4, Ninja 1.13.2, Qt 6.11.2 e SDL2 2.32.10; são versões observadas, não uma matriz garantida de suporte.

As dependências do Suyu incluem Boost, fmt, nlohmann-json, Xbyak, ENet, unordered-dense, cpp-httplib, Vulkan-Headers/Utility-Libraries, SimpleIni, cubeb, SDL3, SPIRV-Headers, Sirit, libusb, Vulkan Memory Allocator e dados de fuso horário. O manifesto `cpmfile.json` fixa as versões e os hashes dos pacotes gerenciados pelo CPM. OpenSSL, zlib, lz4, zstd, opus, FFmpeg e libarchive são providos pelo ambiente UCRT64 conforme a configuração do núcleo e do Shireia. Os avisos de cada dependência permanecem com seus pacotes de origem; veja [`third_party/NOTICES.md`](../third_party/NOTICES.md).

O build script não instala ferramentas automaticamente. O CMake/CPM fixa os repositórios, revisões e hashes de dependências-fonte em `third_party/suyu/cpmfile.json`; elas são baixadas no primeiro build para um cache local. A matriz de revisões e o estado de licença está em [`DEPENDENCIAS_E_LICENCAS.md`](DEPENDENCIAS_E_LICENCAS.md).

## Compilar e testar

A forma recomendada configura, compila e testa o núcleo real antes do frontend:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build.ps1
```

Esse comando usa os caminhos padrão `C:/msys64/ucrt64`. Para outra raiz MSYS2, indique os caminhos e um diretório de build isolado:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build.ps1 `
  -QtPrefix 'D:/msys64/ucrt64' `
  -Toolchain 'D:/msys64/ucrt64' `
  -BuildRoot 'build-local'
```

`-BuildRoot` separa binários e cache CPM. Assim, uma validação limpa pode usar uma pasta inédita, sem apagar builds anteriores. O script configura o Suyu dentro de `<BuildRoot>/suyu-core`, compila `core`, `frontend_common` e `input_common`, configura o frontend com os archives recém-gerados, compila, faz deployment local e executa CTest e os smoke tests de runtime. A pasta de cache fica em `<BuildRoot>/cpm-source-cache`; configure `CPM_SOURCE_CACHE` é isolado do cache padrão `third_party/suyu/.cache/cpm`. Nada é instalado no sistema.

Para compilar somente o frontend sem o núcleo real, use uma pasta de build separada e `-DSHIREIA_BUILD_CORE=OFF`. Esse perfil não valida emulação ou a ligação com Suyu.

`run.ps1` inicia o binário de `build/`; os switches `-CoreSelfTest`, `-ExecutionSelfTest`, `-Vulkan`, `-GpuSmoke` e `-Smoke` executam verificações locais disponíveis. `SHIREIA_TEST_HOMEBREW` só deve apontar para um homebrew autorizado já disponível; o build não baixa jogos.

## Validação da compilação independente

A validação limpa deve usar `-BuildRoot 'build-clean'`, sem preencher previamente esse diretório ou o cache CPM. Isso refaz a obtenção das dependências fixadas, gera os objetos e archives a partir dos fontes vendorizados e registra os resultados de CTest. O perfil integrado não é aprovado se `core-provenance.txt` apontar para fora da cópia pública ou se fontes/objetos vierem de outro build.

### Validação registrada em 21/10/2026

- CMake configurou a fonte vendorizada Suyu commit `347fcf91d95cd39cb5c3db50f1b6d827ad699311` e Dynarmic commit `19249e8cbfb6e0d4c062bd7c4a111f55f220c211`; baixou fontes CPM para o cache novo e compilou os alvos do núcleo, frontend e testes. Os alvos foram ligados usando GCC 16.2.0, CMake 4.4.4, Ninja 1.13.2, Qt 6.11.2 e SDL2 2.32.10 do MSYS2/UCRT64 já instalado.
- Compilação do núcleo e frontend concluída; o deployment local preparou as DLLs necessárias no diretório de build.
- CTest: **9/9 aprovados** (`core-initialize`, `core-execution`, `core-vulkan-execution`, `core-ipc`, `library-resources`, `gpu-sharing`, `backend-tests`, `frontend-smoke`, `setup-wizard-smoke`).
- Com `PATH` limitado ao Windows e sem variáveis Qt de desenvolvimento, passaram `ShireiaCore.exe --self-test`, `Shireia Next.exe --smoke-test --capture` e a abertura/fechamento normal da janela.
- Avisos não fatais: nome/capitalização do finder libusb no SDL3, redefinição de `BUILD_ID`, macros de log reescritas e APIs padrão obsoletas. Não impediram o build ou os testes.
- Esta validação prova compilação limpa de fontes e cache CMake/CPM neste computador, com dependências do sistema já instaladas. Não é reprodução em uma máquina limpa, não fixa pacotes MSYS2 por hash e não valida gameplay comercial, áudio audível ou controle físico.

## Escopo dos targets Suyu

O script solicita somente `core`, `frontend_common` e `input_common`. O CMake do Suyu constrói com eles as dependências transitivas requeridas, entre elas `common`, `network`, `nintendo_library`, `audio_core`, `video_core`, `hid_core`, `shader_recompiler`, Dynarmic e bibliotecas auxiliares. Shireia usa `frontend_common` para os gestores de conteúdo/configuração e `input_common` para dispositivos e serviços HID; não compila o frontend gráfico original do Suyu.

O perfil desativa o frontend Qt do Suyu (`ENABLE_QT=OFF`), a CLI (`YUZU_CMD=OFF`), testes Suyu/Dynarmic, libretro, probes iOS e export de build kit. Mantém suporte de sala no núcleo (`YUZU_ROOM=ON`), mas desativa o executável independente (`YUZU_ROOM_STANDALONE=OFF`). OpenGL continua habilitado no núcleo para preservar as capacidades upstream; a sessão Shireia seleciona Vulkan e mantém o readback síncrono Vulkan para D3D11/Qt. SDL3/cubeb, USB, Dynarmic/JIT e os serviços do sistema emulado continuam habilitados. O script não reduz manualmente a lista de serviços do target `core`.

Os resultados acima são registros da validação de 21/10/2026. Não demonstram compatibilidade ampla, gameplay comercial ou saída de áudio audível.

## Dados gerados

CMake gera executáveis, DLLs implantadas, archives, objetos, relatórios e capturas dentro do `BuildRoot` selecionado; fontes CPM são armazenadas em seu subdiretório `cpm-source-cache`. O cache padrão do projeto é `third_party/suyu/.cache/cpm/`. Builds, caches, saves, NAND, firmware, chaves, jogos, logs e caches de usuário não fazem parte dos fontes nem devem ser adicionados ao repositório.
