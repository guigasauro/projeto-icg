# Projeto 3 - ICG

## Autores
- ### Guilherme Nogueira
- ### Ivanor Meira

## Tema
- ### Simulação de um Sistema Solar

## Divisão de tarefas
- ### Guilherme
  - Ajuste final da posição dos planetas
  - Ajustes de iluminação
  - Ajuste inicial de posição
- ### Ivanor
  - Criação dos sólidos geométricos
  - Movimentação dos "planetas"
  - Texturas
  - Shaders
 
## Requisitos iniciais

- #### Compilador GCC para C++
  - <a href='https://code.visualstudio.com/docs/cpp/config-mingw'>Tutorial de instalação para Windows</a>
  - Na maioria das distros Linux, o compilador GCC vem instalado por padrão

## Instruções de compilação

- ### Instalação de dependências

  <code>sudo apt install freeglut3-dev libglfw3-dev libglew-dev libglm-dev</code>

- ### Execução do código
  
  <code>g++ main.cpp -o sss -lGLEW -lglfw -lGL -lGLU && ./sss</code>

## Controles

- Números de 1 a 9: Visão centralizada dos astros, do Sol a Netuno, respectivamente
- W: Zoom out
- S: Zoom in
- Seta para cima: aumenta ângulo
- Seta para baixo: diminui ângulo
- Seta para esquerda: rotação a esquerda
- Seta para direita: rotação a direita