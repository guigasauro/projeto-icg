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

  - Linux:

    <code>sudo apt install freeglut3-dev libglfw3-dev libglew-dev libglm-dev</code>

  - Windows:
 
    O OpenGL já vem instalado por padrão em versões atuais do Windows, porém certifique-se que sua GPU é compatível com OpenGL e que seus drivers de vídeo estão atualizados 

- ### Execução do código
  
  - Linux:
    
    <code>g++ sim3.cpp -o sim3 -lGLEW -lglfw -lGL -lGLU && ./sim3</code>

## Controles

- Números de 1 a 9: Visão centralizada dos astros, do Sol a Netuno, respectivamente
- W: Zoom out
- S: Zoom in
- W/S + Seta para baixo / Seta pra cima: Ajuste no ângulo vertical da câmera
- Seta pra esquerda / direita: Ajuste no ângulo horizontal da câmera

## Screenshots

- Visão centralizada na Terra

![image](https://github.com/user-attachments/assets/cd52aae3-9fab-4805-9119-03804e55e998)

- Posição inicial

![image](https://github.com/user-attachments/assets/711bbe08-07a1-4ebc-8408-baa0997f63c8)


