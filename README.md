# NEONMU — PC 5.2

Source do cliente PC e dos serviços do servidor MU. Estado atualizado em 01/11/2026.

## Módulos

- `Main5.2`: cliente Windows.
- `GameServer`: GameServer e GameServerCS.
- `DataServer`, `JoinServer`, `ConnectServer`: serviços do servidor.
- `Encoder`, `GameShop`, `Utilities` e bibliotecas auxiliares.
- `TierSystem`: schemas, testes e suporte do sistema de tiers.

## Ambiente

Visual Studio 2022, desenvolvimento para desktop com C++, plataforma Win32. Cliente: configuração Release. GameServer: Release_EX603. Castle Siege: Release_EX603CS. Algumas bibliotecas auxiliares utilizam Lua 5.2; o cliente também contém dependências próprias em Main5.2/dependencies.

## Excellent por tier

Cada opção Excellent pode ser Rare, Legendary ou Epic. O tier substitui o efeito padrão da mesma opção, sem acumular os dois. Skill, Luck, adicional de Life e bônus exclusivo permanecem separados.

Os comandos administrativos `/tierex` suportam tiers uniformes e mistos, inclusive anéis e pendants. Os metadados são salvos por serial no banco; itens sem metadados continuam usando o padrão do jogo.

## Conteúdo e implantação

Este repositório contém a source. Banco de dados de jogadores, backups SQL, logs, dumps, credenciais e executáveis implantados não fazem parte do envio. As pastas de execução do cliente e MuServer continuam separadas.

Antes de usar outra máquina, configurar caminhos das bibliotecas, serviços SQL e arquivos de execução locais. As alterações de 01/11 foram compiladas no ambiente atual; não foi feita uma compilação independente desta cópia do repositório.