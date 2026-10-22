# Licenças e publicação

Este documento resume as licenças e os avisos presentes no snapshot-fonte. Não é parecer jurídico nem substitui os textos de licença e avisos mantidos junto aos componentes.

## Escopo das licenças

`LICENSE` contém o texto integral da GNU GPL versão 3. Willlchrist declarou, em 22/10/2026, ser o responsável humano pelas contribuições próprias, com auxílio do OpenAI Codex, e autorizou GPL-3.0-or-later somente para contribuições originais elegíveis na medida dos direitos que detém. O snapshot não preserva autoria por commit ou linha; essa declaração não atribui a ele código herdado, adaptado ou de terceiros, nem relicencia o conjunto.

O núcleo Suyu e os componentes de Yuzu, Dynarmic e terceiros mantêm suas licenças, avisos de copyright e atribuições. Os cabeçalhos SPDX e os avisos upstream devem permanecer nos arquivos modificados. `third_party/NOTICES.md`, `third_party/licenses/` e `docs/DEPENDENCIAS_E_LICENCAS.md` identificam os avisos, textos e revisões registrados neste snapshot. Sirit v1.0.5 está sob AGPL-3.0; sua seção 13 trata da combinação com GPL-3.0, sem retirar os termos e avisos aplicáveis ao componente.

A relação de arquivos e patches herdados/adaptados e os limites de autoria estão em [Contribuições e autoria](CONTRIBUICOES_E_AUTORIA_PENDENTE.md) e [SHIREIA-PATCHES.md](../third_party/suyu/SHIREIA-PATCHES.md). A auditoria detalhada de recursos e procedência está em [AUDITORIA_FINAL_DIREITOS.md](AUDITORIA_FINAL_DIREITOS.md).

## Referência `GetDriverVersion`

`src/graphics/suyu_overlay/renderer_vulkan.cpp` contém a função herdada do snapshot Suyu/Eden e preserva os avisos SPDX presentes no arquivo. O histórico local mais antigo disponível é o commit Suyu `49f03ddb8fb69fcc8944693f2fdc86e54a1ef327`, uma importação condensada de Eden; a história anterior não está no checkout.

O comentário aponta para `SaschaWillems/vulkan.gpuinfo.org`, revisão `5dddea46ea1120b0df14eef8f15ff8e318e35462`. O código PHP relacionado está em [`functions.php`, linhas 275–307](https://github.com/SaschaWillems/vulkan.gpuinfo.org/blob/5dddea46ea1120b0df14eef8f15ff8e318e35462/functions.php#L275-L307); o intervalo de linhas anotado no comentário upstream está incorreto. O repositório declara AGPL-3.0 nessa revisão e não foi localizado aviso individual ou exceção para o trecho. A seção 13 da AGPL permite combinar obra coberta com GPL-3.0, mantendo os termos aplicáveis a cada parte. Essa evidência documenta a licença declarada pelo repositório de origem; não comprova autor individual ou autorização separada. Os limites estão registrados na [auditoria](AUDITORIA_FINAL_DIREITOS.md).

## Código-fonte e binários

O snapshot-fonte mantém fontes vendorizadas, revisões fixadas, patches downstream, avisos e textos de licença documentados. A declaração GPL-3.0-or-later cobre apenas contribuições próprias elegíveis; a delimitação de trechos e a revisão de identidade visual continuam abertas. A auditoria documental não encontrou incompatibilidade comprovada para o snapshot-fonte Windows descrito, mas não certifica titularidade completa nem elimina a necessidade de revisão humana.

Uma distribuição binária requer auditoria separada do artefato exato, incluindo SBOM, dependências transitivas, DLLs, Qt/LGPL, plugins, codecs, avisos, fontes correspondentes e instruções de instalação. Nenhum binário ou instalador está coberto por este inventário.

## Referências

- [GNU: compatibilidade entre GPLv2 e GPLv3](https://www.gnu.org/licenses/gpl-faq.en.html#v2v3Compatibility)
- [Texto da GNU GPL versão 3](https://www.gnu.org/licenses/gpl-3.0.html)
- [Qt: obrigações GPL/LGPL](https://www.qt.io/development/open-source-lgpl-obligations) e [licenciamento do Qt 6](https://doc.qt.io/qt-6/licensing.html)
- [Avisos de terceiros](../third_party/NOTICES.md) e [textos de licença incluídos](../third_party/licenses/)
- [Compilação e dependências](COMPILACAO.md) e [inventário de dependências](DEPENDENCIAS_E_LICENCAS.md)
- [Política de distribuição](POLITICA_DE_DISTRIBUICAO.md)
