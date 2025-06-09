#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h> // Para malloc, free, exit
#include <string.h> // Para memset
#include <math.h>   // Para sqrt

#include "package.h" // Certifique-se de que este cabeçalho contém as funções de comunicação com a FPGA

#define LW_BRIDGE_BASE   0xFF200000
#define LW_BRIDGE_SPAN   0x00005000
#define LEDR_BASE        0x00000000
#define RETURN_BASE      0x00000010

#define CABECALHO_BMP_SIZE 54 // Tamanho fixo do cabeçalho BMP
#define LARGURA  320
#define ALTURA   240

// --- Estruturas para o Cabeçalho BMP ---
// Assegura o alinhamento correto dos bytes, importante para estruturas de arquivo
#pragma pack(push, 1)

// Bitmap file header
typedef struct {
    uint16_t bfType;      // Specifies the file type (0x4D42 for "BM")
    uint32_t bfSize;      // Specifies the size of the file in bytes
    uint16_t bfReserved1; // Reserved; must be 0
    uint16_t bfReserved2; // Reserved; must be 0
    uint32_t bfOffBits;   // Specifies the offset from the beginning of the file to the bitmap data
} BITMAPFILEHEADER;

// Bitmap info header
typedef struct {
    uint32_t biSize;          // Specifies the size of the BITMAPINFOHEADER structure, in bytes
    int32_t  biWidth;         // Specifies the width of the bitmap, in pixels
    int32_t  biHeight;        // Specifies the height of the bitmap, in pixels
    uint16_t biPlanes;        // Specifies the number of planes for the target device (must be 1)
    uint16_t biBitCount;      // Specifies the number of bits per pixel
    uint32_t biCompression;   // Specifies the type of compression for a compressed bitmap
    uint32_t biSizeImage;     // Specifies the size, in bytes, of the image
    int32_t  biXPelsPerMeter; // Specifies the horizontal resolution, in pixels per meter
    uint32_t biYPelsPerMeter; // Specifies the vertical resolution, in pixels per meter
    uint32_t biClrUsed;       // Specifies the number of color indexes in the color table used by the bitmap
    uint32_t biClrImportant;  // Specifies the number of color indexes that are required for displaying the bitmap
} BITMAPINFOHEADER;

#pragma pack(pop)

uint8_t imagem_gray[ALTURA][LARGURA];         // Imagem original em escala de cinza
uint8_t imagem_gx_raw[ALTURA][LARGURA];       // Valores Gx brutos (0-255, após saturação da FPGA)
uint8_t imagem_gy_raw[ALTURA][LARGURA];       // Valores Gy brutos (0-255, após saturação da FPGA)
uint8_t imagem_final_borda[ALTURA][LARGURA];  // Imagem de borda final (G = sqrt(Gx^2 + Gy^2))

// --- Funções Auxiliares ---

// Função para imprimir matrizes resultantes (útil para depuração)
void imprimir_matriz_resultado(uint8_t vetorC[25], uint8_t matrix_size_bits) {
    printf("Matriz Resultante (FPGA):\n");

    int tamanho = matrix_size_bits + 2; // 00->2x2, 01->3x3, 10->4x4, 11->5x5
    for (int linha = 0; linha < tamanho; linha++) {
        for (int coluna = 0; coluna < tamanho; coluna++) {
            printf("%4u ", (uint8_t)vetorC[linha * 5 + coluna]);
        }
        printf("\n");
    }
}

// Função para obter valor do pixel com tratamento de borda
uint8_t get_valor(uint8_t matriz[ALTURA][LARGURA], int i, int j) {
    if (i >= 0 && i < ALTURA && j >= 0 && j < LARGURA) {
        return matriz[i][j];
    } else {
        return 0; // Retorna 0 (preto) para pixels fora da imagem
    }
}

// Função para salvar imagem em escala de cinza no formato BMP
int salvar_imagem_bmp(const char* nome_arquivo, uint8_t imagem[ALTURA][LARGURA]) {
    FILE* fp = fopen(nome_arquivo, "wb");
    if (!fp) {
        perror("Erro ao abrir arquivo para escrita BMP");
        return -1;
    }

    // Calcular padding para linhas (largura deve ser múltiplo de 4 bytes)
    int padding = (4 - (LARGURA * 3) % 4) % 4; // 3 bytes per pixel (RGB)
    int row_size = LARGURA * 3 + padding;
    uint32_t image_size_bytes = row_size * ALTURA;
    uint32_t file_size_bytes = CABECALHO_BMP_SIZE + image_size_bytes;

    // Preencher o BITMAPFILEHEADER
    BITMAPFILEHEADER bfh;
    bfh.bfType = 0x4D42; // "BM"
    bfh.bfSize = file_size_bytes;
    bfh.bfReserved1 = 0;
    bfh.bfReserved2 = 0;
    bfh.bfOffBits = CABECALHO_BMP_SIZE;

    // Preencher o BITMAPINFOHEADER
    BITMAPINFOHEADER bih;
    bih.biSize = sizeof(BITMAPINFOHEADER);
    bih.biWidth = LARGURA;
    bih.biHeight = ALTURA; // BMPs são armazenados de baixo para cima
    bih.biPlanes = 1;
    bih.biBitCount = 24; // 24 bits por pixel (8 bits R, 8 bits G, 8 bits B)
    bih.biCompression = 0; // Sem compressão (BI_RGB)
    bih.biSizeImage = image_size_bytes;
    bih.biXPelsPerMeter = 0;
    bih.biYPelsPerMeter = 0;
    bih.biClrUsed = 0;
    bih.biClrImportant = 0;

    // Escrever cabeçalhos
    fwrite(&bfh, 1, sizeof(BITMAPFILEHEADER), fp);
    fwrite(&bih, 1, sizeof(BITMAPINFOHEADER), fp);

    // Escrever os pixels
    // BMP armazena as linhas de baixo para cima
    // Cada pixel em escala de cinza (0-255) vira um pixel RGB (Gray, Gray, Gray)
    for (int i = ALTURA - 1; i >= 0; i--) { // Começa da última linha para a primeira
        for (int j = 0; j < LARGURA; j++) {
            uint8_t gray_value = imagem[i][j];
            fputc(gray_value, fp); // Blue
            fputc(gray_value, fp); // Green
            fputc(gray_value, fp); // Red
        }
        // Adicionar padding ao final de cada linha, se necessário
        for (int p = 0; p < padding; p++) {
            fputc(0, fp);
        }
    }

    fclose(fp);
    return 0;
}

// Função para salvar imagem em escala de cinza no formato de texto
int salvar_imagem_txt(const char* nome_arquivo, uint8_t imagem[ALTURA][LARGURA]) {
    FILE* fp = fopen(nome_arquivo, "w");
    if (!fp) {
        perror("Erro ao abrir arquivo para escrita TXT");
        return -1;
    }

    for (int i = 0; i < ALTURA; i++) {
        for (int j = 0; j < LARGURA; j++) {
            fprintf(fp, "%u ", imagem[i][j]);
        }
        fprintf(fp, "\n");
    }

    fclose(fp);
    return 0;
}


// Função para exibir barra de progresso
void print_progress_bar(int current, int total) {
    int width = 25;
    int filled;
    int i;

    if (total == 0) total = 1; // Evita divisão por zero
    filled = (current * width) / total;

     printf("\r[");
    for (i = 0; i < width; i++) {
        if (i < filled)
            printf("#");
        else
            printf(" ");
    }
    printf("] %d%%", (current * 100) / total);
    fflush(stdout); // Garante que a barra de progresso seja atualizada na tela
}

// --- Função Principal de Processamento da Imagem ---
// Agora recebe um ponteiro para a matriz de saída para Gx ou Gy
void processar_imagem_e_salvar_gradiente(volatile uint32_t *LEDR_ptr, uint8_t matrix_size_bits, int8_t matriz_filtro[5][5], uint8_t opcode, uint8_t saida_gradiente[ALTURA][LARGURA]) {
    
    printf("Processando pixels e salvando gradiente...\n");

    for (int i = 0; i < ALTURA; i++) {
        for (int j = 0; j < LARGURA; j++) {
            uint8_t submatriz_entrada[5][5]; // Submatriz de entrada para a FPGA
            uint8_t vetorC_resultado[5][5];  // Matriz resultante da FPGA (espera-se 5x5)

            // Preenche a submatriz 5x5 centrada em (i, j)
            for (int dx = -2; dx <= 2; dx++) {
                for (int dy = -2; dy <= 2; dy++) {
                    // REMOVIDA A DIVISÃO POR 32 AQUI
                    submatriz_entrada[dx + 2][dy + 2] = (get_valor(imagem_gray, i + dx, j + dy)/2);
                }
            }
            
            // --- COMUNICAÇÃO COM A FPGA ---
            uint8_t data_fpga = (matrix_size_bits << 3) | (opcode & 0b111);
            
            enviar_dados_para_fpga(LEDR_ptr, submatriz_entrada, (uint8_t (*)[5])matriz_filtro, data_fpga);
            
            receber_dados_da_fpga(LEDR_ptr, vetorC_resultado);
            // --- FIM DA COMUNICAÇÃO COM A FPGA ---

            // Armazena o elemento central da matriz resultante na matriz de gradiente específica
            // REMOVIDA A MULTIPLICAÇÃO POR 32 AQUI
            
            saida_gradiente[i][j] = (vetorC_resultado[2][2]*2);
            
            // Lógica dos LEDs (opcional, pode ser removida para evitar overhead na barra de progresso)
            *LEDR_ptr |= (1 << 29);
            print_progress_bar(i * LARGURA + j, ALTURA * LARGURA);
            *LEDR_ptr &= ~(1 << 31);
            
        }
    }
    printf("\nProcessamento do gradiente concluído.\n");
}

// --- Nova Função para Combinar Gradientes e Gerar Imagem Final de Bordas ---
void combinar_gradientes_e_limiar(const char* nome_arquivo_saida, uint8_t limiar) {
    printf("\nCombinando gradientes Gx e Gy e aplicando limiar...\n");

    for (int i = 0; i < ALTURA; i++) {
        for (int j = 0; j < LARGURA; j++) {
            // Os valores de imagem_gx_raw e imagem_gy_raw já estão saturados entre 0 e 255
            // pelo módulo da FPGA (convolucao.v).
            // A magnitude do gradiente será calculada usando esses valores.
            double gx_val = (double)imagem_gx_raw[i][j];
            double gy_val = (double)imagem_gy_raw[i][j];

            double magnitude = sqrt(gx_val * gx_val + gy_val * gy_val);

            // Aplicar limiar para binarizar a imagem final de bordas
            if (magnitude > limiar) {
                imagem_final_borda[i][j] = 255; // Borda (branco)
            } else {
                imagem_final_borda[i][j] = magnitude;   // Não-borda (preto)
            }
            print_progress_bar(i * LARGURA + j, ALTURA * LARGURA);
        }
    }
    printf("\nCombinação e binarização concluídas.\n");

    if (salvar_imagem_bmp(nome_arquivo_saida, imagem_final_borda) == 0) {
        printf("Imagem final de bordas salva como '%s'\n", nome_arquivo_saida);
    } else {
        fprintf(stderr, "Falha ao salvar a imagem final de bordas como BMP.\n");
    }
}


// --- Função Principal (main) ---
int main(void) {
    int fd = -1;
    void *LW_virtual = configurar_mapeamento(&fd);
    if (LW_virtual == NULL) {
        fprintf(stderr, "Erro ao configurar mapeamento de memória para FPGA.\n");
        return -1;
    }

    volatile uint32_t *LEDR_ptr = (volatile uint32_t *)(LW_virtual + LEDR_BASE);

    // Configurações iniciais dos LEDs (do seu código original)
    *LEDR_ptr |= (1 << 29);
    *LEDR_ptr &= ~(1 << 31);

    // --- Seleção da Operação ---
    // A operação 'Multiplicação (Elemento a Elemento)' (opcode 3) é a mais relevante para convolução.
    // É importante que a FPGA esteja configurada para realizar essa operação na convolucao.
    printf("Selecionando operação 'Multiplicação (Elemento a Elemento)' para a FPGA.\n");
    uint8_t opcode = 3; // Operação de Multiplicação (Elemento a Elemento)

    uint8_t matrix_size_bits = 0b11; // 11 para 5x5

    // --- Carregamento da Imagem BMP ---
    FILE *arquivo_bmp = fopen("classictest.bmp", "rb");
    if (!arquivo_bmp) {
        perror("Erro ao abrir o arquivo BMP 'Fototestez.bmp'");
        munmap(LW_virtual, LW_BRIDGE_SPAN);
        close(fd);
        return 1;
    }

    fseek(arquivo_bmp, CABECALHO_BMP_SIZE, SEEK_SET); // Usar CABECALHO_BMP_SIZE
    int padding_orig = (4 - (LARGURA * 3) % 4) % 4; // Padding da imagem original BMP

    for (int i = ALTURA - 1; i >= 0; i--) { // Imagens BMP são armazenadas de baixo para cima
        for (int j = 0; j < LARGURA; j++) {
            unsigned char b = fgetc(arquivo_bmp);
            unsigned char g = fgetc(arquivo_bmp);
            unsigned char r = fgetc(arquivo_bmp);
            // Conversão para escala de cinza (luminância padrão)
            uint8_t gray = (uint8_t)(0.299 * r + 0.587 * g + 0.114 * b);
            imagem_gray[i][j] = gray;
        }
        fseek(arquivo_bmp, padding_orig, SEEK_CUR);
    }
    fclose(arquivo_bmp);
    printf("Imagem BMP carregada e convertida para escala de cinza.\n");

    // --- Salvar imagem em escala de cinza para verificação ---
    FILE *saida_txt = fopen("imagem_gray.txt", "w");
    if (!saida_txt) {
        perror("Erro ao criar arquivo de saída");
        return 1;
    }

    for (int i = 0; i < ALTURA; i++) {
        for (int j = 0; j < LARGURA; j++) {
            fprintf(saida_txt, "%3d ", imagem_gray[i][j]);
        }
        fprintf(saida_txt, "\n");
    }

    fclose(saida_txt);
    printf("Conversao para escala de cinza concluida.\n");

    
    /*
    // Definição dos kernels Sobel 3x3
    int8_t matriz_sobel3x3_gx[5][5] = {
        {0,0,0,0,0},
        {0,1,2,1,0},
        {0,0,0,0,0},
        {0,-1,-2,-1,},
        {0,0,0,0,0}
        };
        
    //REVISAR
    int8_t matriz_sobel3x3_gy[5][5] = {
        {0,0,0,0,0},
        {0,-1,0,1,0},
        {0,-2,0,2,0},
        {0,-1,0,-1,},
        {0,0,0,0,0}
        };
            
    //kernels roberts
    int8_t matriz_roberts_gx[5][5] = {
        {0,0,0,0,0},
        {0,0,0,0,0 },
        {0,0,1,0,0},
        {0,0,0,-1,0},
        {0,0,0,0,0}
    };

    int8_t matriz_roberts_gy[5][5] = {
        {0,0,0,0,0},
        {0,0,0,0,0 },
        {0,0,0,1,0},
        {0,0,-1,0,0},
        {0,0,0,0,0}
    };


    //kernels prewitss
    int8_t matriz_prewitt_gx[5][5] = {
        {0,0,0,0,0},
        {0,-1,0,1,0 },
        {0, -1, 0, 1, 0},
        {0,-1,0,1,0},
        {0,0,0,0,0}
    };

    int8_t matriz_prewitt_gy[5][5] = {
        {0,0,0,0,0},
        {0,-1,-1,-1,0 },
        {0, 0, 0, 0, 0},
        {0,1,1,1,0},
        {0,0,0,0,0}
    };


    //Kernel laplace 5x5
    int8_t matriz_laplace[5][5] = {
        {0,0,1,0,0},
        {0,1,2,1,0 },
        {1, 2, -16, 2, 1},
        {0,1,2,1,0},
        {0,0,1,0,0}
    };
    */
    
    
    // Definição dos kernels Sobel 5x5
    int8_t matriz_sobel_gx[5][5] = {
        {-2,-2,-4,-2,-2},
        {-1,-1,-1,-1,-1 },
        {0, 0, 0, 0, 0},
        {1,1,1,1,1 },
        {2,2,4,2,2}
    };
    
    //REVISAR
    int8_t matriz_sobel_gy[5][5] = {
        { -2, -1, 0, 1, 2 },
        { -2, -1, 0, 1, 2 },
        {  -4,  -1,  0,  1,  4 },
        {  -2,  -1,  0,  1,  2 },
        {  -2,  -1,  0,  1,  2 }
    };
    
    /*
    //--- Processamento e Salvar Imagem GX sobel3x3 (raw) ---
    printf("\n--- Processando Imagem com filtro GX ---\n");
    // Passamos a matriz imagem_gx_raw para que os resultados sejam armazenados nela
    processar_imagem_e_salvar_gradiente(LEDR_ptr, matrix_size_bits, matriz_sobel3x3_gx, opcode, imagem_gx_raw);
    if (salvar_imagem_bmp("imagem_gx_sobel3x3.bmp", imagem_gx_raw) == 0) {
        printf("Imagem GX (raw) salva como 'imagem_gx_sobel.bmp'\n");
        } else {
            fprintf(stderr, "Falha ao salvar a imagem GX (sobel3x3) como BMP.\n");
            }
            
            // --- Processamento e Salvar Imagem GY sobel3x3 (raw) ---
            printf("\n--- Processando Imagem com filtro GY ---\n");
            // Passamos a matriz imagem_gy_raw para que os resultados sejam armazenados nela
            processar_imagem_e_salvar_gradiente(LEDR_ptr, matrix_size_bits, matriz_sobel3x3_gy, opcode, imagem_gy_raw);
            if (salvar_imagem_bmp("imagem_gy_sobel3x3.bmp", imagem_gy_raw) == 0) {
                printf("Imagem GY (raw) salva como 'imagem_gy_sobel3x3.bmp'\n");
                } else {
                    fprintf(stderr, "Falha ao salvar a imagem GY (sobel3x3) como BMP.\n");
                    }
                    
                    // --- Combinar Gx e Gy para Imagem Final de Bordas ---
                    // Escolha um limiar adequado. Valores comuns estão entre 50 e 150 para imagens 0-255.
                    // Você pode precisar ajustar isso para sua imagem específica.
                    uint8_t threshold = 255; // Limiar para binarização da magnitude do gradiente
                    combinar_gradientes_e_limiar("imagem_sobel3x3_final.bmp", threshold);
                    
    //--- Processamento e Salvar Imagem GX (raw) ---
    printf("\n--- Processando Imagem com filtro roberts GX ---\n");
    // Passamos a matriz imagem_gx_raw para que os resultados sejam armazenados nela
    processar_imagem_e_salvar_gradiente(LEDR_ptr, matrix_size_bits, matriz_roberts_gx, opcode, imagem_gx_raw);
    if (salvar_imagem_bmp("imagem_gx_roberts.bmp", imagem_gx_raw) == 0) {
        printf("Imagem GX (raw) salva como 'imagem_gx_roberts.bmp'\n");
    } else {
        fprintf(stderr, "Falha ao salvar a imagem GX (raw) como BMP.\n");
    }

    // --- Processamento e Salvar Imagem GY (raw) ---
    printf("\n--- Processando Imagem com filtro GY ---\n");
    // Passamos a matriz imagem_gy_raw para que os resultados sejam armazenados nela
    processar_imagem_e_salvar_gradiente(LEDR_ptr, matrix_size_bits, matriz_roberts_gy, opcode, imagem_gy_raw);
    if (salvar_imagem_bmp("imagem_gy_roberts.bmp", imagem_gy_raw) == 0) {
        printf("Imagem GY (raw) salva como 'imagem_gy_roberts.bmp'\n");
    } else {
        fprintf(stderr, "Falha ao salvar a imagem GY (raw) como BMP.\n");
    }

    // --- Combinar Gx e Gy para Imagem Final de Bordas ---
    // Escolha um limiar adequado. Valores comuns estão entre 50 e 150 para imagens 0-255.
    // Você pode precisar ajustar isso para sua imagem específica.
    uint8_t threshold = 255; // Limiar para binarização da magnitude do gradiente
    combinar_gradientes_e_limiar("imagem_roberts_final.bmp", threshold);

    //--- Processamento e Salvar Imagem GX (raw) ---
    printf("\n--- Processando Imagem com filtro prewitt GX ---\n");
    // Passamos a matriz imagem_gx_raw para que os resultados sejam armazenados nela
    processar_imagem_e_salvar_gradiente(LEDR_ptr, matrix_size_bits, matriz_prewitt_gx, opcode, imagem_gx_raw);
    if (salvar_imagem_bmp("imagem_gx_prewitt.bmp", imagem_gx_raw) == 0) {
        printf("Imagem GX (raw) salva como 'imagem_gx_prewitt.bmp'\n");
    } else {
        fprintf(stderr, "Falha ao salvar a imagem GX (raw) como BMP.\n");
    }

    // --- Processamento e Salvar Imagem GY (raw) ---
    printf("\n--- Processando Imagem com filtro GY ---\n");
    // Passamos a matriz imagem_gy_raw para que os resultados sejam armazenados nela
    processar_imagem_e_salvar_gradiente(LEDR_ptr, matrix_size_bits, matriz_prewitt_gy, opcode, imagem_gy_raw);
    if (salvar_imagem_bmp("imagem_gy_prewitt.bmp", imagem_gy_raw) == 0) {
        printf("Imagem GY (raw) salva como 'imagem_gy_prewitt.bmp'\n");
    } else {
        fprintf(stderr, "Falha ao salvar a imagem GY (raw) como BMP.\n");
    }

    // --- Combinar Gx e Gy para Imagem Final de Bordas ---
    // Escolha um limiar adequado. Valores comuns estão entre 50 e 150 para imagens 0-255.
    // Você pode precisar ajustar isso para sua imagem específica.
    uint8_t threshold = 255; // Limiar para binarização da magnitude do gradiente
    combinar_gradientes_e_limiar("imagem_prewitt_final.bmp", threshold);

    printf("\n--- Processando Imagem com laplace ---\n");
    // Passamos a matriz imagem_gx_raw para que os resultados sejam armazenados nela
    processar_imagem_e_salvar_gradiente(LEDR_ptr, matrix_size_bits, matriz_laplace, opcode, imagem_gx_raw);
    if (salvar_imagem_bmp("laplace.bmp", laplace) == 0) {
        printf("Imagem GX (raw) salva como 'laplace.bmp'\n");
    } else {
        fprintf(stderr, "Falha ao salvar a imagem laplace como BMP.\n");
    }
    
    */
    //--- Processamento e Salvar Imagem GX (raw) ---
    printf("\n--- Processando Imagem com filtro GX ---\n");
    // Passamos a matriz imagem_gx_raw para que os resultados sejam armazenados nela
    processar_imagem_e_salvar_gradiente(LEDR_ptr, matrix_size_bits, matriz_sobel_gx, opcode, imagem_gx_raw);
    if (salvar_imagem_bmp("imagem_gx_sobel.bmp", imagem_gx_raw) == 0) {
        printf("Imagem GX (raw) salva como 'imagem_gx_sobel.bmp'\n");
    } else {
        fprintf(stderr, "Falha ao salvar a imagem GX (raw) como BMP.\n");
}

    // --- Processamento e Salvar Imagem GY (raw) ---
    printf("\n--- Processando Imagem com filtro GY ---\n");
    // Passamos a matriz imagem_gy_raw para que os resultados sejam armazenados nela
    processar_imagem_e_salvar_gradiente(LEDR_ptr, matrix_size_bits, matriz_sobel_gy, opcode, imagem_gy_raw);
    if (salvar_imagem_bmp("imagem_gy_sobel.bmp", imagem_gy_raw) == 0) {
        printf("Imagem GY (raw) salva como 'imagem_gy_sobel.bmp'\n");
    } else {
        fprintf(stderr, "Falha ao salvar a imagem GY (raw) como BMP.\n");
    }
    
    // --- Combinar Gx e Gy para Imagem Final de Bordas ---
    // Escolha um limiar adequado. Valores comuns estão entre 50 e 150 para imagens 0-255.
    // Você pode precisar ajustar isso para sua imagem específica.
    uint8_t threshold = 255; // Limiar para binarização da magnitude do gradiente
    combinar_gradientes_e_limiar("imagem_sobel_final.bmp", threshold);

    // --- Desmapeamento de Memória ---
    munmap(LW_virtual, LW_BRIDGE_SPAN);
    close(fd);
    return 0;
}
