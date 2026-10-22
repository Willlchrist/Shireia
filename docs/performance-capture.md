# Captura de desempenho durante a gameplay

1. Compile e inicie o Shireia com as mesmas opções gráficas usadas no teste normal. Mantenha a resolução interna em 1×.
2. Inicie conteúdo que você esteja autorizado a executar e entre em uma cena reproduzível. Espere terminarem os carregamentos e as compilações iniciais de shaders.
3. Abra **Diagnósticos** na barra lateral e selecione **Iniciar captura de desempenho**.
4. Jogue na mesma cena por 30–60 segundos, sem abrir menus ou trocar de área, e selecione **Parar e salvar**.
5. Registre o arquivo `performance-capture-*.jsonl` indicado pela interface e as configurações gráficas para reproduzir a execução.

O arquivo contém uma linha de metadados com as opções efetivas enviadas ao núcleo e amostras de um segundo com FPS do compositor Vulkan, quadros publicados no transporte, cópia CPU, alocação e gravação de comandos do readback, invalidação do buffer, transferência D3D e envio IPC. O log normal do Shireia também registra médias e percentis do caminho de readback em janelas de 120 quadros.

O caminho ativo usa uma imagem e um buffer Vulkan reutilizáveis, com `scheduler.Finish()` síncrono em cada quadro. Um experimento com readback assíncrono em três slots foi revertido após uma sessão de gameplay registrar falhas críticas no pool de staging Vulkan. Portanto, não use capturas anteriores marcadas `bounded-3-slot-async` para avaliar a estabilidade da versão atual. O FPS do compositor representa quadros entregues pelo renderizador headless, não um contador de FPS interno do jogo. Compare a mesma cena, duração, resolução 1× e opções equivalentes no Suyu.

## Diagnóstico de shaders e quadros lentos

Para associar quedas a esperas de pipeline, abra **Diagnósticos**, ative **Sonda Vulkan**, feche o jogo e reinicie o núcleo. A preferência fica salva e só vale para o próximo processo do núcleo. Faça a captura na mesma cena por 30–60 segundos e confirme `"stallProbeEnabled":true` nos metadados. A sonda fica desligada por padrão e deve permanecer desligada em uso normal.

Também é possível iniciar a interface pelo PowerShell com a sonda habilitada:

```powershell
$env:SUYU_STALL_PROBE = '1'
$env:SUYU_VK_PIPELINE_TIMING = '0'
& 'build/Shireia Next.exe'
```

Faça a captura durante a fase. O arquivo JSON registra `startTimestampMs`; linhas `SHIREIA_HEADLESS_PROBE` no log incluem `steady_ms`, permitindo relacioná-las ao mesmo intervalo. A sonda lista esperas de pipeline, tradução/emissão de shader, compilação de pipeline, busca/configuração do pipeline, espera do scheduler e tempo ocioso de GPU nos quadros acima de 16,7 ms. Essas durações podem se sobrepor e não devem ser somadas como se fossem o frametime. Para medir cada criação de pipeline, use `SUYU_VK_PIPELINE_TIMING='1'` em uma segunda execução: essa opção acrescenta cronômetros e mensagens para cada pipeline novo e pode alterar o perfil da execução; não a use na primeira comparação.

A captura precedente registrou compilação assíncrona de shaders desativada. No código Suyu dessa versão, a fila de workers e a deduplicação por chave já existem mesmo com essa opção desligada. O modo desligado pode fazer o desenho esperar um pipeline pendente; o modo ligado permite pular certos desenhos até o pipeline ficar pronto, podendo omitir objetos temporariamente. Por isso a opção não é ativada automaticamente. O cache de shaders serializado e o cache Vulkan específico do driver já são carregados e salvos pelo núcleo Suyu.
