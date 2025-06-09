#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define LARGURA 320
#define ALTURA 240
#define CABECALHO_BMP 54

uint8_t imagem_gray[ALTURA][LARGURA];

// Funções:
// ******************************************************************************

// Função para obter valor de pixel com verificação de borda
uint8_t get_valor(uint8_t matriz[ALTURA][LARGURA], int i, int j) {
    if (i >= 0 && i < ALTURA && j >= 0 && j < LARGURA) {
        return matriz[i][j];
    } else {
        return 0;
    }
}


// Submatrizes 2x2
void matriz_2x2(uint8_t imagem_gray[ALTURA][LARGURA], int filtro) {
    for (int i = 0; i < ALTURA - 1; i++) {
        for (int j = 0; j < LARGURA - 1; j++) {
            int submatriz[5][5] = {0}; // Inicializa com zeros

            // Preenche a parte 2x2 no canto superior esquerdo
            for (int dx = 0; dx < 2; dx++) {
                for (int dy = 0; dy < 2; dy++) {
                    submatriz[dx][dy] = get_valor(imagem_gray, i + dx, j + dy);
                }
            }

            printf("Submatriz 2x2 (em 5x5) iniciando em (%d, %d):\n", i, j);
            for (int x = 0; x < 5; x++) {
                for (int y = 0; y < 5; y++) {
                    printf("%3d ", submatriz[x][y]);
                }
                printf("\n");
            }
            printf("\n");
        }
    }
}


// Submatrizes 3x3 centradas em cada pixel
void matriz_3x3(uint8_t imagem_gray[ALTURA][LARGURA], int filtro) {
    for (int i = 0; i < ALTURA; i++) {
        for (int j = 0; j < LARGURA; j++) {
            int submatriz[5][5] = {0}; // Inicializa com zeros

            // Preenche a parte 3x3 centralizada na 5x5 (inicia em [1][1])
            for (int dx = -1; dx <= 1; dx++) {
                for (int dy = -1; dy <= 1; dy++) {
                    submatriz[dx + 2][dy + 2] = get_valor(imagem_gray, i + dx, j + dy);
                }
            }

            printf("Submatriz 3x3 (em 5x5) centrada em (%d, %d):\n", i, j);
            for (int x = 0; x < 5; x++) {
                for (int y = 0; y < 5; y++) {
                    printf("%3d ", submatriz[x][y]);
                }
                printf("\n");
            }
            printf("\n");
        }
    }
}


// Submatrizes 5x5 centradas em cada pixel
void matriz_5x5(uint8_t imagem_gray[ALTURA][LARGURA], int filtro) {

    uint8_t matriz[5][5] = { 
        [-5, -8, -10, -8, -5],
        [-4, -10, -20, -10, -4],
        [ 0,   0,   0,   0,  0],
        [ 4,  10,  20,  10,  4],
        [ 5,   8,  10,  8,  5] 
    };

    for (int i = 0; i < ALTURA; i++) {
        for (int j = 0; j < LARGURA; j++) {
            int submatriz[5][5];
            for (int dx = -2; dx <= 2; dx++) {
                for (int dy = -2; dy <= 2; dy++) {
                    submatriz[dx + 2][dy + 2] = get_valor(imagem_gray, i + dx, j + dy);
                }
            }

            printf("Submatriz 5x5 centrada em (%d, %d):\n", i, j);
            for (int x = 0; x < 5; x++) {
                for (int y = 0; y < 5; y++) {
                    printf("%3d ", submatriz[x][y]);
                }
                printf("\n");
            }
            printf("\n");
        }
    }
}

// ******************************************************************************

int main() {
    // Busca arquivo
    FILE *arquivo_bmp = fopen("Fototestez.bmp", "rb");
    // Verifica se o arquivo existe.
    if (!arquivo_bmp) {
        perror("Erro ao abrir o arquivo BMP");
        return 1;
    }

    // Ignora cabeçalho do BMP (assumido como 54 bytes para BMP 24 bits)
    fseek(arquivo_bmp, CABECALHO_BMP, SEEK_SET);

    // Cada linha do BMP é alinhada em múltiplos de 4 bytes
    int padding = (4 - (LARGURA * 3) % 4) % 4;

    for (int i = ALTURA - 1; i >= 0; i--) {  // BMP é salvo de baixo pra cima
        for (int j = 0; j < LARGURA; j++) {
            unsigned char b = fgetc(arquivo_bmp);
            unsigned char g = fgetc(arquivo_bmp);
            unsigned char r = fgetc(arquivo_bmp);

            uint8_t gray = (uint8_t)(0.299 * r + 0.587 * g + 0.114 * b);
            imagem_gray[i][j] = gray;
        }
        // Pula o padding no final de cada linha
        fseek(arquivo_bmp, padding, SEEK_CUR);
    }

    fclose(arquivo_bmp);

    // Salva como texto pra depuração.
    FILE *saida_txt = fopen("imagem_gray.txt", "w");
    if (!saida_txt) {
        perror("Erro ao criar arquivo de saída texto");
        return 1;
    }

    fclose(saida_txt);


    // Salva como arquivo binário
    FILE *saida_bin = fopen("imagem_gray.bin", "wb");
    if (!saida_bin) {
        perror("Erro ao criar arquivo binário");
        return 1;
    }

    fwrite(imagem_gray, sizeof(uint8_t), ALTURA * LARGURA, saida_bin);
    fclose(saida_bin);

    printf("Conversao para escala de cinza concluida. Arquivos gerados.\n");

    // Chamada para processar submatrizes
    char filtro;
    int flag = 0;
    while(!flag){
        

    }

    printf("Processando submatrizes 2x2...\n");
    matriz_2x2(imagem_gray, filtro);

    printf("Processando submatrizes 3x3...\n");
    matriz_3x3(imagem_gray, filtro);

    printf("Processando submatrizes 5x5...\n");
    matriz_5x5(imagem_gray, filtro);

    return 0;
}
