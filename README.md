# AtomTube

O AtomTube nasceu para trazer os netbooks de volta ao YouTube. Ele usa uma interface nativa e um reprodutor leve para pesquisar e assistir vídeos em máquinas com pouca memória, como os netbooks da geração Intel Atom. O ZIP é portátil e foi compilado para Windows 7 SP1 de 32 bits.

**Testado em um Intel Atom N2600 com 2 GB de RAM e Intel GMA 3600.** Em ensaios locais de cinco minutos, o aparelho reproduziu amostras 720p à velocidade normal, mantendo a cadência de origem de 24 ou 30 fps. O ensaio usou Windows 7 SP1 de 64 bits com um driver de testes da Intel. O desempenho com vídeos online varia com os formatos disponíveis, a rede e o driver. [Resultados e condições dos testes](docs/benchmarks.md)

[**Baixar AtomTube 0.9.2 para Windows de 32 bits**](https://github.com/PantuPipa/AtomTube/releases/tag/v0.9.2)

## Usar

Em **Releases**, baixe **AtomTube-0.9.2-x86.zip**, extraia o conteúdo em uma pasta e execute **AtomTube.exe**. Na tela inicial, digite um assunto ou cole um link e pressione Enter. Também é possível usar **Abrir MP4** para reproduzir um arquivo local. A pesquisa e a reprodução de vídeos públicos funcionam sem conta.

A aba **Para você** carrega até 15 vídeos. A seleção parte das buscas recentes e dos canais assistidos neste PC. No primeiro uso, sugere ciência e astronomia. Com uma conta conectada, também inclui publicações de canais inscritos.

## Conectar sua conta do YouTube

A conexão é opcional e libera **Inscrições** e **Ver no AtomTube**. O ZIP requer a configuração de um cliente OAuth do Google antes do primeiro login.

1. No Google Cloud, crie um projeto e ative a **YouTube Data API v3**. [Biblioteca de APIs](https://console.cloud.google.com/apis/library/youtube.googleapis.com)
2. Em **Google Auth Platform**, configure o consentimento e selecione o público **Externo** para uma conta pessoal. Enquanto o projeto estiver em teste, adicione o seu Gmail em **Público-alvo → Usuários de teste**. Em **Acesso a dados**, configure o escopo de leitura `https://www.googleapis.com/auth/youtube.readonly`. [Configuração de consentimento](https://developers.google.com/workspace/guides/configure-oauth-consent)
3. Em **Clientes**, crie um cliente OAuth do tipo **Aplicativo para computador** e baixe o JSON. Abra o AtomTube, clique em **Conectar conta** e selecione esse arquivo quando solicitado. [Guia do Google para aplicativos de computador](https://developers.google.com/identity/protocols/oauth2/native-app)
4. No navegador que abrir no próprio netbook, entre na conta do YouTube e autorize o acesso de leitura. De volta ao AtomTube, aguarde o botão mudar para **Desconectar**.

Se o Google mostrar **403: access_denied** e disser que o aplicativo está em teste, confirme se o e-mail usado no login aparece como usuário de teste no mesmo projeto que gerou o JSON.

## Salve no celular e assista no netbook

1. No YouTube do celular ou do computador, toque em **Salvar** e crie uma playlist chamada **Ver no AtomTube**. A playlist pode ser privada.
2. No netbook, conecte a mesma conta e abra a aba **Ver no AtomTube**.
3. Clique em um cartão para assistir. Use as setas para percorrer a playlist e **Atualizar** para buscar os vídeos que acabou de salvar.

A lista segue a ordem definida no YouTube e carrega até 15 itens por consulta. Ela aceita vídeos públicos e não listados.

**Inscrições** mostra os vídeos públicos recentes de até 12 canais, ordenados por atividade. O aplicativo guarda o token protegido pelo Windows e conserva a lista por uma hora para reduzir consultas. **Desconectar** remove do PC o token e a lista de inscrições. Para revogar também o acesso no Google, use [Conexões da conta](https://myaccount.google.com/connections).

## Busca e controles

Na busca, alterne entre **Vídeos**, **Canais** e **Playlists**. Clique em um título para abri-lo ou use as setas e Enter. A foto e o nome do autor abrem o canal. Use **Carregar mais** para acrescentar até 15 resultados. No canal, as abas **Vídeos**, **Ao vivo** e **Playlists** organizam o catálogo. O campo **Pesquisar neste canal** limita a busca ao canal aberto. As transmissões encerradas podem ser reproduzidas quando o YouTube disponibiliza a gravação.

Durante a reprodução, clique na imagem para pausar ou continuar. O nome do canal abre o catálogo do autor, e **Voltar ao vídeo** retorna ao player. A lista **Relacionados** usa o título atual para buscar outros vídeos públicos. Quando a busca devolve apenas o vídeo atual, **Mais deste canal** mostra outras publicações do autor. Na preparação do vídeo, o Atomzinho indica a etapa enquanto a miniatura ganha cor. Clique na marca **AtomTube** para voltar ao início.

O AtomTube guarda a qualidade, a decodificação, a suavização, o diagnóstico e o volume ao fechar. Clique na busca para rever as últimas 15 pesquisas. Use as setas e Enter para repetir uma delas. 

Abra o menu de três linhas no canto superior esquerdo da tela inicial. As escolhas ficam salvas. O idioma da interface muda imediatamente. O áudio, o perfil de imagem e o alvo de quadros se aplicam ao próximo vídeo aberto.

Imagem: Original usa a seleção padrão da fonte. Alta prioriza maior bitrate e aplica nitidez leve. Média mantém a seleção anterior. Baixa prefere até 650 kbit/s entre faixas equivalentes e reduz a filtragem no decodificador por software. Batata prefere o menor bitrate e ativa a decodificação rápida. A resolução continua no seletor da tela inicial. Quando o YouTube oferece uma única codificação compatível na resolução escolhida, os perfis podem usar a mesma faixa.

FPS: Original da fonte, 60, 50, 40, 35, 30, 24 ou 17. O alvo limita a apresentação dos quadros sem alterar a duração ou o áudio. Uma fonte de 24 fps continua a 24 mesmo com alvo 60. A configuração inicial é Média, 24 fps e Português (Brasil). English e Español também alteram a preferência de áudio do YouTube. Quando o idioma escolhido está indisponível, o player usa uma faixa compatível disponível.

O streaming começa com os primeiros dados de imagem e som. O FFmpeg lê as faixas HTTPS e as combina por cópia em um canal de memória. Em adaptadores Intel Cedar Trail, incluindo a GMA 3600, o AtomTube recebe quadros RGB32 em um buffer com passo alinhado e os apresenta por Direct3D 9 com filtragem linear. **Suavização de bordas** permite desligar essa filtragem durante a reprodução. Nos demais adaptadores, o streaming usa a saída nativa do libVLC quando a fonte já está dentro do alvo de FPS. O seletor CPU/GPU controla a preferência de decodificação nas duas saídas. **MP4 completo** usa preparação em disco até 720p. Para arquivos locais e preparados, o player usa o libVLC quando precisa aplicar um perfil de decodificação, nitidez ou limite de quadros. Nos demais casos usa Media Foundation. [Callbacks de vídeo do libVLC](https://videolan.videolan.me/vlc-3.0/group__libvlc__media__player.html)

Para limites de 480p e 720p, o aplicativo executa em paralelo a busca rápida, sem JavaScript, e a consulta completa com QuickJS em prioridade reduzida. A busca rápida continua vencendo quando já encontra a qualidade pedida. Quando ela encontra uma resolução menor, a consulta completa já avançou. Um plugin local do AtomTube habilita o cache do player JavaScript pré-processado; o aplicativo preserva no máximo dois players e 12 MiB. Assim, consultas seguintes que usam o mesmo player evitam seu processamento mais caro. O Python do extrator já vem descompactado. Os MP4 sintéticos continuam no kit anterior, `AtomTube-teste-x86.zip`. [Roteiro para o netbook](LEIA-ME.txt)

Quando a busca rápida encontra uma faixa abaixo do limite escolhido, o AtomTube aguarda a consulta completa que já está em andamento para procurar a resolução solicitada. Vídeos de 25 ou 30 fps mantêm a duração e o áudio originais, enquanto o buffer RGB32 limita a apresentação ao alvo escolhido. A decodificação continua recebendo os quadros da fonte.

A opção **Tela 1024 × 600** preserva a proporção do vídeo, ocupando normalmente 1024 × 576 pixels em tela cheia. Ela prefere uma fonte 720p e só usa uma resolução inferior quando 720p não está disponível; não solicita ao YouTube uma falsa resolução intermediária.

**Abrir MP4** ou arrastar um arquivo para a janela inicia a reprodução local. Clique ou arraste a barra para escolher a posição. No streaming, o salto reabre as faixas e pode começar em um quadro-chave próximo. O botão de volume alterna o mudo. **Tela cheia**, F11 ou um clique duplo na imagem ocupam o monitor. Passe o mouse perto da borda inferior para revelar os controles e pressione Esc para voltar. Com foco no player, Espaço alterna pausa, ← e → movem 10 segundos, e ↑ e ↓ ajustam o volume. Ctrl+L seleciona o campo de busca.

Em **Decodificação**, a opção inicial é **Preferir GPU (DXVA2)**. **Usar CPU** força a decodificação por software. Selecionar outro modo reabre o vídeo desde o início. O painel **Diagnóstico** fica recolhido por padrão. A coleta continua a cada cinco segundos durante a reprodução.

## Escolhas de implementação

Para preparar um fork ou localizar o código de uma funcionalidade, comece pelo [guia de manutenção](docs/manutencao.md). O código próprio do AtomTube usa a [licença MIT](LICENSE). Os componentes de terceiros seguem suas respectivas licenças.

- A interface usa C++ e controles Win32. No caminho Media Foundation para arquivos locais e MP4 completo, o leitor de MP4, os decodificadores, a sincronização de áudio e o EVR pertencem ao Media Foundation. Isso permite testar o contêiner MP4 com H.264 e AAC sem instalar um splitter DirectShow no netbook. [Fonte MPEG-4 da Microsoft](https://learn.microsoft.com/en-us/windows/win32/medfound/mpeg-4-file-source)
- **Preferir GPU (DXVA2)** configura a topologia com `MFTOPOLOGY_DXVA_FULL`. **Usar CPU** usa `MFTOPOLOGY_DXVA_NONE` para desativar aceleração no processamento do vídeo. O EVR continua responsável pela apresentação nos dois casos. O relatório identifica as transformações de vídeo negociadas e o formato de saída quando disponíveis. A ativação efetiva do decodificador de hardware exige evidência adicional do driver. [Modos DXVA para Windows 7](https://learn.microsoft.com/en-us/windows/win32/api/mfidl/ne-mfidl-mftopology_dxva_mode)
- A inspiração do Tilefinch é entregar vídeo compatível ao decodificador do aparelho e manter a interface leve. O PSP e o N2600 têm blocos de vídeo e drivers diferentes. A reprodução publicada para o PSP orienta o desenho, e os resultados do Atom orientam a seleção de formatos. [Tilefinch](https://github.com/stjanovitz/tilefinch) · [KZE PSP Tube](https://kze.info/apps/psp-tube)
- A pesquisa carrega 15 títulos por página em segundo plano com o extrator do kit. O Windows baixa e decodifica miniaturas sob demanda da pesquisa, com cache em memória na lista atual. A escolha do resultado usa o mesmo seletor de formatos dos links individuais. Ele exige H.264 em MP4, áudio AAC, largura até 1280, altura dentro da opção escolhida e FPS de origem até 30 para os alvos de até 30 fps. A resolução recebida aparece no rodapé.

## Auxiliares x86 e empacotamento

O empacotamento usa Python 3.9+ com PyInstaller (variável opcional ATOMTUBE_PYTHON). No PC de desenvolvimento, execute `build.cmd` e depois `powershell -ExecutionPolicy Bypass -File tools\preparar-kit.ps1`. O script consulta os pacotes dos mantenedores, verifica os SHA-256 fixados e confere a arquitetura de cada EXE, DLL e PYD antes de montar o ZIP. As versões e licenças acompanham os seguintes componentes.

- yt-dlp Vista+ x86 2026.10.26, com Python 3.12 e EJS já extraídos no kit. A variante substitui o Python 3.15 do kit anterior, que exigia AddDllDirectory ao iniciar. [Distribuição de 3dyd](https://github.com/3dyd/yt-dlp-builds/releases/tag/2026.10.26)
- QuickJS de 2026-06-04, compilação Windows i686 do autor. [Binários QuickJS](https://bellard.org/quickjs/binary_releases/)
- FFmpeg 6.0, revisão 549430e14d, variante Win32 LGPL compartilhada, usada para combinar faixas. A compilação 7.1.5 de novembro de 2026 avaliada no desenvolvimento importava funções posteriores ao Windows 7. [Distribuição Win32](https://github.com/sudo-nautilus/FFmpeg-Builds-Win32) · [Fonte do FFmpeg](https://github.com/FFmpeg/FFmpeg/tree/549430e14d)

O streaming usa módulos Win32 do libVLC 3.0.23. [Distribuição VideoLAN](https://download.videolan.org/pub/videolan/vlc/3.0.23/win32/)

## Diagnóstico

O TXT contém horário com milissegundos, sistema operacional, CPU, RAM, adaptador D3D9 padrão, versão do driver, modo solicitado, características da mídia, eventos e erros com HRESULT. A CPU do processo é expressa como percentual da capacidade conjunta dos processadores lógicos. A RAM do processo é o conjunto de trabalho. No streaming o log registra consulta, primeiro quadro observado, contadores de vídeo e áudio do libVLC. No modo MP4 os FPS apresentados são a diferença entre contadores do EVR dividida pelo tempo de relógio entre amostras. As perdas informadas são as do renderizador. Uma pausa ou repetição reinicia o intervalo de medição. Não há leitura direta de quadros perdidos antes do EVR nem comparação automática entre a imagem e o áudio.

A exportação usa UTF-16 LE com BOM e quebras de linha do Windows. O histórico em memória guarda até 2.048 entradas. Em `%LOCALAPPDATA%\AtomTube`, `ultima.txt` e `anterior.txt` guardam registros limitados a aproximadamente 1 MiB por arquivo. A aplicação omite o caminho e o nome do vídeo no diagnóstico.

## Compilar

No **x86 Native Tools Command Prompt for VS 2019**, com C++ v142 e o Windows SDK instalados, execute `build.cmd` na raiz. O resultado fica em `build\AtomTube.exe`. A compilação usa CRT estático, APIs de destino Windows 7 e subsistema 6.01. O kit foi compilado com MSVC 19.29. O PC de desenvolvimento pode ser x64.

## Reproduzir os testes

1. Para recriar os vídeos, execute `powershell -ExecutionPolicy Bypass -File tools\gerar-amostras.ps1` no PC de desenvolvimento com FFmpeg, libx264, filtros lavfi/drawtext e a fonte Arial do Windows. Os arquivos são gravados em `amostras`.
2. No mesmo prompt x86 de compilação, execute `tests\build.cmd`. O programa de integração utiliza o mesmo código de reprodução da interface, uma janela de teste oculta e a saída de áudio padrão.
3. Execute os comandos abaixo para um ensaio curto. O retorno zero indica que abriu, iniciou, pausou, retomou, terminou, reiniciou e gravou o log.
4. Para o ensaio contínuo, passe um arquivo de cinco minutos e o argumento final `completo`. O teste espera o fim e aceita até 330 segundos. Use `cpu` ou `dxva` como segundo argumento.

```bat
ffmpeg -i amostras\C_1280x720_24fps.mp4 -t 7 -c copy build\curto.mp4
build\playback.exe "%CD%\build\curto.mp4" cpu build\curto.txt
build\playback.exe "%CD%\amostras\C_1280x720_24fps.mp4" dxva build\continuo.txt completo
```

Os critérios e o registro de validação ficam em [Benchmarks](docs/benchmarks.md).

Para testar o caminho online, extraia o kit no diretório `build`, ao lado de `playback.exe`, e execute o ensaio.

```bat
build\playback.exe "https://youtu.be/HOfdboHvshg" dxva build\online.txt youtube
```

A verificação de regressão usa `python tests/importacoes_win7.py caminho/para/youtube` no PC de desenvolvimento com pefile e PyInstaller. Ela inspeciona as importações obrigatórias do diretório e dos módulos embutidos no extrator para identificar a dependência que falhou no Atom. A validação final ocorre no netbook.

O teste de cores usa um MP4 H.264/AAC de oito segundos, 854 × 480 a 24 fps, com faixas verticais vermelha, verde e azul de larguras 214, 426 e 214 pixels. Sirva o MP4 em HTTP no loopback e rode `build\cores.exe http://127.0.0.1:18762/rgb.mp4 cpu` e depois `dxva`. O teste lê a cor em oito posições do buffer desenhado pelo AtomTube.
