#include <stdio.h>
#include <stdint.h>

#define LARGURA 320
#define ALTURA 240

uint8_t imagem_gray[ALTURA][LARGURA];

uint8_t get_valor(uint8_t matriz[ALTURA][LARGURA], int i, int j) {
    if (i >= 0 && i < ALTURA && j >= 0 && j < LARGURA) {
        return matriz[i][j];
    } else {
        return 0;
    }
}

int main() {
    FILE *entrada_bin = fopen("imagem_gray.bin", "rb");
    if (!entrada_bin) {
        perror("Erro ao abrir imagem_gray.bin");
        return 1;
    }

    fread(imagem_gray, sizeof(uint8_t), ALTURA * LARGURA, entrada_bin);
    fclose(entrada_bin);

    // Processar e imprimir submatriz 5x5 centrada em cada pixel da linha 0
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 20; j++) {
            int submatriz[5][5];

            for (int dx = -2; dx <= 2; dx++) {
                for (int dy = -2; dy <= 2; dy++) {
                    submatriz[dx + 2][dy + 2] = get_valor(imagem_gray, i + dx, j + dy);
                }
            }

            // Imprimir submatriz 5x5
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

    return 0;
}
