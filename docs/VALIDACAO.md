# Validacao do simulador

Este documento mostra como provar que o simulador esta executando o algoritmo de Tomasulo corretamente e usando a nomenclatura dos slides.

## 1. Demonstracao ciclo a ciclo

Para mostrar o passo a passo completo:

```powershell
.\tomasulo.exe .\examples\hennessy.txt --step
```

Em cada ciclo aparecem as mesmas estruturas dos slides:

- `Instruction status`: `Issue`, `Execute`, `Write result`, `Commit`.
- `Reorder buffer`: `Entry`, `Busy`, `Instruction`, `State`, `Destination`, `Value`.
- `Reservation stations`: `Name`, `Busy`, `Op`, `Vj`, `Vk`, `Qj`, `Qk`, `Dest`, `A`.
- `FP registers status`: `Field`, registradores, `Reorder #`, `Busy`.

O que observar:

- Quando um operando esta pronto, aparece em `Vj` ou `Vk`.
- Quando um operando ainda depende de outra instrucao, aparece em `Qj` ou `Qk` como tag do ROB, por exemplo `#3`.
- Quando a unidade funcional termina, o evento aparece como `Write result/CDB`.
- O `Commit` acontece em ordem no `Reorder buffer`, mesmo que o `Write result` aconteca fora de ordem.

## 2. Validacao automatica com EXPECT

Os arquivos de teste usam linhas `EXPECT` para verificar resultados finais e quantidade de ciclos:

```txt
EXPECT F10 = 13
EXPECT MEM[108] = 7
EXPECT CYCLES = 12
```

Ao final da execucao, o simulador imprime:

```txt
Validation report (EXPECT)
Check  Expected  Actual  Status
...
Resultado: todos os EXPECT passaram.
```

Se algum valor estiver errado, o status vira `FAIL` e o programa retorna codigo de erro.

## 3. Rodar todos os testes

Compile:

```powershell
cl /std:c++17 /EHsc /W4 /permissive- src\main.cpp /Fe:tomasulo.exe
```

Rode a suite:

```powershell
.\run_tests.ps1
```

Se a politica de execucao do Windows bloquear scripts:

```powershell
powershell.exe -ExecutionPolicy Bypass -File .\run_tests.ps1
```

Tambem e possivel rodar um teste isolado:

```powershell
.\tomasulo.exe .\tests\03_waw_false_dependency.txt --quiet
```

## 4. Matriz de testes

| Arquivo | O que valida |
| --- | --- |
| `examples/hennessy.txt` | Sequencia classica dos slides: loads, `MUL.D`, `SUB.D`, `DIV.D`, `ADD.D`, ROB e CDB. |
| `examples/store.txt` | Store buffer, `S.D`, commit em memoria e resultado final em `Mem[...]`. |
| `tests/01_independent_out_of_order.txt` | Instrucoes independentes, issue multiplo e execucao fora de ordem. |
| `tests/02_raw_chain.txt` | Dependencia verdadeira RAW em cadeia. |
| `tests/03_waw_false_dependency.txt` | Dependencia falsa WAW resolvida por renomeacao via ROB. |
| `tests/04_war_false_dependency.txt` | Dependencia falsa WAR resolvida porque a estacao captura/tagueia operandos no issue. |
| `tests/05_load_store_raw.txt` | Store esperando valor produzido por load antes de gravar na memoria. |
| `tests/06_rob_full_structural_hazard.txt` | Hazard estrutural por ROB cheio; nova instrucao espera entrada livre. |
| `tests/07_cdb_contention.txt` | Contencao no `Common data bus (CDB)` com `CDB_WIDTH = 1`. |
| `tests/08_negative_offset_division.txt` | Endereco com deslocamento negativo, `DIV.D` e cadeia de dependencias. |
| `tests/09_memory_same_address_order.txt` | Load mais novo espera store mais antigo para o mesmo endereco. |
| `tests/10_memory_different_address_no_block.txt` | Load pode prosseguir quando store mais antigo usa outro endereco. |

## 5. Relacao com dependencias

- RAW: aparece quando `Qj`/`Qk` fica preenchido ate o produtor publicar no CDB.
- WAW: dois ROBs diferentes apontam para o mesmo registrador, mas `FP registers status` guarda apenas o produtor mais novo.
- WAR: uma instrucao mais antiga mantem seu valor em `Vj`/`Vk` ou sua tag em `Qj`/`Qk`, entao uma escrita mais nova no mesmo registrador nao altera a leitura antiga.
- Hazards estruturais: aparecem como eventos `Issue parado`, por falta de estacao de reserva ou ROB cheio.
- Memoria: stores escrevem na memoria apenas no `Commit`; loads aguardam stores mais antigos para o mesmo endereco.
