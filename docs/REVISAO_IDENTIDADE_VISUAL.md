# Revisao independente da identidade visual Shireia

## Procedencia declarada

Willlchrist declarou que a identidade visual foi desenvolvida especificamente para o Shireia sob sua direcao criativa e com auxilio de inteligencia artificial. A autorizacao para usar, reproduzir, adaptar e publicar os arquivos listados abaixo aplica-se apenas aos direitos que detem. Nao se afirma exclusividade autoral sobre elementos puramente gerados por IA nem ausencia de direitos de terceiros.

A arte de referencia original (assets/references/settings.png) nao esta presente neste snapshot publico. A declaracao de procedencia e os limites estao registrados em assets/brand/NOTICE.md.

## Arquivos a revisar

- assets/brand/shireia-logo.png - arte principal da borboleta, usada como fonte para os icones de plataforma.
- assets/brand/butterfly.png - variante compacta da identidade.
- assets/brand/shireia.ico - icone Windows do app Shireia.
- assets/brand/shireia.rc - recurso Windows que associa o executavel ao icone Shireia.
- third_party/suyu/dist/shireia.bmp - derivado do logo, 256 x 256, usado para a janela SDL do alvo CLI.
- third_party/suyu/dist/shireia.icns - derivado do logo, usado no bundle macOS.

Os dois ultimos arquivos foram gerados neste snapshot a partir de shireia-logo.png, com proporcao preservada e transparencia. As referencias CMake/plist foram atualizadas; os avisos de copyright herdados nos arquivos Suyu foram mantidos.

## Roteiro de comparacao independente

1. Comparar silhueta das asas, simetria, joia central, gradientes roxos, brilhos e detalhes internos com marcas registradas e obras visuais relevantes.
2. Considerar software, jogos, emuladores e produtos digitais nas jurisdicoes/classes onde o projeto possa ser distribuido.
3. Examinar resultados figurativos e nominativos para "Shireia" e variantes visuais da borboleta.
4. Registrar bases pesquisadas, termos, datas, territorios e resultados semelhantes; encaminhar possiveis conflitos a profissional qualificado.

Uma busca exploratoria anterior encontrou borboletas roxas usadas por outros produtos, sem estabelecer correspondencia direta. Ela nao consultou exaustivamente bases oficiais e nao equivale a liberacao juridica de marca. A revisao independente continua pendente; a identidade atual foi preservada por decisao de Willlchrist.

## Identidade herdada ainda presente no snapshot

Recursos com identidade Suyu permanecem no snapshot herdado: `third_party/suyu/dist/suyu.svg`, `suyu.ico`, `suyu_tile_mask.png`, variantes em `dist/icon_variations/`, referencias em QRCs e arquivos da interface Qt Suyu, e icones Android legacy. Os recursos `.rc` de Windows para Qt, CLI e room apontam para o ICO Shireia. O frontend Qt Suyu e o alvo CLI estao desativados no perfil Windows Shireia; Android nao foi validado. Avisos e identidade herdada foram preservados.

`dist/shireia.bmp` e `dist/shireia.icns` substituem os recursos homonimos Suyu. Os quatro `.rc` Windows agora usam `dist/shireia.ico`; o recurso Suyu `dist/suyu.ico` permanece no snapshot por ser referenciado pelo `suyu.qrc` da interface Qt desativada. Permanecem tambem `suyu.svg`, `suyu_tile_mask.png`, `icon_variations`, outros recursos Qt e Android. A interface Qt e o app Android requerem migracao coordenada e build proprio antes de qualquer distribuicao como Shireia. Os arquivos upstream foram preservados; veja a secao de confirmacao criativa e ativos Windows abaixo.

## Perguntas para revisao independente

- A marca nominativa "Shireia" conflita com marcas existentes nos territorios relevantes para software, jogos, emulacao e servicos digitais?
- A silhueta da borboleta, joia central, simetria, paleta roxa, brilhos e composicao se aproximam de obras ou marcas figurativas protegidas?
- Os direitos e termos da ferramenta de IA vigentes na data de geracao permitem os usos declarados, e existe componente de terceiro reconhecivel apesar da intencao declarada de nao incorpora-lo?
- A avaliacao deve abranger nomes e simbolos herdados Suyu mantidos em alvos opcionais e na arvore Android?
- Quais paises, classes de registro e canais de distribuicao devem ser considerados?

A declaracao de procedencia e autorizacao de Willlchrist esta limitada aos direitos que detem, conforme `assets/brand/NOTICE.md`. Nenhuma busca exaustiva em registros oficiais ou parecer profissional foi concluido. A avaliacao de semelhanca e conflito permanece pendente, sem conclusao de exclusividade ou ausencia de conflito.


## Confirmacao criativa e ativos Windows

Declaracao de Willlchrist datada de 22/10/2026: a borboleta foi criada sob sua direcao criativa, com assistencia do ChatGPT, para ser a identidade visual oficial do Shireia. A declaracao nao comprova registro de marca, exclusividade juridica ou ausencia de conflitos. A procedencia e os limites de direitos estao descritos em `assets/brand/NOTICE.md`.

A aplicacao Windows principal ja usa `assets/brand/shireia.rc` e `assets/brand/shireia.ico`. Para alvos Windows opcionais herdados, a copia `third_party/suyu/dist/shireia.ico` e byte a byte igual ao arquivo de identidade existente; `suyu.rc`, `suyu_cmd/suyu.rc`, `dedicated_room/suyu_room.rc` e `dedicated_room/yuzu_room.rc` agora apontam para ela. Os cabecalhos SPDX/copyright herdados foram mantidos. O macOS ja aponta para `dist/shireia.icns`; o perfil de compilacao desta copia, porem, desativa o frontend Qt e o CLI e nao compila o bundle macOS.

### Alvos herdados que permanecem sem migracao visual

- **Qt/Suyu:** `ENABLE_QT=OFF` no perfil Shireia. O `suyu.qrc`, di?logos, tela de carregamento e `gamer_environment` usam `suyu.svg`, `suyu_tile_mask.png` e imagens Suyu. Esses recursos t?m pap?is de m?scara/tema e aparecem em v?rias telas. A migra??o requer altera??es coordenadas em QRC, UI e c?digo e uma compila??o e valida??o pr?prias. N?o ativar essa interface como frontend Shireia antes de migr?-la e valid?-la. Os assets e avisos herdados permanecem preservados.
- **Android:** a arvore Android herdada usa `@mipmap/ic_launcher`, icones vetoriais `ic_suyu*`/`ic_yuzu*`, nomes e outros recursos proprios do app upstream. Nao ha aplicativo Android Shireia integrado ou validado neste snapshot. A migracao requer adaptar o conjunto de icones/adaptive icon e a identidade do aplicativo, mantendo os avisos herdados; nenhum desses arquivos foi substituido. Nao anunciar nem distribuir um app Android Shireia com esses recursos sem uma migracao e build Android proprios.

A revisao visual registra procedencia e alvos; pesquisa independente de marca continua fora do que foi comprovado.
