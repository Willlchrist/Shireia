# Recursos, validação e limitações

Este inventário distingue uma integração encontrada no código de um resultado testado. “Implementado” não equivale a compatibilidade de todos os jogos nem à validação em toda configuração de hardware.

## Matriz de recursos

| Área | Estado | O que o código oferece | Evidência e limite |
| --- | --- | --- | --- |
| Biblioteca | Implementada; caminhos principais testados | Lista NSP/XCI/NRO/NSO de pastas locais, busca, favoritos, recentes consultados, filtros, ordenação e capas locais. Metadados dependem do loader original. | Fixtures sintéticos cobrem varredura e persistência. A enumeração da interface não é recursiva. “Recente” significa consultado pela interface, não necessariamente executado. |
| Identificação | Implementada e testada com fixtures | Usa `Loader::IdentifyFile` sem carregar o jogo. | Teste sintético NRO e PFS0/NSP; não certifica validade ou executabilidade do conteúdo. |
| Carregamento | Implementado; homebrew validado | Delegação de NRO/NSO/NSP/XCI ao loader Suyu, `Load`/`Run`, primeiro frame antes do estado `Running`. | 2048 1.1.0 teve sessão gráfica validada. NSO, XCI e ampla matriz de títulos não foram cobertos de modo funcional. |
| Assistente inicial | Implementado; smoke test existente | Boas-vindas, aviso legal e confirmação versionada, seleção/validação técnica de keys e firmware, biblioteca opcional. | Teste automatizado de navegação. Importação real de firmware/keys depende dos arquivos locais; versão do firmware não é inferida. |
| Configuração global | Implementada | Geral, Sistema, Gráficos, Áudio, Controles, Online, Interface, Emulador, Logs e Gráficos avançados. Opções futuras podem estar desativadas. | Preferências e rascunhos persistem via QSettings; várias opções exigem novo início de jogo. “Online” é uma página de integração futura. |
| Configuração por jogo | Parcialmente validada | Substituições gráficas, áudio/volume e velocidade persistidas por Title ID quando disponível ou por identidade do arquivo. | Aplicação das opções de criação ocorre antes da próxima carga; não há prova de cada combinação por jogo. |
| Resolução interna | Implementada para próxima sessão | Escala de resolução encaminhada à configuração do núcleo ao carregar. Saída da superfície é configurada separadamente em 720p/1080p. | Alteração em sessão ativa não está implementada. FPS e qualidade dependem do título, GPU, driver e escala. |
| Filtro e AA | Implementados; efeito visual incompletamente validado | Filtro de escalonamento e valores de AA usam settings Suyu; o menu rápido envia mudança à sessão pelo IPC. | Alguns filtros afetam a apresentação. Não existe validação perceptual abrangente de FXAA/SMAA ou prova de alteração visual em cada título. |
| Vulkan → Qt/D3D11 | Implementado, protótipo com limitações | Vulkan headless, readback síncrono, conversão, upload D3D11 e três texturas compartilhadas. | Homebrew exibido na interface e teste GPU sintético. O teste sintético não é emulação. Pipeline não é zero-copy; resolução e cadência são limitadas pela ponte. |
| Overlay de desempenho | Implementado | Desligado por padrão; mostra contagens do compositor e publicação, resolução de saída/escala configuradas e gráfico de intervalos de chegada. Medição Qt está disponível na interface quando há amostra. | São métricas de etapas distintas, não FPS interno do jogo nem refresh efetivo do monitor. Stutter não é classificado por todos os critérios possíveis. |
| Captura de desempenho | Implementada | Captura JSONL de amostras em intervalos, inclui métricas do núcleo, cópia, publicação, IPC, transferência e opções reportadas. | Não é benchmark universal. Métricas têm limites e não isolam o frametime interno da aplicação convidada. |
| Screenshot | Implementado | Captura a área de jogo apresentada pela janela, valida imagem, salva PNG em pasta por título, mostra resultado temporário e oferece abertura da pasta. | Obtém `grabWindow()` do Qt e recorta `GpuSurface`; não é screenshot nativo do Vulkan. Overlay fica oculto durante captura quando possível. |
| Mods | Implementado; efeito delegado ao núcleo | Importa árvore de mod com estrutura Suyu LayeredFS/ExeFS para área por Title ID, habilita/desabilita e remove pelo frontend. | Importação valida limites e estrutura. Efeito no jogo não é demonstrado automaticamente; aplica-se em novo início conforme o núcleo. |
| Cheats | Implementado; parsing testável | Importa `.txt` associado a Build ID e valida via parser original do Suyu; habilita/desabilita por jogo. | Parser aceitar sintaxe não garante Build ID compatível, ativação ou efeito correto. |
| Updates/DLC | Implementado no fluxo de instalação | Encaminha NSP ao `ContentManager::InstallNSP` original; recusa pacote base e instala update/DLC na NAND privada. | Instalação de pacote de usuário não foi validada de ponta a ponta nesta revisão. Requer encerramento de jogo e recursos apropriados. |
| Cache por jogo | Implementado | Consulta contagem/tamanho, abre pasta por Title ID, pede confirmação e limpa somente cache de shaders/pipeline quando sessão encerrada. | Testes cobrem segurança de escopo; operação em cache comercial de usuário não foi realizada nesta revisão. |
| Controles | Implementados; cobertura de entrada de teste | SDL2 hotplug, teclado, navegação por UI, entrada de jogo, remapeamento, layout, perfis e até dois jogadores locais. Estado é enviado ao HID virtual Suyu. | Testes usam teclado e controles SDL virtuais, inclusive o segundo jogador. Nem todos os modelos físicos, Bluetooth, rumble, sensores ou jogos foram validados. |
| Teclado virtual | Integrado ao applet original; validação limitada | `QtSoftwareKeyboardDialog` é fornecido como implementação do software keyboard do Suyu e mostrado em janela Qt. | A abertura foi observada em jogo de teste; comportamento responsivo e foco/ordem de janelas não têm cobertura completa automatizada. |
| Áudio | Parcial | Seleção de backend Cubeb original, consulta de dispositivos e volume. | Saída audível e dispositivos de hardware não foram validados de forma conclusiva. |
| Histórico de compatibilidade/falhas | Implementado | Registro persistente por Title ID e sessão, estados editáveis, configuração selecionada e erro sanitizado; agrupa falhas e pergunta se deseja investigar. | Ausência de Title ID limita associação; sessão interrompida pode ter causa fora do jogo. Avaliação é manual, sem inferência automática de compatibilidade. |
| Exportar diagnóstico | Implementado | Exportação de resumo/log com filtragem de caminhos e tokens identificáveis. | Sanitização é baseada em padrões reconhecidos; revise o conteúdo exportado antes de compartilhar. |
| Online | Não implementado | A categoria de interface informa que a integração não está disponível. | Não há serviços de rede do Shireia apresentados como funcionais. |

## Testes automatizados

O CMake registra estes testes; `integrated-homebrew` aparece somente quando `SHIREIA_TEST_HOMEBREW` aponta para um arquivo local existente.

| Teste | Cobertura indicada pelo alvo |
| --- | --- |
| `core-initialize` | Inicialização/destruição de Core::System. |
| `core-execution` | Programa AArch64 de contador sintético, sem alegação de jogo ou imagem. |
| `core-vulkan-execution` | Programa sintético usando backend Vulkan headless. |
| `core-ipc` | Envelope, comandos, rejeições, falhas isoladas, identificação e fluxo do filho. |
| `library-resources` | Leitura/cache de metadados com fixture homebrew sintético e validação dos recursos selecionados. |
| `gpu-sharing` | Compartilhamento de textura D3D11 com padrão sintético; não é teste de jogo. |
| `integrated-homebrew` | Sessão com homebrew local autorizado, quando configurado. |
| `backend-tests` | Preferências e controles pelo backend SDL virtual/teste. |
| `frontend-smoke` | Navegação QML, biblioteca, configurações, entrada e fluxo do supervisor com fixture sintético. |
| `setup-wizard-smoke` | Navegação e validações de fluxo do assistente. |

## Limitações conhecidas

1. Compatibilidade permanece experimental e dependente dos serviços, estado e requisitos de cada título.
2. O estado `Running` é condicionado ao primeiro frame; porém ausência de frame pode ter várias causas e não identifica automaticamente a origem do bloqueio.
3. A ponte de apresentação faz readback síncrono e cópias. Não reativar o readback assíncrono experimental descartado após corrupção visual.
4. Taxas do compositor, transporte e Qt não equivalem a FPS interno do jogo ou taxa efetivamente percebida no monitor.
5. Áudio audível, ampla compatibilidade, instalação de conteúdo em instalações arbitrárias e todas as opções gráficas não foram validados em amplitude.
6. A distribuição binária requer auditoria própria de runtime, licenças, avisos e código-fonte correspondente; ela não está coberta pela validação deste snapshot-fonte.
