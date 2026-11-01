# Tiers independentes — instalado em 18/10/2026

## Regras implementadas e aprovadas
- Todo novo drop de equipamento nas categorias 0 a 13 recebe um tier C/B/A/S. O tier do item e os tiers dos adicionais sao sorteados separadamente e gravados pelo serial.
- Dano/defesa do item: C=25%, B=50%, A=75%, S=100% da contribuicao normal do equipamento. Nao altera o dano/defesa natural da classe.
- Adicionais Excellent: tabelas individuais em TABELAS_EXCELLENT.md. Vida Excellent: C=1%, B=2%, A=3%, S=4%. Adicional individual Ancient: C=25%, B=50%, A=75%, S=100% do valor original.
- Valores diretos arredondam para o inteiro mais proximo, com minimo 1 para um bonus positivo. Divisores de dano por nivel aumentam nos tiers menores, usando teto para evitar aumentar o bonus.
- Bonus ativados pelo conjunto Ancient conservados. PvP +1..+10 continua separado/desligado; evolucao de equipamento na Chaos Machine continua cancelada.
- Pesos iniciais configuraveis: C60/B25/A12/S3, tanto para item quanto para cada adicional. A mesma chance nao obriga os resultados a serem iguais.
- Sem rerrolagem ao pegar, equipar, guardar, trocar, vender ou reconectar. Itens antigos sem metadados mantem os valores originais.

## Interface
- Tooltip informa o tier do item e a letra de cada adicional, com os valores efetivos informados pelo servidor.
- Consulta em inventario, equipamentos normais/extras, bau, os dois lados do trade, loja NPC e lojas pessoais.
- Respostas vinculadas a contexto/slot/tipo/chave local/nonce; troca de vendedor e de item invalidam respostas antigas.
- Catalogos de NPC sem instancia persistida mantem a descricao original. Criacao especial por Chaos Machine, entregas diretas/loja e comandos que usam rotas especiais nao recebe sorteio novo por este hook. Asas recebem tier do item; seus efeitos especiais nao sao tratados como os seis adicionais Excellent de armas/armaduras.

## Instalacao realizada
- Cliente: E:/Client5.2/Main.exe
- DataServer: C:/MuServer1/2.- DataServer/DataServer.exe
- GameServer: C:/MuServer1/4.- GameServer/GameServer.exe
- Subsala: C:/MuServer1/4.2- GameServer/SubSala.exe
- Castle Siege: C:/MuServer1/5.- GameServerCS/GameServerCS.exe
- Encoder: C:/MuServer1/Tools/MainInfo/Encoder.exe
- MainInfo e sidecars cliente/servidor com ItemTierSystemEnable, ItemTierVisualEnable, AdditionalTierSystemEnable, AdditionalTierVisualEnable, MetadataTransportEnable e IndependentDropCreationEnable ativados.
- Tabelas configuraveis: ItemTierPercent, AdditionalTierPercent, LifePercent, ItemTierWeights e AdditionalTierWeights.
- Banco confirmado pelo DSN x86 MuOnlineBase: tabela dbo.NeonItemTiers e procedimentos dbo.NeonItemTierRead/Create instalados. Nenhum blob de inventario foi reescrito.
- Protocolo de consulta C1/F3/63 versao 6; maximo 174 bytes. GS/DS usa C2/E9 versao 1.
- Backup: C:\MuServer1\_Backup_IndependentTiers_20261018_194631
- Restauracao: RESTAURAR_ANTES_DOS_TIERS.ps1, com executaveis fechados. Mantem metadados SQL para nao perder os sorteios ja criados.

## Validacao concluida
- Build Release x86 completo: Lua52, DataServer, GameServer, GameServerCS, Main e Encoder.
- Testes do modelo: 20.480 combinacoes independentes; codecs/protocolos e seus limites; criacao sem rerrolagem; conflitos; escala de adicionais; cache, timeout, repeticao automatica e expiracao protegendo equipamento em uso; exportacao de configuracoes.
- Teste com CQueryManager real do DataServer + ODBC x86 + SQL MuOnlineBase: gravacao, leitura de binario, item C/adicional Excellent S/Ancient A, repeticao idempotente, conflito sem sobrescrita e rollback. Zero registros artificiais restantes.
- SHA256 dos seis executaveis instalados conferido contra ReleaseCandidate; configuracoes conferidas pela API Win32.
- DataServer instalado iniciou e escutou a porta 45960 no smoke test, encerrado ao fim do teste.

## Teste no jogo a executar
1. Inicie o servidor e abra E:/Client5.2/Main.exe.
2. Obtenha um novo drop de equipamento de monstro; confira o tier na descricao. Para adicionais, use um novo Excellent/Ancient.
3. Compare valores da descricao com o efeito nos atributos; mova o item entre inventario, equipamento, bau, trade e loja pessoal.
4. Reconecte e confira se as mesmas letras e valores permaneceram.
5. Para desativar, altere os respectivos controles no MainInfo, gere/exporte os sidecars e reinicie os processos que os leem.

A validacao visual e de combate em uma sessao real ainda depende desse teste no jogo. Build e testes automatizados nao equivalem a essa verificacao.

## Correcao de tooltip — 18/10/2026
A primeira verificacao no jogo mostrou que as letras e valores nao apareciam no inventario. Causa encontrada: a consulta usava SendPacket com o buffer logico sem o XOR exigido pelo protocolo. O socket aplica outra protecao, mas nao substitui esse XOR; o servidor recebia um subcodigo incorreto.

IndependentTierRequestSender agora usa o StreamPacketEngine real do cliente. Teste de regressao capturou o pacote enviado, confirmou que a versao anterior seria rejeitada e refez a inversao XOR usada pelo servidor, validando os dez contextos de consulta. Build completo do Main passou. Main corrigido instalado e hash conferido. O processo do cliente que estava aberto foi preservado: fechar/reabrir E:/Client5.2/Main.exe para carregar a correcao. Pode usar o mesmo item ja criado; metadados foram confirmados no SQL.

## Atualizacao Excellent — 19/10/2026
Tabelas proprias por adicional, qualidade Comum/Raro/Lendario/Epico e cores por linha. SD maximo substitui mana maxima apenas nos adicionais com tier salvo. Recuperacao por kill usa fracoes exatas antes do arredondamento final. Consulte TABELAS_EXCELLENT.md. Reinicie GameServer, GameServerCS, SubSala (se utilizada) e Main para ativar o protocolo 6. As letras existentes permanecem. Validacao visual e de combate continua pendente do teste no jogo.
