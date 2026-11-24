# Ensaio de reprodução local

## Netbook de referência

O AtomTube 0.1 x86 completou quatro reproduções locais de cinco minutos no Atom N2600 a 1,60 GHz, com 2.028 MiB de RAM e GMA 3600 (driver informado pelo D3D9 1.8.0.15526). O usuário relatou reprodução sem travamentos ou erros perceptíveis nos quatro ensaios. O cabeçalho identifica Windows 7 SP1 build 7601 **x64**. A compatibilidade com uma instalação de Windows 7 x86 continua pendente.

| Amostra | Modo solicitado | FPS apresentados médios | CPU média do app | CPU média do sistema | Último contador de perdas no EVR |
|---|---|---:|---:|---:|---:|
| C · 720p24 | DXVA2 | 24,00 | 0,71% | 49,13% | 0 |
| C · 720p24 | CPU | 24,00 | 30,75% | 78,54% | 0 |
| D · 720p30 | DXVA2 | 30,00 | 0,86% | 67,43% | 0 |
| D · 720p30 | CPU | 30,01 | 40,08% | 94,56% | 0 |

Cada média usa as 56 leituras periódicas com FPS numérico de uma reprodução. A leitura final repetida e a linha inicial sem FPS foram excluídas. O contador do EVR permaneceu em zero em todas as leituras disponíveis de perdas. As últimas contagens de quadros exibidos foram 7.134, 7.112, 8.914 e 8.914, respectivamente, antes do evento de fim. A cauda entre a última leitura válida e o fim fica sem contagem. Os quatro arquivos do RAR correspondem a quatro sessões distintas. A taxa de origem registrada identifica a amostra D nos dois arquivos cujo nome menciona 30 fps.

A política DXVA preservou a cadência das duas amostras com menor uso de CPU. Por isso, **Preferir GPU (DXVA2)** é a escolha inicial para esse aparelho. O uso efetivo do bloco de decodificação ficou como N/D nos logs. O sincronismo audiovisual não foi medido nem avaliado separadamente no relato.

O usuário confirmou que esse netbook usa um driver interno de testes da Intel para Windows 7 de 64 bits. Os demais modelos de destino usam principalmente Windows de 32 bits. A distribuição segue x86, inclusive nos auxiliares de rede e mídia.

## Arquivos de referência

Valores lidos com ffprobe nos arquivos incluídos no pacote. Cada amostra dura 300 s e contém vídeo progressivo H.264 Main, nível 3.1, YUV 4:2:0 de 8 bits e áudio AAC-LC mono a 48 kHz.

| Amostra | Resolução | FPS de origem | Quadros no arquivo | Bitrate médio de vídeo |
|---|---:|---:|---:|---:|
| A | 640 × 360 | 24 | 7.200 | 299 kbit/s |
| B | 854 × 480 | 24 | 7.200 | 418 kbit/s |
| C | 1280 × 720 | 24 | 7.200 | 819 kbit/s |
| D | 1280 × 720 | 30 | 9.000 | 903 kbit/s |

A sequência de dez segundos se repete trinta vezes. O texto estático permite inspecionar a legibilidade e a área central exercita movimento. O quadrado amarelo aparece na mesma marca temporal do início do tom, uma vez por segundo. O gerador mantém perfil, nível, limite de bitrate e qualidade de codificação iguais. O bitrate efetivo varia com o conteúdo e a resolução. Uma aula com câmera em movimento, texto menor ou outro perfil de codificação exige uma medição própria.

## Comparação no Atom

1. Use o mesmo tamanho de janela e o mesmo plano de energia. Anote se o netbook está na tomada e se outros programas estavam abertos.
2. Faça C nos dois modos seguindo o [roteiro do kit](../LEIA-ME.txt). Depois execute A, B e D nas mesmas condições. A seleção do modo reinicia o vídeo. Exporte após cada execução, inclusive se falhar. A exportação inclui os ensaios anteriores da mesma sessão.
3. Para cada par, anote se a fala ou o bip acompanha a imagem, se o texto continua legível e o instante aproximado de cada engasgo. Compare os FPS apresentados depois da partida, os contadores cumulativos do EVR, o uso de CPU e a RAM. Uma média alta pode esconder pausas longas.
4. Se faltar um contador, mantenha N/D. Se houver falha antes da abertura da janela, registre a mensagem do Windows. Quando o EVR encerrar antes da última consulta, o painel conserva a última leitura válida e indica essa condição.
5. Compare o mesmo vídeo em ambos os modos antes de testar uma resolução menor. Um ganho no modo DXVA é uma observação de desempenho. Confirmar qual parte da decodificação ocorreu em hardware exige outra evidência.

## Verificações no computador de desenvolvimento

- Compilação MSVC v142 x86 em Windows 10 x64, com CRT estático. O cabeçalho PE registra máquina x86, PE32 e subsistema 6.01. As importações diretas vêm de DLLs do Windows.
- Oito execuções com trechos de sete segundos copiados dos arquivos A, B, C e D. Nos modos DXVA solicitado e CPU, o teste abriu o arquivo com áudio, entrou em reprodução, pausou, retomou, recebeu o evento de fim, reiniciou e exportou o TXT. Os oito processos retornaram zero.
- Em três verificações negativas, o log guardou a operação e o HRESULT. Arquivo ausente: 0x80070002. MP4 com conteúdo inválido: 0xC00D36C4. MP4 com vídeo MPEG-4 Part 2, fora do escopo do protótipo: 0xC00D36B4. Nenhum dos três logs incluiu o caminho usado para abrir o arquivo.
- O teste de integração usa uma janela oculta. A inspeção visual e a avaliação auditiva de sincronismo devem ocorrer no aparelho. O contador do EVR mede apresentação pelo renderizador, sujeito ao suporte oferecido pelo sistema.

### Ensaio contínuo de 720p24 no PC de desenvolvimento

A amostra C chegou ao evento de fim após cinco minutos contínuos em Windows 10 x64, com Xeon E5-2660 v3 e GeForce RTX 3060. O executável x86 solicitou DXVA e negociou o Microsoft H264 Video Decoder MFT com saída NV12. A média das 60 leituras de FPS foi 23,99. Na última consulta válida, o EVR registrava 7.196 quadros apresentados e zero perdas no renderizador. A sessão terminou cerca de 0,17 s depois dessa consulta, quando os contadores já estavam indisponíveis. A diferença até os 7.200 quadros da fonte fica sem medição. [Log integral do ensaio](ensaio-pc-720p24.txt)

### Ensaio online da versão 0.2 no PC de desenvolvimento

O teste passou um link público ao mesmo código de preparação usado pelo botão **Assistir**. Com limite de 720p, o trailer de *Sintel* produziu H.264 High em 1280 × 546 a 24 fps, com áudio. A preparação com os auxiliares x86 do kit levou 14,7 s nessa conexão. A sessão de reprodução recebeu o evento de fim após cerca de 53 s. O tamanho reflete a proporção do trailer. O tempo de preparação inclui a consulta, o download e a montagem do MP4. A integração online ainda precisa de ensaio no netbook. [Trailer do Blender](https://www.youtube.com/watch?v=HOfdboHvshg) · [Log do teste](ensaio-pc-youtube.txt)

Pela interface do executável extraído do ZIP, o ensaio iniciou uma preparação, cancelou e abriu o mesmo link novamente com limite de 360p. O novo vídeo chegou ao fim em 640 × 272 a 24 fps. Os testes de arquivos locais incluíram a amostra curta de 720p24 em CPU e a de 720p30 com DXVA solicitado.

### Falha de inicialização do extrator 0.2 no Atom e correção em 0.2.1

No Windows 7 SP1 do N2600, as três tentativas terminaram ao carregar o Python do extrator, com código 0xFFFFFFFF. A captura de tela identificou a função AddDllDirectory em KERNEL32.dll. O ensaio não chegou ao download nem à decodificação. A inspeção de python315.dll do kit 0.2 confirmou as importações obrigatórias de AddDllDirectory e RemoveDllDirectory.

A versão 0.2.1 usa o extrator x86 Vista+ de 3dyd com Python 3.12. O teste de importações voltou a detectar o problema no kit anterior e verificou 123 imagens no diretório youtube do kit novo, incluindo os módulos internos do extrator. Todas eram x86 e passaram na verificação das três funções de diretório de DLL.

No PC de desenvolvimento com Windows 10, o teste usou os auxiliares extraídos do ZIP 0.2.1 e o mesmo caminho de download e reprodução da interface. A preparação do trailer de Sintel levou 15,6 s e o vídeo H.264 de 1280 × 546 a 24 fps chegou ao fim com saída de áudio solicitada. A execução no netbook após a correção aguarda confirmação.

### Streaming em 0.3

No N2600, o usuário confirmou reprodução fluida com 0.2.1 e relatou quase três minutos de espera inicial. As medições abaixo usaram o trailer de Sintel, o kit 0.3 extraído no PC de desenvolvimento e a saída de áudio padrão. O teste observou o contador de quadros apresentados, pausou, retomou e verificou o fim contra a duração informada pelo YouTube.

| Seleção | Imagem recebida | Consulta | Clique até primeiro quadro | Transferência no primeiro quadro |
|---|---|---:|---:|---|
| Até 720p, preferência DXVA2 | 1280 × 546 | 3,4 s | 4,44 s | Em andamento, 40.420 bytes lidos pelo player |
| Até 360p, CPU | 640 × 272 | 4,0 s | 5,19 s | Em andamento, 36.848 bytes lidos pelo player |

O teste de 1280 × 546 terminou com 1.242 quadros exibidos, quatro perdas informadas pelo renderizador e 2.250 buffers de áudio reproduzidos. O modo MP4 completo preparou o arquivo em 14,2 s e o reproduziu até o fim. Esses tempos dependem do aparelho, da rede e dos formatos disponíveis para cada vídeo. A fluidez do novo caminho libVLC e o tempo de consulta no Atom precisam de medição no netbook.

### Imagem triplicada na GMA 3600 e teste da versão 0.3.1

No Windows 7 SP1 x64 do N2600, a versão 0.3 recebeu H.264 em 1280 × 720. Com preferência por DXVA2, a consulta levou 25,5 s e o primeiro quadro apareceu aos 33,77 s, com transferência em andamento. O log também registra a troca para CPU. A captura feita com o vídeo em pausa nesse modo mostra três cópias estreitas do quadro e perda de cor. Os dois modos usavam a saída Direct3D 9.

Na 0.3.1, o aplicativo seleciona a saída RGB do GDI nos adaptadores Intel Cedar Trail. O teste no PC de desenvolvimento usou os módulos extraídos do ZIP e um padrão H.264 de 1280 × 720 a 24 fps transmitido por HTTP em loopback. A leitura dos pixels da janela confirmou as três faixas de referência (vermelha, verde e azul), nas larguras e posições esperadas. O teste passou em 640 × 360 e após redimensionar para 480 × 270, com CPU e com DXVA2 solicitado. A confirmação da imagem e da fluidez com o driver Intel 1.8.0.15526 depende do novo ensaio no Atom.

O ensaio real da 0.3.1 confirmou que o aplicativo detectou o dispositivo PCI 8086:0BE1 e solicitou GDI RGB. Mesmo assim, a captura de 854 × 480 apresentou quatro cópias estreitas do quadro. Isso mostrou que o defeito estava no passo de memória entregue pelo caminho de saída do libVLC nesse driver. A captura foi feita depois da troca entre GPU e CPU, e os dois modos mantiveram o defeito.

Na 0.3.2, o libVLC decodifica para um buffer RV32 reservado pelo AtomTube. A largura, a altura e o passo são definidos antes da decodificação. Para 854 × 480, o passo de 3.416 bytes é alinhado para 3.424 bytes. O AtomTube desenha somente os 854 pixels visíveis de cada linha. O teste atualizado usa exatamente 854 × 480 e passou em CPU e com DXVA2 solicitado, antes e depois de redimensionar a janela. A confirmação final continua sendo o ensaio no driver Intel do Atom.

### Qualidade, limite de apresentação e tela cheia em 0.4

O usuário confirmou que o buffer RGB32 da 0.3.2 corrigiu a imagem sem cor e repetida no Atom. Essa detecção da GMA 3600 e o passo alinhado permanecem ativos.

A busca rápida selecionava 640 × 360 para `51tJc5PQwVI` e 854 × 480 para `9H3uQO2h3uo`. A consulta completa encontrou H.264 em 1280 × 720 nos dois vídeos. A 0.4 repete automaticamente a consulta quando o resultado rápido fica abaixo do limite escolhido. O teste do resolvedor confirmou 1280 × 720 a 25 fps no primeiro exemplo e 1280 × 720 a 30 fps no segundo.

Para fontes acima de 24 fps, a saída RGB32 da GMA 3600 limita as atualizações visuais a 24 fps com relógio de alta resolução. O áudio e a duração permanecem inalterados. O padrão de 854 × 480 apresentou 73 quadros em uma janela de medição de três segundos, tanto em CPU quanto com DXVA2 solicitado. As cores e o redimensionamento continuaram corretos.

O teste da janela confirmou que o comando de tela cheia remove a moldura, ocupa o monitor com a superfície de vídeo e restaura a janela com Escape. F11, o botão e clique duplo acionam a mesma operação.

### Consulta paralela e cache do player em 0.4.1

O log real do N2600 mostrou 94,5 s entre o clique e o fim da consulta de um vídeo que exigia formatos adicionais. A busca rápida consumiu aproximadamente 15,4 s e a consulta completa seguinte, com o desafio JavaScript, 76,4 s. O primeiro quadro apareceu 2,62 s depois da consulta. Portanto, o gargalo era a resolução dos formatos, não o início do FFmpeg ou do libVLC.

A 0.4.1 inicia as duas consultas em paralelo para os limites de 480p e 720p. A completa usa prioridade reduzida enquanto a rápida pode encerrar primeiro. O plugin local habilita o cache do player pré-processado que o yt-dlp 2026.10.26 mantém desativado por padrão por não possuir rotação. O AtomTube acrescenta essa rotação: no máximo dois arquivos e 12 MiB em `%TEMP%\AtomTube-video\extrator\challenge-solver`.

No PC de desenvolvimento, uma consulta completa fria de `51tJc5PQwVI`, executada pelo novo resolvedor paralelo, levou 13,49 s. A repetição com o mesmo player do YouTube levou 4,94 s e retornou H.264 1280 × 720 a 25 fps. Uma consulta somente rápida até 360p levou 3,91 s. São medições locais do PC moderno, não previsões para o Atom. O ganho real e o custo das duas consultas concorrentes ainda precisam ser medidos no N2600.

### Engasgos codificados no vídeo `26P1KyvrmjI`

O formato escolhido pelo AtomTube é H.264 Main 1280 × 720 a 24000/1001 fps, com AAC-LC estéreo a 44,1 kHz. O vídeo dura 1.733,148 s e o áudio 1.733,207 s. A faixa de áudio tem timestamps contínuos; o maior intervalo medido entre pacotes foi 0,02322 s.

A decodificação dos primeiros 180 s e a comparação MD5 de cada quadro encontrou repetições exatas já presentes na faixa publicada: seis quadros iguais a partir de 45,67 s; várias sequências de dois ou três quadros entre 63,48 e 64,19 s; e nove quadros iguais a partir de 75,87 s, equivalentes a cerca de 0,38 s de imagem congelada. Há repetições menores em 84,25–84,42 s e 93,26 s. Isso explica o engasgo nos mesmos instantes no Atom e no PC potente. O AtomTube não remove esses quadros: corrigi-los exigiria alterar a cadência ou recodificar o conteúdo original, contrariando o caminho de cópia leve usado pelo projeto.

### Suavização e tela 1024 × 600 em 0.4.2

As capturas reais mostraram serrilhado nas diagonais somente no streaming da GMA 3600. O buffer alinhado estava correto; a causa era o `StretchDIBits` com `COLORONCOLOR`, que reduz a imagem sem gerar pixels intermediários. Num padrão diagonal sintético, esse modo produziu zero pixels intermediários, enquanto o HALFTONE produziu 1.773. Porém, no PC de desenvolvimento, o custo medido por redução de 1280 × 720 para 1024 × 576 foi 0,579 ms com `COLORONCOLOR` e 16,375 ms com HALFTONE. Por isso HALFTONE ficou apenas como fallback.

A saída principal da GMA continua recebendo RV32 no buffer de passo alinhado, preservando a correção da imagem triplicada, mas agora copia esse quadro para uma superfície Direct3D 9 e usa `StretchRect` linear na GPU. A opção **Suavização de bordas**, ligada por padrão, pode ser desligada em tempo de execução; nesse estado o apresentador volta ao caminho GDI `COLORONCOLOR` rápido usado anteriormente. Nos testes CPU e DXVA2 solicitado, as faixas RGB permaneceram nas posições corretas antes e depois de redimensionar a janela. Foram apresentados 72 e 73 quadros, respectivamente, em três segundos de uma fonte de 30 fps limitada a 24 fps.

A opção **Tela 1024 × 600** usa 1024 × 576 para conteúdo 16:9 e mantém a preferência por H.264 1280 × 720. Os vídeos `51tJc5PQwVI` e `9H3uQO2h3uo` resolveram para 1280 × 720 nessa opção. A redução ocorre somente na apresentação; 720p continua disponível separadamente e nenhuma das opções distorce a proporção.
## Interface 0.5.0

No PC de desenvolvimento com Windows 10 x64, o executável x86 apresentou 12 resultados na pesquisa por “matemática ensino médio”, com miniaturas verificadas na captura da janela de 1024 × 560. A seleção abriu um streaming. Passaram os testes de pausa, retomada, volume, salto de teclado, barra de posição, tela cheia e retorno à janela. O MP4 local também passou pelos testes de transporte. A regressão RGB apresentou 73 quadros em três segundos tanto em CPU quanto no modo que solicita DXVA, com cores corretas.

Os resultados usam pesquisa plana em processo auxiliar com prioridade reduzida. As miniaturas são redimensionadas para 112 × 63, em memória, com até 12 imagens. A animação ocorre durante consultas e abertura de mídia. A barra de posição é atualizada a intervalos de 300 ms durante a reprodução quando está visível. O streaming busca posição reabrindo as faixas por cópia, a partir de um ponto de acesso próximo, e o MP4 usa o relógio e a posição do Media Foundation.

O vídeo `51tJc5PQwVI` selecionou uma trilha 720p na consulta ampliada, mas a transferência encerrou com código 1, tanto no ZIP 0.4.2 quanto no 0.5.0. O ensaio de consumo e compatibilidade da interface no Windows 7 e no Atom N2600 segue pendente.

## Reprodução, busca e tela cheia na 0.5.1

No mesmo PC, o FFmpeg recebeu HTTP 403 ao combinar as faixas escolhidas para `YSw0hFnxYMY` e `51tJc5PQwVI`. Com a verificação de disponibilidade das faixas pelo extrator, o ZIP 0.5.1 reproduziu ambos em 1280 × 720. O primeiro quadro foi observado após 11,53 s e 11,70 s, respectivamente, durante a transferência.

A pesquisa por “matemática ensino médio” retornou 25 itens. A página seguinte aumentou a lista para 46 vídeos distintos. O teste abriu um resultado, revelou os controles de tela cheia pelo mouse e confirmou que o botão sob o ponteiro recebia cliques. A mesma verificação passou com os dois links e o MP4 local. Estes tempos foram medidos no Windows 10 x64 do PC de desenvolvimento.

## Interface e catálogo 0.7.0

No Windows 10 x64, o executável extraído do ZIP x86 passou por três ciclos de redimensionamento entre 760 × 440, janela maximizada e 1024 × 560. A pesquisa por “matemática ensino médio” retornou 15 vídeos, depois 30 com **Carregar mais**. O teste abriu uma playlist de matemática, listou seus primeiros 15 vídeos e voltou à seleção anterior. No canal Manual do Mundo, abriu as abas de vídeos, transmissões e playlists, pesquisou “submarino” dentro do canal e acessou o canal durante a reprodução.

Um resultado da busca, o link de `YSw0hFnxYMY` e a amostra local 720p24 passaram pelos testes de clique para pausar e continuar em janela e em tela cheia, volume, avanço pelo teclado e reposicionamento pela barra. Ambos os vídeos online apresentaram sugestões na lateral. Na regressão do buffer RGB32, as cores permaneceram nas posições esperadas após redimensionar. O teste contou 72 quadros em três segundos em CPU e 73 solicitando DXVA2. O ensaio físico desta interface no N2600 com Windows 7 continua pendente.

## Configurações extras 0.8.0

No PC de desenvolvimento, uma fonte H.264 de 60 fps com barras de cores produziu, em janelas de aproximadamente três segundos, 52, 72, 91, 105, 119, 149 e 184 quadros apresentados nos alvos de 17 a 60 fps. Em Original foram 184 quadros. O teste conferiu também o padrão de cores após redimensionar.

O resolvedor selecionou 1280 × 720 para o vídeo multilíngue 26P1KyvrmjI nos testes em português brasileiro, inglês e espanhol. As faixas retornadas foram pt-BR, en-US e es. Com um catálogo sintético contendo três bitrates na mesma resolução e uma faixa adicional de 360p, o extrator do pacote escolheu 2300, 610 e 240 kbit/s nos perfis Alta, Baixa e Batata, todos em 720p. Esses bitrates pertencem à amostra de teste.

A interface do ZIP foi aberta em 1024 × 560 e 760 × 440. Foram verificadas a configuração inicial, a troca de idioma e a recuperação das três preferências depois de reabrir. Os testes de MP4 720p30 com alvo de 17 fps incluíram clique na imagem, pausa, volume, salto, barra de posição e tela cheia. O desempenho desta versão no N2600 físico ainda precisa ser medido.