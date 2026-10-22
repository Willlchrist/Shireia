# Inventário técnico de dependências e licenças

Este inventário é gerado a partir de `third_party/suyu/cpmfile.json` e da configuração Windows UCRT64 observada. Os hashes/versionamentos listados são entradas do manifesto, não prova de que todas as entradas sejam baixadas ou ligadas no perfil. A licença exata deve ser confirmada nos avisos da revisão fixada. Não é parecer jurídico nem SBOM dos executáveis.

## Código-fonte incluído

- Suyu: snapshot `third_party/suyu/`, commit `347fcf91d95cd39cb5c3db50f1b6d827ad699311`, GPL-3.0-or-later com arquivos herdados GPL-2.0-or-later. Copyrights e SPDX dos arquivos originais permanecem. As modificações downstream estão documentadas em `third_party/suyu/SHIREIA-PATCHES.md`, `shireia-core.patch` (seis arquivos de integração) e `shireia-publication.patch` (três arquivos de empacotamento).
- Dynarmic: incluído como fonte em `third_party/suyu/externals/dynarmic/`, commit `19249e8cbfb6e0d4c062bd7c4a111f55f220c211`. A licença no arquivo preservado é 0BSD (copyright de merryhime), não MIT.
- O restante das dependências CPM é obtido de repositórios públicos na primeira configuração e guardado somente no cache local. Não é incorporado na árvore versionável. Os textos de licença acompanham essas fontes baixadas; CC BY 4.0 para os dados de gramática SPIR-V agora também está em `third_party/licenses/`.

## Fontes CPM fixadas

| Identificador | Revisão | Hash SHA-512 | Licença identificada/estado | Perfil Windows |
|---|---|---|---|---|
| biscuit | v0.19.0 | `1229f345b014f7ca544dedb4edb3311e41ba736f9aa9a67f88b5f26f3c983288c6bb6cdedcfb0b8a02c63088a37e6a0d7ba97d9c2a4d721b213916327cffe28a` | MIT | Condicional ou não observada como download ativo neste perfil |
| boost | boost-1.90.0 | `6ae6e94664fe7f2fb01976b59b276ac5df8085c7503fa829d810fbfe495960cfec44fa2c36e2cb23480bc19c956ed199d4952b02639a00a6c07625d4e7130c2d` | BSL-1.0 | Selecionada/solicitada no perfil Windows; validar link final |
| boost_headers | boost-1.90.0 | `4ef845775e2277a8104ded6ddf749aa262ce52cf8438042869a048f9a0156dd772fbbcfa74efa1378fecef339b7286f6fe4b4feb5c45d49966b35d08e3e83507` | BSL-1.0 | Condicional ou não observada como download ativo neste perfil |
| catch2 | v3.13.0 | `7eea385d79d88a5690cde131fe7ccda97d5c54ea09d6f515000d7bf07c828809d61c1ac99912c1ee507cf933f61c1c47ecdcc45df7850ffa82714034b0fccf35` | BSL-1.0 | Condicional ou não observada como download ativo neste perfil |
| cpp-jwt | 7f24eb4c32 | `d11cbd5ddb3197b4c5ca15679bcd76a49963e7b530b7dd132db91e042925efa20dfb2c24ccfbe7ef82a7012af80deff0f72ee25851312ae80381a462df8534b8` | MIT | Condicional ou não observada como download ativo neste perfil |
| cubeb | fa02160712 | `8a4bcb2f83ba590f52c66626e895304a73eb61928dbc57777e1822e55378e3568366f17f9da4b80036cc2ef4ea9723c32abf6e7d9bbe00fb03654f0991596ab0` | ISC | Selecionada/solicitada no perfil Windows; validar link final |
| discord-rpc | 0d8b2d6a37 | `8213c43dcb0f7d479f5861091d111ed12fbdec1e62e6d729d65a4bc181d82f48a35d5fd3cd5c291f2393ac7c9681eabc6b76609755f55376284c8a8d67e148f3` | MIT | Condicional ou não observada como download ativo neste perfil |
| enet | v1.3.18 | `a0d2fa8c957704dd49e00a726284ac5ca034b50b00d2b20a94fa1bbfbb80841467834bfdc84aa0ed0d6aab894608fd6c86c3b94eee46343f0e6d9c22e391dbf9` | MIT | Selecionada/solicitada no perfil Windows; validar link final |
| ffmpeg | c7b5f1537d | `ed177621176b3961bdcaa339187d3a7688c1c8b060b79c4bb0257cbc67ad7021ae5d5adca5303b45625abbbe3d9aafdd87ce777b8690ac295290d744c875489a` | Base LGPL-2.1-or-later; optional GPL-2.0-or-later parts and file-specific terms; configure flags determine aggregate | CPM source not downloaded in Windows cache; separate MSYS2 package used in build |
| ffmpeg-ci | 8.0.1-c7b5f1537d | crueter-ci/FFmpeg; ci=true; no SHA-512 | Exact CI revision not verified; do not infer from regular FFmpeg | Conditional; not selected in audited Windows CMakeCache |
| fmt | 12.1.0 | `f0da82c545b01692e9fd30fdfb613dbb8dd9716983dcd0ff19ac2a8d36f74beb5540ef38072fdecc1e34191b3682a8542ecbf3a61ef287dbba0a2679d4e023f2` | MIT | Selecionada/solicitada no perfil Windows; validar link final |
| frozen | 61dce5ae18 | `b8dfe741c82bc178dfc9749d4ab5a130cee718d9ee7b71d9b547cf5f7f23027ed0152ad250012a8546399fcc1e12187efc68d89d6731256c4d2df7d04eef8d5c` | Apache-2.0 (`LICENSE` at pinned ref) | Conditional or not observed in this Windows build |
| gamemode | ce6fe122f3 | `e87ec14ed3e826d578ebf095c41580069dda603792ba91efa84f45f4571a28f4d91889675055fd6f042d7dc25b0b9443daf70963ae463e38b11bcba95f4c65a9` | BSD-3-Clause | Condicional ou não observada como download ativo neste perfil |
| httplib | v0.46.0 | `159ed94965018f2a371d45a3bfc1961e5fb1549e501ded70a6b4532d7fe99d0579c18b5195aff6e35f96f399b426cea2650ec9fb75ef80d4c9edeccb51f2e6c9` | MIT | Selecionada/solicitada no perfil Windows; validar link final |
| lagoon | 1.0.0 | `b9380f99c6effaeccc6d8f81d4942e852c11ad28613df637e155451556ae5826f93765bee57a5c87a9740d2bd1db463ad0f55a947772fe9d57eeabae3efa373e` | MIT; copyright Yang Liu 2026 ([LICENSE na tag 1.0.0](https://github.com/loongson-community/lagoon/blob/1.0.0/LICENSE)); texto preservado em `third_party/licenses/Lagoon-MIT-LICENSE.txt` | COMPROVADO COM OBRIGAÇÕES: preservar aviso e texto MIT; selecionada para LoongArch64, fora do alvo Windows x64 validado |
| libadrenotools | 8ba23b42d7 | `f6526620cb752876edc5ed4c0925d57b873a8218ee09ad10859ee476e9333259784f61c1dcc55a2bcba597352d18aff22cd2e4c1925ec2ae94074e09d7da2265` | BSD-2-Clause (`LICENSE` at pinned ref) | Conditional Android dependency; not selected for Windows x64 |
| libusb | v1.0.29 | `98c5f7940ff06b25c9aa65aa98e23de4c79a4c1067595f4c73cc145af23a1c286639e1ba11185cd91bab702081f307b973f08a4c9746576dc8d01b3620a3aeb5` | LGPL-2.1 | Condicional ou não observada como download ativo neste perfil |
| linkernsbypass | aa3975893d | `bbe3f1f08e2bc7172b36e8f052912cc374289fc9a8e5a39ae7547a0c232de8f57ba24883451896b7a9a5d1be0e4de5c5b0f70c2022eda18d7d5a754847521800` | BSD-2-Clause; upstream preserva `LICENSE` e SPDX nos fontes ([fonte](https://github.com/bylaws/liblinkernsbypass/blob/aa3975893d/LICENSE)) | Android; não selecionada no alvo Windows x64 |
| llvm-mingw | 20250828 | clang-rt-builtins.tar.zst; fixed hash | Exact artifact/toolchain notices not verified | Conditional Clang/clang-cl toolchain; not selected in audited profile |
| lz4 | ebb370ca83 | `35c21a5d9cfb5bbf314a5321d02b36819491d2ee3cf8007030ca09d13ca4dae672247b7aeab553e973093604fc48221cb03dc92197c6efe8fc3746891363fdab` | BSD-2-Clause (alguns arquivos GPL-2.0-or-later) | Condicional ou não observada como download ativo neste perfil |
| moltenvk | v1.4.1-ryujinx | MoltenVK-macOS.tar; fixed hash | Exact Ryujinx artifact license not confirmed | Conditional Apple/macOS; not selected in audited profile |
| nlohmann | v3.12.0 | `6cc1e86261f8fac21cc17a33da3b6b3c3cd5c116755651642af3c9e99bb3538fd42c1bd50397a77c8fb6821bc62d90e6b91bcdde77a78f58f2416c62fc53b97d` | MIT | Selecionada/solicitada no perfil Windows; validar link final |
| oaknut | v2.0.3 | `9697e80a7d5d9bcb3ce51051a9a24962fb90ca79d215f1f03ae6b58da8ba13a63b5dda1b4dde3d26ac6445029696b8ef2883f4e5a777b342bba01283ed293856` | MIT, copyright merryhime 2022 ([fonte](https://github.com/eden-emulator/oaknut/blob/v2.0.3/LICENSE)) | Condicional: AArch64 ou testes Dynarmic; não selecionada no alvo Windows x64 com testes Dynarmic desativados |
| oboe | 1.10.0 | `ce4011afe7345370d4ead3b891cd69a5ef224b129535783586c0ca75051d303ed446e6c7f10bde8da31fff58d6e307f1732a3ffd03b249f9ef1fd48fd4132715` | Apache-2.0 | Condicional ou não observada como download ativo neste perfil |
| openssl | openssl-3.6.2 | `29002ce50cb95a4f4f1d0e9d3f684401fbd4eac34203dc2eef3b6334af5d44aa46bf788b63a6f5c139c383eafb7269ae87a58a9a3ad5912903b9773e545ccc0a` | Apache-2.0 | Condicional ou não observada como download ativo neste perfil |
| openssl-ci | 4.0.0-11b7b6ea3b | crueter-ci/OpenSSL; ci=true; no SHA-512 | Exact CI revision not verified; do not infer regular Apache-2.0 | Bundled option OFF in audited Windows cache |
| openssl-cmake | 3.6.2 | `2cc185c924fd70e7d886257ca0caa42b3b8f7f712f2052b4f94dde74759e27022de76178460e18c9bdfc57c366583999e198fbb6052d4e7d91c099d15a0ca63e` | MIT for the CMake wrapper; OpenSSL source keeps its own Apache-2.0 license | Bundled build helper; not an independent OpenSSL license |
| opus | a3f0ec02b3 | `9506147b0de35befda8633ff272981cc2575c860874791bd455b752f797fd7dbd1079f0ba42ccdd7bb1fe6773fa5e84b3d75667c2883dd1fb2d0e4a5fa4f8387` | BSD-3-Clause | Condicional ou não observada como download ativo neste perfil |
| quazip | 2e95c9001b | `609c240c7f029ac26a37d8fbab51bc16284e05e128b78b9b9c0e95d083538c36047a67d682759ac990e4adb0eeb90f04f1ea7fe2253bbda7e7e3bcce32e53dd8` | LGPL-2.1-or-later | Condicional ou não observada como download ativo neste perfil |
| sdl3 | release-3.4.8 | `df5a323af7ac366661a3c0e887969c72584d232f3cc211419d59b0487b620b6b2859d4549c9e8df002ee489290062e466fcfddf7edc0872a37b1f2845e81c0f3` | zlib | Selecionada/solicitada no perfil Windows; validar link final |
| sdl3-ci | 3.4.8-d57c3b685c | crueter-ci/SDL3; ci=true; no SHA-512 | Exact CI revision not verified; do not infer regular zlib | Bundled option OFF in audited Windows cache |
| simpleini | v4.25 | `b937c18a7b6277d77ca7ebfb216af4984810f77af4c32d101b7685369a4bd5eb61406223f82698e167e6311a728d07415ab59639fdf19eff71ad6dc2abfda989` | MIT | Selecionada/solicitada no perfil Windows; validar link final |
| sirit | v1.0.5 | `b7cd6885acae3fc8698288d19febba0dd45e4a02a2b6563d2eb995a988d0847046e8d16a5dea7e0bf832b4321bcec134cdbafc553f6ede039e4cf34525c5dce5` | AGPL-3.0; pinned source LICENSE checked; section 13 expressly permits combination with GPL-3.0 works | Selected in Windows profile; retain AGPL notice for Sirit and GPL notices for combined work |
| sirit-ci | 1.0.5 | eden-emulator/sirit; ci=true; distinct CI ref | Exact CI revision not verified; do not infer regular AGPL or legacy BSD | Bundled option OFF in audited Windows cache |
| spirv-headers | vulkan-sdk-1.4.341.0 | `d624371dd455c66a300344c89812598ffe11b5eedba555779f789e85c29dc67317741858c60e0744a1e6755cc0d2759b8659f0674f4cc31479c4cb6fc25ed23b` | Apache-2.0; dados de gramática CC BY 4.0 | Selecionada/solicitada no perfil Windows; validar link final |
| tzdb | 230326 | `cce65a12bf90f4ead43b24a0b95dfad77ac3d9bfbaaf66c55e6701346e7a1e44ca5d2f23f47ee35ee02271eb1082bf1762af207aad9fb236f1c8476812d008ed` | Projeto conversor MIT; fontes `date.c`, `newstrftime.3` e `strftime.c` BSD-3-Clause; dados gerados em domínio público conforme README upstream | Selecionada/solicitada no perfil Windows; texto de licença não acompanha arquivo de dados em cache |
| unordered-dense | 7b55cab841 | `d2106f6640f6bfb81755e4b8bfb64982e46ec4a507cacdb38f940123212ccf35a20b43c70c6f01d7bfb8c246d1a16f7845d8052971949cea9def1475e3fa02c8` | MIT | Selecionada/solicitada no perfil Windows; validar link final |
| vulkan-headers | v1.4.345 | `d2846ea228415772645eea4b52a9efd33e6a563043dd3de059e798be6391a8f0ca089f455ae420ff22574939ed0f48ed7c6ff3d5a9987d5231dbf3b3f89b484b` | Apache-2.0 | Selecionada/solicitada no perfil Windows; validar link final |
| vulkan-memory-allocator | v3.3.0 | `deb5902ef8db0e329fbd5f3f4385eb0e26bdd9f14f3a2334823fb3fe18f36bc5d235d620d6e5f6fe3551ec3ea7038638899db8778c09f6d5c278f5ff95c3344b` | MIT | Selecionada/solicitada no perfil Windows; validar link final |
| vulkan-utility-libraries | v1.4.345 | `114f6b237a6dcba923ccc576befb5dea3f1c9b3a30de7dc741f234a831d1c2d52d8a224afb37dd57dffca67ac0df461eaaab6a5ab5e503b393f91c166680c3e1` | Apache-2.0 | Selecionada/solicitada no perfil Windows; validar link final |
| vulkan-validation-layers | vulkan-sdk-%NUMERIC_VERSION% (1.4.341.0) | android-binaries-%NUMERIC_VERSION%.zip; fixed hash | Exact Android artifact license not confirmed | Android not selected in audited Windows cache |
| xbyak | v7.35.2 | `b6475276b2faaeb315734ea8f4f8bd87ededcee768961b39679bee547e7f3e98884d8b7851e176d861dab30a80a76e6ea302f8c111483607dde969b4797ea95a` | BSD-3-Clause | Selecionada/solicitada no perfil Windows; validar link final |
| zlib | v1.3.2 | `16fea4df307a68cf0035858abe2fd550250618a97590e202037acd18a666f57afc10f8836cbbd472d54a0e76539d0e558cb26f059d53de52ff90634bbf4f47d4` | zlib | Condicional ou não observada como download ativo neste perfil |
| zstd | b8d6101fba | `cc5ad4b119a9c2ea57f0b71eeff01113bb506e0d17000159c5409cb8236d22e38c52d5e9e97e7947a4bf1b2dfc44b6c503ab2d9aedbd59458435c6a2849cb029` | BSD-3-Clause OR GPL-2.0-or-later | Condicional ou não observada como download ativo neste perfil |

## Bibliotecas do ambiente MSYS2/UCRT64 observadas

| Componente | Versão local observada | Licença declarada no pacote | Origem |
|---|---:|---|---|
| GCC / MinGW-w64 | 16.2.0-6 | GCC GPL-3.0-or-later com Runtime Library Exception; runtimes MinGW/GCC têm avisos próprios | pacotes MSYS2 UCRT64 |
| CMake | 4.4.4-3 | MIT | MSYS2 UCRT64 |
| Ninja | 1.13.2-1 | Apache-2.0 | MSYS2 UCRT64 |
| Qt Base + Declarative (Quick/QML) | 6.11.2-2 / 6.11.2-1 | LGPL-3.0-only com Qt GPL Exception e componentes terceiros com licenças próprias; alguns módulos Qt só sob GPL | MSYS2 UCRT64 |
| SDL2 | 2.32.10-1 | zlib | MSYS2 UCRT64 |
| FFmpeg | 9.0.2-1 | GPL-3.0-or-later no pacote MSYS2 observado; codecs x264/x265 trazem obrigações/avisos próprios | MSYS2 UCRT64 |
| OpenSSL | 3.6.5-1 | Apache-2.0 | MSYS2 UCRT64 |
| libarchive | 3.8.9-6 | BSD-2-Clause e avisos adicionais | MSYS2 UCRT64 |
| libusb | 1.0.30-1 | LGPL-2.1 | MSYS2 UCRT64 |
| lz4 | 1.10.0-1 | BSD e GPL por componentes | MSYS2 UCRT64 |
| zstd | 1.5.7-2 | BSD-3-Clause OR GPL-2.0-or-later | MSYS2 UCRT64 |
| zlib | 1.3.2-2 | zlib | MSYS2 UCRT64 |
| Opus | 1.6.1-1 | BSD-3-Clause | MSYS2 UCRT64 |
| ICU | 78.3 | licença ICU; verificar no pacote efetivamente distribuído | dependência transitiva UCRT64 |

As versões acima são as instalações observadas nesta máquina em 21/10/2026, não versões travadas pelo projeto. MSYS2 é mutável; para builds bit a bit reproduzíveis ainda falta um lockfile/ambiente congelado para versões e hashes dos pacotes de sistema.

## Pendências antes de publicar fontes ou binários

1. No build Windows x64 desta revisão, os avisos/licenças do código CPM baixado foram conferidos nas fontes exatas presentes no cache local: Boost (`LICENSE_1_0.txt`), cubeb, ENet, fmt, gamemode, cpp-httplib, nlohmann_json, SDL3, SimpleIni (`LICENCE.txt`), Sirit, SPIRV-Headers, unordered_dense, Vulkan-Headers, Vulkan Memory Allocator, Vulkan Utility Libraries e Xbyak (`COPYRIGHT`/README). Para tzdb, o cache contém os dados empacotados sem um arquivo LICENSE; a página/README upstream descreve o código conversor como MIT, três fontes derivadas de eggert/tz como BSD-3-Clause e os dados produzidos como domínio público ([repositório upstream](https://git.eden-emu.dev/eden-emu/tzdb_to_nx)). Isso resolve as identificações de licença de topo desses pacotes do cache, mas não constitui auditoria de todas as dependências transitivas, todos os 44 registros CPM nem os binários MSYS2.
2. Gerar uma SBOM do build distribuído após decidir os binários, módulos Qt/QML, codecs FFmpeg, DLLs e forma de vinculação. A configuração observada liga Suyu/Dynarmic como archives estáticos e usa dependências do MSYS2; o deployment precisa preservar todos os avisos e fontes correspondentes aplicáveis.
3. `third_party/NOTICES.md` é um resumo de interface, não uma lista completa de copyright por dependência transitiva. Pacotes redistribuídos devem vir acompanhados dos avisos/licenças de suas revisões exatas.
4. Não foi encontrado requisito de autorização proprietária especial no material auditado para as dependências OSS listadas; isso não substitui a verificação de cada licença, aviso e direito sobre marcas/patentes, especialmente FFmpeg/codecs e redistribuição de pacotes MSYS2.
5. A declaração de Willlchrist permite GPL-3.0-or-later para contribuições originais elegíveis na medida dos direitos detidos; a confirmação de origem linha a linha ainda requer revisão humana. Veja `AUDITORIA_FINAL_DIREITOS.md` e `LICENCAS_E_PUBLICACAO.md`.
6. `third_party/suyu/src/android/app/debug.keystore` é uma chave de assinatura de depuração do projeto Android upstream, sem uso no alvo Windows do Shireia. Foi acrescentada ao `.gitignore` para não publicar material de assinatura. Cópias de backup `*.bak`, bytecode Python/cache, o arquivo de archive redundante `spirv-headers-new.zip` e sua extração local também foram excluídos dos candidatos, sem apagar os arquivos locais. A compilação limpa demonstrou que o build Windows obtém a revisão SPIRV-Headers fixada por CPM sem esses dois itens.
7. O invent?rio do snapshot identificou 566 caminhos em `third_party/suyu/dist/` (incluindo 18 manifestos QRC, 468 PNGs, traduções e temas) e 84 arquivos de teste sob `third_party/suyu/tests/`; não foram encontrados ROMs ou pacotes de firmware entre esses caminhos. Cabeçalhos SPDX nos manifestos QRC, README de icon variations e licenças do QDarkStyle foram preservados quando presentes, mas isso não prova origem/licença de todo asset referenciado. Os três PNGs não referenciados em `third_party/suyu/img/` foram excluídos do candidato Git por falta de procedência licenciada; `dist/Assets.car` continua excluído. Os arquivos de fontes compartilhadas, logo/áudio e Glyphicons citados na auditoria também estão excluídos; a fonte vem da firmware local do usuário. Continua necessária revisão por grupo/arquivo dos demais assets herdados, marcas e fixtures sem aviso específico.

## Complemento da auditoria final de direitos — 21/10/2026

A tabela acima é um inventário técnico do manifesto CPM; ela não deve ser lida como prova individual de licença para cada pacote ou como SBOM de um binário. A avaliação de recursos binários, fontes incorporadas, marca, permissões pendentes e checklists está em [AUDITORIA_FINAL_DIREITOS.md](AUDITORIA_FINAL_DIREITOS.md). O relatório distingue licenças confirmadas na revisão fixada de itens apenas identificados por cabeçalho ou nome de pacote.

Confirmações pontuais feitas nas fontes oficiais das revisões exatas:

- Lagoon 1.0.0: MIT, copyright Yang Liu 2026; cópia do aviso em `third_party/licenses/Lagoon-MIT-LICENSE.txt`.
- Oaknut v2.0.3: MIT, copyright merryhime 2022; licença no snapshot externo correspondente e na tag fixada.
- linkernsbypass `aa3975893d83ef1bc84c321ec60c65fbf1287887`: BSD-2-Clause, copyright Billy Laws 2021; componente condicional Android, não selecionado para Windows x64.

As dependências de sistema MSYS2 (Qt, SDL2, FFmpeg, OpenSSL, ICU, GCC runtime e outras) não são travadas a versões e hashes pelo projeto. Uma futura distribuição Windows deve ser auditada com a lista real de DLLs, plugins Qt, imports QML e configuração real do FFmpeg incluídos no pacote.






## Revisão de dependencias para publicacao somente do codigo-fonte - 22/10/2026

O manifesto tem 44 registros CPM. No cache do build Windows foram encontrados 18 registros correspondentes a 17 arvores de fonte distintas (Boost e `boost_headers` usam a mesma arvore). Foram conferidos os avisos de topo de 16 pacotes CPM no cache, e a procedencia/licenca do tzdb foi verificada no repositorio upstream. Isso confirma os avisos de topo, nao cada arquivo interno ou subdependencia.

Dos 26 registros fora do cache, 19 tiveram arquivos de licença/avisos examinados na revisão indicada: doze foram classificados por SPDX e sete lidos manualmente. Os sete pins CI/plataforma restantes não tiveram a revisão/artifact exata confirmada; não estender status da dependência base à variante.

A revisao corrigiu tres rotulos anteriores: `frozen` e Apache-2.0 (nao BSL-1.0); `libadrenotools` e BSD-2-Clause (nao MIT); `openssl-cmake` e MIT (nao Apache-2.0). FFmpeg source nao tem uma unica classificacao GPL: a licenca da revisao declara LGPL-2.1-or-later como base, com partes opcionais GPL-2.0-or-later ativadas por configuracao; arquivos individuais podem ter outros termos. O pacote FFmpeg do MSYS2 usado no build e distinto da entrada CPM. Consulte [o arquivo LICENSE.md do FFmpeg na revisao fixada](https://github.com/FFmpeg/FFmpeg/blob/c7b5f1537d/LICENSE.md).
A fonte fixada de Sirit `v1.0.5` foi conferida diretamente e traz AGPL-3.0 (não BSD-3-Clause); o texto correto foi incluído em `third_party/licenses/sirit-AGPL-3.0-LICENSE.txt`. O arquivo legado `third_party/licenses/sirit-LICENSE.txt` contém BSD-3-Clause e não corresponde à licença da revisão CPM atual; sua origem/aplicabilidade histórica não foi comprovada. A seção 13 da AGPL-3.0 permite combinar o componente com uma obra GPL-3.0, mantendo os termos AGPL para o componente Sirit. [LICENSE na tag exata](https://github.com/eden-emulator/sirit/blob/v1.0.5/LICENSE).

Sete refs CPM exatos continuam sem confirmacao documental de licenca do artifact: `ffmpeg-ci` 8.0.1-c7b5f1537d, `llvm-mingw` 20250828, `moltenvk` v1.4.1-ryujinx, `openssl-ci` 4.0.0-11b7b6ea3b, `sdl3-ci` 3.4.8-d57c3b685c, `sirit-ci` 1.0.5 e `vulkan-validation-layers` 1.4.341.0. Eles nao estao vendorizados no snapshot e nao foram selecionados no perfil Windows x64 auditado; nao inferir licenca de um pacote base para o artifact/fork.

No `third_party/suyu/CMakeModules/CPMUtil.cmake`, as quatro entradas `ci=true` resolvem artifacts de release por plataforma/arquitetura e baixam o arquivo `.sha512sum` durante a configuracao; o digest nao fica fixado no `cpmfile.json`. Isso cria uma pendencia de verificabilidade/reprodutibilidade do artifact alem da licenca. `llvm-mingw`, `moltenvk` e `vulkan-validation-layers` fixam SHA-512 no manifesto, mas seus archives e notices ainda precisam ser inspecionados. Nenhum desses sete artifacts e copiado para o snapshot-fonte. Esta ressalva nao demonstra incompatibilidade para a publicacao do snapshot-fonte Windows, mas impede anunciar esses perfis como auditados ou redistribuir seus artifacts.

O manifesto e as configuracoes CMake nao fornecem um lockfile completo do grafo transitivo por plataforma. `cubeb` foi configurado com `BUNDLE_SPEEX=ON`; os componentes incluidos por essa opcao e outras transitivas precisam ser conferidos nas fontes resolvidas para a plataforma pretendida. Os pacotes MSYS2 continuam mutaveis e sem hashes fixados pelo projeto. O inventario direto esta substancialmente conferido, mas a verificacao transitiva completa permanece pendente. Isso e separado da SBOM de uma futura distribuicao binaria, que deve listar DLLs, plugins Qt/QML, codecs e avisos reais.

O `dep5` upstream registra recursos CC BY-SA 3.0, incluindo dois arquivos marcados como modificados. A Creative Commons nao lista licencas nao-CC como compativeis com BY-SA 3.0. Os arquivos permanecem separados no repositorio-fonte, e o frontend Qt Suyu nao e usado no build Shireia; isso nao comprova conflito no fonte, mas exige revisao antes de embutir esses recursos em binario GPL. Veja [compatibilidade CC BY-SA](https://creativecommons.org/compatible-licenses/).

## Pendências CPM para perfis alternativos — 22/10/2026

O build Windows x64 auditado não seleciona as sete referências exatas pendentes: variantes CI desligadas; Android, Apple e toolchain Clang não selecionados. Essas fontes/artifacts não estão vendorizadas no snapshot Git. A incerteza não demonstra incompatibilidade para o snapshot-fonte atual, mas precisa ser resolvida antes de habilitar ou redistribuir esses perfis:

- `ffmpeg-ci`: revisar LICENSE e componentes da revisão CI exata; licença do FFmpeg normal não basta.
- `llvm-mingw`: conferir avisos do archive de toolchain e runtimes incluídos.
- `moltenvk`: confirmar licença e notices do artifact/tag Ryujinx exatos.
- `openssl-ci` e `sdl3-ci`: revisar forks/commits CI; Apache-2.0 e zlib das fontes normais não provam os termos das variantes.
- `sirit-ci`: confirmar revisão/licença própria; não inferir AGPL-3.0 da dependência Sirit normal.
- `vulkan-validation-layers`: confirmar licença do artifact Android/Vulkan SDK exato.

O CMakeCache de `build-audit-clean-20261021` mostra `YUZU_USE_BUNDLED_FFMPEG=OFF`, `YUZU_USE_BUNDLED_OPENSSL=OFF`, `YUZU_USE_BUNDLED_SDL3=OFF`, `YUZU_USE_BUNDLED_SIRIT=OFF` e Android não selecionado. O escopo não certifica Android, macOS, toolchain Clang, CI alternativo nem seus binários. Para esses é necessário inventário próprio e, para distribuição binária, SBOM.
