Licenças e atribuições

O Shireia Next incorpora o núcleo Suyu e componentes de terceiros. Os avisos
originais permanecem junto às fontes. Esta lista é um resumo para a interface;
não substitui os avisos e licenças de cada revisão nem uma SBOM do binário.

Núcleo de emulação e bibliotecas
- Suyu: GNU GPL, versão 3 ou posterior. O snapshot também contém arquivos
  herdados sob GPL-2.0-or-later. Preserve cabeçalhos, avisos e fonte correspondente.
- Partes herdadas de Citra/yuzu: GPL-2.0-or-later; preserve os avisos por arquivo.
- Dynarmic: o arquivo de licença principal incluído declara 0BSD; subcomponentes
  podem ter avisos próprios.
- Boost: Boost Software License 1.0.
- ENet: MIT. fmt: MIT; preserve também os avisos dos arquivos de documentação.
- Sirit v1.0.5: AGPL-3.0; preserve `licenses/sirit-AGPL-3.0-LICENSE.txt`. AGPL-3.0 section 13 permits combination with GPL-3.0 works, while its terms continue to govern the covered component. The BSD text remains an unassigned historical notice, is not linked as Sirit in the license UI, and is not included in the app license resources. The existing `licenses/sirit-LICENSE.txt` is BSD-3-Clause text and does not match this pinned version; its historical applicability is unverified.
- `GetDriverVersion` in `src/graphics/suyu_overlay/renderer_vulkan.cpp`: inherited through the Suyu snapshot's squashed Eden import, with matching vendor-version extraction logic referenced to `SaschaWillems/vulkan.gpuinfo.org`, revision `5dddea46ea1120b0df14eef8f15ff8e318e35462`. That repository carries AGPL-3.0 at the cited revision; preserve the AGPL notice text in `licenses/sirit-AGPL-3.0-LICENSE.txt` and the source attribution. Section 13 permits combination with GPL-3.0 and keeps AGPL terms on the covered portion. The upstream source comment's `#L308-L314` anchor is inaccurate; the corresponding PHP code is at `#L275-L307`.
- SPIRV-Headers: Apache-2.0; alguns dados de gramática estão sob CC BY 4.0.
- Decodificador BC: Mozilla Public License 2.0.
- tzdb_to_nx: o projeto conversor declara MIT; arquivos de eggert/tz usados por ele declaram BSD-3-Clause; os dados produzidos são descritos upstream como domínio público. O pacote 230326 está fixado por hash no manifesto CPM.
- LLVM Demangle: Apache-2.0 com LLVM Exceptions. SDL 3: zlib. glad: GPL-2.0-or-later.
- Lagoon 1.0.0: MIT, copyright Yang Liu 2026; aplicável ao alvo LoongArch64.
- FFmpeg e codecs: a licença efetiva depende da revisão, opções de build e codecs
  realmente ligados. Não inferir que x264/x265 estão incluídos sem evidência do
  build concreto. Consulte a auditoria antes de distribuir binários.

Interface, assets e ferramentas de suporte
- Teclado virtual Qt reutilizado de Suyu/yuzu: GPL-2.0-or-later; os recursos
  associados mantêm a atribuição dos projetos originais.
- Qt 6: LGPL-3.0 com exceção GPL do Qt; módulos e componentes terceiros têm
  licenças próprias. SDL 2: zlib.
- libarchive: BSD de 2 cláusulas, com avisos adicionais. OpenSSL 3: Apache-2.0.
- zlib: zlib.
- QDarkStyle: código MIT; LICENSE e dep5 distinguem imagens CC BY 4.0, outros ícones com termos próprios e arquivos sem identificacao específica. Nao presumir que todos os PNGs em `qt_themes/qdarkstyle/icons/` sejam CC BY 4.0.
- Variações de ícones Suyu: a documentação upstream declara GPLv3; avalie direitos de marca separadamente. Os MP3, logo Suyu e fontes compartilhadas apontados na auditoria estão excluídos do candidato Git. O Shireia usa arquivos de fonte da firmware do usuário. Os três PNGs sem procedência em `third_party/suyu/img/` também estão excluídos. A revisão de outros assets/fixtures Suyu e de similaridade da marca Shireia continua pendente. Veja `docs/AUDITORIA_FINAL_DIREITOS.md`.

Os textos de licença incluídos no produto podem ser consultados nesta tela. O
projeto é independente e não declara afiliação oficial com a Nintendo.
