# Projeto 3 - ICG

## Autores
- ### Guilherme Nogueira
- ### Ivanor Meira

## Tema
- ### Simulação de um Sistema Solar

## Divisão de tarefas
- ### Guilherme
  - Ajuste final da posição dos planetas
  - Texturas e ajustes de iluminação
  - Criação do grid de simulação de gravidade
- ### Ivanor
  - Criação dos sólidos geométricos
  - Movimentação dos "planetas"
  - Ajuste inicial de posição
 
## Requisitos iniciais

- #### Compilador GCC para C++
  - <a href='https://code.visualstudio.com/docs/cpp/config-mingw'>Tutorial de instalação para Windows</a>
  - Na maioria das distros Linux, o compilador GCC vem instalado por padrão

## Instruções de compilação

- ### Instalação de dependências

  <code>sudo apt install freeglut3-dev libglfw3-dev libglew-dev libglm-dev</code>

- ### Execução do código
  
  <code>g++ sim3.cpp -o sim3 -lGLEW -lglfw -lGL -lGLU & ./sim3</code>

## Controles

- Números de 1 a 0: Visão centralizada dos astros, do Sol a Netuno, respectivamente
- W: Zoom out
- S: Zoom in
