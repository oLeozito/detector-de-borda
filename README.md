# Filtro Detector de Borda com Aceleração em Hardware (DE1-SoC)

![Status](https://img.shields.io/badge/status-conclu%C3%ADdo-brightgreen)
![Linguagem](https://img.shields.io/badge/linguagem-C%20%7C%20Assembly%20ARM-blue)
![Plataforma](https://img.shields.io/badge/plataforma-DE1--SoC-red)

Relatório técnico do Problema #3 da disciplina de MI - Sistemas Digitais (2025.1), do curso de Engenharia de Computação da Universidade Estadual de Feira de Santana (UEFS).

### Autores
* [Nome Completo do Integrante 1]
* [Nome Completo do Integrante 2]
* [Nome Completo do Integrante 3]

---

### **Resumo**

*Este projeto detalha o desenvolvimento de um sistema de detecção de bordas em imagens digitais, implementado na plataforma DE1-SoC. A solução emprega uma abordagem de co-design hardware-software, onde uma aplicação de alto nível em linguagem C, executada no HPS (ARM Cortex-A9), gerencia o fluxo de dados e a interface com o usuário. As operações de convolução 2D, computacionalmente intensivas, são delegadas a um coprocessador especializado, desenvolvido em Verilog e sintetizado na FPGA. A comunicação entre o software e o hardware é realizada através da ponte Lightweight HPS-to-FPGA, sendo orquestrada por uma biblioteca de baixo nível em Assembly ARM, que implementa um protocolo de handshake para garantir a sincronia e integridade dos dados. Foram implementados e validados cinco diferentes filtros de borda: Sobel (3x3 e 5x5), Prewitt (3x3), Roberts (2x2) e Laplaciano (5x5). O sistema processa uma imagem de entrada de 320x240 pixels e gera como saída uma nova imagem com as bordas detectadas, demonstrando a viabilidade e os benefícios de performance da aceleração de hardware em tarefas de processamento de imagem.*

**Palavras-chave:** Processamento de Imagens, Detecção de Borda, Filtro Sobel, FPGA, DE1-SoC, Co-design Hardware-Software, Assembly ARM.

---

### **1. Introdução**

[cite_start]A detecção de bordas é uma técnica fundamental no campo de processamento de imagens e visão computacional. [cite_start]Seu objetivo é identificar pontos em uma imagem onde a intensidade da luminosidade muda bruscamente, o que geralmente corresponde aos contornos de objetos. [cite_start]Para essa finalidade, diversos algoritmos baseados em operadores de gradiente, como Sobel, Prewitt e Roberts, e operadores de segunda derivada, como o Laplaciano, são amplamente utilizados.

[cite_start]Este projeto teve como objetivo principal o desenvolvimento de um programa em linguagem C para aplicar diferentes filtros de detecção de borda em uma imagem de 320x240 pixels[cite: 21, 25]. [cite_start]Uma restrição fundamental era a utilização de um coprocessador aritmético, previamente desenvolvido para aceleração de multiplicação matricial [cite: 19][cite_start], e de uma biblioteca em Assembly para mediar a comunicação com o hardware. A plataforma de hardware utilizada foi o kit de desenvolvimento DE1-SoC, que integra um processador ARM (HPS) e uma FPGA.

### **2. Arquitetura da Solução**

A solução foi concebida sob o paradigma de **co-design Hardware-Software**, dividindo as responsabilidades entre o processador (HPS) e a lógica programável (FPGA) para otimizar o desempenho.

* **HPS (Hard Processor System - ARM Cortex-A9):**
    * [cite_start]**Aplicação Principal (`main.c`):** Escrita em C, é responsável por toda a lógica de alto nível:
        1.  Carregar a imagem de entrada no formato BMP.
        2.  [cite_start]Realizar o pré-processamento, convertendo-a para escala de cinza com 8 bits por pixel.
        3.  Apresentar um menu interativo ao usuário para a seleção do filtro desejado.
        4.  Percorrer a imagem com uma janela deslizante e enviar as submatrizes para a FPGA.
        5.  Receber os pixels processados e, quando aplicável (filtros Gx/Gy), calcular a magnitude do gradiente final.
        6.  Salvar a imagem resultante em um novo arquivo BMP.

* **FPGA (Field-Programmable Gate Array):**
    * **Coprocessador de Convolução (Verilog):** Contém um módulo de hardware dedicado para realizar a operação de convolução 2D. Ele recebe uma janela de pixels (até 5x5) e um kernel de filtro, realizando a multiplicação e soma de forma massivamente paralela e retornando o valor do pixel central processado.

* **Comunicação HPS-FPGA:**
    * [cite_start]**Lightweight HPS-to-FPGA Bridge:** A comunicação é feita através desta ponte de barramento, que permite ao processador ARM acessar os registradores do coprocessador na FPGA como se fossem posições de memória.
    * [cite_start]**Mapeamento de Memória (`mmap`):** A aplicação em C, através de uma função em Assembly, utiliza a chamada de sistema `mmap` para mapear o espaço de endereços físicos da ponte para o espaço de endereços virtuais do processo, obtendo um ponteiro para acesso direto ao hardware.

* **Biblioteca de Interface (`package.s`):**
    * [cite_start]Escrita em Assembly ARM, esta biblioteca implementa as funções de baixo nível `enviar_dados_para_fpga` e `receber_dados_da_fpga`. Ela é a responsável por implementar o protocolo de comunicação (handshake) com o coprocessador, escrevendo e lendo diretamente nos registradores mapeados.

### **3. Detalhes da Implementação**

A implementação foi dividida em três frentes principais: o software de controle, a interface de comunicação e a lógica de hardware. Este documento foca nos dois primeiros.

#### **3.1. Software de Controle (`main.c`)**

O arquivo `main.c` contém a lógica principal. Ele inicializa definindo os kernels dos filtros (Sobel, Prewitt, Roberts, Laplaciano) como matrizes 5x5 de inteiros de 8 bits para manter a compatibilidade com a interface do coprocessador.

A função `apply_filter` orquestra a aplicação de cada filtro. Para operadores como Sobel, Prewitt e Roberts, ela chama a rotina de processamento em hardware duas vezes: uma para o gradiente horizontal (Gx) e outra para o vertical (Gy). Em seguida, a função `combinar_gradientes_e_limiar` calcula a magnitude final do gradiente para cada pixel pela fórmula $G = \sqrt{G_x^2 + G_y^2}$. Para o filtro Laplaciano, que é um operador de segunda derivada, o processamento é feito em uma única passada.

#### **3.2. Interface de Comunicação (`package.s`)**

[cite_start]A biblioteca `package.s` é o núcleo da interação hardware-software. Ela implementa um protocolo de handshake para garantir uma comunicação síncrona e livre de erros.

* **`enviar_dados_para_fpga`:**
    1.  O software (ARM) verifica um bit de status no registrador de retorno da FPGA para saber se ela está pronta para receber um novo dado.
    2.  Quando a FPGA está pronta, o ARM monta uma palavra de 32 bits contendo o pixel da imagem, o valor correspondente do kernel do filtro e dados de controle (tamanho da matriz e opcode).
    3.  O ARM escreve essa palavra em um registrador de dados na FPGA.
    4.  O ARM define um bit de "dado válido" para sinalizar à FPGA que ela pode iniciar o processamento.
    5.  O ARM aguarda a FPGA sinalizar de volta que o dado foi lido, completando o handshake para aquele valor.
    6.  O processo se repete para todos os 25 valores da janela 5x5.

* **`receber_dados_da_fpga`:**
    1.  O ARM aguarda a FPGA definir um bit de "resultado pronto" no registrador de status.
    2.  Ao receber o sinal, o ARM lê o registrador de dados, que contém os pixels processados (a FPGA empacota até 3 resultados de 8 bits em uma única palavra de 32 bits para otimizar o barramento).
    3.  O ARM armazena os resultados na matriz de destino.
    4.  O ARM define um bit de "dado lido" para que a FPGA saiba que pode enviar o próximo resultado.

### **4. Pré-requisitos**

[cite_start]Esta seção detalha o software e hardware necessários para compilar e executar o projeto.

* **Hardware:**
    * Kit de Desenvolvimento Terasic DE1-SoC.
* **Software:**
    * Distribuição Linux para o HPS da DE1-SoC (ex: Ubuntu, Debian).
    * Toolchain de compilação cruzada GCC para ARM (ex: `arm-linux-gnueabihf-gcc`).
    * Utilitário `make`.

### **5. Compilação e Execução**

[cite_start]Siga os passos abaixo para compilar e rodar o projeto.

1.  **Clone o Repositório:**
    ```bash
    git clone [URL_DO_SEU_REPOSITORIO]
    cd [NOME_DO_DIRETORIO]
    ```

2.  **Prepare a Imagem de Entrada:**
    * Coloque um arquivo de imagem no formato BMP, com resolução de **320x240 pixels**, na raiz do diretório do projeto. O arquivo deve ser nomeado como `imagem1.bmp`.

3.  **Compile o Projeto:**
    * Um `Makefile` está incluído para automatizar o processo de compilação do código C e da biblioteca Assembly. Execute o seguinte comando:
    ```bash
    make
    ```
    * Isso irá gerar um arquivo executável chamado `main`.

4.  **Execute na DE1-SoC:**
    * Transfira o executável `main` e a imagem `imagem1.bmp` para a placa DE1-SoC.
    * Execute o programa a partir do terminal Linux na placa:
    ```bash
    ./main
    ```
    * Siga as instruções no menu interativo para escolher o filtro que deseja aplicar. As imagens de saída (intermediárias e finais) serão salvas nas pastas `Intermediarias/` e `Processadas/`.

### **6. Resultados e Análise**

[cite_start]O sistema foi testado com sucesso, gerando imagens de saída para cada um dos cinco filtros implementados. A análise visual dos resultados corrobora a teoria do processamento de imagens:

| Filtro | Análise dos Resultados |
| :--- | :--- |
| **Sobel (3x3 e 5x5)** | Ambos detectaram bem as bordas. [cite_start]A versão 5x5 produziu bordas mais espessas e com melhor supressão de ruído, conforme esperado de um kernel maior. |
| **Prewitt (3x3)** | [cite_start]O resultado foi muito similar ao Sobel 3x3, mas com um pouco mais de sensibilidade a ruído devido aos seus pesos uniformes. |
| **Roberts (2x2)** | [cite_start]Realçou melhor as bordas finas e diagonais, mas as linhas de contorno ficaram mais "quebradas" em comparação com o Sobel, demonstrando sua maior suscetibilidade a distorções. |
| **Laplaciano (5x5)** | [cite_start]Como um operador de segunda derivada, ele identificou áreas de rápida mudança de intensidade, resultando em bordas mais finas (às vezes duplas) e com um realce significativo do ruído presente na imagem. |

### **7. Conclusão**

Este projeto alcançou com sucesso seus objetivos, implementando um sistema funcional de detecção de bordas que explora o co-design hardware-software na plataforma DE1-SoC. [cite_start]Os objetivos de aprendizagem foram atingidos através da aplicação prática de conceitos como mapeamento de memória, programação em Assembly ARM e integração entre HPS e FPGA.

O trabalho demonstrou de forma eficaz como tarefas computacionalmente intensivas, como a convolução de imagens, podem ser delegadas a um hardware customizado para obter uma aceleração significativa, liberando o processador principal para tarefas de controle e gerenciamento.

### **8. Referências**

[1] MATURANA, Patrícia Salles. [cite_start]**Algoritmos de detecção de bordas implementados em FPGA.** Ilha Solteira: Universidade Estadual Paulista “Júlio de Mesquita Filho”, 2010. Disponível em: <https://www.feis.unesp.br/Home/departamentos/engenhariaeletrica/pos-graduacao/273-dissertacao_patricia.pdf>.
