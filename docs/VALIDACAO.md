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

Os arquivos de teste usam linhas `EXPECT` para verificar resultados finais, quantidade de ciclos e etapas da tabela `Instruction status`:

```txt
EXPECT F10 = 13
EXPECT MEM[108] = 7
EXPECT CYCLES = 12
EXPECT ISSUE 3 = 5
EXPECT EXECUTE 3 = 7-10
EXPECT WRITE 3 = 11
EXPECT COMMIT 3 = 12
```

Nas validacoes por instrucao, o numero depois do campo e a posicao da instrucao no arquivo. Por exemplo, `EXPECT EXECUTE 3 = 7-10` valida que a terceira instrucao executou do ciclo 7 ao ciclo 10.

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

## 4. Rodar um teste por vez com passo a passo

Liste todos os testes disponiveis:

```powershell
.\run_one_test.ps1 -List
```

Rode um teste especifico mostrando todos os ciclos, com todas as tabelas:

```powershell
.\run_one_test.ps1 03
.\run_one_test.ps1 waw
.\run_one_test.ps1 hennessy
```

Rode pausando a cada ciclo, ideal para apresentar em sala:

```powershell
.\run_one_test.ps1 03 -Step
```

Rode apenas o resumo final e o `Validation report`:

```powershell
.\run_one_test.ps1 03 -Quiet
```

Se a politica de execucao do Windows bloquear scripts:

```powershell
powershell.exe -ExecutionPolicy Bypass -File .\run_one_test.ps1 03 -Step
```

## 5. Matriz de testes

| Arquivo | O que valida |
| --- | --- |
| `examples/hennessy.txt` | Sequencia classica dos slides: loads, `MUL.D`, `SUB.D`, `DIV.D`, `ADD.D`, ROB e CDB. |
| `examples/store.txt` | Store buffer, `S.D`, commit em memoria e resultado final em `Mem[...]`. |
| `tests/01_independent_out_of_order.txt` | Instrucoes independentes, issue multiplo e execucao fora de ordem. |
| `tests/02_raw_chain.txt` | Dependencia verdadeira RAW em cadeia. |
| `tests/03_waw_false_dependency.txt` | Dependencia falsa WAW resolvida por renomeacao via ROB. Exemplo: `.\run_one_test.ps1 03 -Step`. |
| `tests/04_war_false_dependency.txt` | Dependencia falsa WAR resolvida porque a estacao captura/tagueia operandos no issue. |
| `tests/05_load_store_raw.txt` | Store esperando valor produzido por load antes de gravar na memoria. |
| `tests/06_rob_full_structural_hazard.txt` | Hazard estrutural por ROB cheio; nova instrucao espera entrada livre. |
| `tests/07_cdb_contention.txt` | Contencao no `Common data bus (CDB)` com `CDB_WIDTH = 1`. |
| `tests/08_negative_offset_division.txt` | Endereco com deslocamento negativo, `DIV.D` e cadeia de dependencias. |
| `tests/09_memory_same_address_order.txt` | Load mais novo espera store mais antigo para o mesmo endereco. |
| `tests/10_memory_different_address_no_block.txt` | Load pode prosseguir quando store mais antigo usa outro endereco. |
| `tests/11_same_register_sources.txt` | Mesmo registrador usado em `Vj` e `Vk`, inclusive dependencia duplicada por tag. |
| `tests/12_self_overwrite_raw.txt` | Registrador usado como origem e destino na mesma instrucao. |
| `tests/13_multiple_waw_chain.txt` | Tres WAW seguidos no mesmo registrador e consumo apenas da ultima versao. |
| `tests/14_load_uninitialized_memory.txt` | Load de memoria nao inicializada, tratada como valor 0. |
| `tests/15_store_uses_latest_waw_value.txt` | Store depois de WAW grava a versao mais nova do registrador. |
| `tests/16_store_keeps_old_producer.txt` | Store entre duas escritas mantem a tag do produtor antigo correto. |
| `tests/17_load_buffer_full.txt` | Hazard estrutural por `Load buffer` cheio. |
| `tests/18_add_station_full.txt` | Hazard estrutural por estacao `Add` cheia. |
| `tests/19_commit_width_two.txt` | `COMMIT_WIDTH = 2`, duas instrucoes confirmando no mesmo ciclo. |
| `tests/20_long_div_blocks_commit.txt` | Instrucao nova termina antes, mas commit espera DIV antiga no ROB. |
| `tests/21_memory_store_load_store_load.txt` | Sequencia store/load/store/load no mesmo endereco preservando ordem de memoria. |
| `tests/22_decimal_and_negative_values.txt` | Valores decimais e negativos em operacoes de ponto flutuante. |
| `tests/23_rob_wraparound_many_instructions.txt` | ROB pequeno reutilizando entradas apos commits. |
| `tests/24_mixed_all_operations.txt` | Mistura de `L.D`, `S.D`, `ADD.D`, `SUB.D`, `MUL.D`, `DIV.D`. |
| `tests/25_store_buffer_full.txt` | Hazard estrutural por `Store buffer` cheio. |

## 6. Relacao com dependencias

- RAW: aparece quando `Qj`/`Qk` fica preenchido ate o produtor publicar no CDB.
- WAW: dois ROBs diferentes apontam para o mesmo registrador, mas `FP registers status` guarda apenas o produtor mais novo.
- WAR: uma instrucao mais antiga mantem seu valor em `Vj`/`Vk` ou sua tag em `Qj`/`Qk`, entao uma escrita mais nova no mesmo registrador nao altera a leitura antiga.
- Hazards estruturais: aparecem como eventos `Issue parado`, por falta de estacao de reserva ou ROB cheio.
- Memoria: stores escrevem na memoria apenas no `Commit`; loads aguardam stores mais antigos para o mesmo endereco.
