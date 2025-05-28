import numpy as np
import matplotlib.pyplot as plt

# Ler o arquivo de texto com os valores da imagem em escala de cinza
imagem = np.loadtxt('.../imagem_gray.txt')

plt.imshow(imagem, cmap='gray', vmin=0, vmax=255)
plt.title('Imagem em escala de cinza')
plt.show()
