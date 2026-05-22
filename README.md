# Trabalho 02 - Simulador do algoritmo de Tomasulo

Simulador em C++ do algoritmo de Tomasulo para instrucoes de ponto flutuante no estilo MIPS. O programa le um arquivo `.txt`, executa as instrucoes ciclo a ciclo e imprime as tabelas usadas nos slides: `Instruction status`, `Reorder buffer`, `Reservation stations` e `FP registers status`.

## Como compilar

No Windows com `g++`:

```powershell
g++ -std=c++17 -O2 -Wall -Wextra .\src\main.cpp -o .\tomasulo.exe
```

No Developer Command Prompt do Visual Studio/MSVC:

```bat
cl /std:c++17 /EHsc /W4 /permissive- src\main.cpp /Fe:tomasulo.exe
```

Em Linux/macOS:

```bash
g++ -std=c++17 -O2 -Wall -Wextra ./src/main.cpp -o ./tomasulo
```

## Como executar

Execucao completa, imprimindo todos os ciclos:

```powershell
.\tomasulo.exe .\examples\hennessy.txt
```

Execucao pausada ciclo a ciclo:

```powershell
.\tomasulo.exe .\examples\hennessy.txt --step
```

Resumo final sem tabelas intermediarias:

```powershell
.\tomasulo.exe .\examples\hennessy.txt --quiet
```

Exemplo menor com `S.D` e commit em memoria:

```powershell
.\tomasulo.exe .\examples\store.txt --quiet
```

Rodar a bateria completa de testes:

```powershell
.\run_tests.ps1
```

Listar os testes disponiveis:

```powershell
.\run_one_test.ps1 -List
```

Rodar apenas um teste mostrando o passo a passo completo, ciclo por ciclo, sem pausar:

```powershell
.\run_one_test.ps1 03
.\run_one_test.ps1 waw
.\run_one_test.ps1 hennessy
```

Rodar apenas um teste pausando a cada ciclo:

```powershell
.\run_one_test.ps1 03 -Step
```

Rodar apenas um teste em modo resumo:

```powershell
.\run_one_test.ps1 03 -Quiet
```

Se o Windows bloquear scripts PowerShell, use apenas nesta execucao:

```powershell
powershell.exe -ExecutionPolicy Bypass -File .\run_tests.ps1
powershell.exe -ExecutionPolicy Bypass -File .\run_one_test.ps1 03 -Step
```

## Formato da entrada

O arquivo de entrada aceita comentarios com `#` ou `//`, configuracoes opcionais, valores iniciais e instrucoes.

Exemplo:

```txt
CONFIG ROB_ENTRIES 6
CONFIG LOAD_BUFFERS 2
CONFIG ADD_RS 3
CONFIG MULT_RS 2
CONFIG LOAD_LATENCY 2
CONFIG MUL_LATENCY 10

R2 = 0
R3 = 0
F4 = 2
MEM[32] = 10
MEM[44] = 20

L.D   F6, 32(R2)
L.D   F2, 44(R3)
MUL.D F0, F2, F4
SUB.D F8, F2, F6
DIV.D F10, F0, F6
ADD.D F6, F8, F2
```

Instrucoes implementadas:

- `L.D Fd, deslocamento(Rb)`
- `S.D Fs, deslocamento(Rb)`
- `ADD.D Fd, Fs, Ft`
- `SUB.D Fd, Fs, Ft`
- `MUL.D Fd, Fs, Ft`
- `DIV.D Fd, Fs, Ft`

Validacoes automaticas:

- `EXPECT Fd = valor`
- `EXPECT Rd = valor`
- `EXPECT MEM[endereco] = valor`
- `EXPECT CYCLES = valor`
- `EXPECT ISSUE N = ciclo`
- `EXPECT EXECUTE N = ciclo` ou `EXPECT EXECUTE N = inicio-fim`
- `EXPECT WRITE N = ciclo`
- `EXPECT COMMIT N = ciclo`

Nas validacoes por instrucao, `N` e a posicao da instrucao no arquivo, com a primeira instrucao sendo `1`.

Quando um arquivo contem `EXPECT`, o simulador imprime um `Validation report (EXPECT)` e retorna erro se algum valor esperado nao bater com o valor obtido.

Configuracoes aceitas:

- Estruturas: `ROB_ENTRIES`, `LOAD_BUFFERS`, `STORE_BUFFERS`, `ADD_RS`, `MULT_RS`
- Larguras: `ISSUE_WIDTH`, `COMMIT_WIDTH`, `CDB_WIDTH`
- Latencias: `LOAD_LATENCY`, `STORE_LATENCY`, `ADD_LATENCY`, `SUB_LATENCY`, `MUL_LATENCY`, `DIV_LATENCY`
- Unidades funcionais: `LOAD_UNITS`, `STORE_UNITS`, `ADD_UNITS`, `MULT_UNITS`

Valores padrao: 6 entradas no ROB, 2 load buffers, 2 store buffers, 3 estacoes de soma/subtracao, 2 estacoes de multiplicacao/divisao, issue/commit/CDB de largura 1, latencias `Load=2`, `Store=2`, `Add=2`, `Sub=2`, `Mul=10`, `Div=40`.

## Aspectos implementados do Tomasulo

- Busca sequencial na fila de instrucoes e emissao (`Issue`) respeitando hazards estruturais.
- Estacoes de reserva separadas para `Load`, `Store`, `Add` e `Mult`.
- Renomeacao de registradores por tags do `Reorder buffer`.
- Tratamento de dependencia verdadeira (`RAW`) por `Qj` e `Qk`.
- Eliminacao de `WAR` e `WAW` pela renomeacao via ROB.
- Execucao fora de ordem quando os operandos ficam prontos.
- Escrita de resultados pelo `Common data bus (CDB)`.
- `Reorder buffer` com `Commit` em ordem, garantindo estado arquitetural preciso.
- Loads e stores em memoria, com stores gravando na memoria apenas no `Commit`.
- Loads mais novos aguardam stores mais antigos para o mesmo endereco, preservando a ordem correta de memoria.
- Relatorio final dos registradores usados nas instrucoes e das posicoes de memoria acessadas.
- Validacao automatica por `EXPECT`, usada nos arquivos de teste.

## Correspondencia com a nomenclatura dos slides

| Nome nos slides | Nome usado no simulador | Observacao |
| --- | --- | --- |
| `Instruction status` | `Instruction status` | Mostra `Issue`, `Execute`, `Write result` e tambem `Commit`, pois o simulador usa ROB. |
| `Issue` | `Issue` | Ciclo em que a instrucao entra em uma estacao de reserva e ganha uma entrada no ROB. |
| `Execute` | `Execute` | Intervalo de ciclos em que a instrucao ocupa a unidade funcional. |
| `Write result` | `Write result/CDB` | Ciclo em que o resultado e publicado no CDB ou fica pronto no ROB, no caso de store. |
| `Reorder buffer` | `Reorder buffer` | Tabela com `Entry`, `Busy`, `Instruction`, `State`, `Destination`, `Value`. |
| `Reservation stations` | `Reservation stations` | Tabela com `Name`, `Busy`, `Op`, `Vj`, `Vk`, `Qj`, `Qk`, `Dest`, `A`. |
| `Name` | `Load1`, `Load2`, `Store1`, `Add1`, `Mult1` | Mesma ideia dos nomes `Load1`, `Add1`, `Mult1` mostrados nos slides. |
| `Vj`, `Vk` | `Vj`, `Vk` | Valores dos operandos quando ja estao disponiveis. |
| `Qj`, `Qk` | `Qj`, `Qk` | Tags do ROB (`#1`, `#2`, etc.) quando o operando ainda depende de outra instrucao. |
| `Dest` | `Dest` | Entrada do ROB que recebera o resultado da estacao. |
| `A` | `A` | Expressao do endereco efetivo em loads/stores, como `32 + Regs[R2]`. |
| `FP registers status` | `FP registers status` | Tabela com `Field`, registradores, `Reorder #` e `Busy`. |
| `Register status / Qi` | `FP registers status / Reorder #` | Nos slides sem ROB, `Qi` aponta para a estacao produtora. Com ROB, o equivalente e o numero da entrada do ROB que produz o registrador. |
| `Common data bus (CDB)` | `Write result/CDB` | Evento impresso no ciclo em que a difusao acontece. |

Assim, as nomenclaturas usadas pelo simulador sao correspondentes as dos prints. A diferenca principal e que o simulador adota a versao com `Reorder buffer`; por isso o estado dos registradores usa `Reorder #` em vez de apenas `Qi`.

## Organizacao do codigo

- `src/main.cpp`: parser da entrada, estruturas do simulador, ciclo principal e impressao das tabelas.
- `examples/hennessy.txt`: exemplo baseado nas instrucoes dos slides.
- `examples/store.txt`: exemplo curto com load, soma e store.
- `tests/*.txt`: bateria com 25 testes para RAW, WAR, WAW, CDB, ROB cheio, buffer cheio, stores, loads, memoria, decimais e wrap-around do ROB.
- `docs/VALIDACAO.md`: roteiro para demonstrar funcionamento, nomenclaturas e testes.
- `run_tests.ps1`: script para executar todos os testes com `EXPECT`.
- `run_one_test.ps1`: script para executar apenas um teste por vez, com passo a passo completo ou pausado.

O ciclo principal executa as fases nesta ordem:

1. `Commit`: confirma em ordem a cabeca do ROB quando ela esta pronta.
2. `Execute`: inicia ou avanca instrucoes prontas nas unidades funcionais.
3. `Write result`: publica resultados no CDB e atualiza dependentes.
4. `Issue`: tenta emitir novas instrucoes para as estacoes de reserva.

Essa ordem impede que uma instrucao emitida em um ciclo execute no mesmo ciclo e faz com que dependencias acordadas pelo CDB comecem a executar apenas em ciclos seguintes.
