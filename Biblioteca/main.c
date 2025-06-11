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

uint8_t imagem_gray[ALTURA][LARGURA];      // Imagem original em escala de cinza 
uint8_t imagem_gx_raw[ALTURA][LARGURA];    // Valores Gx brutos (0-255, após saturação da FPGA) 
uint8_t imagem_gy_raw[ALTURA][LARGURA];    // Valores Gy brutos (0-255, após saturação da FPGA) 
uint8_t imagem_final_borda[ALTURA][LARGURA]; // Imagem de borda final (G = sqrt(Gx^2 + Gy^2)) 

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

 // Função para salvar imagem em escala de cinza no formato de texto caso queira depurar.
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
               
             // O cast para (uint8_t (*)[5])matriz_filtro é importante pra compatibilidade. 
             enviar_dados_para_fpga(LEDR_ptr, submatriz_entrada, (uint8_t (*)[5])matriz_filtro, data_fpga); 
              
             receber_dados_da_fpga(LEDR_ptr, vetorC_resultado); 
             // --- FIM DA COMUNICAÇÃO COM A FPGA --- 

             // Armazena o elemento central da matriz resultante na matriz de gradiente específica.
             saida_gradiente[i][j] = (vetorC_resultado[2][2]*2); 
              
             // Lógica dos LEDs (opcional, pode ser removida para evitar overhead na barra de progresso) 
             *LEDR_ptr |= (1 << 29); 
             print_progress_bar(i * LARGURA + j, ALTURA * LARGURA); 
             *LEDR_ptr &= ~(1 << 31); 
              
         } 
         
     } 
    print_progress_bar(ALTURA * LARGURA, ALTURA * LARGURA); // Força a exibição de 100%
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
             // Se o resultado for maior que 255 seta pra 255, se não deixa o resultado, pois ele nao pode dar negativo
             if (magnitude > limiar) { 
                 imagem_final_borda[i][j] = 255; // Borda (branco) 
             } else { 
                 imagem_final_borda[i][j] = magnitude;    
             } 
             print_progress_bar(i * LARGURA + j, ALTURA * LARGURA); 
         } 
     } 
     print_progress_bar(ALTURA * LARGURA, ALTURA * LARGURA);
     printf("\nCombinação e binarização concluídas.\n"); 

     if (salvar_imagem_bmp(nome_arquivo_saida, imagem_final_borda) == 0) { 
         printf("Imagem final de bordas salva como '%s'\n", nome_arquivo_saida); 
     } else { 
         fprintf(stderr, "Falha ao salvar a imagem final de bordas como BMP.\n"); 
     } 
 } 

 // Função para processar um único filtro e salvar a saída 
 void apply_filter(volatile uint32_t *LEDR_ptr, const char* filter_name, 
                   int8_t (*matriz_filtro_gx)[5][5], int8_t (*matriz_filtro_gy)[5][5], 
                   uint8_t opcode, uint8_t matrix_size_bits, int8_t (*matriz_laplace)[5][5]) { 
      
     printf("\n--- Aplicando Filtro %s ---\n", filter_name); 

     if (strcmp(filter_name, "Laplaciano 5x5") == 0) { 
         // Pra Laplaciano, usamos apenas uma convolução.
         // A saída do Laplaciano já é a imagem final de bordas.
         processar_imagem_e_salvar_gradiente(LEDR_ptr, matrix_size_bits, *matriz_laplace, opcode, imagem_final_borda); 
         char filename_final[256];
         sprintf(filename_final, "Processadas/imagem_laplaciano_final.bmp"); // Caminho para a pasta Processadas
         if (salvar_imagem_bmp(filename_final, imagem_final_borda) == 0) { 
             printf("Imagem Laplaciana salva como '%s'\n", filename_final); 
         } else { 
             fprintf(stderr, "Falha ao salvar a imagem Laplaciana como BMP.\n"); 
         } 
     } else { 
         // Processa GX 
         processar_imagem_e_salvar_gradiente(LEDR_ptr, matrix_size_bits, *matriz_filtro_gx, opcode, imagem_gx_raw); 
         char filename_gx[256]; 
         sprintf(filename_gx, "Intermediarias/imagem_gx_%s.bmp", filter_name); // Caminho para a pasta Intermediarias (G(x) e G(y))
         if (salvar_imagem_bmp(filename_gx, imagem_gx_raw) == 0) { 
             printf("Imagem GX (bruta) salva como '%s'\n", filename_gx); 
         } else { 
             fprintf(stderr, "Falha ao salvar a imagem GX (bruta) como BMP.\n"); 
         } 

         // Processa GY 
         processar_imagem_e_salvar_gradiente(LEDR_ptr, matrix_size_bits, *matriz_filtro_gy, opcode, imagem_gy_raw); 
         char filename_gy[256]; 
         sprintf(filename_gy, "Intermediarias/imagem_gy_%s.bmp", filter_name); // Caminho para a pasta Intermediarias (G(x) e G(y))
         if (salvar_imagem_bmp(filename_gy, imagem_gy_raw) == 0) { 
             printf("Imagem GY (bruta) salva como '%s'\n", filename_gy); 
         } else { 
             fprintf(stderr, "Falha ao salvar a imagem GY (bruta) como BMP.\n"); 
         } 

         // Combina Gx e Gy para a imagem final de bordas 
         uint8_t threshold = 255; // Limiar para binarização da magnitude do gradiente 
         char filename_final[256]; 
         sprintf(filename_final, "Processadas/imagem_%s_final.bmp", filter_name); // Caminho para a pasta Processadas
         combinar_gradientes_e_limiar(filename_final, threshold); 
     } 
 } 


 // --- Função Principal --- 
 int main(void) { 
     int fd = -1; 
     void *LW_virtual = configurar_mapeamento(&fd); 
     if (LW_virtual == NULL) { 
         fprintf(stderr, "Erro ao configurar mapeamento de memória para FPGA.\n"); 
         return -1; 
     } 

     volatile uint32_t *LEDR_ptr = (volatile uint32_t *)(LW_virtual + LEDR_BASE); 

     // Enviando sinal de reset da maquina de estados do gerenciador.
     *LEDR_ptr |= (1 << 29); 
     *LEDR_ptr &= ~(1 << 31); 

     // --- Seleção da Operação --- 
     uint8_t opcode = 5; // Operação de Convolução. (Não faz diferença trocar pois predefinimos como 3'b101 dentro do coprocessador);

     // --- Carregamento da Imagem BMP --- 
     FILE *arquivo_bmp = fopen("imagem1.bmp", "rb"); // Trocar o nome do arquivo se quiser testar outra imagem.
     if (!arquivo_bmp) { 
         perror("Erro ao abrir o arquivo BMP 'imagem1.bmp'"); 
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

     // --- Salvar imagem em escala de cinza para verificação (opcional) --- 
     // Esta imagem não precisa ir para uma pasta específica, pois é a entrada
     if (salvar_imagem_txt("imagem_gray.txt", imagem_gray) == 0) { 
         printf("Imagem em tons de cinza salva como 'imagem_gray.txt' pra depuração.\n"); 
     } else { 
         fprintf(stderr, "Falha ao salvar a imagem em tons de cinza como TXT.\n"); 
     } 


     // #################################################

     // --- Kernels dos Filtros --- 
     // Kernels Sobel 3x3 
     int8_t matriz_sobel3x3_gx[5][5] = { 
         {0,0,0,0,0}, 
         {0,1,0,-1,0}, 
         {0,2,0,-2,0}, 
         {0,1,0,-1,0}, 
         {0,0,0,0,0} 
     }; 
              
     int8_t matriz_sobel3x3_gy[5][5] = { 
         {0,0,0,0,0}, 
         {0,-1,-2,-1,0}, 
         {0,0,0,0,0}, 
         {0,1,2,1,0}, 
         {0,0,0,0,0} 
     }; 

     // Kernels Sobel 5x5 
     int8_t matriz_sobel5x5_gx[5][5] = { 
         {-2,-2,-4,-2,-2}, 
         {-1,-1,-1,-1,-1}, 
         {0, 0, 0, 0, 0}, 
         {1,1,1,1,1}, 
         {2,2,4,2,2} 
     }; 
              
     int8_t matriz_sobel5x5_gy[5][5] = { 
         {-2,-1,0,1,2}, 
         {-2,-1,0,1,2}, 
         {-4,-1,0,1,4}, 
         {-2,-1,0,1,2}, 
         {-2,-1,0,1,2} 
     }; 

     // Kernels Roberts 2x2 em 5x5 pra compatibilidade
     int8_t matriz_roberts_gx[5][5] = { 
         {0,0,0,0,0}, 
         {0,0,0,0,0}, 
         {0,0,1,0,0}, 
         {0,0,0,-1,0}, 
         {0,0,0,0,0} 
     }; 

     int8_t matriz_roberts_gy[5][5] = { 
         {0,0,0,0,0}, 
         {0,0,0,0,0}, 
         {0,0,0,1,0}, 
         {0,0,-1,0,0}, 
         {0,0,0,0,0} 
     }; 

     // Kernels Prewitt 5x5 
     int8_t matriz_prewitt_gx[5][5] = { 
         {0,0,0,0,0}, 
         {-1,0,1,-1,0}, 
         {-1,0,1,-1,0}, 
         {-1,0,1,-1,0}, 
         {0,0,0,0,0} 
     }; 

     int8_t matriz_prewitt_gy[5][5] = { 
         {0,0,0,0,0}, 
         {-1,-1,-1,-1,0}, 
         {0,0,0,0,0}, 
         {1,1,1,1,0}, 
         {0,0,0,0,0} 
     }; 

     // Kernel Laplaciano 5x5 
     int8_t matriz_laplace[5][5] = { 
         {0,0,1,0,0}, 
         {0,1,2,1,0}, 
         {1,2,-16,2,1}, 
         {0,1,2,1,0}, 
         {0,0,1,0,0} 
     }; 

     // #################################################

     int choice; 
     uint8_t matrix_size_bits;  

     do { 
         printf("\n--- Selecione um Filtro para Aplicar ---\n"); 
         printf("1. Sobel 3x3\n"); 
         printf("2. Sobel 5x5 (Expandido)\n"); 
         printf("3. Roberts 2x2\n"); 
         printf("4. Prewitt 5x5\n"); 
         printf("5. Laplaciano 5x5\n"); 
         printf("0. Sair\n"); 
         printf("Digite sua escolha: "); 
         scanf("%d", &choice); 

         switch (choice) { 
             case 1: 
                 matrix_size_bits = 0b01; // Para 3x3 
                 apply_filter(LEDR_ptr, "sobel3x3", &matriz_sobel3x3_gx, &matriz_sobel3x3_gy, opcode, matrix_size_bits, NULL); 
                 break; 
             case 2: 
                 matrix_size_bits = 0b11; // Para 5x5 
                 apply_filter(LEDR_ptr, "sobel5x5", &matriz_sobel5x5_gx, &matriz_sobel5x5_gy, opcode, matrix_size_bits, NULL); 
                 break; 
             case 3: 
                 matrix_size_bits = 0b00; // Para 2x2 
                 apply_filter(LEDR_ptr, "roberts", &matriz_roberts_gx, &matriz_roberts_gy, opcode, matrix_size_bits, NULL); 
                 break; 
             case 4: 
                 matrix_size_bits = 0b11; // Para 5x5 
                 apply_filter(LEDR_ptr, "prewitt", &matriz_prewitt_gx, &matriz_prewitt_gy, opcode, matrix_size_bits, NULL); 
                 break; 
             case 5: 
                 matrix_size_bits = 0b11; // Para 5x5 
                 apply_filter(LEDR_ptr, "Laplaciano 5x5", NULL, NULL, opcode, matrix_size_bits, &matriz_laplace); 
                 break; 
             case 0: 
                 printf("Saindo do programa...\n"); 
                 break; 
             default: 
                 printf("Escolha inválida. Por favor, tente novamente.\n"); 
                 break; 
         } 
     } while (choice != 0); 

     // --- Desmapeamento de Memória --- 
     munmap(LW_virtual, LW_BRIDGE_SPAN); 
     close(fd); 
     return 0; 
 }