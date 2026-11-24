# Manutenção e forks

O AtomTube é um executável C++ nativo em Win32. Ele usa auxiliares do pacote portátil para consultar o catálogo, selecionar faixas e reproduzir vídeos. O projeto mantém o alvo x86 e o subsistema Windows 6.01 para atender ao Windows 7 SP1.

## Onde alterar cada parte

| Área | Arquivos | Responsabilidade |
| --- | --- | --- |
| Janela e navegação | [app.cpp](../src/app.cpp), [theme.cpp](../src/theme.cpp), [ui_language.h](../src/ui_language.h) | Telas, comandos, cartões, preferências e textos traduzidos. |
| Pesquisa e miniaturas | [search.cpp](../src/search.cpp), [metadata.h](../src/metadata.h) | Consultas em segundo plano, campos devolvidos pelo extrator e imagem dos resultados. |
| Escolha de formatos | [youtube.cpp](../src/youtube.cpp), [options.h](../src/options.h) | Seleção de faixa H.264/AAC, limites de resolução, preferências de áudio e preparação no modo MP4 completo. |
| Reprodução | [player.cpp](../src/player.cpp), [stream.cpp](../src/stream.cpp) | Sessões de mídia, fluxo FFmpeg/libVLC e apresentação das imagens. |
| Conta, inscrições e playlist | [atomtube_home.py](../tools/yt-dlp-plugins/atomtube/yt_dlp_plugins/extractor/atomtube_home.py) | OAuth, seleção do conteúdo para a home, leitura das inscrições e paginação de **Ver no AtomTube**. |
| Diagnóstico | [diagnostics.cpp](../src/diagnostics.cpp) | Relatório de hardware, logs de sessão e exportação. |

## Como o vídeo chega à tela

1. A interface recebe o clique ou link e entrega o identificador ao resolvedor em `youtube.cpp`. O resolvedor chama o extrator portátil e seleciona as faixas de áudio e vídeo.
2. No streaming, o FFmpeg combina as faixas por cópia e encaminha os dados para o libVLC. Assim, o fluxo usa as faixas codificadas existentes. No modo **MP4 completo**, a preparação grava um arquivo em disco antes da reprodução.
3. No Cedar Trail, o caminho RGB32 usa um passo alinhado em memória e apresenta a imagem por Direct3D 9 ou GDI. Ao mexer nesse caminho, teste uma resolução com largura de 854 pixels e compare as cores antes e depois de redimensionar. Isso cobre a regressão de imagens repetidas observada na GMA 3600.
4. O alvo de FPS controla a apresentação de quadros. O áudio continua no tempo original e a decodificação ainda recebe os quadros da fonte. Os testes de desempenho devem distinguir esses contadores.

## Como a conta chega à home

`app.cpp` usa o mesmo mecanismo de trabalho em segundo plano da pesquisa para solicitar o conteúdo da home. A implementação em Python roda dentro do extrator portátil. O login abre o navegador do sistema, usa PKCE e recebe o código por uma porta temporária em `127.0.0.1`. A sessão usa apenas o escopo de leitura do YouTube. O token de renovação é protegido pela DPAPI do Windows no perfil local.

**Ver no AtomTube** procura uma playlist da conta conectada pelo nome. O plugin entrega os vídeos e os cursores da próxima página e da anterior para que a janela carregue só a página necessária. Ao alterar essa interface, mantenha consistentes o formato de retorno do plugin, a leitura em `search.cpp` e a navegação em `app.cpp`. Cada fork pode configurar seu próprio cliente OAuth seguindo o [guia de login](../README.md#conectar-sua-conta-do-youtube).

## Compilar e validar um fork

1. Faça um fork pelo GitHub, clone-o e crie uma branch para a mudança.
2. No **x86 Native Tools Command Prompt for VS 2019**, com MSVC v142 e Windows SDK, execute `build.cmd` e depois `tests\build.cmd`. Os binários aparecem em `build`.
3. Use [as instruções para reproduzir os testes](../README.md#reproduzir-os-testes) para validar reprodução e importações no Windows 7. Rode também `python -B tests/home_account.py` no Windows para testar a conta e a paginação com dados simulados.
4. Em mudanças no player, registre resolução, formato, cadência, modo CPU/DXVA2, sistema e versão do driver. Separe o tempo de consulta até a abertura do tempo de reprodução efetiva. Consulte os [ensaios de referência](benchmarks.md).
5. Para gerar um ZIP, instale PyInstaller no Python do PC de desenvolvimento e execute [preparar-kit.ps1](../tools/preparar-kit.ps1) após a compilação. O script verifica as dependências baixadas por SHA-256 e confere se os executáveis e módulos são x86. Mantenha junto do pacote os avisos de licença e a referência à origem das dependências.

No pull request, descreva de que forma o problema aparece e quais testes você executou. Para erros de reprodução, inclua o resultado em CPU e DXVA2 quando possível. Se anexar um relatório, revise o conteúdo para compartilhar somente os dados que ajudam a reproduzir o problema.
