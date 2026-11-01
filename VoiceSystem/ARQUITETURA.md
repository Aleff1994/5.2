# Sistema de voz do NeonMU

## Decisao

O `Main.exe` atual e Win32. O SDK C++ oficial do LiveKit para Windows e
distribuido para x64. Por isso o SDK nao deve ser carregado diretamente no
processo do jogo.

A integracao sera dividida assim:

1. `Main.exe` (Win32): interface, selecao Party/Guild e estado visual.
2. `VoiceClient.dll` (Win): API estavel consumida pelo Main e IPC local.
3. `VoiceBridge.exe` (x64): captura, WebRTC/Opus, AEC, NS, AGC e LiveKit.
4. `GameServer`: valida conta/personagem/Party/Guild e emite autorizacao curta.
5. LiveKit Server: transporta apenas a midia, sem passar audio pelo GameServer.

## Tecla de voz

`PushToTalkKey` aceita `F8`, `F9`, `F10`, `F11`, `F12`, `MOUSE4`, `MOUSE5`,
`RALT` e `RCTRL`. O padrao solicitado e `F11`.

Existe atualmente um atalho F11 no cliente para backup da camera e uma leitura
de F11 em `CameraMove.cpp`. A integracao de voz deve coexistir com esses atalhos;
nao se deve apagar ou trocar o comportamento existente.

## Salas e autorizacao

- Party: `party:<PartyNumber>`
- Guild: `guild:<GuildNumber>`
- A identidade publicada deve ser unica por sessao.
- O cliente nunca envia um numero de Party/Guild escolhido por ele.
- O GameServer deriva a sala do `PartyNumber`/`GuildNumber` do objeto conectado.
- O token deve expirar em 60 segundos e conter conta, personagem, sala e tipo.
- A chave secreta do LiveKit fica somente no GameServer/servico emissor.

## Ciclo de sessao

- Login/personagem pronto: solicitar token novo.
- Entrada/saida/troca de Party ou Guild: revogar sala anterior e emitir token novo.
- Logout, troca de personagem, OffHelper e OffAttack: fechar microfone, sair e
  desconectar.
- Reconnect: descartar a sessao anterior e somente voltar depois de receber novo
  token do GameServer.

## Pacotes de controle reservados

- `F3:E0` cliente solicita autorizacao para PARTY ou GUILD.
- `F3:E1` servidor entrega URL, sala, identidade, expiracao e token.
- `F3:E2` servidor manda sair/desconectar.
- `F3:E3` atualizacao de estado/membros para a interface.

Nenhum desses pacotes transporta audio.

## Etapas de implementacao

1. Contrato da DLL, carregamento opcional e configuracao.
2. Captura KeyDown/KeyUp e PTT real, sem trabalho de audio na render thread.
3. Pacotes e autorizacao do GameServer.
4. Bridge x64 com LiveKit/Opus.
5. Interface pequena de Party/Guild, volume, mute e indicador de fala.
6. Hooks de Party, Guild, logout, OffHelper, OffAttack e reconnect.
7. Limites, logs sem token, protecao contra replay e sessao duplicada.
8. Testes com dois clientes reais, perda de rede, troca de canal e reconexao.

## Dados ainda necessarios para ativacao real

- URL publica do LiveKit (normalmente `wss://...`).
- API key e secret, instalados somente no servidor.
- Certificado TLS e portas liberadas para WebRTC.
- Binarios x64 do SDK/bridge e redistribuiveis no ambiente de build.

Enquanto esses dados nao existirem, `127.0.0.1:7880` serve apenas para teste
local e o sistema nao deve ser anunciado como voz funcional em producao.
