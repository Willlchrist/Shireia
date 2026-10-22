# Controles e perfis

A página **Controles** separa a navegação da interface dos comandos enviados ao jogo. Teclado e dispositivos SDL2 são lidos no processo da interface; o estado dos controles ativos é enviado pelo IPC ao controle virtual do núcleo.

A entrada local oferece até dois jogadores: o dispositivo principal e um segundo controle SDL. O teste automatizado usa controles virtuais para verificar seleção, mapeamento, hotplug e envio do estado do segundo jogador. A compatibilidade com todos os modelos físicos, Bluetooth, rumble, sensores, NFC e câmeras não foi validada.

Os mapeamentos são editados como rascunho. **Aplicar** grava as alterações e **Cancelar** descarta o rascunho. Perfis de controle são locais e associados ao GUID do dispositivo. Nomes aceitam letras, números, espaços, ponto, hífen e sublinhado, com até 40 caracteres; a exclusão pede confirmação.

A página **Perfis** lista contas locais fornecidas pelo núcleo, destaca a conta selecionada e informa que a escolha vale para a próxima sessão. Alterar nome ou imagem fica indisponível durante uma sessão. A categoria Perfis nas configurações abre a mesma página de contas para evitar caminhos duplicados.

Os perfis de controle do Shireia armazenam mapeamentos de entrada por dispositivo; eles não substituem os perfis por jogador configurados pelo frontend Qt do Suyu.
