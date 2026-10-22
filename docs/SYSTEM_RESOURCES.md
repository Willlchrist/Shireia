# Chaves, firmware e metadados da biblioteca

Em **Configurações → Sistema**, selecione `prod.keys` e a pasta de firmware extraído contendo `.nca` diretamente dentro dela. Os seletores são nativos do Windows. Clique em Aplicar/OK para confirmar. Cancelar não importa os arquivos selecionados no rascunho. ZIP não é extraído automaticamente.

O Shireia inicia `ShireiaCore --library-job` em segundo plano. Esse processo usa as APIs reais do núcleo, sem carregar ou executar jogos. A interface continua responsiva e mostra o resultado na biblioteca e na categoria Sistema. Se o núcleo de sessão estiver ativo, a análise aguarda seu encerramento; não é encerrado automaticamente.

## Pastas próprias

- Chaves importadas: `build/core-data/keys/prod.keys`.
- Firmware: `build/core-data/nand/system/Contents/registered`.
- Ícones extraídos: `build/core-data/cache/icons`.

Os caminhos são relativos ao diretório do executável, portanto acompanham uma distribuição local. As APIs `SetSuyuPath(KeysDir/NANDDir)` apontam tanto o processo de análise quanto o núcleo de sessão para essas pastas. O APPDATA continua isolado em core-data/roaming. O checkout Suyu original é somente leitura.

O conteúdo de prod.keys não entra em QSettings, respostas JSON ou mensagens da interface. O arquivo selecionado é primeiro carregado em uma pasta temporária pelo KeyManager original. Se nenhuma combinação reconhecida de chaves 128/256 bits puder ser carregada, a importação é rejeitada sem substituir o arquivo anterior. Reconhecer as chaves não prova que todas as chaves necessárias para cada conteúdo estão presentes ou corretas.

Firmware é copiado em blocos para uma área temporária na NAND privada. Os nomes devem manter o content ID original de 32 dígitos hexadecimais (`<id>.nca` ou `<id>.cnmt.nca`), conforme exigido pelo cache do núcleo. Cada NCA copiado precisa ser legível pelo parser original e possuir title ID de sistema. Só após todos passarem a área preparada é promovida a registered. A instalação anterior é movida para `registered-backup-<identificador>` na mesma pasta Contents e preservada. Arquivos de origem não são movidos nem removidos. Um lock impede dois importadores simultâneos.

A verificação não é uma certificação criptográfica completa nem garante a presença de todos os componentes de uma versão do firmware. Uma pasta incompleta de NCAs individualmente legíveis pode ser importada; nenhuma versão é inventada. Falta de chaves, arquivos inválidos, erros de cópia e espaço insuficiente são informados. As duas renomeações de troca não constituem uma transação indivisível do Windows: se o processo for encerrado exatamente entre elas, a instalação anterior permanece em registered-backup e pode precisar ser restaurada manualmente. Áreas temporárias interrompidas podem permanecer no disco.

Um manifesto com nomes/tamanhos/datas da origem evita recópias do mesmo conjunto; não é um teste de integridade dos dados já instalados. A instalação anterior também é preservada em caso de falha detectada antes da troca. Pastas redirecionadas ou registered como link são rejeitadas.

**Limpar** remove a referência à origem do rascunho; não desinstala cópias já importadas. Restaurar padrões também não apaga a NAND ou as chaves importadas.

## Atualização automática

Após confirmar as configurações, ao abrir o programa, ao atualizar a biblioteca ou quando o arquivo/pasta de chaves monitorado mudar, a análise chama `Loader::GetLoader`, `ReadTitle` e `ReadIcon` nos arquivos NSP, XCI, NRO e NSO locais. São utilizados os dados realmente retornados pelo núcleo. Não há busca na internet nem catálogo de títulos inventados.

Arquivo + tamanho + data de modificação identificam a versão do conteúdo. Metadados válidos ficam em QSettings e os ícones em cache. Alterar o arquivo invalida o metadado correspondente. Favoritos, recentes e capas manuais são preservados; a capa manual tem prioridade. Sem título/ícone legível, permanecem o nome local e o placeholder. Últimos metadados válidos do mesmo arquivo podem permanecer quando uma análise posterior não consegue lê-los.

Alterações de origem durante uma análise são verificadas ao concluir. O botão **Analisar e atualizar agora** força uma nova tentativa. O prazo máximo do processo é de dez minutos; falha do leitor não fecha a interface. A enumeração continua sem subpastas. Instaladores NSP, updates/DLC separados, metadados ausentes no conteúdo e formatos não suportados pelo loader podem não fornecer nomes ou ícones. Instalar firmware/chaves não garante compatibilidade; consulte [Recursos, validação e limitações](RECURSOS_E_VALIDACAO.md).

## Validação

O CTest `library-resources` gera um NRO de teste autoral com NACP e uma pequena imagem PNG, verifica a extração pelo núcleo original, cache persistente, atualização após alteração do conteúdo e prioridade da capa manual. Verifica também que textos inválidos escolhidos como chaves/firmware não substituem arquivos anteriores. Nenhum fixture contém chaves reais, firmware ou ROMs.

O fluxo de importação foi testado com fixtures sintéticas inválidas, que não substituem um teste com firmware do usuário. A importação de firmware real e a leitura de metadados de jogos criptografados dependem de arquivos locais apropriados.
