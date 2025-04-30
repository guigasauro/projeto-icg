# Projeto 3 - ICG

## Autores
- ### Guilherme Nogueira
- ### Ivanor Meira

## Tema
- ### Simulação de um Sistema Solar
  
  - Descrição:

    Simulação de um sistema solar, com textura dos planetas e duas opções de simulação (sem a Lua)

    - Simulação "real"
 
      Astros têm raio e distância de órbita para o sol reais, porém em escala reduzida. Além disso, há um plano de fundo de estrelas.

    - Simulação de iluminação
   
      Astros têm raio e distância para o sol seguindo a seguinte escala:

        $$\sqrt[3]{d} \cdot C $$
  
        C = escala (<code>radiusScale</code>)
      
        d = distância do planeta para o Sol (<code>realRadius</code>)

      Além disso, o Sol é renderizado como um ponto de luz.


## Screenshots


- #### Simulação real

  ![image](https://github.com/user-attachments/assets/0e674706-3761-4f11-a187-25b179911066)

  ![image](https://github.com/user-attachments/assets/04761e5a-04fa-45cc-ae82-b63b3a018d46)

- #### Simulação de iluminação
  
  ![image](https://github.com/user-attachments/assets/2376eaed-e39c-4e31-9386-9d579b9c8c6a)

  ![image](https://github.com/user-attachments/assets/0dc3c3a7-b4dd-47ee-ba26-0f3781c5ef26)

 
## Requisitos iniciais

- #### Compilador GCC para C++
  - Na maioria das distros Linux, o compilador GCC vem instalado por padrão
 
- #### OpenGL e bibliotecas utilizadas

  - Instalação:

        sudo apt install freeglut3-dev libglfw3-dev libglew-dev libglm-dev

## Instruções de execução

Na pasta do repositório, execute o seguinte comando respectivo à simulação que queira utilizar

  - #### Simulação "real"
    
        ./run_BG.sh

  - #### Simulação de iluminação
    
        ./run_illum.sh

## Controles

- Números de 1 a 9: Visão centralizada dos astros, do Sol a Netuno, respectivamente
- W: Zoom out
- S: Zoom in
- Seta para cima: aumenta ângulo
- Seta para baixo: diminui ângulo
- Seta para esquerda: rotação a esquerda
- Seta para direita: rotação a direita

## Problemas encontrados e pontos a melhorar

O principal problema encontrado foi o fato de que a implementação do background requeria um fragment shader diferente do utilizado na implementação de iluminação. Após diversas tentativas, não foi possível fazer a implementação dos dois juntos.

#### Pontos a melhorar

  - Implementação conjunta do plano de fundo e da iluminação

  - Simular a gravidade com uma malha que distorce de acordo com a massa dos planetas

    - [Inspiração](https://www.youtube.com/watch?v=_YbGWoUaZg0)
   
## Divisão de tarefas

- ### Guilherme
  
  - Ajuste da posição dos planetas
  - Iluminação do sol
  - Shaders de iluminação
    
- ### Ivanor
  
  - Criação dos sólidos geométricos
  - Movimentação dos planetas
  - Texturas
  - Shaders

