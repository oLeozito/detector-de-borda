#include <stdio.h>
#include <stdint.h>

// Definições
#define LARGURA 320
#define ALTURA 240

// A imagem é uma matriz de inteiros de 8 bits
uint8_t imagem_gray[ALTURA][LARGURA];

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
    for (int i = 0; i < ALTURA -1; i++) {
        for (int j = 0; j < LARGURA -1; j++) {
            int submatriz[2][2];
            for (int dx = 0; dx < 2; dx++) {
                for (int dy = 0; dy < 2; dy++) {
                    submatriz[dx][dy] = get_valor(imagem_gray, i + dx, j + dy);
                }
            }

            printf("Submatriz 2x2 iniciando em (%d, %d):\n", i, j);
            for (int x = 0; x < 2; x++) {
                for (int y = 0; y < 2; y++) {
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
            int submatriz[3][3];
            for (int dx = -1; dx <= 1; dx++) {
                for (int dy = -1; dy <= 1; dy++) {
                    submatriz[dx + 1][dy + 1] = get_valor(imagem_gray, i + dx, j + dy);
                }
            }

            printf("Submatriz 3x3 centrada em (%d, %d):\n", i, j);
            for (int x = 0; x < 3; x++) {
                for (int y = 0; y < 3; y++) {
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

int main() {
    
    // Abre um arquivo de uma imagem binária
    FILE *entrada_bin = fopen("imagem_gray.bin", "rb");
    if (!entrada_bin) {
        perror("Erro ao abrir imagem_gray.bin");
        return 1;
    }

    // Lê o arquivo binário e guarda na matriz
    fread(imagem_gray, sizeof(uint8_t), ALTURA * LARGURA, entrada_bin);
    fclose(entrada_bin);

    
    // Chamada para processar submatrizes
    printf("Processando submatrizes 2x2...\n");
    matriz_2x2(imagem_gray, 0);

    printf("Processando submatrizes 3x3...\n");
    matriz_3x3(imagem_gray, 0);

    printf("Processando submatrizes 5x5...\n");
    matriz_5x5(imagem_gray, 0);

    return 0;
}
