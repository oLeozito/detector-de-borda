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

    // Exemplo de preenchimento com valores 1 a 320*240 (só para teste, opcional)
    /*
    for (int i = 0; i < ALTURA; i++) {
        for (int j = 0; j < LARGURA; j++) {
            imagem_gray[i][j] = i * LARGURA + j + 1;
        }
    }
    */

    // Percorrer todos os elementos tratando cada um como centro de uma submatriz 5x5
    for (int i = 0; i < 1; i++) {
        for (int j = 0; j < LARGURA; j++) {

            int submatriz[5][5];

            for (int dx = -2; dx <= 2; dx++) {
                for (int dy = -2; dy <= 2; dy++) {
                    submatriz[dx + 2][dy + 2] = get_valor(imagem_gray, i + dx, j + dy);
                }
            }

            // Exemplo: imprimir o valor central da submatriz
            printf("Centro da submatriz 5x5 em (%d, %d): %d\n", i, j, submatriz[2][2]);
        }
    }

    return 0;
}
