# VitaTPBPlayer

![Status](https://img.shields.io/badge/status-em%20desenvolvimento-yellow)
![Platform](https://img.shields.io/badge/platform-PS%20Vita-blue)
![Language](https://img.shields.io/badge/language-C-orange)

Um cliente de torrent funcional para o PlayStation Vita, que utiliza as APIs do The Pirate Bay e do Real-Debrid para buscar e preparar arquivos para download.

**Atenção:** Este projeto está em uma fase de transição. A interface gráfica já foi modernizada, mas a implementação do download em segundo plano ainda está em andamento.

### Visão Geral

O VitaTPBPlayer permite que você busque torrents diretamente no seu PS Vita, utilize o poder do Real-Debrid para obter um link de download direto e, em seguida, baixe o arquivo para o seu console.

O projeto foi recentemente refatorado para usar a biblioteca gráfica `libvita2d`, resultando em uma interface muito mais estável e visualmente limpa em comparação com as versões iniciais.

## O que ele faz?

A ideia do VitaTPBPlayer é criar um fluxo simples para assistir a conteúdo no Vita:

1.  **Busca**: O usuário digita uma busca usando um teclado virtual na tela.
2.  **Lista de Resultados**: O aplicativo busca torrents no The Pirate Bay e exibe uma lista de resultados com nome, seeders e leechers.
3.  **Seleção**: O usuário seleciona um item da lista.
4.  **"Debriding"**: O link magnético do torrent é enviado para a API do Real-Debrid.
5.  **Download**: O aplicativo aguarda o Real-Debrid preparar o arquivo e, em seguida, baixa o link direto para o cartão de memória (`ux0:data/movie.mp4`).
6.  **Reprodução**: Ao final do download, ele tenta executar um player de vídeo externo para reproduzir o arquivo baixado.

## Funcionalidades Atuais (e Planejadas)

-   [x] Busca na API do The Pirate Bay.
-   [x] Integração com a API do Real-Debrid (adicionar magnet, verificar status, obter link).
-   [x] Download de arquivos via `libcurl`.
-   [x] Interface de texto simples usando uma biblioteca de `debugScreen` customizada.
-   [x] Teclado virtual na tela para inserção de texto.
-   [x] Paginação nos resultados da busca.
-   [ ] Melhorar a interface do usuário (UI).
-   [ ] Salvar configurações (como o token do Real-Debrid) em um arquivo.
-   [ ] Gerenciamento de múltiplos downloads/arquivos.
-   [ ] Player de vídeo integrado (um objetivo de longo prazo).

## Como Funciona

O aplicativo é escrito em C e utiliza o **VitaSDK** para compilação. A lógica de rede é totalmente baseada na `libcurl` para fazer requisições HTTP para:
* **`apibay.org`**: Para buscar torrents de forma anônima.
* **`api.real-debrid.com`**: Para todas as operações relacionadas à conversão de torrents em links diretos.

A interface é renderizada diretamente no framebuffer do PS Vita, proporcionando uma experiência leve e sem dependências de bibliotecas de UI complexas.

## Requisitos para Compilação

* Um PlayStation Vita com [HENkaku](https://henkaku.xyz/) ou h-encore.
* [VitaSDK](https://vitasdk.org/) devidamente instalado e configurado no seu ambiente de desenvolvimento.
* Uma conta no [Real-Debrid](https://real-debrid.com) com uma assinatura ativa.

## Como Compilar

1.  Clone este repositório:
    ```bash
    git clone https://github.com/mzzvxm/VitaTPBPlayer.git
    cd VitaTPBPlayer
    ```

2.  **Adicione seu Token do Real-Debrid**: Este é o passo mais importante. Abra o arquivo `src/realdebrid.c` e insira seu token pessoal na linha indicada.
    ```c
    // Encontre esta linha:
    #define RD_TOKEN "COLOQUE_SEU_TOKEN_AQUI"
    
    // E substitua pelo seu token:
    #define RD_TOKEN "SEUTOKENPESSOALDOREALDEBRID"
    ```

3.  Compile o projeto:
    ```bash
    make
    ```

4.  Isso irá gerar um arquivo `VitaTPBPlayer.vpk` na raiz do projeto. Transfira este arquivo para o seu PS Vita e instale-o usando o VitaShell.

## Aviso Legal

* Este projeto é fornecido apenas para fins educacionais.
* O usuário é totalmente responsável pelo conteúdo que acessa através deste aplicativo.
* Este projeto não é afiliado ao The Pirate Bay, Real-Debrid ou qualquer outra entidade mencionada.
* Sempre respeite as leis de direitos autorais do seu país.
