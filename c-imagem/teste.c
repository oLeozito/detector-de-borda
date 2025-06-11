#include <stdio.h>
#include <stdlib.h>

#define LARGURA 320
#define ALTURA 240

int main() {
    FILE *arquivo = fopen("imagem_gray.txt", "r");
    if (!arquivo) {
        perror("Erro ao abrir arquivo");
        return 1;
    }

    int imagem[ALTURA][LARGURA];

    // Ler todos os pixels da imagem no arquivo texto
    for (int i = 0; i < ALTURA; i++) {
        for (int j = 0; j < LARGURA; j++) {
            fscanf(arquivo, "%d", &imagem[i][j]);
        }
    }

    fclose(arquivo);

    // Imprimir os 3 primeiros pixels das 3 primeiras linhas
    for (int i = 0; i < 3; i++) {
        printf("Linha %d: ", i + 1);
        for (int j = 0; j < 3; j++) {
            printf("%d ", imagem[i][j]);
        }
        printf("\n");
    }

    return 0;
}
